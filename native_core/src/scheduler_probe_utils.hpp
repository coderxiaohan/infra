#pragma once

#include "inferlab/continuous_batch_scheduler.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab::probe {

inline constexpr std::size_t kMaxSchedulerProbeTraceSteps = 1048576U;

struct SchedulerProbeExecution {
  std::vector<SchedulerStepStats> trace;
  std::vector<RequestLifecycleMetrics> requests;
  SchedulerPressureProfile profile;
  std::size_t step_budget = 0;
};

inline bool same_scheduler_probe_request(
    const InferenceRequest& left, const InferenceRequest& right) {
  return left.request_id == right.request_id &&
         left.prompt_tokens == right.prompt_tokens &&
         left.max_decode_tokens == right.max_decode_tokens;
}

inline bool same_scheduler_probe_workload(
    const std::vector<InferenceRequest>& left,
    const std::vector<InferenceRequest>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0U; i < left.size(); ++i) {
    if (!same_scheduler_probe_request(left[i], right[i])) {
      return false;
    }
  }
  return true;
}

inline bool same_scheduler_probe_lifecycle_metric(
    const RequestLifecycleMetrics& left,
    const RequestLifecycleMetrics& right) {
  return left.request_id == right.request_id &&
         left.state == right.state &&
         left.prompt_tokens == right.prompt_tokens &&
         left.max_decode_tokens == right.max_decode_tokens &&
         left.submitted_step == right.submitted_step &&
         left.first_scheduled_step == right.first_scheduled_step &&
         left.first_decode_step == right.first_decode_step &&
         left.finished_step == right.finished_step &&
         left.generated_tokens == right.generated_tokens &&
         left.queue_wait_steps == right.queue_wait_steps &&
         left.time_to_first_token_steps ==
             right.time_to_first_token_steps &&
         left.total_latency_steps == right.total_latency_steps &&
         left.decode_steps_per_token == right.decode_steps_per_token;
}

inline bool same_scheduler_probe_lifecycle_metrics(
    const std::vector<RequestLifecycleMetrics>& left,
    const std::vector<RequestLifecycleMetrics>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0U; i < left.size(); ++i) {
    if (!same_scheduler_probe_lifecycle_metric(left[i], right[i])) {
      return false;
    }
  }
  return true;
}

inline bool same_scheduler_probe_pressure_profile(
    const SchedulerPressureProfile& left,
    const SchedulerPressureProfile& right) {
  return left.steps == right.steps &&
         left.request_count == right.request_count &&
         left.finished_request_count == right.finished_request_count &&
         left.rejected_request_count == right.rejected_request_count &&
         left.total_admitted == right.total_admitted &&
         left.total_decoded_tokens == right.total_decoded_tokens &&
         left.total_finished == right.total_finished &&
         left.total_rejected == right.total_rejected &&
         left.total_backpressure_deferred ==
             right.total_backpressure_deferred &&
         left.peak_active_requests == right.peak_active_requests &&
         left.peak_used_blocks == right.peak_used_blocks &&
         left.peak_reserved_bytes == right.peak_reserved_bytes &&
         left.peak_total_tokens == right.peak_total_tokens &&
         left.peak_cache_utilization == right.peak_cache_utilization &&
         left.average_active_requests == right.average_active_requests &&
         left.average_used_blocks == right.average_used_blocks &&
         left.average_cache_utilization ==
             right.average_cache_utilization &&
         left.average_queue_wait_steps ==
             right.average_queue_wait_steps &&
         left.average_ttft_steps == right.average_ttft_steps &&
         left.average_decode_steps_per_token ==
             right.average_decode_steps_per_token &&
         left.tokens_per_step == right.tokens_per_step;
}

inline const char* request_state_name(const RequestState state) {
  switch (state) {
    case RequestState::kWaiting:
      return "waiting";
    case RequestState::kRunning:
      return "running";
    case RequestState::kFinished:
      return "finished";
    case RequestState::kRejected:
      return "rejected";
  }
  throw std::invalid_argument("scheduler probe request state is invalid");
}

template <typename RuntimeRequest, typename RequestIndex>
RuntimeRequest& scheduler_probe_request_at(
    std::vector<RuntimeRequest>& requests, const RequestIndex& index,
    const RequestId request_id) {
  const auto it = index.find(request_id);
  if (it == index.end()) {
    throw std::logic_error("scheduler probe request id is not indexed");
  }
  return requests.at(it->second);
}

template <typename RuntimeRequest, typename RequestIndex>
const RuntimeRequest& scheduler_probe_request_at(
    const std::vector<RuntimeRequest>& requests, const RequestIndex& index,
    const RequestId request_id) {
  const auto it = index.find(request_id);
  if (it == index.end()) {
    throw std::logic_error("scheduler probe request id is not indexed");
  }
  return requests.at(it->second);
}

