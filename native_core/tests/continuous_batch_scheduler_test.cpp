#include "inferlab/continuous_batch_scheduler.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void require_near(const double actual, const double expected,
                  const std::string& message) {
  if (std::fabs(actual - expected) > 1e-6) {
    throw std::runtime_error(message);
  }
}

void test_single_request_finishes_after_decode_steps() {
  inferlab::ContinuousBatchScheduler scheduler({8, 4, 10}, 2);
  require(scheduler.submit({1, 3, 2}), "submit failed");

  const auto step1 = scheduler.step();
  require(step1.admitted == 1, "request not admitted on first step");
  require(step1.decoded == 1, "first decode step mismatch");

  const auto step2 = scheduler.step();
  require(step2.decoded == 1, "second decode step mismatch");
  require(step2.finished == 1, "request should finish on second step");

  const auto request = scheduler.request(1);
  require(request.has_value(), "request missing");
  require(request->state == inferlab::RequestState::kFinished,
          "request did not finish");
  require(request->generated_tokens == 2, "generated token count mismatch");
}

void test_waiting_request_enters_after_capacity_frees() {
  inferlab::ContinuousBatchScheduler scheduler({8, 4, 10}, 1);
  require(scheduler.submit({1, 2, 1}), "submit request 1 failed");
  require(scheduler.submit({2, 2, 1}), "submit request 2 failed");

  const auto step1 = scheduler.step();
  require(step1.admitted == 1, "first step should admit one request");
  require(step1.finished == 1, "first request should finish");

  const auto step2 = scheduler.step();
  require(step2.admitted == 1, "second request should be admitted later");
  require(step2.finished == 1, "second request should finish");

  require(scheduler.request(2)->first_scheduled_step == 2,
          "second request scheduled at wrong step");
}

void test_request_lifecycle_metrics_track_ttft_and_latency() {
  inferlab::ContinuousBatchScheduler scheduler({8, 4, 10}, 1);
  require(scheduler.submit({1, 2, 2}), "submit request 1 failed");
  require(scheduler.submit({2, 2, 1}), "submit request 2 failed");

  scheduler.step();
  scheduler.step();
  scheduler.step();

  const auto first = scheduler.request_metrics(1);
  require(first.has_value(), "first metrics missing");
  require(first->queue_wait_steps == 1, "first queue wait mismatch");
  require(first->time_to_first_token_steps == 1, "first TTFT mismatch");
  require(first->total_latency_steps == 2, "first latency mismatch");
  require(first->decode_steps_per_token == 1.0,
          "first decode steps per token mismatch");

  const auto second = scheduler.request_metrics(2);
  require(second.has_value(), "second metrics missing");
  require(second->queue_wait_steps == 3, "second queue wait mismatch");
  require(second->time_to_first_token_steps == 3, "second TTFT mismatch");
  require(second->total_latency_steps == 3, "second latency mismatch");
}

void test_request_metrics_are_sorted_by_request_id() {
  inferlab::ContinuousBatchScheduler scheduler({8, 4, 10}, 2);
  require(scheduler.submit({3, 1, 1}), "submit request 3 failed");
  require(scheduler.submit({1, 1, 1}), "submit request 1 failed");
  scheduler.step();

  const auto metrics = scheduler.request_metrics();
  require(metrics.size() == 2, "metrics size mismatch");
  require(metrics[0].request_id == 1, "metrics not sorted by request id");
  require(metrics[1].request_id == 3, "metrics not sorted by request id");
}

void test_prefill_capacity_rejection() {
  inferlab::ContinuousBatchScheduler scheduler({1, 4, 10}, 2);
  require(scheduler.submit({1, 8, 1}), "submit failed");

  const auto step = scheduler.step();
  require(step.rejected == 1, "oversized prompt should be rejected");
  require(scheduler.request(1)->state == inferlab::RequestState::kRejected,
          "request state should be rejected");
}

void test_decode_capacity_rejection_releases_request() {
  inferlab::ContinuousBatchScheduler scheduler({1, 4, 10}, 1);
  require(scheduler.submit({1, 4, 1}), "submit failed");

  const auto step = scheduler.step();
  require(step.rejected == 1, "decode should be rejected when no block remains");
  require(step.cache_stats.used_blocks == 0, "rejected request should release cache");
  require(scheduler.request(1)->state == inferlab::RequestState::kRejected,
          "request state should be rejected");
}

