#include "scheduler_capacity_sweep_model.hpp"

#include "paged_kv_probe_utils.hpp"
#include "scheduler_probe_utils.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::scheduler_capacity_sweep {
namespace {

void validate_inputs(const SweepPlan& plan) {
  if (plan.block_size_tokens <= 0 || plan.bytes_per_token <= 0 ||
      plan.max_active_requests <= 0 || plan.capacity_points.empty()) {
    throw std::invalid_argument(
        "scheduler capacity sweep resources are invalid");
  }
  (void)probe::scheduler_probe_step_budget(plan.workload);
  for (std::size_t i = 0U; i < plan.capacity_points.size(); ++i) {
    const int point = plan.capacity_points[i];
    (void)probe::require_paged_kv_probe_capacity(
        point, "scheduler capacity sweep point");
    if (i > 0U && point <= plan.capacity_points[i - 1U]) {
      throw std::invalid_argument(
          "scheduler capacity points must be strictly increasing");
    }
  }
}

CapacityResult run_capacity(
    const int num_blocks, const int block_size_tokens,
    const int bytes_per_token, const int max_active_requests,
    const std::vector<InferenceRequest>& workload) {
  auto execution = probe::run_continuous_scheduler_probe(
      {num_blocks, block_size_tokens, bytes_per_token},
      max_active_requests, workload);
  return {num_blocks, execution.profile, std::move(execution.requests),
          execution.step_budget};
}

template <typename Metric>
std::size_t highest_index(const std::vector<CapacityResult>& results,
                          Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) > metric(results[best])) {
      best = i;
    }
  }
  return best;
}

ThresholdSummary make_threshold_summary(
    const std::vector<CapacityResult>& results) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "capacity sweep results");
  ThresholdSummary summary;
  for (std::size_t i = 0U; i < results.size(); ++i) {
    if (results[i].profile.rejected_request_count != 0) {
      continue;
    }
    if (!summary.has_no_rejection) {
      summary.has_no_rejection = true;
      summary.minimum_no_rejection_index = i;
      summary.best_no_rejection_index = i;
    } else if (results[i].profile.tokens_per_step >
               results[summary.best_no_rejection_index]
                   .profile.tokens_per_step) {
      summary.best_no_rejection_index = i;
    }
  }
  summary.highest_tokens_per_step_index = highest_index(
      results, [](const auto& result) {
        return result.profile.tokens_per_step;
      });
  summary.highest_peak_cache_utilization_index = highest_index(
      results, [](const auto& result) {
        return result.profile.peak_cache_utilization;
      });
  summary.highest_rejected_index = highest_index(
      results, [](const auto& result) {
        return result.profile.rejected_request_count;
      });
  summary.total_rejected_requests =
      probe::checked_scheduler_result_metric_total(
          results,
          [](const auto& result) {
            return result.profile.rejected_request_count;
          },
          "capacity sweep rejected request total");
  return summary;
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) +
                           " is outside result range");
  }
}

void validate_result(const CapacityResult& result, const int capacity_point,
                     const SweepPlan& plan) {
  if (result.num_blocks != capacity_point) {
    throw std::logic_error(
        "scheduler capacity result identity is not aligned");
  }
  const auto expected = run_capacity(
      capacity_point, plan.block_size_tokens, plan.bytes_per_token,
      plan.max_active_requests, plan.workload);
  if (!probe::same_scheduler_probe_pressure_profile(
          result.profile, expected.profile) ||
      !probe::same_scheduler_probe_lifecycle_metrics(
          result.requests, expected.requests) ||
      result.step_budget != expected.step_budget) {
    throw std::logic_error(
        "scheduler capacity result is not canonical");
  }
}

}  // namespace

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.block_size_tokens = 8;
  plan.bytes_per_token = 128;
  plan.max_active_requests = 3;
  plan.capacity_points = {3, 4, 5, 6, 8, 10, 11, 12};
  plan.workload = {
      {1, 24, 4},
      {2, 24, 4},
      {3, 16, 4},
      {4, 12, 2},
      {5, 8, 2},
  };
  validate_inputs(plan);
  plan.results.reserve(plan.capacity_points.size());
  for (const auto point : plan.capacity_points) {
    plan.results.push_back(run_capacity(
        point, plan.block_size_tokens, plan.bytes_per_token,
        plan.max_active_requests, plan.workload));
  }
  plan.threshold = make_threshold_summary(plan.results);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_inputs(plan);
  if (plan.results.size() != plan.capacity_points.size()) {
    throw std::logic_error(
        "scheduler capacity point and result counts are not aligned");
  }
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    validate_result(plan.results[i], plan.capacity_points[i], plan);
  }

  const auto count = plan.results.size();
  require_index(plan.threshold.highest_tokens_per_step_index, count,
                "highest tokens-per-step index");
  require_index(
      plan.threshold.highest_peak_cache_utilization_index, count,
      "highest peak-cache-utilization index");
  require_index(plan.threshold.highest_rejected_index, count,
                "highest rejected index");
  if (plan.threshold.has_no_rejection) {
    require_index(plan.threshold.minimum_no_rejection_index, count,
                  "minimum no-rejection index");
    require_index(plan.threshold.best_no_rejection_index, count,
                  "best no-rejection index");
  }

  const auto expected = make_threshold_summary(plan.results);
  if (plan.threshold.has_no_rejection !=
          expected.has_no_rejection ||
      plan.threshold.minimum_no_rejection_index !=
          expected.minimum_no_rejection_index ||
      plan.threshold.best_no_rejection_index !=
          expected.best_no_rejection_index ||
      plan.threshold.highest_tokens_per_step_index !=
          expected.highest_tokens_per_step_index ||
      plan.threshold.highest_peak_cache_utilization_index !=
          expected.highest_peak_cache_utilization_index ||
      plan.threshold.highest_rejected_index !=
          expected.highest_rejected_index ||
      plan.threshold.total_rejected_requests !=
          expected.total_rejected_requests) {
    throw std::logic_error(
        "scheduler capacity threshold summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_capacity_sweep
