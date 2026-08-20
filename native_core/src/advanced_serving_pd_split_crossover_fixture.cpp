#include "inferlab/advanced_serving_runtime_communication.hpp"

#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

PdSplitCrossoverScenario make_scenario(
    std::string name,
    std::vector<std::string> product_profile_ids,
    const int prompt_tokens,
    const int output_tokens,
    const int concurrent_requests,
    const double prefill_ms_per_token,
    const double decode_ms_per_token,
    const double kv_bytes_per_token,
    const double transfer_bandwidth_gbps,
    const double transfer_setup_us_per_chunk,
    const double overlap_efficiency,
    const double monolithic_queue_ms,
    const double pd_queue_ms,
    const double slo_ttft_ms,
    const double slo_tpot_ms) {
  PdSplitCrossoverScenario scenario;
  scenario.name = std::move(name);
  scenario.product_profile_ids = std::move(product_profile_ids);
  scenario.concurrent_requests = concurrent_requests;
  scenario.transfer.name = scenario.name;
  scenario.transfer.topology =
      PdTransferTopology::prefill_decode_split;
  scenario.transfer.prompt_tokens = prompt_tokens;
  scenario.transfer.output_tokens = output_tokens;
  scenario.transfer.prefill_chunk_tokens = prompt_tokens;
  scenario.transfer.decode_chunk_tokens = output_tokens;
  scenario.transfer.prefill_workers = 1;
  scenario.transfer.decode_workers = 1;
  scenario.transfer.prefill_ms_per_token = prefill_ms_per_token;
  scenario.transfer.decode_ms_per_token = decode_ms_per_token;
  scenario.transfer.kv_bytes_per_token = kv_bytes_per_token;
  scenario.transfer.link_bandwidth_gbps = transfer_bandwidth_gbps;
  scenario.transfer.serialization_us_per_chunk =
      transfer_setup_us_per_chunk;
  scenario.transfer.receiver_setup_us_per_chunk = 0.0;
  scenario.transfer.overlap_efficiency = overlap_efficiency;
  scenario.monolithic_queue_ms = monolithic_queue_ms;
  scenario.pd_queue_ms = pd_queue_ms;
  scenario.slo_ttft_ms = slo_ttft_ms;
  scenario.slo_tpot_ms = slo_tpot_ms;
  return scenario;
}

}  // namespace

PdSplitCrossoverAssumptions default_pd_split_crossover_assumptions() {
  PdSplitCrossoverAssumptions assumptions;
  assumptions.transfer = default_pd_transfer_overlap_assumptions();
  assumptions.transfer.profile_name =
      "compiled_pd_split_crossover_transfer_pipeline_v2";
  return assumptions;
}

std::vector<PdSplitCrossoverScenario>
default_pd_split_crossover_scenarios() {
  return {
      make_scenario(
          "short_chat_monolithic_preferred", {"vllm", "sglang"},
          2048, 256, 16, 0.018, 1.30, 524288.0, 90.0, 700.0,
          0.45, 25.0, 36.0, 160.0, 2.0),
      make_scenario(
          "long_context_pd_candidate", {"dynamo", "mooncake"},
          32768, 384, 24, 0.017, 1.35, 524288.0, 100.0, 950.0,
          0.72, 210.0, 74.0, 900.0, 2.2),
      make_scenario(
          "network_bound_remote_kv", {"mooncake"},
          65536, 256, 20, 0.016, 1.45, 524288.0, 28.0, 1400.0,
          0.35, 320.0, 96.0, 1300.0, 2.4),
      make_scenario(
          "decode_heavy_pd_helpful", {"kserve_llmd", "dynamo"},
          8192, 2048, 48, 0.020, 1.75, 524288.0, 80.0, 800.0,
          0.62, 260.0, 82.0, 650.0, 2.8),
      make_scenario(
          "same_node_high_overlap", {"tensorrtllm"},
          49152, 512, 32, 0.015, 1.28, 524288.0, 160.0, 500.0,
          0.86, 260.0, 55.0, 1200.0, 2.1),
  };
}

}  // namespace inferlab
