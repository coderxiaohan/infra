#include "streaming_sse_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>

namespace inferlab::streaming_sse_plan {
namespace {

std::string scenario_json(const StreamingSseScenario& s, const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2, "streaming SSE scenario indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"avg_chunk_gap_ms\": " << std::fixed << std::setprecision(6) << s.avg_chunk_gap_ms << ",\n";
  out << nested << "\"backend\": " << inferlab_plan::json_string(s.backend) << ",\n";
  out << nested << "\"chunk_jitter_ms\": " << std::fixed << std::setprecision(6) << s.chunk_jitter_ms << ",\n";
  out << nested << "\"client_disconnect_rate\": " << std::fixed << std::setprecision(6) << s.client_disconnect_rate << ",\n";
  out << nested << "\"name\": " << inferlab_plan::json_string(s.name) << ",\n";
  out << nested << "\"network_rtt_ms\": " << std::fixed << std::setprecision(6) << s.network_rtt_ms << ",\n";
  out << nested << "\"output_tokens\": " << s.output_tokens << ",\n";
  out << nested << "\"request_qps\": " << std::fixed << std::setprecision(6) << s.request_qps << ",\n";
  out << nested << "\"stream_timeout_ms\": " << std::fixed << std::setprecision(6) << s.stream_timeout_ms << ",\n";
  out << nested << "\"tokens_per_chunk\": " << s.tokens_per_chunk << ",\n";
  out << nested << "\"ttft_ms\": " << std::fixed << std::setprecision(6) << s.ttft_ms << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const StreamingSseResult& r, const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int ni = internal::checked_add_int(indent, 2, "streaming SSE record indent");
  const auto nested = inferlab_plan::json_indent(ni);
  const auto timeline = inferlab_plan::json_indent(
      internal::checked_add_int(ni, 2, "streaming SSE timeline indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cancellation_cleanup_ms\": " << std::fixed << std::setprecision(6) << r.cancellation_cleanup_ms << ",\n";
  out << nested << "\"chunks_per_request\": " << r.chunks_per_request << ",\n";
  out << nested << "\"disconnects_per_minute\": " << std::fixed << std::setprecision(6) << r.disconnects_per_minute << ",\n";
  out << nested << "\"estimated_stream_duration_ms\": " << std::fixed << std::setprecision(6) << r.estimated_stream_duration_ms << ",\n";
  out << nested << "\"events_per_second\": " << std::fixed << std::setprecision(6) << r.events_per_second << ",\n";
  out << nested << "\"first_byte_visible_ms\": " << std::fixed << std::setprecision(6) << r.first_byte_visible_ms << ",\n";
  out << nested << "\"p99_chunk_gap_ms\": " << std::fixed << std::setprecision(6) << r.p99_chunk_gap_ms << ",\n";
  out << nested << "\"request_timeline\": {\n";
  out << timeline << "\"ttft_ms\": " << std::fixed << std::setprecision(6) << r.scenario.ttft_ms << ",\n";
  out << timeline << "\"first_event_ms\": " << r.first_byte_visible_ms << ",\n";
  out << timeline << "\"last_event_ms\": " << r.estimated_stream_duration_ms << ",\n";
  out << timeline << "\"cancel_cleanup_ms\": " << r.cancellation_cleanup_ms << "\n";
  out << nested << "},\n";
  out << nested << "\"scheduler_slot_reclaim_ms\": " << r.scheduler_slot_reclaim_ms << ",\n";
  out << nested << "\"scenario\": " << scenario_json(r.scenario, ni) << ",\n";
  out << nested << "\"server_backpressure_score\": " << r.server_backpressure_score << ",\n";
  out << nested << "\"slow_client_lag_ms\": " << r.slow_client_lag_ms << ",\n";
  out << nested << "\"streaming_policy\": " << inferlab_plan::json_string(r.streaming_policy) << ",\n";
  out << nested << "\"timeout_risk_score\": " << r.timeout_risk_score << ",\n";
  out << nested << "\"user_visible_stall_rate\": " << r.user_visible_stall_rate << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& events = plan.records[plan.summary.max_events_index];
  const auto& stall = plan.records[plan.summary.max_stall_index];
  const auto& disconnects = plan.records[plan.summary.max_disconnects_index];
  std::ostringstream out;
  out << "{\n  \"schema_version\": 1,\n  \"artifact_type\": \"native_streaming_sse_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n  \"executed\": false,\n  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"native streaming/SSE planning artifact only; does not run benchmarks, external products, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  out << "    \"max_events_backend\": " << inferlab_plan::json_string(events.scenario.backend) << ",\n";
  out << "    \"max_events_per_second\": " << std::fixed << std::setprecision(6) << events.events_per_second << ",\n";
  out << "    \"max_disconnects_per_minute\": " << disconnects.disconnects_per_minute << ",\n";
  out << "    \"max_disconnects_scenario\": " << inferlab_plan::json_string(disconnects.scenario.name) << ",\n";
  out << "    \"max_stall_backend\": " << inferlab_plan::json_string(stall.scenario.backend) << ",\n";
  out << "    \"max_stall_rate\": " << stall.user_visible_stall_rate << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n  },\n";
  out << "  \"records\": [\n";
  for (std::size_t i = 0U; i < plan.records.size(); ++i) {
    out << record_json(plan.records[i], 4)
        << (i == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ]\n}\n";
  return out.str();
}

}  // namespace inferlab::streaming_sse_plan
