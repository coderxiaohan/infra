#include "artifact_contract_model.hpp"

#include "deep_dive_core_artifact_registry.hpp"
#include "deep_dive_evidence_bundle_fixture.hpp"
#include "mature_product_artifact_dispatch.hpp"
#include "plan_json_utils.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inferlab_artifact_contract {
namespace {

enum class FieldProfile {
  NativePlan,
  DeepDive,
  Audit,
  ProductReplay,
};

void append_unique_all(std::vector<std::string>& values,
                       const std::vector<std::string>& additions) {
  for (const auto& value : additions) {
    inferlab_plan::append_unique_string(values, value);
  }
}

std::vector<std::string> public_native_fields() {
  return {"schema_version", "status", "artifact_type", "mode",
          "executed", "replayed", "claim_scope", "summary", "records",
          "schema", "generated_at", "environment", "official_sources",
          "claim_scope_detail", "refresh_commands", "migration_commands",
          "not_run", "recommended_next_commands"};
}

std::vector<std::string> fields_for(const FieldProfile profile) {
  auto fields = public_native_fields();
  switch (profile) {
    case FieldProfile::NativePlan:
      return fields;
    case FieldProfile::DeepDive:
      append_unique_all(fields,
                        {"deep_dive_type", "research_basis",
                         "product_alignment", "evidence_bridge_inputs"});
      return fields;
    case FieldProfile::Audit:
      append_unique_all(fields, {"audit_type", "research_basis"});
      return fields;
    case FieldProfile::ProductReplay:
      append_unique_all(fields,
                        {"sample_replay_source", "sample_sources",
                         "normalized_events", "product_family_coverage",
                         "event_schema_coverage", "native_alignment_coverage",
                         "release_pipeline_linkage"});
      return fields;
  }
  throw std::logic_error("unknown artifact contract field profile");
}

Contract external_contract(std::string name,
                           std::string artifact,
                           std::string producer,
                           std::string producer_target,
                           std::string evidence_level,
                           EvidenceRequirements requirements,
                           std::vector<std::string> required_fields,
                           std::vector<std::string> required_metrics,
                           std::vector<std::string> blockers,
                           std::vector<std::string> refresh_commands) {
  Contract contract;
  contract.name = std::move(name);
  contract.artifact = std::move(artifact);
  contract.producer = std::move(producer);
  contract.producer_target = std::move(producer_target);
  contract.evidence_level = std::move(evidence_level);
  contract.source = ContractSource::ExternalEvidence;
  contract.requirements = requirements;
  contract.required_fields = std::move(required_fields);
  contract.required_metrics = std::move(required_metrics);
  contract.blockers = std::move(blockers);
  contract.refresh_commands = std::move(refresh_commands);
  contract.claim_boundary =
      "External evidence contract is a migration declaration only; this plan does not run the producer, inspect its output, or establish measured performance, profiler, product, hardware, or release evidence.";
  return contract;
}

Contract mature_product_dispatch_contract(
    const std::string_view artifact_type,
    std::string evidence_level,
    const FieldProfile profile,
    std::vector<std::string> required_metrics,
    std::vector<std::string> extra_fields = {}) {
  const auto* dispatch = inferlab_mature_product::find_exact(artifact_type);
  if (dispatch == nullptr) {
    throw std::logic_error(
        "artifact contract references an unknown mature-product dispatch: " +
        std::string(artifact_type));
  }
  auto fields = fields_for(profile);
  append_unique_all(fields, extra_fields);
  Contract contract;
  contract.name = std::string(dispatch->artifact_type);
  contract.artifact =
      "results/raw/" + inferlab_mature_product::artifact_filename(*dispatch);
  contract.producer = "C++ native_core mature-product dispatch";
  contract.producer_target =
      std::string(inferlab_mature_product::canonical_target(dispatch->family));
  contract.evidence_level = std::move(evidence_level);
  contract.source = ContractSource::MatureProductDispatch;
  contract.required_fields = std::move(fields);
  contract.required_metrics = std::move(required_metrics);
  contract.refresh_commands = {
      inferlab_mature_product::refresh_command(dispatch->artifact_type)};
  contract.claim_boundary =
      "Canonical mature-product dispatch contract for an InferLab mechanism slice only; the external product owns its runtime and this declaration does not prove replay, execute, performance, or release readiness.";
  return contract;
}

Contract deep_dive_wiring_contract(
    const std::string& contract_name,
    std::string evidence_level,
    const FieldProfile profile,
    std::vector<std::string> required_metrics,
    std::vector<std::string> extra_fields = {}) {
  const auto& wiring =
      inferlab_deep_dive_bundle::require_artifact_wiring_spec(contract_name);
  auto fields = fields_for(profile);
  append_unique_all(fields, extra_fields);
  Contract contract;
  contract.name = wiring.contract_name;
  contract.artifact = "results/raw/" + wiring.artifact;
  contract.producer = "C++ native_core deep-dive wiring registry";
  contract.producer_target = wiring.target;
  contract.evidence_level = std::move(evidence_level);
  contract.source = ContractSource::DeepDiveWiringRegistry;
  contract.required_fields = std::move(fields);
  contract.required_metrics = std::move(required_metrics);
  contract.refresh_commands = {wiring.command};
  contract.claim_boundary =
      "Canonical Deep-Dive wiring contract declaration only; it does not read the artifact or prove benchmark, replay, execute, freshness, performance, or release readiness.";
  return contract;
}

Contract derived_report_contract() {
  Contract contract;
  contract.name = "final_report";
  contract.artifact = "reports/final_report.md";
  contract.producer = "report renderer";
  contract.producer_target = "scripts/render_final_report.py";
  contract.evidence_level = "derived_report";
  contract.source = ContractSource::DerivedReport;
  contract.required_fields =
      {"module_coverage", "missing_evidence", "reproducible_commands"};
  contract.required_metrics =
      {"available_sources", "missing_sources", "blocked_runtime_items"};
  contract.blockers = {"runtime_artifacts_stale"};
  contract.refresh_commands = {
      "python3 scripts/summarize_benchmarks.py --repo-root . --results results/raw --output results/tables/benchmark_summary.md --json-output results/raw/benchmark_summary.json",
      "python3 scripts/render_final_report.py --repo-root . --results results/raw --summary results/raw/benchmark_summary.json --output reports/final_report.md --json-output results/raw/final_report_index.json"};
  contract.claim_boundary =
      "Final report is a derived consumer of existing evidence; rendering it does not create benchmark, replay, execute, freshness, or release evidence.";
  return contract;
}

}  // namespace

