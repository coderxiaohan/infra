#include "scheduler_pressure_sweep_model.hpp"

#include "paged_kv_probe_utils.hpp"
#include "scheduler_probe_utils.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::scheduler_pressure_sweep {
namespace {

bool same_cache_config(const PagedKVCacheConfig& lhs,
                       const PagedKVCacheConfig& rhs) {
  return lhs.num_blocks == rhs.num_blocks &&
         lhs.block_size_tokens == rhs.block_size_tokens &&
         lhs.bytes_per_token == rhs.bytes_per_token;
}

bool same_scenario(const ScenarioConfig& lhs,
                   const ScenarioConfig& rhs) {
  return lhs.name == rhs.name &&
         same_cache_config(lhs.cache_config, rhs.cache_config) &&
         lhs.max_active_requests == rhs.max_active_requests;
}

void validate_inputs(
    const std::vector<InferenceRequest>& workload,
    const std::vector<ScenarioConfig>& scenarios) {
  (void)probe::scheduler_probe_step_budget(workload);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "scheduler pressure scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() ||
        !names.insert(scenario.name).second ||
        scenario.max_active_requests <= 0 ||
        scenario.cache_config.block_size_tokens <= 0 ||
        scenario.cache_config.bytes_per_token <= 0) {
      throw std::invalid_argument(
          "scheduler pressure scenario is invalid");
    }
    (void)probe::require_paged_kv_probe_capacity(
        scenario.cache_config.num_blocks,
        "scheduler pressure scenario cache blocks");
  }
}

ScenarioResult run_scenario(
    const ScenarioConfig& config,
    const std::vector<InferenceRequest>& workload) {
  auto execution = probe::run_continuous_scheduler_probe(
      config.cache_config, config.max_active_requests, workload);
  return {config, execution.profile, std::move(execution.requests),
          execution.step_budget};
}

template <typename Metric>
std::size_t highest_index(const std::vector<ScenarioResult>& results,
                          Metric metric) {
  std::size_t best = 0U;
  for (std::size_t index = 1U; index < results.size(); ++index) {
    if (metric(results[index]) > metric(results[best])) {
      best = index;
    }
  }
  return best;
}

std::size_t lowest_ttft_index(
    const std::vector<ScenarioResult>& results,
    const bool filter_to_no_rejection) {
  std::size_t best = results.size();
  for (std::size_t index = 0U; index < results.size(); ++index) {
    if (filter_to_no_rejection &&
        results[index].profile.rejected_request_count != 0) {
      continue;
    }
    if (best == results.size() ||
        results[index].profile.average_ttft_steps <
            results[best].profile.average_ttft_steps) {
      best = index;
    }
  }
  if (best == results.size()) {
    throw std::logic_error(
        "scheduler pressure TTFT selection has no candidate");
  }
  return best;
}

TradeoffSummary make_tradeoff(
    const std::vector<ScenarioResult>& results) {
  probe::require_nonempty_scheduler_probe_results(
      results.size(), "pressure sweep results");
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
          "pressure sweep rejected request total");
  return summary;
}

void validate_result(
    const ScenarioResult& result, const ScenarioConfig& scenario,
    const std::vector<InferenceRequest>& workload) {
  if (!same_scenario(result.config, scenario)) {
    throw std::logic_error(
        "scheduler pressure result scenario is not canonical");
  }
  const auto expected = run_scenario(scenario, workload);
  if (!probe::same_scheduler_probe_pressure_profile(
          result.profile, expected.profile) ||
      !probe::same_scheduler_probe_lifecycle_metrics(
          result.requests, expected.requests) ||
      result.step_budget != expected.step_budget) {
    throw std::logic_error(
        "scheduler pressure result is not canonical");
  }
}

void require_index(const std::size_t index, const std::size_t count,
                   const char* label) {
  if (index >= count) {
    throw std::logic_error(
        std::string(label) + " is outside result range");
  }
}

}  // namespace

SweepPlan build_default_plan() {
  SweepPlan plan;
  plan.workload = {
      {1, 6, 3},
      {2, 9, 2},
      {3, 4, 4},
      {4, 12, 2},
      {5, 3, 3},
  };
  plan.scenarios = {
      {"serial_latency", {8, 8, 128}, 1},
      {"balanced_batch", {8, 8, 128}, 2},
      {"wide_batch", {8, 8, 128}, 3},
      {"cache_constrained", {3, 8, 128}, 3},
  };
  validate_inputs(plan.workload, plan.scenarios);
  plan.results.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.results.push_back(run_scenario(scenario, plan.workload));
  }
  plan.tradeoff = make_tradeoff(plan.results);
  validate_plan(plan);
  return plan;
}

void validate_plan(const SweepPlan& plan) {
  validate_inputs(plan.workload, plan.scenarios);
  if (plan.results.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "scheduler pressure result count is not aligned");
  }
  for (std::size_t index = 0U; index < plan.results.size(); ++index) {
    validate_result(
        plan.results[index], plan.scenarios[index], plan.workload);
  }
  const std::size_t count = plan.results.size();
  require_index(plan.tradeoff.best_tokens_per_step_index, count,
                "best tokens-per-step index");
  require_index(plan.tradeoff.lowest_average_ttft_index, count,
                "lowest average TTFT index");
  require_index(plan.tradeoff.highest_peak_used_blocks_index, count,
                "highest peak-used-blocks index");
  require_index(plan.tradeoff.highest_rejected_index, count,
                "highest rejected index");
  const auto expected = make_tradeoff(plan.results);
  if (plan.tradeoff.best_tokens_per_step_index !=
          expected.best_tokens_per_step_index ||
      plan.tradeoff.lowest_average_ttft_index !=
          expected.lowest_average_ttft_index ||
      plan.tradeoff.lowest_ttft_filtered_to_no_rejection !=
          expected.lowest_ttft_filtered_to_no_rejection ||
      plan.tradeoff.highest_peak_used_blocks_index !=
          expected.highest_peak_used_blocks_index ||
      plan.tradeoff.highest_rejected_index !=
          expected.highest_rejected_index ||
      plan.tradeoff.total_rejected_requests !=
          expected.total_rejected_requests) {
    throw std::logic_error(
        "scheduler pressure tradeoff summary is not canonical");
  }
}

}  // namespace inferlab::scheduler_pressure_sweep
