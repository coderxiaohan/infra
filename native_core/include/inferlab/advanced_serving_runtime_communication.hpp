#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace inferlab {

enum class KvTransportReplayBackend {
  gpu_gpu_nvlink,
  gpu_cpu_pcie,
  nvme_gds,
  rdma,
  ucx,
  tcp_fallback,
};

const char* kv_transport_replay_backend_name(
    KvTransportReplayBackend backend);

enum class KvTransportTimelinePhase {
  prepare_descriptor,
  sender_wait,
  kv_transfer,
  receiver_wait,
  decode_blocked,
};

const char* kv_transport_timeline_phase_name(
    KvTransportTimelinePhase phase);
const char* kv_transport_timeline_resource_name(
    KvTransportTimelinePhase phase);

struct KvTransportReplayAssumptions {
  std::string profile_name =
      "compiled_kv_transport_timeline_analytical_fixture_v1";
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  int retry_count_reference_requests = 10;
  double bandwidth_score_reference_gbps = 250.0;
  double overlap_score_weight = 0.35;
  double bandwidth_score_weight = 0.35;
  double reliability_score_weight = 0.20;
  double low_blocking_bonus_weight = 0.10;
  double low_blocking_threshold_ms = 25.0;
  double preferred_transport_score_threshold = 0.70;
  double recompute_blocked_threshold_ms = 80.0;
};

struct KvTransportReplayScenario {
  std::string name;
  KvTransportReplayBackend transport =
      KvTransportReplayBackend::tcp_fallback;
  double kv_gib = 0.0;
  double bandwidth_gbps = 0.0;
  double setup_ms = 0.0;
  double sender_wait_ms = 0.0;
  double receiver_wait_ms = 0.0;
  double compute_overlap_ms = 0.0;
  double retry_risk = 0.0;
};

struct KvTransportTimelineEvent {
  KvTransportTimelinePhase phase =
      KvTransportTimelinePhase::prepare_descriptor;
  double start_ms = 0.0;
  double duration_ms = 0.0;
};

struct KvTransportReplayResult {
  KvTransportReplayScenario scenario;
  double kv_bytes = 0.0;
  double payload_serialization_ms = 0.0;
  double transfer_ms = 0.0;
  double end_to_end_ms = 0.0;
  double effective_bandwidth_gbps = 0.0;
  double end_to_end_effective_bandwidth_gbps = 0.0;
  double sender_wait_ms = 0.0;
  double receiver_wait_ms = 0.0;
  double overlap_hidden_ms = 0.0;
  double overlap_ratio = 0.0;
  double decode_blocked_ms = 0.0;
  int retry_or_fallback_count = 0;
  double transport_score = 0.0;
  std::vector<KvTransportTimelineEvent> timeline;
  bool active_transfer_workload = false;
  bool ranking_applicable = false;
  std::string recommendation;
};

KvTransportReplayAssumptions default_kv_transport_replay_assumptions();
KvTransportReplayResult estimate_kv_transport_replay(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayScenario& scenario);

enum class PdTransferTopology {
  aggregated_reference,
  prefill_decode_split,
};

const char* pd_transfer_topology_name(PdTransferTopology topology);

enum class PdTransferStageKind {
  prefill_chunk,
  kv_send_recv,
  decode_chunk,
};

const char* pd_transfer_stage_kind_name(PdTransferStageKind kind);

enum class PdTransferResourceKind {
  prefill_worker,
  transfer_link,
  decode_worker,
};

const char* pd_transfer_resource_kind_name(
    PdTransferResourceKind kind);

enum class PdTransferRecommendation {
  no_active_workload,
  aggregated_reference_only,
  increase_transfer_bandwidth_or_prestage_kv,
  transfer_fabric_is_saturated,
  pd_split_is_promising,
  prefer_simpler_aggregated_serving,
};

const char* pd_transfer_recommendation_name(
    PdTransferRecommendation recommendation);
const char* pd_transfer_recommendation_text(
    PdTransferRecommendation recommendation);

struct PdTransferOverlapAssumptions {
  std::string profile_name =
      "compiled_pd_transfer_overlap_analytical_fixture_v2";
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  double high_decode_stall_to_decode_ratio_threshold = 0.25;
  double high_transfer_link_busy_ratio_threshold = 0.85;
  double pd_speedup_vs_aggregated_threshold = 1.20;
  bool decode_requires_complete_prompt_kv = true;
  bool sender_receiver_overhead_is_per_chunk = true;
  bool overlap_efficiency_is_streaming_readiness = true;
};

