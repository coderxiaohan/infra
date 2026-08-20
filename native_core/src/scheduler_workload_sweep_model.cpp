#include "scheduler_workload_sweep_model.hpp"

#include "paged_kv_probe_utils.hpp"
#include "scheduler_probe_utils.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::scheduler_workload_sweep {
namespace {

void validate_inputs(const PagedKVCacheConfig& cache_config,
                     const int max_active_requests,
                     const std::vector<WorkloadCase>& workloads) {
  if (cache_config.block_size_tokens <= 0 ||
      cache_config.bytes_per_token <= 0 || max_active_requests <= 0 ||
      workloads.empty()) {
    throw std::invalid_argument(
        "scheduler workload sweep resources and workloads are invalid");
  }
  (void)probe::require_paged_kv_probe_capacity(
      cache_config.num_blocks, "scheduler workload sweep cache blocks");
  std::unordered_set<std::string> names;
  names.reserve(workloads.size());
  for (const auto& workload : workloads) {
    if (workload.name.empty() ||
        !names.insert(workload.name).second) {
      throw std::invalid_argument(
          "scheduler workload names must be nonempty and unique");
    }
    (void)probe::scheduler_probe_step_budget(workload.requests);
  }
}

WorkloadResult run_workload(
    const WorkloadCase& workload,
    const PagedKVCacheConfig& cache_config,
    const int max_active_requests) {
  int prompt_tokens = 0;
  int requested_decode_tokens = 0;
  for (const auto& request : workload.requests) {
    prompt_tokens = internal::checked_add_int(
        prompt_tokens, request.prompt_tokens,
        "scheduler workload prompt-token total");
    requested_decode_tokens = internal::checked_add_int(
        requested_decode_tokens, request.max_decode_tokens,
        "scheduler workload decode-token total");
  }
  auto execution = probe::run_continuous_scheduler_probe(
      cache_config, max_active_requests, workload.requests);
  const double ratio =
      requested_decode_tokens == 0
          ? 0.0
          : static_cast<double>(prompt_tokens) /
                static_cast<double>(requested_decode_tokens);
  if (!std::isfinite(ratio) || ratio < 0.0) {
    throw std::overflow_error(
        "scheduler workload prompt-to-decode ratio is invalid");
  }
  return {workload, execution.profile, std::move(execution.requests),
          prompt_tokens, requested_decode_tokens, ratio,
          execution.step_budget};
}

template <typename Metric>
std::size_t highest_index(const std::vector<WorkloadResult>& results,
                          Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) > metric(results[best])) {
      best = i;
    }
  }
  return best;
}

std::size_t lowest_ttft_index(
    const std::vector<WorkloadResult>& results,
    const bool filter_to_no_rejection) {
  std::size_t best = results.size();
  for (std::size_t i = 0U; i < results.size(); ++i) {
    if (filter_to_no_rejection &&
        results[i].profile.rejected_request_count != 0) {
      continue;
    }
    if (best == results.size() ||
        results[i].profile.average_ttft_steps <
            results[best].profile.average_ttft_steps) {
      best = i;
    }
  }
  if (best == results.size()) {
    throw std::logic_error(
        "scheduler workload TTFT selection has no candidate");
  }
  return best;
}

TradeoffSummary make_tradeoff_summary(
    const std::vector<WorkloadResult>& results) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "scheduler workload sweep results");
  TradeoffSummary summary;
  summary.best_tokens_per_step_index = highest_index(
      results, [](const auto& result) {
        return result.profile.tokens_per_step;
      });
  for (const auto& result : results) {
    if (result.profile.rejected_request_count == 0) {
      summary.lowest_ttft_filtered_to_no_rejection = true;
      break;
    }
  }
  summary.lowest_average_ttft_index = lowest_ttft_index(
      results, summary.lowest_ttft_filtered_to_no_rejection);
  summary.highest_average_queue_wait_index = highest_index(
      results, [](const auto& result) {
        return result.profile.average_queue_wait_steps;
      });
  summary.highest_peak_used_blocks_index = highest_index(
      results, [](const auto& result) {
        return result.profile.peak_used_blocks;
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
          "workload sweep rejected request total");
  return summary;
}