inline int inference_request_total_tokens(const InferenceRequest& request) {
  if (request.prompt_tokens < 0 || request.max_decode_tokens < 0) {
    throw std::invalid_argument(
        "scheduler probe request token counts must be non-negative");
  }
  return internal::checked_add_int(request.prompt_tokens,
                                   request.max_decode_tokens,
                                   "scheduler probe request total tokens");
}

inline int scheduler_probe_step_delta(const int end_step,
                                      const int begin_step,
                                      const char* name) {
  const auto delta = static_cast<std::int64_t>(end_step) -
                     static_cast<std::int64_t>(begin_step);
  return internal::checked_int_from_i64(delta, name);
}

inline RequestLifecycleMetrics make_scheduler_probe_lifecycle_metrics(
    const InferenceRequest& request, const RequestState state,
    const int submitted_step, const int generated_tokens,
    const int first_scheduled_step, const int first_decode_step,
    const int finished_step) {
  if (request.request_id <= 0) {
    throw std::invalid_argument(
        "scheduler probe lifecycle request_id must be positive");
  }
  (void)request_state_name(state);
  (void)inference_request_total_tokens(request);
  if (submitted_step < 0 || generated_tokens < 0 ||
      generated_tokens > request.max_decode_tokens ||
      first_scheduled_step < -1 || first_decode_step < -1 ||
      finished_step < -1) {
    throw std::invalid_argument(
        "scheduler probe lifecycle fields are outside valid ranges");
  }
  if ((first_scheduled_step >= 0 &&
       first_scheduled_step < submitted_step) ||
      (first_decode_step >= 0 && first_decode_step < submitted_step) ||
      (finished_step >= 0 && finished_step < submitted_step) ||
      (first_scheduled_step >= 0 && first_decode_step >= 0 &&
       first_decode_step < first_scheduled_step) ||
      (first_decode_step >= 0 && finished_step >= 0 &&
       finished_step < first_decode_step)) {
    throw std::invalid_argument(
        "scheduler probe lifecycle step ordering is invalid");
  }
  if ((state == RequestState::kFinished ||
       state == RequestState::kRejected) != (finished_step >= 0)) {
    throw std::invalid_argument(
        "scheduler probe terminal state and finished_step disagree");
  }
  if (state == RequestState::kFinished &&
      generated_tokens != request.max_decode_tokens) {
    throw std::invalid_argument(
        "scheduler probe finished request has incomplete decode tokens");
  }
  if (state == RequestState::kRunning && first_scheduled_step < 0) {
    throw std::invalid_argument(
        "scheduler probe running request was never scheduled");
  }
  if (generated_tokens > 0 && first_decode_step < 0) {
    throw std::invalid_argument(
        "scheduler probe generated tokens require first_decode_step");
  }

  RequestLifecycleMetrics metrics;
  metrics.request_id = request.request_id;
  metrics.state = state;
  metrics.prompt_tokens = request.prompt_tokens;
  metrics.max_decode_tokens = request.max_decode_tokens;
  metrics.submitted_step = submitted_step;
  metrics.first_scheduled_step = first_scheduled_step;
  metrics.first_decode_step = first_decode_step;
  metrics.finished_step = finished_step;
  metrics.generated_tokens = generated_tokens;
  if (first_scheduled_step >= 0) {
    metrics.queue_wait_steps = scheduler_probe_step_delta(
        first_scheduled_step, submitted_step,
        "scheduler probe queue wait steps");
  }
  if (first_decode_step >= 0) {
    metrics.time_to_first_token_steps = scheduler_probe_step_delta(
        first_decode_step, submitted_step,
        "scheduler probe time to first token steps");
  }
  if (finished_step >= 0) {
    metrics.total_latency_steps = scheduler_probe_step_delta(
        finished_step, submitted_step,
        "scheduler probe total latency steps");
  }
  if (generated_tokens > 0 && first_decode_step >= 0 &&
      finished_step >= first_decode_step) {
    const auto decode_span = internal::checked_add_i64(
        static_cast<std::int64_t>(finished_step) -
            static_cast<std::int64_t>(first_decode_step),
        1, "scheduler probe decode step span");
    metrics.decode_steps_per_token =
        static_cast<double>(decode_span) /
        static_cast<double>(generated_tokens);
  }
  return metrics;
}

inline std::vector<RequestLifecycleMetrics>
sorted_scheduler_probe_lifecycle_metrics(
    std::vector<RequestLifecycleMetrics> metrics) {
  std::sort(metrics.begin(), metrics.end(),
            [](const RequestLifecycleMetrics& left,
               const RequestLifecycleMetrics& right) {
              return left.request_id < right.request_id;
            });
  for (std::size_t i = 1; i < metrics.size(); ++i) {
    if (metrics[i - 1].request_id == metrics[i].request_id) {
      throw std::invalid_argument(
          "scheduler probe lifecycle request_id values must be unique");
    }
  }
  return metrics;
}

