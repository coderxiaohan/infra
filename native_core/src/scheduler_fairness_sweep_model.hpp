#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::scheduler_fairness {

enum class Policy {
  kFcfs,
  kShortestDecodeFirst,
  kLongestDecodeFirst,
  kOldestWaitFirst,
  kStarvationGuardedShortest,
};

struct ArrivalRequest {
  InferenceRequest request;
  int submit_step = 0;
};

struct RuntimeRequest {
  InferenceRequest request;
  RequestState state = RequestState::kWaiting;
  int submit_step = 0;
  bool arrived = false;
  int generated_tokens = 0;
  int first_scheduled_step = -1;
  int first_decode_step = -1;
  int finished_step = -1;
};

struct FairnessMetrics {
  int starvation_threshold_steps = 0;
  int starved_request_count = 0;
  RequestId worst_queue_wait_request_id = 0;
  int max_queue_wait_steps = 0;
  double average_queue_wait_steps = 0.0;
  double queue_wait_stddev_steps = 0.0;
  double jain_queue_wait_fairness = 1.0;
  double average_total_latency_steps = 0.0;
  double max_total_latency_steps = 0.0;
};

struct PolicyResult {
  Policy policy = Policy::kFcfs;
  std::vector<SchedulerStepStats> trace;
  std::vector<RuntimeRequest> requests;
  std::vector<RequestLifecycleMetrics> lifecycle_metrics;
  SchedulerPressureProfile profile;
  FairnessMetrics fairness;
  std::size_t step_budget = 0U;
};

struct TradeoffSummary {
  double balanced_jain_floor = 0.5;
  std::size_t balanced_policy_index = 0U;
  std::size_t best_tokens_per_step_index = 0U;
  std::size_t lowest_average_ttft_index = 0U;
  std::size_t highest_jain_fairness_index = 0U;
  std::size_t lowest_starvation_index = 0U;
  std::size_t worst_starvation_index = 0U;
};

struct SweepPlan {
  PagedKVCacheConfig cache_config;
  int max_active_requests = 0;
  int starvation_guard_threshold_steps = 0;
  int starvation_threshold_steps = 0;
  std::vector<ArrivalRequest> workload;
  std::vector<PolicyResult> results;
  TradeoffSummary tradeoff;
};

const char* policy_name(Policy policy);

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_fairness
