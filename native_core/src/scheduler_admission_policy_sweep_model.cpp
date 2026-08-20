#include "scheduler_admission_policy_sweep_model.hpp"

#include "paged_kv_probe_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::scheduler_admission_policy {
namespace {

int total_tokens(const InferenceRequest& request) {
  return probe::inference_request_total_tokens(request);
}

void validate_inputs(const PagedKVCacheConfig& cache_config,
                     const int max_active_requests,
                     const std::vector<InferenceRequest>& workload) {
  if (cache_config.block_size_tokens <= 0 ||
      cache_config.bytes_per_token <= 0 || max_active_requests <= 0 ||
      workload.empty()) {
    throw std::invalid_argument(
        "admission policy scheduler resources and workload are invalid");
  }
  (void)probe::require_paged_kv_probe_capacity(
      cache_config.num_blocks, "admission policy cache block count");
  std::unordered_set<RequestId> request_ids;
  request_ids.reserve(workload.size());
  for (const auto& request : workload) {
    if (request.prompt_tokens < 0 || request.max_decode_tokens < 0) {
      throw std::invalid_argument(
          "admission policy workload tokens must be non-negative");
    }
    if (!request_ids.insert(request.request_id).second) {
      throw std::invalid_argument(
          "admission policy workload request ids must be unique");
    }
  }
}

std::vector<InferenceRequest> order_workload(
    std::vector<InferenceRequest> workload, const Policy policy) {
  switch (policy) {
    case Policy::kFcfs:
      break;
    case Policy::kShortTotalFirst:
      std::stable_sort(
          workload.begin(), workload.end(),
          [](const auto& left, const auto& right) {
            return total_tokens(left) < total_tokens(right);
          });
      break;
    case Policy::kShortPromptFirst:
      std::stable_sort(
          workload.begin(), workload.end(),
          [](const auto& left, const auto& right) {
            return left.prompt_tokens < right.prompt_tokens;
          });
      break;
    case Policy::kShortDecodeFirst:
      std::stable_sort(
          workload.begin(), workload.end(),
          [](const auto& left, const auto& right) {
            return left.max_decode_tokens < right.max_decode_tokens;
          });
      break;
    case Policy::kLongTotalFirst:
      std::stable_sort(
          workload.begin(), workload.end(),
          [](const auto& left, const auto& right) {
            return total_tokens(left) > total_tokens(right);
          });
      break;
  }
  return workload;
}

PolicyResult run_policy(const Policy policy,
                        const std::vector<InferenceRequest>& workload,
                        const PagedKVCacheConfig& cache_config,
                        const int max_active_requests) {
  (void)policy_name(policy);
  const auto ordered = order_workload(workload, policy);
  auto execution = probe::run_continuous_scheduler_probe(
      cache_config, max_active_requests, ordered);
  return {policy, ordered, execution.profile,
          std::move(execution.requests), execution.step_budget};
}

std::size_t best_finished_index(const std::vector<PolicyResult>& results) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (results[i].profile.finished_request_count >
            results[best].profile.finished_request_count ||
        (results[i].profile.finished_request_count ==
             results[best].profile.finished_request_count &&
         results[i].profile.rejected_request_count <
             results[best].profile.rejected_request_count)) {
      best = i;
    }
  }
  return best;
}

template <typename Metric>
std::size_t highest_index(const std::vector<PolicyResult>& results,
                          Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) > metric(results[best])) {
      best = i;
    }
  }
  return best;
}

template <typename Metric>
std::size_t lowest_index(const std::vector<PolicyResult>& results,
                         Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < results.size(); ++i) {
    if (metric(results[i]) < metric(results[best])) {
      best = i;
    }
  }
  return best;
}

TradeoffSummary make_tradeoff_summary(
    const std::vector<PolicyResult>& results) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "admission policy sweep results");
  TradeoffSummary summary;
  summary.best_finished_policy_index = best_finished_index(results);
  summary.best_tokens_per_step_index = highest_index(
      results, [](const auto& result) {
        return result.profile.tokens_per_step;
      });
  summary.lowest_average_queue_wait_index = lowest_index(
      results, [](const auto& result) {
        return result.profile.average_queue_wait_steps;
      });
  summary.lowest_average_ttft_index = lowest_index(
      results, [](const auto& result) {
        return result.profile.average_ttft_steps;
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
          "admission sweep rejected request total");
  return summary;
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(std::string(label) + " is outside result range");
  }
}

}  // namespace

