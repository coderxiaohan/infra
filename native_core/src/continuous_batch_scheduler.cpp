#include "inferlab/continuous_batch_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "checked_index.hpp"

namespace inferlab {
namespace {

void require_non_negative(const int value, const char* name) {
  if (value < 0) {
    throw std::invalid_argument(std::string(name) + " must be non-negative");
  }
}

void require_non_negative_i64(const std::int64_t value, const char* name) {
  if (value < 0) {
    throw std::invalid_argument(std::string(name) + " must be non-negative");
  }
}

void require_finite_non_negative(const double value, const char* name) {
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(std::string(name) +
                                " must be finite and non-negative");
  }
}

bool valid_request_state(const RequestState state) {
  switch (state) {
    case RequestState::kWaiting:
    case RequestState::kRunning:
    case RequestState::kFinished:
    case RequestState::kRejected:
      return true;
  }
  return false;
}

void validate_allocation_stats(const AllocationStats& stats) {
  require_non_negative(stats.total_blocks, "scheduler total_blocks");
  require_non_negative(stats.free_blocks, "scheduler free_blocks");
  require_non_negative(stats.used_blocks, "scheduler used_blocks");
  require_non_negative(stats.active_requests, "scheduler active_requests");
  require_non_negative_i64(stats.total_tokens, "scheduler total_tokens");
  require_non_negative_i64(stats.used_bytes, "scheduler used_bytes");
  require_non_negative_i64(stats.reserved_bytes, "scheduler reserved_bytes");
  require_finite_non_negative(stats.utilization, "scheduler utilization");
  if (stats.utilization > 1.0) {
    throw std::invalid_argument(
        "scheduler utilization must not exceed one");
  }
  if (stats.total_blocks > 0 &&
      (stats.free_blocks > stats.total_blocks ||
       stats.used_blocks > stats.total_blocks)) {
    throw std::invalid_argument(
        "scheduler cache block counts must not exceed total_blocks");
  }
  if (stats.used_bytes > stats.reserved_bytes) {
    throw std::invalid_argument(
        "scheduler used_bytes must not exceed reserved_bytes");
  }
  if (stats.reserved_bytes == 0 && stats.utilization != 0.0) {
    throw std::invalid_argument(
        "scheduler zero reserved_bytes requires zero utilization");
  }
}

void validate_step_stats(const SchedulerStepStats& step) {
  require_non_negative(step.step, "scheduler step");
  require_non_negative(step.admitted, "scheduler admitted");
  require_non_negative(step.decoded, "scheduler decoded");
  require_non_negative(step.finished, "scheduler finished");
  require_non_negative(step.rejected, "scheduler rejected");
  require_non_negative(step.backpressure_deferred,
                       "scheduler backpressure_deferred");
  validate_allocation_stats(step.cache_stats);
}

void validate_request_metrics(const RequestLifecycleMetrics& request) {
  if (request.request_id <= 0) {
    throw std::invalid_argument("scheduler request_id must be positive");
  }
  if (!valid_request_state(request.state)) {
    throw std::invalid_argument("scheduler request state is invalid");
  }
  require_non_negative(request.prompt_tokens, "scheduler prompt_tokens");
  require_non_negative(request.max_decode_tokens,
                       "scheduler max_decode_tokens");
  require_non_negative(request.submitted_step, "scheduler submitted_step");
  require_non_negative(request.generated_tokens, "scheduler generated_tokens");
  if (request.max_decode_tokens > 0 &&
      request.generated_tokens > request.max_decode_tokens) {
    throw std::invalid_argument(
        "scheduler generated_tokens must not exceed max_decode_tokens");
  }
  if (request.first_scheduled_step < -1 || request.first_decode_step < -1 ||
      request.finished_step < -1 || request.queue_wait_steps < -1 ||
      request.time_to_first_token_steps < -1 ||
      request.total_latency_steps < -1) {
    throw std::invalid_argument(
        "scheduler lifecycle step fields must be -1 or non-negative");
  }
  require_finite_non_negative(request.decode_steps_per_token,
                              "scheduler decode_steps_per_token");
  if (request.first_scheduled_step >= 0 &&
      request.first_scheduled_step < request.submitted_step) {
    throw std::invalid_argument(
        "scheduler first_scheduled_step must not precede submitted_step");
  }
  if (request.first_decode_step >= 0 &&
      request.first_decode_step < request.submitted_step) {
    throw std::invalid_argument(
        "scheduler first_decode_step must not precede submitted_step");
  }
  if (request.finished_step >= 0 &&
      request.finished_step < request.submitted_step) {
    throw std::invalid_argument(
        "scheduler finished_step must not precede submitted_step");
  }
  if (request.first_decode_step >= 0 && request.finished_step >= 0 &&
      request.finished_step < request.first_decode_step) {
    throw std::invalid_argument(
        "scheduler finished_step must not precede first_decode_step");
  }
}

void add_to_finite_total(double& total, const double value, const char* name) {
  total += value;
  if (!std::isfinite(total)) {
    throw std::invalid_argument(std::string(name) + " must remain finite");
  }
}

}  // namespace

