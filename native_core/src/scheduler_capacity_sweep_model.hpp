#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::scheduler_capacity_sweep {

struct CapacityResult {
  int num_blocks = 0;
  SchedulerPressureProfile profile;
  std::vector<RequestLifecycleMetrics> requests;
  std::size_t step_budget = 0U;
};

struct ThresholdSummary {
  bool has_no_rejection = false;
  std::size_t minimum_no_rejection_index = 0U;
  std::size_t best_no_rejection_index = 0U;
  std::size_t highest_tokens_per_step_index = 0U;
  std::size_t highest_peak_cache_utilization_index = 0U;
  std::size_t highest_rejected_index = 0U;
  int total_rejected_requests = 0;
};

struct SweepPlan {
  int block_size_tokens = 0;
  int bytes_per_token = 0;
  int max_active_requests = 0;
  std::vector<int> capacity_points;
  std::vector<InferenceRequest> workload;
  std::vector<CapacityResult> results;
  ThresholdSummary threshold;
};

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_capacity_sweep