const std::vector<Contract>& contract_extension_registry() {
  static const std::vector<Contract> contracts = [] {
    constexpr std::size_t kExpectedNonCatalogContractCount = 33U;
    std::vector<Contract> contracts;
    contracts.reserve(kExpectedNonCatalogContractCount);
  contracts.push_back(deep_dive_wiring_contract(
      "native_kv_lifecycle_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"logical_to_physical_blocks", "prefix_sharing", "fragmentation",
       "tier_decisions", "transport_timeline", "transfer_vs_prefill",
       "lifecycle_score"}));
  contracts.push_back(deep_dive_wiring_contract(
      "native_kv_tier_decision_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"selected_tier", "tier_candidates", "hbm_pressure",
       "prefill_recompute_ms", "net_latency_delta_ms", "decision"}));
  contracts.push_back(deep_dive_wiring_contract(
      "native_pd_split_crossover_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      inferlab_deep_dive_core::required_metrics_with_native_trace(
          {"kv_bytes_to_transfer", "kv_transfer_ms", "decode_blocked_ms",
           "ttft_delta_ms", "tpot_delta_ms", "crossover_decision"}),
      {"native_trace_context"}));
  contracts.push_back(deep_dive_wiring_contract(
      "native_scheduler_root_cause_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      inferlab_deep_dive_core::required_metrics_with_native_trace(
          {"request_id", "queue_wait_ms", "decode_stall_ms", "ttft_ms",
           "tpot_ms", "p99_risk", "root_cause"}),
      {"native_trace_context"}));
  contracts.push_back(deep_dive_wiring_contract(
      "native_kernel_compatibility_matrix",
      "kernel_compatibility_contract", FieldProfile::DeepDive,
      {"operator", "shape_signature", "dtype", "selected_backend",
       "fallback_reason", "correctness_contract",
       "profiling_metadata_required"}));
  contracts.push_back(deep_dive_wiring_contract(
      "product_replay_semantic_events",
      "product_replay_semantic_extractor", FieldProfile::ProductReplay,
      {"event_type", "source_artifact", "native_alignment",
       "observed_signal", "sample_event_count", "request_trace_join",
       "replay_ready_request_count",
       "trace_correlation_eligible_request_count",
       "trace_join_token_count", "distinct_trace_join_token_count",
       "trace_join_token_algorithm",
       "request_span_context_request_count",
       "request_span_lineage_eligible_request_count",
       "request_span_join_token_count",
       "distinct_request_span_join_token_count",
       "request_span_binding_scope",
       "request_span_join_token_algorithm",
       "request_span_binding_conflict_request_count",
       "explicit_request_span_lifecycle_supported",
       "request_terminal_event_count", "span_lifecycle_supplied",
       "span_lifecycle_contract_ready", "span_terminal_status",
       "span_lifecycle_status",
       "span_lifecycle_supplied_request_count",
       "span_lifecycle_ready_request_count",
       "span_lifecycle_blocked_request_count",
       "span_lifecycle_completed_request_count",
       "span_lifecycle_error_request_count",
       "span_lifecycle_cancelled_request_count",
       "span_lifecycle_rejected_request_count",
       "span_lifecycle_contract",
       "trace_correlation_set_fingerprint", "clock_calibration",
       "clock_calibration_eligible_request_count",
       "clock_calibrated_request_count",
       "clock_uncalibrated_request_count",
       "clock_normalization_performed",
       "absolute_timestamp_values_emitted",
       "relative_timestamp_values_emitted",
       "relative_time_window_present",
       "trace_relative_window_start_ns",
       "trace_relative_window_end_ns", "clock_uncertainty_ns",
       "claim_boundary"}));

  contracts.push_back(mature_product_dispatch_contract(
      "native_streaming_backpressure_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"first_event_latency_ms", "inter_token_gap_ms",
       "client_receive_lag_ms", "slot_release_risk", "streaming_policy"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_structured_output_parser_contract",
      "parser_contract", FieldProfile::DeepDive,
      {"json_parse_success_rate", "schema_valid_rate",
       "required_args_missing_rate", "parser_policy"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_autoscaler_cost_slo_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"current_replicas", "desired_replicas", "queue_pressure",
       "slo_violation_risk", "gpu_hour_cost_delta"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_lora_adapter_cache_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"cache_hit_rate", "evictions_per_minute", "adapter_memory_mb",
       "load_penalty_p99_ms", "kv_share_allowed_across_adapters"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_grammar_backend_matrix",
      "parser_contract", FieldProfile::DeepDive,
      {"schema_coverage", "compile_cache_hit_rate", "mask_cost_ms",
       "fallback_rate", "supported_surface"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_edge_compiler_artifact_matrix",
      "edge_artifact_contract", FieldProfile::DeepDive,
      {"artifact_completeness", "device_memory_risk", "abi_readiness",
       "backend_compatibility", "artifact_layout"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_kv_transport_timeline_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"kv_bytes", "sender_queue_ms", "network_ms",
       "receiver_restore_ms", "decode_blocked_ms", "overlap_ratio"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_radix_prefix_cache_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"prefix_tokens", "shared_physical_blocks", "hit_rate",
       "eviction_cost_ms", "fragmentation_score", "cache_policy"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_kv_compression_blend_risk",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"reuse_risk", "compression_ratio", "expected_restore_ms",
       "prefill_recompute_ms", "correctness_risk", "policy"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_chunked_prefill_stall_free_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"prefill_chunk_size", "decode_stall_ms", "ttft_delta_ms",
       "tpot_delta_ms", "stall_free_score", "p99_risk"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_tenant_quota_admission_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"tenant_quota_tokens", "queue_depth", "fairness_score",
       "admission_score", "slo_violation_risk", "admission_decision"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_live_request_migration_deep_dive",
      "deep_dive_evidence_bridge", FieldProfile::DeepDive,
      {"kv_bytes_to_move", "migration_ms", "rollback_ms",
       "slo_rescue_score", "decode_pause_ms", "decision"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_quant_correctness_contract",
      "kernel_correctness_contract", FieldProfile::DeepDive,
      {"max_abs_error_tolerance", "scale_granularity", "range_clip_rate",
       "fallback_risk", "dtype", "reference"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_paged_kv_gather_kernel_contract",
      "kernel_correctness_contract", FieldProfile::DeepDive,
      {"logical_tokens", "physical_blocks", "estimated_bytes",
       "coalescing_risk", "fallback_risk", "layout"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_flashinfer_replay_alignment",
      "product_kernel_replay_alignment", FieldProfile::DeepDive,
      {"shape_coverage", "dtype_coverage", "fallback_rate",
       "required_metadata_present", "operator", "mode"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_trtllm_engine_quant_alignment",
      "product_engine_quant_alignment", FieldProfile::DeepDive,
      {"engine_metadata_completeness", "precision_contract_present",
       "paged_kv_contract_present", "release_blocker_count", "precision"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_gateway_policy_matrix",
      "control_plane_policy_matrix", FieldProfile::DeepDive,
      {"prefix_affinity_score", "adapter_affinity_score", "queue_penalty",
       "tenant_quota_risk", "selected_policy"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_deployment_readiness_matrix",
      "deployment_readiness_matrix", FieldProfile::DeepDive,
      {"manifest_completeness", "metrics_evidence_present",
       "rollback_readiness", "release_blocker_count", "deployment_mode"}));
  contracts.push_back(mature_product_dispatch_contract(
      "native_openai_api_conformance_matrix",
      "api_conformance_matrix", FieldProfile::DeepDive,
      {"surface_coverage", "streaming_case_coverage",
       "tool_call_case_coverage", "error_mapping_coverage", "api_surface"}));
  contracts.push_back(deep_dive_wiring_contract(
      "product_replay_alignment",
      "product_replay_alignment", FieldProfile::Audit,
      {"alignment_score", "product_coverage", "replay_evidence_coverage",
       "native_coverage", "aligned_rule_count"},
      {"not_implemented_by_inferlab"}));
  contracts.push_back(deep_dive_wiring_contract(
      "native_deep_dive_contract_check",
      "deep_dive_contract_check", FieldProfile::Audit,
      {"contract_score", "pass_count", "warn_count", "fail_count",
       "gate_decision"},
      {"strict_missing"}));
  contracts.push_back(external_contract(
      "online_vllm_serving_benchmark",
      "results/raw/vllm_serving_benchmark.json", "backend harness",
      "scripts/run_vllm_benchmark.py", "real_backend_trace",
      {true, true, false},
      {"schema_version", "status", "mode", "environment", "preflight",
       "requests", "summary"},
      {"request_count", "completed_requests", "failed_requests",
       "avg_latency_ms", "median_latency_ms", "p90_latency_ms",
       "request_throughput_per_s", "completion_token_throughput_per_s"},
      {"backend_not_running", "model_missing", "gpu_unavailable"},
      {"python3 scripts/run_vllm_benchmark.py --repo-root . --base-url http://127.0.0.1:8000 --output results/raw/vllm_serving_benchmark.json"}));
  contracts.push_back(external_contract(
      "online_trtllm_serving_benchmark",
      "results/raw/trtllm_benchmark.json", "backend harness",
      "scripts/run_trtllm_benchmark.py", "real_backend_trace",
      {true, true, false},
      {"schema_version", "status", "mode", "environment", "preflight",
       "run", "metrics", "summary"},
      {"avg_latency_ms", "p50_latency_ms", "p90_latency_ms",
       "p95_latency_ms", "p99_latency_ms", "request_throughput_per_s",
       "token_throughput_per_s", "throughput_per_s"},
      {"backend_not_running", "engine_missing", "gpu_unavailable"},
      {"python3 scripts/run_trtllm_benchmark.py --repo-root . --mode bench "
       "--engine-dir engines/model --output "
       "results/raw/trtllm_benchmark.json"}));
  contracts.push_back(external_contract(
      "profiling_artifacts", "results/raw/profiling_artifacts.json",
      "artifact importer", "scripts/parse_profiling_artifacts.py",
      "external_profiler_trace", {true, true, false},
      {"schema_version", "status", "executed", "artifact_root", "records",
       "summary"},
      {"profiler_file_count", "kernel_count", "hotspot_count"},
      {"nsight_or_perf_artifacts_missing"},
      {"python3 scripts/parse_profiling_artifacts.py --repo-root . --artifact-root profiling --output results/raw/profiling_artifacts.json"}));
  contracts.push_back(derived_report_contract());

    if (contracts.size() != kExpectedNonCatalogContractCount) {
      throw std::logic_error(
          "artifact contract non-catalog inventory count drift");
    }
    return contracts;
  }();
  return contracts;
}

const Contract& require_contract_extension(const std::string& name) {
  if (name.empty()) {
    throw std::invalid_argument(
        "artifact contract lookup name must not be empty");
  }
  const Contract* match = nullptr;
  for (const auto& contract : contract_extension_registry()) {
    if (contract.name != name) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "artifact contract lookup is ambiguous: " + name);
    }
    match = &contract;
  }
  if (match == nullptr) {
    throw std::out_of_range("unknown artifact contract: " + name);
  }
  return *match;
}

}  // namespace inferlab_artifact_contract