inline std::size_t scheduler_probe_step_budget(
    const std::vector<InferenceRequest>& workload) {
  if (workload.empty()) {
    throw std::invalid_argument("scheduler probe workload must not be empty");
  }
  if (workload.size() > kMaxSchedulerProbeTraceSteps) {
    throw std::invalid_argument(
        "scheduler probe workload count exceeds trace step cap");
  }
  std::unordered_set<RequestId> request_ids;
  request_ids.reserve(workload.size());
  std::size_t step_budget = 0;
  for (const auto& request : workload) {
    if (request.request_id <= 0) {
      throw std::invalid_argument(
          "scheduler probe request_id must be positive");
    }
    (void)inference_request_total_tokens(request);
    if (!request_ids.insert(request.request_id).second) {
      throw std::invalid_argument(
          "scheduler probe request_id values must be unique");
    }
    const auto request_steps = static_cast<std::size_t>(
        std::max(1, request.max_decode_tokens));
    step_budget = internal::checked_add_size(
        step_budget, request_steps, "scheduler probe step budget");
  }
  if (step_budget > kMaxSchedulerProbeTraceSteps) {
    throw std::invalid_argument(
        "scheduler probe workload exceeds trace step cap");
  }
  return step_budget;
}

inline std::size_t scheduler_probe_step_budget_with_delay(
    const std::vector<InferenceRequest>& workload, const int max_delay_steps,
    const char* name) {
  if (max_delay_steps < 0) {
    throw std::invalid_argument(std::string(name) +
                                " delay must be non-negative");
  }
  const auto budget = internal::checked_add_size(
      scheduler_probe_step_budget(workload),
      static_cast<std::size_t>(max_delay_steps), name);
  if (budget > kMaxSchedulerProbeTraceSteps) {
    throw std::invalid_argument(std::string(name) +
                                " exceeds trace step cap");
  }
  return budget;
}

inline void require_nonempty_scheduler_probe_results(
    const std::size_t count, const char* name) {
  if (count == 0) {
    throw std::invalid_argument(std::string(name) + " must not be empty");
  }
}

inline void release_scheduler_probe_request(PagedKVCacheManager& cache,
                                            const RequestId request_id,
                                            const char* context) {
  if (!cache.release_request(request_id)) {
    throw std::logic_error(std::string(context) +
                           " request is missing from KV cache");
  }
}

inline double checked_scheduler_probe_double_product(
    const double lhs, const double rhs, const char* name) {
  if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
    throw std::invalid_argument(std::string(name) +
                                " inputs must be finite");
  }
  const double product = lhs * rhs;
  if (!std::isfinite(product)) {
    throw std::overflow_error(std::string(name) + " is non-finite");
  }
  return product;
}

inline void add_to_scheduler_probe_finite_total(double& total,
                                                const double value,
                                                const char* name) {
  if (!std::isfinite(total) || !std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) +
                                " inputs must be finite");
  }
  total += value;
  if (!std::isfinite(total)) {
    throw std::overflow_error(std::string(name) + " is non-finite");
  }
}

template <typename Result, typename MetricSelector>
int checked_scheduler_result_metric_total(
    const std::vector<Result>& results, const MetricSelector& metric,
    const char* name) {
  int total = 0;
  for (const auto& result : results) {
    const int value = metric(result);
    if (value < 0) {
      throw std::invalid_argument(std::string(name) +
                                  " contains a negative metric");
    }
    total = internal::checked_add_int(total, value, name);
  }
  return total;
}

inline SchedulerProbeExecution run_continuous_scheduler_probe(
    const PagedKVCacheConfig& cache_config, const int max_active_requests,
    const std::vector<InferenceRequest>& workload) {
  const auto step_budget = scheduler_probe_step_budget(workload);
  ContinuousBatchScheduler scheduler(cache_config, max_active_requests);
  for (const auto& request : workload) {
    if (!scheduler.submit(request)) {
      throw std::invalid_argument(
          "scheduler probe failed to submit validated workload");
    }
  }

  std::vector<SchedulerStepStats> trace;
  trace.reserve(step_budget);
  while (scheduler.has_work()) {
    if (trace.size() >= step_budget) {
      throw std::logic_error(
          "scheduler probe exceeded derived progress step budget");
    }
    trace.push_back(scheduler.step());
  }
  auto requests = scheduler.request_metrics();
  if (requests.size() != workload.size()) {
    throw std::logic_error(
        "scheduler probe lifecycle count disagrees with workload");
  }
  auto profile = profile_scheduler_pressure(trace, requests);
  if (profile.steps < 0 ||
      static_cast<std::size_t>(profile.steps) != trace.size() ||
      profile.request_count < 0 ||
      static_cast<std::size_t>(profile.request_count) != workload.size()) {
    throw std::logic_error(
        "scheduler probe profile disagrees with execution trace");
  }
  return SchedulerProbeExecution{std::move(trace), std::move(requests),
                                 profile, step_budget};
}

}  // namespace inferlab::probe