ContinuousBatchScheduler::ContinuousBatchScheduler(
    PagedKVCacheConfig cache_config, const int max_active_requests)
    : max_active_requests_(max_active_requests), cache_(cache_config) {
  if (max_active_requests_ <= 0) {
    throw std::invalid_argument("max_active_requests must be positive");
  }
}

bool ContinuousBatchScheduler::submit(InferenceRequest request) {
  if (request.request_id <= 0 || request.prompt_tokens < 0 ||
      request.max_decode_tokens < 0) {
    return false;
  }
  if (requests_.find(request.request_id) != requests_.end()) {
    return false;
  }
  RequestRuntime runtime;
  runtime.request = request;
  runtime.submitted_step = step_;
  const auto [it, inserted] =
      requests_.emplace(request.request_id, std::move(runtime));
  if (!inserted) {
    return false;
  }
  try {
    waiting_.push(request.request_id);
  } catch (...) {
    requests_.erase(it);
    throw;
  }
  return true;
}

SchedulerStepStats ContinuousBatchScheduler::step() {
  step_ = internal::checked_add_int(step_, 1, "scheduler step");
  SchedulerStepStats stats;
  stats.step = step_;

  admit_waiting(stats);

  std::vector<RequestId> still_running;
  still_running.reserve(running_.size());
  for (const auto request_id : running_) {
    auto& runtime = requests_.at(request_id);
    if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
      if (!cache_.release_request(request_id)) {
        throw std::runtime_error(
            "scheduler running request missing from KV cache on finish");
      }
      runtime.state = RequestState::kFinished;
      runtime.finished_step = step_;
      ++stats.finished;
      continue;
    }

    if (!cache_.append_tokens(request_id, 1)) {
      if (!cache_.release_request(request_id)) {
        throw std::runtime_error(
            "scheduler running request missing from KV cache on rejection");
      }
      runtime.state = RequestState::kRejected;
      runtime.finished_step = step_;
      ++stats.rejected;
      continue;
    }
    runtime.generated_tokens =
        internal::checked_add_int(runtime.generated_tokens, 1,
                                  "generated tokens");
    if (runtime.first_decode_step < 0) {
      runtime.first_decode_step = step_;
    }
    ++stats.decoded;

    if (runtime.generated_tokens >= runtime.request.max_decode_tokens) {
      if (!cache_.release_request(request_id)) {
        throw std::runtime_error(
            "scheduler running request missing from KV cache after decode");
      }
      runtime.state = RequestState::kFinished;
      runtime.finished_step = step_;
      ++stats.finished;
    } else {
      still_running.push_back(request_id);
    }
  }
  running_ = std::move(still_running);

  stats.cache_stats = cache_.stats();
  return stats;
}

bool ContinuousBatchScheduler::has_work() const {
  return !waiting_.empty() || !running_.empty();
}

std::optional<RequestRuntime> ContinuousBatchScheduler::request(
    const RequestId request_id) const {
  const auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<RequestLifecycleMetrics>
ContinuousBatchScheduler::request_metrics(const RequestId request_id) const {
  const auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return std::nullopt;
  }
  return make_metrics(it->second);
}