struct PdTransferOverlapScenario {
  std::string name;
  PdTransferTopology topology =
      PdTransferTopology::aggregated_reference;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int prefill_chunk_tokens = 1;
  int decode_chunk_tokens = 1;
  int prefill_workers = 1;
  int decode_workers = 1;
  double prefill_ms_per_token = 0.0;
  double decode_ms_per_token = 0.0;
  double kv_bytes_per_token = 0.0;
  double link_bandwidth_gbps = 0.0;
  double serialization_us_per_chunk = 0.0;
  double receiver_setup_us_per_chunk = 0.0;
  double overlap_efficiency = 0.0;
};

struct PdTransferStage {
  PdTransferStageKind kind = PdTransferStageKind::prefill_chunk;
  PdTransferResourceKind resource =
      PdTransferResourceKind::prefill_worker;
  int chunk_index = 0;
  int token_count = 0;
  double start_ms = 0.0;
  double duration_ms = 0.0;
  double end_ms = 0.0;
};

struct PdTransferOverlapResult {
  PdTransferOverlapScenario scenario;
  bool active_workload = false;
  bool transfer_required = false;
  bool ranking_applicable = false;
  int prefill_chunks = 0;
  int decode_chunks = 0;
  std::int64_t prompt_kv_token_count = 0;
  double prompt_kv_bytes = 0.0;
  double prompt_kv_gib = 0.0;
  double prefill_compute_ms = 0.0;
  double decode_compute_ms = 0.0;
  double aggregated_baseline_ms = 0.0;
  double transfer_payload_serialization_ms = 0.0;
  double transfer_overhead_ms = 0.0;
  double kv_transfer_total_ms = 0.0;
  double transfer_pipeline_span_ms = 0.0;
  double transfer_link_busy_ratio = 0.0;
  double full_prompt_kv_ready_ms = 0.0;
  double disaggregated_no_overlap_ms = 0.0;
  double overlapped_end_to_end_ms = 0.0;
  double transfer_exposed_ms = 0.0;
  double transfer_hidden_ms = 0.0;
  double decode_stall_ms = 0.0;
  double ttft_ms = 0.0;
  double tpot_ms = 0.0;
  double overlap_gain_ms = 0.0;
  double speedup_vs_no_overlap = 0.0;
  double speedup_vs_aggregated = 0.0;
  double fabric_utilization_score = 0.0;
  PdTransferRecommendation recommendation =
      PdTransferRecommendation::no_active_workload;
  std::vector<PdTransferStage> stages;
};

struct PdTransferOverlapPlan {
  PdTransferOverlapAssumptions assumptions;
  std::vector<PdTransferOverlapScenario> scenarios;
  std::vector<PdTransferOverlapResult> records;
  std::size_t active_record_count = 0;
  std::size_t comparable_record_count = 0;
  std::optional<std::size_t> highest_overlap_gain_record_index;
  std::optional<std::size_t> highest_decode_stall_record_index;
  std::optional<std::size_t> highest_pd_speedup_record_index;
};

PdTransferOverlapAssumptions
default_pd_transfer_overlap_assumptions();
std::vector<PdTransferOverlapScenario>
default_pd_transfer_overlap_scenarios();
PdTransferOverlapResult estimate_pd_transfer_overlap(
    const PdTransferOverlapAssumptions& assumptions,
    const PdTransferOverlapScenario& scenario);
PdTransferOverlapPlan build_pd_transfer_overlap_plan(
    const PdTransferOverlapAssumptions& assumptions,
    const std::vector<PdTransferOverlapScenario>& scenarios);

enum class PdSplitCrossoverBottleneck {
  no_active_workload,
  kv_transfer_path,
  prefill_compute_or_scheduler_queue,
};

const char* pd_split_crossover_bottleneck_name(
    PdSplitCrossoverBottleneck bottleneck);

enum class PdSplitCrossoverDecision {
  no_active_workload,
  prefer_prefill_decode_disaggregation,
  keep_monolithic_or_improve_kv_transport,
  pd_split_needs_more_queue_pressure_or_overlap,
};

const char* pd_split_crossover_decision_name(
    PdSplitCrossoverDecision decision);

enum class PdSplitCrossoverRecommendation {
  no_active_workload,
  ready_to_compare_with_product_replay,
  collect_product_pd_trace_or_reduce_transfer_cost,
};

const char* pd_split_crossover_recommendation_name(
    PdSplitCrossoverRecommendation recommendation);

struct PdSplitCrossoverAssumptions {
  std::string profile_name =
      "compiled_pd_split_crossover_analytical_fixture_v2";
  PdTransferOverlapAssumptions transfer;
  double monolithic_prefill_interference_reference_ms = 2800.0;
  double monolithic_tpot_interference_cap = 0.80;
  double pd_decode_stall_reference_ms = 600.0;
  double pd_tpot_stall_cap = 0.35;
  double queue_relief_weight = 0.45;
  double tpot_relief_weight = 0.35;
  double overlap_efficiency_weight = 0.20;
  double capacity_relief_weight = 0.55;
  double ttft_headroom_weight = 0.25;
  double tpot_improvement_bonus = 0.20;
  double no_tpot_improvement_bonus = 0.04;
  double crossover_threshold = 0.70;
  double transfer_bottleneck_to_prefill_ratio_threshold = 0.35;
};