const char* policy_name(const Policy policy) {
  switch (policy) {
    case Policy::kFcfs:
      return "fcfs";
    case Policy::kShortTotalFirst:
      return "short_total_first";
    case Policy::kShortPromptFirst:
      return "short_prompt_first";
    case Policy::kShortDecodeFirst:
      return "short_decode_first";
    case Policy::kLongTotalFirst:
      return "long_total_first";
  }
  throw std::invalid_argument("admission policy is invalid");
}

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.cache_config = PagedKVCacheConfig{8, 8, 128};
  plan.max_active_requests = 3;
  plan.workload = {
      {1, 24, 4},
      {2, 4, 2},
      {3, 8, 8},
      {4, 20, 1},
      {5, 6, 3},
      {6, 12, 6},
  };
  constexpr Policy kPolicies[] = {
      Policy::kFcfs,
      Policy::kShortTotalFirst,
      Policy::kShortPromptFirst,
      Policy::kShortDecodeFirst,
      Policy::kLongTotalFirst,
  };
  validate_inputs(
      plan.cache_config, plan.max_active_requests, plan.workload);
  plan.results.reserve(sizeof(kPolicies) / sizeof(kPolicies[0]));
  for (const auto policy : kPolicies) {
    plan.results.push_back(run_policy(
        policy, plan.workload, plan.cache_config,
        plan.max_active_requests));
  }
  plan.tradeoff = make_tradeoff_summary(plan.results);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_inputs(
      plan.cache_config, plan.max_active_requests, plan.workload);
  if (plan.results.empty()) {
    throw std::logic_error(
        "admission policy plan results must not be empty");
  }
  std::unordered_set<std::string> policies;
  policies.reserve(plan.results.size());
  for (const auto& result : plan.results) {
    if (!policies.insert(policy_name(result.policy)).second) {
      throw std::logic_error(
          "admission policy identities must be unique");
    }
    const auto expected = run_policy(
        result.policy, plan.workload, plan.cache_config,
        plan.max_active_requests);
    if (!probe::same_scheduler_probe_workload(
            result.ordered_workload, expected.ordered_workload) ||
        !probe::same_scheduler_probe_pressure_profile(
            result.profile, expected.profile) ||
        !probe::same_scheduler_probe_lifecycle_metrics(
            result.requests, expected.requests) ||
        result.step_budget != expected.step_budget) {
      throw std::logic_error(
          "admission policy result is not canonical");
    }
  }

  const auto count = plan.results.size();
  require_index(plan.tradeoff.best_finished_policy_index, count,
                "best finished policy index");
  require_index(plan.tradeoff.best_tokens_per_step_index, count,
                "best tokens-per-step index");
  require_index(plan.tradeoff.lowest_average_queue_wait_index, count,
                "lowest average queue-wait index");
  require_index(plan.tradeoff.lowest_average_ttft_index, count,
                "lowest average TTFT index");
  require_index(plan.tradeoff.highest_rejected_index, count,
                "highest rejected index");
  const auto expected = make_tradeoff_summary(plan.results);
  if (plan.tradeoff.best_finished_policy_index !=
          expected.best_finished_policy_index ||
      plan.tradeoff.best_tokens_per_step_index !=
          expected.best_tokens_per_step_index ||
      plan.tradeoff.lowest_average_queue_wait_index !=
          expected.lowest_average_queue_wait_index ||
      plan.tradeoff.lowest_average_ttft_index !=
          expected.lowest_average_ttft_index ||
      plan.tradeoff.highest_rejected_index !=
          expected.highest_rejected_index ||
      plan.tradeoff.total_rejected_requests !=
          expected.total_rejected_requests) {
    throw std::logic_error(
        "admission policy tradeoff summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_admission_policy
