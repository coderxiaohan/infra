#include "inferlab/advanced_serving_structured_delivery.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>

namespace inferlab {

StreamingSseResult estimate_streaming_sse(
    const StreamingSseScenario& scenario) {
  using advanced_serving_internal::finite_product;
  using advanced_serving_internal::finite_unit_interval;
  using advanced_serving_internal::non_negative_finite_or_zero;
  using advanced_serving_internal::positive_finite_or_default;
  using advanced_serving_internal::require_finite;

  const double request_qps = non_negative_finite_or_zero(
      scenario.request_qps, "streaming SSE request QPS");
  const int output_tokens = std::max(0, scenario.output_tokens);
  const int tokens_per_chunk =
      advanced_serving_internal::positive_or_one(scenario.tokens_per_chunk);
  const double ttft = non_negative_finite_or_zero(
      scenario.ttft_ms, "streaming SSE TTFT");
  const double chunk_gap = non_negative_finite_or_zero(
      scenario.avg_chunk_gap_ms, "streaming SSE average chunk gap");
  const double jitter = non_negative_finite_or_zero(
      scenario.chunk_jitter_ms, "streaming SSE chunk jitter");
  const double network_rtt = non_negative_finite_or_zero(
      scenario.network_rtt_ms, "streaming SSE network RTT");
  const double disconnect_rate = finite_unit_interval(
      scenario.client_disconnect_rate,
      "streaming SSE client disconnect rate");
  const double timeout_ms = positive_finite_or_default(
      scenario.stream_timeout_ms, 30000.0, "streaming SSE timeout");

  const int chunks = internal::checked_ceil_div_non_negative_int(
      output_tokens, tokens_per_chunk, "streaming SSE chunks");
  const bool active_stream = request_qps > 0.0 && chunks > 0;
  const bool has_inter_chunk_gap = chunks > 1;
  const double p99_gap =
      has_inter_chunk_gap
          ? require_finite(chunk_gap + jitter * 2.33 + network_rtt * 0.10,
                           "streaming SSE p99 chunk gap")
          : 0.0;
  const double inter_chunk_duration =
      chunks > 1
          ? finite_product(static_cast<double>(chunks - 1), chunk_gap,
                           "streaming SSE inter-chunk duration")
          : 0.0;
  const double stream_duration =
      chunks == 0
          ? 0.0
          : require_finite(ttft + inter_chunk_duration + network_rtt,
                           "streaming SSE stream duration");
  const double first_byte =
      chunks == 0
          ? 0.0
          : require_finite(ttft + network_rtt * 0.50,
                           "streaming SSE first-byte latency");
  const double stall_rate =
      has_inter_chunk_gap
          ? finite_unit_interval((p99_gap - 250.0) / 750.0,
                                 "streaming SSE stall rate")
          : 0.0;
  const double timeout_risk =
      active_stream
          ? finite_unit_interval(
                std::max(0.0, stream_duration / timeout_ms - 0.80) / 0.40 +
                    disconnect_rate * 0.50,
                "streaming SSE timeout risk")
          : 0.0;
  const double events_per_second =
      chunks == 0
          ? 0.0
          : finite_product(request_qps, static_cast<double>(chunks),
                           "streaming SSE events per second");
  const double disconnects_per_minute =
      active_stream
          ? finite_product(
                finite_product(request_qps, disconnect_rate,
                               "streaming SSE disconnect rate"),
                60.0, "streaming SSE disconnects per minute")
          : 0.0;
  const double slow_client_lag =
      active_stream ? std::max(0.0, p99_gap - chunk_gap) : 0.0;
  const double cancellation_cleanup =
      active_stream && disconnect_rate > 0.0
          ? finite_product(
                disconnect_rate,
                std::max(1.0, static_cast<double>(chunks) * 0.35),
                "streaming SSE cancellation cleanup")
          : 0.0;
  const double scheduler_slot_reclaim =
      active_stream && disconnect_rate > 0.0
          ? std::max(1.0,
                     require_finite(chunk_gap + cancellation_cleanup,
                                    "streaming SSE slot reclaim latency"))
          : 0.0;
  const double server_backpressure =
      active_stream
          ? finite_unit_interval(
                require_finite(events_per_second / 12000.0 +
                                   slow_client_lag / 1000.0 +
                                   disconnect_rate * 2.0,
                               "streaming SSE server backpressure"),
                "streaming SSE server backpressure")
          : 0.0;

  StreamingSseResult result;
  result.scenario = scenario;
  result.scenario.request_qps = request_qps;
  result.scenario.output_tokens = output_tokens;
  result.scenario.tokens_per_chunk = tokens_per_chunk;
  result.scenario.ttft_ms = ttft;
  result.scenario.avg_chunk_gap_ms = chunk_gap;
  result.scenario.chunk_jitter_ms = jitter;
  result.scenario.network_rtt_ms = network_rtt;
  result.scenario.client_disconnect_rate = disconnect_rate;
  result.scenario.stream_timeout_ms = timeout_ms;
  result.chunks_per_request = chunks;
  result.events_per_second = events_per_second;
  result.estimated_stream_duration_ms = stream_duration;
  result.p99_chunk_gap_ms = p99_gap;
  result.first_byte_visible_ms = first_byte;
  result.user_visible_stall_rate = stall_rate;
  result.timeout_risk_score = timeout_risk;
  result.disconnects_per_minute = disconnects_per_minute;
  result.slow_client_lag_ms = slow_client_lag;
  result.cancellation_cleanup_ms = cancellation_cleanup;
  result.scheduler_slot_reclaim_ms = scheduler_slot_reclaim;
  result.server_backpressure_score = server_backpressure;
  result.streaming_policy =
      !active_stream
          ? "no_active_stream_workload"
          : (timeout_risk >= 0.50
                 ? "shorten_chunks_or_raise_timeout"
                 : (stall_rate >= 0.25
                        ? "flush_each_token_and_reduce_jitter"
                        : "standard_sse"));
  return result;
}

std::vector<StreamingSseScenario> default_streaming_sse_scenarios() {
  return {
      {"vllm_chat_stream", "vllm", 80.0, 256, 1, 520.0, 24.0, 6.0,
       20.0, 0.004, 30000.0},
      {"sglang_agent_stream", "sglang", 60.0, 384, 1, 480.0, 22.0, 5.0,
       20.0, 0.006, 30000.0},
      {"tgi_sse_stream", "tgi", 70.0, 256, 1, 610.0, 28.0, 9.0, 25.0,
       0.008, 30000.0},
      {"llama_cpp_edge_stream", "llama_cpp", 8.0, 192, 2, 950.0, 55.0,
       20.0, 35.0, 0.015, 45000.0},
      {"long_reasoning_stream", "native", 20.0, 2048, 2, 1400.0, 42.0,
       18.0, 30.0, 0.020, 90000.0},
  };
}

}  // namespace inferlab
