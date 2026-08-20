#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::scheduler_admission_policy {

enum class Policy {
  kFcfs,
  kShortTotalFirst,
  kShortPromptFirst,
  kShortDecodeFirst,
  kLongTotalFirst,
};

struct PolicyResult {
  Policy policy = Policy::kFcfs;
  std::vector<InferenceRequest> ordered_workload;
  SchedulerPressureProfile profile;
  std::vector<RequestLifecycleMetrics> requests;
  std::size_t step_budget = 0U;
};

struct TradeoffSummary {
  std::size_t best_finished_policy_index = 0U;
  std::size_t best_tokens_per_step_index = 0U;
  std::size_t lowest_average_queue_wait_index = 0U;
  std::size_t lowest_average_ttft_index = 0U;
  std::size_t highest_rejected_index = 0U;
  int total_rejected_requests = 0;
};

struct SweepPlan {
  PagedKVCacheConfig cache_config;
  int max_active_requests = 0;
  std::vector<InferenceRequest> workload;
  std::vector<PolicyResult> results;
  TradeoffSummary tradeoff;
};

const char* policy_name(Policy policy);

SweepPlan build_default_plan();

void validate_plan(const SweepPlan& plan);

}  // namespace inferlab::scheduler_admission_policy
