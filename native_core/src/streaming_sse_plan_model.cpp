#include "streaming_sse_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace inferlab::streaming_sse_plan {
namespace {

bool same_scenario(const StreamingSseScenario& a,
                   const StreamingSseScenario& b) {
  return a.name == b.name && a.backend == b.backend &&
         a.request_qps == b.request_qps &&
         a.output_tokens == b.output_tokens &&
         a.tokens_per_chunk == b.tokens_per_chunk &&
         a.ttft_ms == b.ttft_ms &&
         a.avg_chunk_gap_ms == b.avg_chunk_gap_ms &&
         a.chunk_jitter_ms == b.chunk_jitter_ms &&
         a.network_rtt_ms == b.network_rtt_ms &&
         a.client_disconnect_rate == b.client_disconnect_rate &&
         a.stream_timeout_ms == b.stream_timeout_ms;
}

bool same_result(const StreamingSseResult& a,
                 const StreamingSseResult& b) {
  return same_scenario(a.scenario, b.scenario) &&
         a.chunks_per_request == b.chunks_per_request &&
         a.events_per_second == b.events_per_second &&
         a.estimated_stream_duration_ms == b.estimated_stream_duration_ms &&
         a.p99_chunk_gap_ms == b.p99_chunk_gap_ms &&
         a.first_byte_visible_ms == b.first_byte_visible_ms &&
         a.user_visible_stall_rate == b.user_visible_stall_rate &&
         a.timeout_risk_score == b.timeout_risk_score &&
         a.disconnects_per_minute == b.disconnects_per_minute &&
         a.slow_client_lag_ms == b.slow_client_lag_ms &&
         a.cancellation_cleanup_ms == b.cancellation_cleanup_ms &&
         a.scheduler_slot_reclaim_ms == b.scheduler_slot_reclaim_ms &&
         a.server_backpressure_score == b.server_backpressure_score &&
         a.streaming_policy == b.streaming_policy;
}

void validate_scenarios(const std::vector<StreamingSseScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error("streaming SSE scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() || scenario.backend.empty() ||
        !names.insert(scenario.name).second) {
      throw std::logic_error("streaming SSE scenario identity is invalid");
    }
    (void)estimate_streaming_sse(scenario);
  }
}

template <typename Metric>
std::size_t highest(const std::vector<StreamingSseResult>& records,
                    Metric metric) {
  std::size_t best = 0U;
  for (std::size_t i = 1U; i < records.size(); ++i) {
    if (metric(records[i]) > metric(records[best])) {
      best = i;
    }
  }
  return best;
}

Summary make_summary(const std::vector<StreamingSseResult>& records) {
  if (records.empty()) {
    throw std::logic_error("streaming SSE records must not be empty");
  }
  return {
      highest(records, [](const auto& r) { return r.events_per_second; }),
      highest(records,
              [](const auto& r) { return r.user_visible_stall_rate; }),
      highest(records,
              [](const auto& r) { return r.disconnects_per_minute; }),
  };
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_streaming_sse_scenarios();
  validate_scenarios(plan.scenarios);
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_streaming_sse(scenario));
  }
  plan.summary = make_summary(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error("streaming SSE scenario/record count mismatch");
  }
  for (std::size_t i = 0U; i < plan.records.size(); ++i) {
    const auto expected = estimate_streaming_sse(plan.scenarios[i]);
    if (!same_scenario(plan.records[i].scenario, plan.scenarios[i]) ||
        !same_result(plan.records[i], expected)) {
      throw std::logic_error("streaming SSE record is not canonical");
    }
  }
  const auto expected = make_summary(plan.records);
  if (plan.summary.max_events_index >= plan.records.size() ||
      plan.summary.max_stall_index >= plan.records.size() ||
      plan.summary.max_disconnects_index >= plan.records.size() ||
      plan.summary.max_events_index != expected.max_events_index ||
      plan.summary.max_stall_index != expected.max_stall_index ||
      plan.summary.max_disconnects_index != expected.max_disconnects_index) {
    throw std::logic_error("streaming SSE summary is not canonical");
  }
}

}  // namespace inferlab::streaming_sse_plan