void test_scheduler_pressure_profile_summarizes_trace() {
  std::vector<inferlab::SchedulerStepStats> trace(3);
  trace[0].admitted = 1;
  trace[0].decoded = 1;
  trace[0].cache_stats.active_requests = 1;
  trace[0].cache_stats.used_blocks = 2;
  trace[0].cache_stats.reserved_bytes = 80;
  trace[0].cache_stats.total_tokens = 3;
  trace[0].cache_stats.utilization = 0.5;

  trace[1].decoded = 2;
  trace[1].finished = 1;
  trace[1].cache_stats.active_requests = 2;
  trace[1].cache_stats.used_blocks = 3;
  trace[1].cache_stats.reserved_bytes = 120;
  trace[1].cache_stats.total_tokens = 5;
  trace[1].cache_stats.utilization = 0.75;

  trace[2].rejected = 1;
  trace[2].cache_stats.active_requests = 0;
  trace[2].cache_stats.used_blocks = 0;
  trace[2].cache_stats.reserved_bytes = 0;
  trace[2].cache_stats.total_tokens = 0;
  trace[2].cache_stats.utilization = 0.0;

  std::vector<inferlab::RequestLifecycleMetrics> requests(2);
  requests[0].request_id = 1;
  requests[0].state = inferlab::RequestState::kFinished;
  requests[0].queue_wait_steps = 1;
  requests[0].time_to_first_token_steps = 1;
  requests[0].generated_tokens = 2;
  requests[0].decode_steps_per_token = 1.0;
  requests[1].request_id = 2;
  requests[1].state = inferlab::RequestState::kRejected;

  const auto profile = inferlab::profile_scheduler_pressure(trace, requests);
  require(profile.steps == 3, "pressure profile step count mismatch");
  require(profile.request_count == 2, "pressure profile request count mismatch");
  require(profile.finished_request_count == 1,
          "pressure profile finished count mismatch");
  require(profile.rejected_request_count == 1,
          "pressure profile rejected count mismatch");
  require(profile.total_admitted == 1,
          "pressure profile admitted total mismatch");
  require(profile.total_decoded_tokens == 3,
          "pressure profile decoded total mismatch");
  require(profile.total_finished == 1,
          "pressure profile finished total mismatch");
  require(profile.total_rejected == 1,
          "pressure profile rejected total mismatch");
  require(profile.peak_active_requests == 2,
          "pressure profile peak active mismatch");
  require(profile.peak_used_blocks == 3,
          "pressure profile peak used blocks mismatch");
  require(profile.peak_reserved_bytes == 120,
          "pressure profile peak reserved bytes mismatch");
  require(profile.peak_total_tokens == 5,
          "pressure profile peak total tokens mismatch");
  require_near(profile.peak_cache_utilization, 0.75,
               "pressure profile peak utilization mismatch");
  require_near(profile.average_active_requests, 1.0,
               "pressure profile average active mismatch");
  require_near(profile.average_used_blocks, 5.0 / 3.0,
               "pressure profile average used blocks mismatch");
  require_near(profile.average_cache_utilization, 1.25 / 3.0,
               "pressure profile average utilization mismatch");
  require_near(profile.average_queue_wait_steps, 1.0,
               "pressure profile average queue wait mismatch");
  require_near(profile.average_ttft_steps, 1.0,
               "pressure profile average TTFT mismatch");
  require_near(profile.average_decode_steps_per_token, 1.0,
               "pressure profile average decode steps mismatch");
  require_near(profile.tokens_per_step, 1.0,
               "pressure profile tokens per step mismatch");
}

}  // namespace

int main() {
  try {
    test_single_request_finishes_after_decode_steps();
    test_waiting_request_enters_after_capacity_frees();
    test_request_lifecycle_metrics_track_ttft_and_latency();
    test_request_metrics_are_sorted_by_request_id();
    test_prefill_capacity_rejection();
    test_decode_capacity_rejection_releases_request();
    test_scheduler_pressure_profile_summarizes_trace();
  } catch (const std::exception& error) {
    std::cerr << "continuous_batch_scheduler_test failed: " << inferlab_test::redact_test_error(error.what())
              << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