std::vector<RequestLifecycleMetrics>
ContinuousBatchScheduler::request_metrics() const {
  std::vector<RequestLifecycleMetrics> metrics;
  metrics.reserve(requests_.size());
  for (const auto& item : requests_) {
    metrics.push_back(make_metrics(item.second));
  }
  std::sort(metrics.begin(), metrics.end(),
            [](const RequestLifecycleMetrics& left,
               const RequestLifecycleMetrics& right) {
              return left.request_id < right.request_id;
            });
  return metrics;
}

int ContinuousBatchScheduler::current_step() const { return step_; }

SchedulerPressureProfile profile_scheduler_pressure(
    const std::vector<SchedulerStepStats>& trace,
    const std::vector<RequestLifecycleMetrics>& requests) {
  SchedulerPressureProfile profile;
  profile.steps = internal::checked_int_from_size(trace.size(), "scheduler steps");
  profile.request_count =
      internal::checked_int_from_size(requests.size(), "scheduler requests");

  double active_request_total = 0.0;
  double used_block_total = 0.0;
  double cache_utilization_total = 0.0;
  for (const auto& step : trace) {
    validate_step_stats(step);
    profile.total_admitted = internal::checked_add_int(
        profile.total_admitted, step.admitted, "total admitted requests");
    profile.total_decoded_tokens = internal::checked_add_int(
        profile.total_decoded_tokens, step.decoded, "total decoded tokens");
    profile.total_finished = internal::checked_add_int(
        profile.total_finished, step.finished, "total finished requests");
    profile.total_rejected = internal::checked_add_int(
        profile.total_rejected, step.rejected, "total rejected requests");
    profile.total_backpressure_deferred = internal::checked_add_int(
        profile.total_backpressure_deferred, step.backpressure_deferred,
        "total backpressure deferrals");
    profile.peak_active_requests =
        std::max(profile.peak_active_requests, step.cache_stats.active_requests);
    profile.peak_used_blocks =
        std::max(profile.peak_used_blocks, step.cache_stats.used_blocks);
    profile.peak_reserved_bytes =
        std::max(profile.peak_reserved_bytes, step.cache_stats.reserved_bytes);
    profile.peak_total_tokens =
        std::max(profile.peak_total_tokens, step.cache_stats.total_tokens);
    profile.peak_cache_utilization =
        std::max(profile.peak_cache_utilization, step.cache_stats.utilization);
    add_to_finite_total(active_request_total,
                        static_cast<double>(step.cache_stats.active_requests),
                        "scheduler active request total");
    add_to_finite_total(used_block_total,
                        static_cast<double>(step.cache_stats.used_blocks),
                        "scheduler used block total");
    add_to_finite_total(cache_utilization_total, step.cache_stats.utilization,
                        "scheduler cache utilization total");
  }

  if (profile.steps > 0) {
    const auto divisor = static_cast<double>(profile.steps);
    profile.average_active_requests = active_request_total / divisor;
    profile.average_used_blocks = used_block_total / divisor;
    profile.average_cache_utilization = cache_utilization_total / divisor;
    profile.tokens_per_step =
        static_cast<double>(profile.total_decoded_tokens) / divisor;
  }

  double queue_wait_total = 0.0;
  int queue_wait_count = 0;
  double ttft_total = 0.0;
  int ttft_count = 0;
  double decode_step_total = 0.0;
  int decode_step_count = 0;
  for (const auto& request : requests) {
    validate_request_metrics(request);
    if (request.state == RequestState::kFinished) {
      profile.finished_request_count = internal::checked_add_int(
          profile.finished_request_count, 1, "finished request count");
    } else if (request.state == RequestState::kRejected) {
      profile.rejected_request_count = internal::checked_add_int(
          profile.rejected_request_count, 1, "rejected request count");
    }
    if (request.queue_wait_steps >= 0) {
      add_to_finite_total(queue_wait_total,
                          static_cast<double>(request.queue_wait_steps),
                          "scheduler queue wait total");
      queue_wait_count = internal::checked_add_int(
          queue_wait_count, 1, "scheduler queue wait count");
    }
    if (request.time_to_first_token_steps >= 0) {
      add_to_finite_total(ttft_total,
                          static_cast<double>(request.time_to_first_token_steps),
                          "scheduler ttft total");
      ttft_count =
          internal::checked_add_int(ttft_count, 1, "scheduler ttft count");
    }
    if (request.decode_steps_per_token > 0.0) {
      add_to_finite_total(decode_step_total,
                          request.decode_steps_per_token,
                          "scheduler decode step total");
      decode_step_count = internal::checked_add_int(
          decode_step_count, 1, "scheduler decode step count");
    }
  }
  if (queue_wait_count > 0) {
    profile.average_queue_wait_steps =
        queue_wait_total / static_cast<double>(queue_wait_count);
  }
  if (ttft_count > 0) {
    profile.average_ttft_steps = ttft_total / static_cast<double>(ttft_count);
  }
  if (decode_step_count > 0) {
    profile.average_decode_steps_per_token =
        decode_step_total / static_cast<double>(decode_step_count);
  }
  return profile;
}

