#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::scheduler_pressure_sweep {

struct ScenarioConfig {
  std::string name;
  PagedKVCacheConfig cache_config;
  int max_active_requests = 0;
};

struct ScenarioResult {
  ScenarioConfig config;
  SchedulerPressureProfile profile;
  std::vector<RequestLifecycleMetrics> requests;
  std::size_t step_budget = 0U;
};

struct TradeoffSummary {
  std::size_t best_tokens_per_step_index = 0U;
  std::size_t lowest_average_ttft_index = 0U;
  bool lowest_ttft_filtered_to_no_rejection = false;
  std::size_t highest_peak_used_blocks_index = 0U;
  std::size_t highest_rejected_index = 0U;
  int total_rejected_requests = 0;
};

struct SweepPlan {
  std::vector<InferenceRequest> workload;
  std::vector<ScenarioConfig> scenarios;
  std::vector<ScenarioResult> results;
  TradeoffSummary tradeoff;
};

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_pressure_sweep
