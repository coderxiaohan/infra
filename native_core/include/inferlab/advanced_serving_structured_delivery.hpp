#pragma once

#include <string>
#include <vector>

namespace inferlab {

struct StreamingSseScenario {
  std::string name;
  std::string backend;
  double request_qps = 1.0;
  int output_tokens = 128;
  int tokens_per_chunk = 1;
  double ttft_ms = 500.0;
  double avg_chunk_gap_ms = 35.0;
  double chunk_jitter_ms = 5.0;
  double network_rtt_ms = 20.0;
  double client_disconnect_rate = 0.0;
  double stream_timeout_ms = 30000.0;
};

struct StreamingSseResult {
  StreamingSseScenario scenario;
  int chunks_per_request = 0;
  double events_per_second = 0.0;
  double estimated_stream_duration_ms = 0.0;
  double p99_chunk_gap_ms = 0.0;
  double first_byte_visible_ms = 0.0;
  double user_visible_stall_rate = 0.0;
  double timeout_risk_score = 0.0;
  double disconnects_per_minute = 0.0;
  double slow_client_lag_ms = 0.0;
  double cancellation_cleanup_ms = 0.0;
  double scheduler_slot_reclaim_ms = 0.0;
  double server_backpressure_score = 0.0;
  std::string streaming_policy;
};

StreamingSseResult estimate_streaming_sse(
    const StreamingSseScenario& scenario);

std::vector<StreamingSseScenario> default_streaming_sse_scenarios();

}  // namespace inferlab
