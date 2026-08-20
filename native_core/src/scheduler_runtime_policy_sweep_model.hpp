#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::scheduler_runtime_policy {

struct PolicyConfig {
  std::string name;
  bool enable_backpressure = false;
  int reserve_free_blocks = 0;
  bool preempt_on_append_pressure = false;
  int max_active_requests = 1;
};

struct RuntimeRequest {
  InferenceRequest request;
  RequestState state = RequestState::kWaiting;
  int submitted_step = 0;
  int generated_tokens = 0;
  int first_scheduled_step = -1;
  int first_decode_step = -1;
  int finished_step = -1;
  int preemptions = 0;
  int recompute_tokens = 0;
};

struct RuntimeStepStats {
  int step = 0;
  int admitted = 0;
  int decoded = 0;
  int finished = 0;
  int rejected = 0;
  int preempted = 0;
  int backpressure_deferred = 0;
  int append_pressure_events = 0;
  AllocationStats cache_stats;
};

struct PolicyResult {
  PolicyConfig config;
  SchedulerPressureProfile profile;
  std::vector<RuntimeStepStats> trace;
  std::vector<RuntimeRequest> requests;
  std::vector<RequestLifecycleMetrics> request_metrics;
  int total_preemptions = 0;
  int preempted_request_count = 0;
  int max_preemptions_per_request = 0;
  int total_recompute_tokens = 0;
  int total_backpressure_deferrals = 0;
  int append_pressure_events = 0;
  std::size_t step_budget = 0U;
};

struct TradeoffSummary {
  std::size_t best_finished_policy_index = 0U;
  std::size_t best_tokens_per_step_index = 0U;
  std::size_t lowest_average_ttft_index = 0U;
  std::size_t highest_preemption_index = 0U;
  std::size_t highest_backpressure_deferral_index = 0U;
  int total_backpressure_deferrals = 0;
  int total_preemptions = 0;
  int total_recompute_tokens = 0;
};

struct SweepPlan {
  PagedKVCacheConfig cache_config;
  std::vector<InferenceRequest> workload;
  std::vector<PolicyResult> results;
  TradeoffSummary tradeoff;
};

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_runtime_policy