bool same_case(const WorkloadCase& left, const WorkloadCase& right) {
  return left.name == right.name &&
         probe::same_scheduler_probe_workload(
             left.requests, right.requests);
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) +
                           " is outside result range");
  }
}

void validate_result(const WorkloadResult& result,
                     const WorkloadCase& workload,
                     const PagedKVCacheConfig& cache_config,
                     const int max_active_requests) {
  if (!same_case(result.workload, workload)) {
    throw std::logic_error(
        "scheduler workload result identity is not canonical");
  }
  const auto expected =
      run_workload(workload, cache_config, max_active_requests);
  if (!probe::same_scheduler_probe_pressure_profile(
          result.profile, expected.profile) ||
      !probe::same_scheduler_probe_lifecycle_metrics(
          result.requests, expected.requests) ||
      result.prompt_tokens != expected.prompt_tokens ||
      result.requested_decode_tokens !=
          expected.requested_decode_tokens ||
      result.prompt_to_decode_ratio !=
          expected.prompt_to_decode_ratio ||
      result.step_budget != expected.step_budget) {
    throw std::logic_error(
        "scheduler workload result is not canonical");
  }
}

}  // namespace

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.cache_config = PagedKVCacheConfig{10, 8, 128};
  plan.max_active_requests = 3;
  plan.workloads = {
      {"short_chat",
       {{1, 6, 2}, {2, 5, 2}, {3, 4, 1}, {4, 7, 2}, {5, 3, 1}}},
      {"decode_heavy",
       {{1, 4, 8}, {2, 5, 7}, {3, 6, 6}, {4, 4, 5}, {5, 5, 4}}},
      {"prefill_heavy",
       {{1, 18, 1}, {2, 20, 1}, {3, 16, 2}, {4, 12, 1}, {5, 14, 1}}},
      {"long_tail",
       {{1, 6, 10}, {2, 6, 1}, {3, 6, 1}, {4, 6, 1}, {5, 6, 1}}},
      {"cache_stress",
       {{1, 24, 4}, {2, 24, 4}, {3, 24, 4}, {4, 8, 2}, {5, 8, 2}}},
  };
  validate_inputs(
      plan.cache_config, plan.max_active_requests, plan.workloads);
  plan.results.reserve(plan.workloads.size());
  for (const auto& workload : plan.workloads) {
    plan.results.push_back(run_workload(
        workload, plan.cache_config, plan.max_active_requests));
  }
  plan.tradeoff = make_tradeoff_summary(plan.results);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_inputs(
      plan.cache_config, plan.max_active_requests, plan.workloads);
  if (plan.results.size() != plan.workloads.size()) {
    throw std::logic_error(
        "scheduler workload result count is not aligned");
  }
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    validate_result(
        plan.results[i], plan.workloads[i], plan.cache_config,
        plan.max_active_requests);
  }

  const auto count = plan.results.size();
  require_index(plan.tradeoff.best_tokens_per_step_index, count,
                "best tokens-per-step index");
  require_index(plan.tradeoff.lowest_average_ttft_index, count,
                "lowest average TTFT index");
  require_index(plan.tradeoff.highest_average_queue_wait_index, count,
                "highest average queue-wait index");
  require_index(plan.tradeoff.highest_peak_used_blocks_index, count,
                "highest peak-used-blocks index");
  require_index(plan.tradeoff.highest_rejected_index, count,
                "highest rejected index");

  const auto expected = make_tradeoff_summary(plan.results);
  if (plan.tradeoff.best_tokens_per_step_index !=
          expected.best_tokens_per_step_index ||
      plan.tradeoff.lowest_average_ttft_index !=
          expected.lowest_average_ttft_index ||
      plan.tradeoff.lowest_ttft_filtered_to_no_rejection !=
          expected.lowest_ttft_filtered_to_no_rejection ||
      plan.tradeoff.highest_average_queue_wait_index !=
          expected.highest_average_queue_wait_index ||
      plan.tradeoff.highest_peak_used_blocks_index !=
          expected.highest_peak_used_blocks_index ||
      plan.tradeoff.highest_rejected_index !=
          expected.highest_rejected_index ||
      plan.tradeoff.total_rejected_requests !=
          expected.total_rejected_requests) {
    throw std::logic_error(
        "scheduler workload tradeoff summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_workload_sweep