void ContinuousBatchScheduler::admit_waiting(SchedulerStepStats& stats) {
  const auto max_active_requests =
      static_cast<std::size_t>(max_active_requests_);
  while (!waiting_.empty() &&
         running_.size() < max_active_requests) {
    const auto request_id = waiting_.front();

    auto& runtime = requests_.at(request_id);
    if (!cache_.can_fit_request(runtime.request.prompt_tokens)) {
      waiting_.pop();
      runtime.state = RequestState::kRejected;
      runtime.finished_step = step_;
      ++stats.rejected;
      continue;
    }

    const auto next_running_size = internal::checked_add_size(
        running_.size(), 1U, "scheduler running request count");
    if (running_.capacity() < next_running_size) {
      running_.reserve(next_running_size);
    }
    if (!cache_.add_request(request_id, runtime.request.prompt_tokens)) {
      ++stats.backpressure_deferred;
      break;
    }

    waiting_.pop();
    runtime.state = RequestState::kRunning;
    runtime.first_scheduled_step = step_;
    running_.push_back(request_id);
    ++stats.admitted;
  }
}

RequestLifecycleMetrics ContinuousBatchScheduler::make_metrics(
    const RequestRuntime& runtime) const {
  RequestLifecycleMetrics metrics;
  metrics.request_id = runtime.request.request_id;
  metrics.state = runtime.state;
  metrics.prompt_tokens = runtime.request.prompt_tokens;
  metrics.max_decode_tokens = runtime.request.max_decode_tokens;
  metrics.submitted_step = runtime.submitted_step;
  metrics.first_scheduled_step = runtime.first_scheduled_step;
  metrics.first_decode_step = runtime.first_decode_step;
  metrics.finished_step = runtime.finished_step;
  metrics.generated_tokens = runtime.generated_tokens;
  if (runtime.first_scheduled_step >= 0) {
    metrics.queue_wait_steps =
        runtime.first_scheduled_step - runtime.submitted_step;
  }
  if (runtime.first_decode_step >= 0) {
    metrics.time_to_first_token_steps =
        runtime.first_decode_step - runtime.submitted_step;
  }
  if (runtime.finished_step >= 0) {
    metrics.total_latency_steps =
        runtime.finished_step - runtime.submitted_step;
  }
  if (runtime.generated_tokens > 0 && runtime.first_decode_step >= 0 &&
      runtime.finished_step >= runtime.first_decode_step) {
    const auto decode_step_span = internal::checked_add_i64(
        static_cast<std::int64_t>(runtime.finished_step) -
            static_cast<std::int64_t>(runtime.first_decode_step),
        1, "scheduler decode step span");
    metrics.decode_steps_per_token =
        static_cast<double>(decode_step_span) /
        static_cast<double>(runtime.generated_tokens);
  }
  return metrics;
}

}  // namespace inferlab
