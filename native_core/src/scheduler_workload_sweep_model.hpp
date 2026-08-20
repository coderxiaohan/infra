#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::scheduler_workload_sweep {

struct WorkloadCase {
  std::string name;
  std::vector<InferenceRequest> requests;
};

struct WorkloadResult {
  WorkloadCase workload;
  SchedulerPressureProfile profile;
  std::vector<RequestLifecycleMetrics> requests;
  int prompt_tokens = 0;
  int requested_decode_tokens = 0;
  double prompt_to_decode_ratio = 0.0;
  std::size_t step_budget = 0U;
};

struct TradeoffSummary {
  std::size_t best_tokens_per_step_index = 0U;
  std::size_t lowest_average_ttft_index = 0U;
  bool lowest_ttft_filtered_to_no_rejection = false;
  std::size_t highest_average_queue_wait_index = 0U;
  std::size_t highest_peak_used_blocks_index = 0U;
  std::size_t highest_rejected_index = 0U;
  int total_rejected_requests = 0;
};

struct SweepPlan {
  PagedKVCacheConfig cache_config;
  int max_active_requests = 0;
  std::vector<WorkloadCase> workloads;
  std::vector<WorkloadResult> results;
  TradeoffSummary tradeoff;
};

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_workload_sweep
