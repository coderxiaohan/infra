#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

#include "inferlab/paged_kv_cache.hpp"

namespace inferlab {

enum class RequestState {
  kWaiting,
  kRunning,
  kFinished,
  kRejected,
};

struct InferenceRequest {
  RequestId request_id = 0;
  int prompt_tokens = 0;
  int max_decode_tokens = 0;
};

struct RequestRuntime {
  InferenceRequest request;
  RequestState state = RequestState::kWaiting;
  int submitted_step = 0;
  int generated_tokens = 0;
  int first_scheduled_step = -1;
  int first_decode_step = -1;
  int finished_step = -1;
};

struct RequestLifecycleMetrics {
  RequestId request_id = 0;
  RequestState state = RequestState::kWaiting;
  int prompt_tokens = 0;
  int max_decode_tokens = 0;
  int submitted_step = 0;
  int first_scheduled_step = -1;
  int first_decode_step = -1;
  int finished_step = -1;
  int generated_tokens = 0;
  int queue_wait_steps = -1;
  int time_to_first_token_steps = -1;
  int total_latency_steps = -1;
  double decode_steps_per_token = 0.0;
};

struct SchedulerStepStats {
  int step = 0;
  int admitted = 0;
  int decoded = 0;
  int finished = 0;
  int rejected = 0;
  int backpressure_deferred = 0;
  AllocationStats cache_stats;
};

struct SchedulerPressureProfile {
  int steps = 0;
  int request_count = 0;
  int finished_request_count = 0;
  int rejected_request_count = 0;
  int total_admitted = 0;
  int total_decoded_tokens = 0;
  int total_finished = 0;
  int total_rejected = 0;
  int total_backpressure_deferred = 0;
  int peak_active_requests = 0;
  int peak_used_blocks = 0;
  std::int64_t peak_reserved_bytes = 0;
  std::int64_t peak_total_tokens = 0;
  double peak_cache_utilization = 0.0;
  double average_active_requests = 0.0;
  double average_used_blocks = 0.0;
  double average_cache_utilization = 0.0;
  double average_queue_wait_steps = 0.0;
  double average_ttft_steps = 0.0;
  double average_decode_steps_per_token = 0.0;
  double tokens_per_step = 0.0;
};

class ContinuousBatchScheduler {
 public:
  ContinuousBatchScheduler(PagedKVCacheConfig cache_config,
                           int max_active_requests);

  bool submit(InferenceRequest request);
  SchedulerStepStats step();

  bool has_work() const;
  std::optional<RequestRuntime> request(RequestId request_id) const;
  std::optional<RequestLifecycleMetrics> request_metrics(
      RequestId request_id) const;
  std::vector<RequestLifecycleMetrics> request_metrics() const;
  int current_step() const;

 private:
  void admit_waiting(SchedulerStepStats& stats);
  RequestLifecycleMetrics make_metrics(const RequestRuntime& runtime) const;

  int max_active_requests_ = 0;
  int step_ = 0;
  PagedKVCacheManager cache_;
  std::queue<RequestId> waiting_;
  std::vector<RequestId> running_;
  std::unordered_map<RequestId, RequestRuntime> requests_;
};

SchedulerPressureProfile profile_scheduler_pressure(
    const std::vector<SchedulerStepStats>& trace,
    const std::vector<RequestLifecycleMetrics>& requests);

}  // namespace inferlab