struct PdSplitCrossoverScenario {
  std::string name;
  std::vector<std::string> product_profile_ids;
  int concurrent_requests = 1;
  PdTransferOverlapScenario transfer;
  double monolithic_queue_ms = 0.0;
  double pd_queue_ms = 0.0;
  double slo_ttft_ms = 1.0;
  double slo_tpot_ms = 1.0;
};

struct PdSplitCrossoverResult {
  PdSplitCrossoverScenario scenario;
  std::vector<std::string> product_alignment;
  PdTransferOverlapResult transfer;
  double transfer_setup_ms_per_chunk = 0.0;
  bool active_workload = false;
  bool ranking_applicable = false;
  bool ttft_slo_met = false;
  bool tpot_slo_met = false;
  double monolithic_ttft_ms = 0.0;
  double pd_ttft_ms = 0.0;
  double monolithic_tpot_ms = 0.0;
  double pd_tpot_ms = 0.0;
  double ttft_delta_ms = 0.0;
  double tpot_delta_ms = 0.0;
  double monolithic_prefill_interference_uncapped = 0.0;
  double monolithic_prefill_interference = 0.0;
  double pd_decode_stall_pressure_uncapped = 0.0;
  double pd_decode_stall_pressure = 0.0;
  double queue_relief_ratio = 0.0;
  double tpot_relief_ratio = 0.0;
  double ttft_risk_uncapped = 0.0;
  double ttft_risk = 0.0;
  double capacity_relief_score_uncapped = 0.0;
  double capacity_relief_score = 0.0;
  double crossover_score_uncapped = 0.0;
  double crossover_score = 0.0;
  PdSplitCrossoverBottleneck bottleneck =
      PdSplitCrossoverBottleneck::no_active_workload;
  PdSplitCrossoverDecision decision =
      PdSplitCrossoverDecision::no_active_workload;
  PdSplitCrossoverRecommendation recommendation =
      PdSplitCrossoverRecommendation::no_active_workload;
};

struct PdSplitCrossoverPlan {
  PdSplitCrossoverAssumptions assumptions;
  std::vector<PdSplitCrossoverScenario> scenarios;
  std::vector<PdSplitCrossoverResult> records;
  std::vector<std::string> product_alignment;
  std::size_t active_record_count = 0;
  std::size_t comparable_record_count = 0;
  std::size_t preferred_pd_record_count = 0;
  std::optional<std::size_t> best_crossover_record_index;
  std::optional<std::size_t> most_decode_blocked_record_index;
};

PdSplitCrossoverAssumptions default_pd_split_crossover_assumptions();
std::vector<PdSplitCrossoverScenario>
default_pd_split_crossover_scenarios();
PdSplitCrossoverResult estimate_pd_split_crossover(
    const PdSplitCrossoverAssumptions& assumptions,
    const PdSplitCrossoverScenario& scenario);
PdSplitCrossoverPlan build_pd_split_crossover_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios);
void validate_pd_split_crossover_plan(
    const PdSplitCrossoverPlan& plan);

struct TensorParallelCommScenario {
  std::string name;
  std::string backend;
  int tensor_parallel_size = 1;
  int pipeline_parallel_size = 1;
  int batch_size = 1;
  int hidden_size = 4096;
  int sequence_tokens = 2048;
  int layers = 32;
  double activation_bytes = 2.0;
  double link_bandwidth_gbps = 300.0;
  double collective_latency_us = 18.0;
  double overlap_ratio = 0.45;
  double compute_ms_per_layer = 1.0;
  int replica_count = 0;
  int total_accelerator_count = 0;
  int accelerators_per_replica = 0;
  int accelerators_per_node = 0;
  double accelerator_memory_gib = 0.0;
};

struct TensorParallelCommResult {
  TensorParallelCommScenario scenario;
  double activation_mb_per_layer = 0.0;
  double all_reduce_mb = 0.0;
  double all_gather_mb = 0.0;
  double collective_latency_ms = 0.0;
  double communication_ms_per_layer = 0.0;
  double exposed_comm_ms = 0.0;
  double compute_ms_total = 0.0;
  double comm_to_compute_ratio = 0.0;
  double scaling_efficiency = 0.0;
  std::string parallelism_policy;
  std::string recommendation;
};

TensorParallelCommResult estimate_tensor_parallel_comm(
    const TensorParallelCommScenario& scenario);

std::vector<TensorParallelCommScenario>
default_tensor_parallel_comm_scenarios();

}  // namespace inferlab
