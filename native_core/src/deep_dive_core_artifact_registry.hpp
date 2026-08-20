#pragma once

#include "advanced_serving_internal.hpp"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_deep_dive_core {

struct ArtifactSpec {
  std::string id;
  std::string legacy_bundle_name;
  std::string artifact;
  std::string source_path;
  std::string report_path;
  std::string target;
  std::string matrix_entry;
  std::string command;
  std::string expected_kind;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
  std::string claim_boundary;
};

inline const std::vector<std::string>& common_required_fields() {
  static const std::vector<std::string> fields = {
      "schema_version",
      "status",
      "artifact_type",
      "schema",
      "mode",
      "executed",
      "replayed",
      "generated_at",
      "environment",
      "official_sources",
      "claim_scope_detail",
      "refresh_commands",
      "migration_commands",
      "not_run",
      "claim_scope",
      "summary",
      "records",
      "deep_dive_type",
      "research_basis",
      "product_alignment",
      "evidence_bridge_inputs",
      "native_trace_context"};
  return fields;
}

inline const std::vector<std::string>& native_trace_contract_metrics() {
  static const std::vector<std::string> metrics = {
      "native_trace_context_decision",
      "context_present_count",
      "trace_join_token_count",
      "distinct_trace_join_token_count",
      "span_join_token_count",
      "parent_span_join_token_count",
      "clock_domain_count",
      "join_token_algorithm",
      "source_contract_ready",
      "raw_identity_values_emitted",
      "timestamp_values_emitted",
      "absolute_timestamp_values_emitted",
      "relative_timestamp_values_emitted",
      "join_tokens_are_cryptographic",
      "cross_artifact_join_performed",
      "clock_normalization_performed",
      "clock_calibration",
      "clock_calibrated_record_count",
      "clock_uncalibrated_record_count",
      "clock_calibration_status",
      "relative_timestamp_present",
      "trace_relative_timestamp_ns",
      "clock_uncertainty_ns",
      "span_parentage_proven",
      "causal_relation_proven"};
  return metrics;
}

inline std::vector<std::string> required_metrics_with_native_trace(
    const std::initializer_list<const char*> domain_metrics) {
  const auto& trace_metrics = native_trace_contract_metrics();
  std::vector<std::string> metrics;
  metrics.reserve(domain_metrics.size() + trace_metrics.size());
  for (const char* metric : domain_metrics) {
    metrics.emplace_back(metric);
  }
  metrics.insert(metrics.end(), trace_metrics.begin(),
                 trace_metrics.end());
  return metrics;
}

inline bool safe_json_filename(const std::string& value) {
  return value.size() > 5U &&
         value.compare(value.size() - 5U, 5U, ".json") == 0 &&
         value.find('/') == std::string::npos &&
         value.find('\\') == std::string::npos &&
         value.find("..") == std::string::npos;
}

inline bool starts_with(const std::string& value,
                        const std::string& prefix) {
  return value.rfind(prefix, 0U) == 0U;
}

inline bool ends_with(const std::string& value,
                      const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

inline bool safe_relative_repository_path(const std::string& value) {
  return !value.empty() && value.front() != '/' &&
         value.find('\\') == std::string::npos &&
         value.find("..") == std::string::npos;
}

inline bool safe_identity_token(const std::string& value) {
  if (value.empty()) return false;
  for (const char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
        ch == '_') {
      continue;
    }
    return false;
  }
  return true;
}

inline void validate_registry(const std::vector<ArtifactSpec>& specs) {
  constexpr std::size_t kExpectedSpecCount = 3U;
  if (specs.size() != kExpectedSpecCount) {
    throw std::logic_error("core deep-dive artifact registry count drift");
  }
  for (std::size_t i = 0U; i < specs.size(); ++i) {
    const auto& spec = specs[i];
    if (spec.id.empty() || spec.legacy_bundle_name.empty() ||
        !safe_json_filename(spec.artifact) || spec.source_path.empty() ||
        spec.report_path.empty() || spec.target.empty() ||
        spec.matrix_entry.empty() || spec.command.empty() ||
        spec.expected_kind.empty() || spec.required_fields.empty() ||
        spec.required_metrics.empty() || spec.claim_boundary.empty()) {
      throw std::logic_error(
          "core deep-dive artifact registry contains an invalid record");
    }
    const std::string artifact_stem =
        spec.artifact.substr(0U, spec.artifact.size() - 5U);
    const std::string expected_matrix_entry = artifact_stem + "_refresh";
    const std::string expected_command =
        "./build/" + spec.target +
        " --repo-root . --output results/raw/" + spec.artifact;
    if (!safe_identity_token(spec.id) ||
        !safe_identity_token(spec.legacy_bundle_name) ||
        !starts_with(spec.artifact, "native_") ||
        !safe_relative_repository_path(spec.source_path) ||
        !starts_with(spec.source_path, "native_core/src/") ||
        !ends_with(spec.source_path, "_main.cpp") ||
        !safe_relative_repository_path(spec.report_path) ||
        !starts_with(spec.report_path, "reports/") ||
        !ends_with(spec.report_path, ".md") ||
        !starts_with(spec.target, "inferlab_") ||
        !safe_identity_token(spec.target) ||
        spec.matrix_entry != expected_matrix_entry ||
        spec.command != expected_command ||
        spec.legacy_bundle_name != spec.expected_kind ||
        spec.required_fields != common_required_fields()) {
      throw std::logic_error(
          "core deep-dive artifact registry wiring contract is invalid: " +
          spec.id);
    }
    inferlab::advanced_serving_internal::require_unique_nonempty_strings(
        spec.required_fields, "core deep-dive required fields");
    inferlab::advanced_serving_internal::require_unique_nonempty_strings(
        spec.required_metrics, "core deep-dive required metrics");
    for (std::size_t j = i + 1U; j < specs.size(); ++j) {
      if (spec.id == specs[j].id || spec.legacy_bundle_name ==
                                        specs[j].legacy_bundle_name ||
          spec.artifact == specs[j].artifact ||
          spec.matrix_entry == specs[j].matrix_entry) {
        throw std::logic_error(
            "core deep-dive artifact registry identity is duplicated");
      }
    }
  }
}

inline const std::vector<ArtifactSpec>& artifact_specs() {
  static const std::vector<ArtifactSpec> specs = {
      {"kv_paged_attention_deep_dive",
       "kv_paged_attention",
       "native_kv_paged_attention_deep_dive.json",
       "native_core/src/kv_paged_attention_deep_dive_main.cpp",
       "reports/61_kv_paged_attention_deep_dive.md",
       "inferlab_kv_paged_attention_deep_dive",
       "native_kv_paged_attention_deep_dive_refresh",
       "./build/inferlab_kv_paged_attention_deep_dive --repo-root . --output results/raw/native_kv_paged_attention_deep_dive.json",
       "kv_paged_attention",
       common_required_fields(),
       required_metrics_with_native_trace(
           {"logical_to_physical_samples",
            "read_amplification",
            "prefix_sharing_rate",
            "transfer_vs_prefill_decision",
            "internal_fragmentation_ratio",
            "external_fragmentation_ratio"}),
       "native KV/PagedAttention evidence bridge only; mature-product PagedAttention remains external"},
      {"scheduler_batching_deep_dive",
       "scheduler_batching",
       "native_scheduler_batching_deep_dive.json",
       "native_core/src/scheduler_batching_deep_dive_main.cpp",
       "reports/62_scheduler_batching_deep_dive.md",
       "inferlab_scheduler_batching_deep_dive",
       "native_scheduler_batching_deep_dive_refresh",
       "./build/inferlab_scheduler_batching_deep_dive --repo-root . --output results/raw/native_scheduler_batching_deep_dive.json",
       "scheduler_batching",
       common_required_fields(),
       required_metrics_with_native_trace(
           {"ttft_ms",
            "tpot_ms",
            "p99_latency_ms",
            "decode_stall_ms",
            "prefill_chunk_timeline",
            "slo_violation_risk",
            "admission_decision"}),
       "native scheduler/batching evidence bridge only; mature-product schedulers remain external"},
      {"cuda_quant_microkernel_deep_dive",
       "cuda_quant_microkernel",
       "native_cuda_quant_microkernel_deep_dive.json",
       "native_core/src/cuda_quant_microkernel_deep_dive_main.cpp",
       "reports/63_cuda_quant_microkernel_deep_dive.md",
       "inferlab_cuda_quant_microkernel_deep_dive",
       "native_cuda_quant_microkernel_deep_dive_refresh",
       "./build/inferlab_cuda_quant_microkernel_deep_dive --repo-root . --output results/raw/native_cuda_quant_microkernel_deep_dive.json",
       "cuda_quant_microkernel",
       common_required_fields(),
       required_metrics_with_native_trace(
           {"arithmetic_intensity_flops_per_byte",
            "bottleneck_model",
            "correctness_contract",
            "fallback_contract",
            "profiling_metadata_required",
            "microkernel_readiness_score"}),
       "native CUDA/Quant evidence bridge only; FlashInfer/TensorRT-LLM/CUTLASS kernels remain external"}};
  static const bool validated = [] {
    validate_registry(specs);
    return true;
  }();
  (void)validated;
  return specs;
}

inline const ArtifactSpec* find_by_artifact(const std::string& artifact) {
  for (const auto& spec : artifact_specs()) {
    if (spec.artifact == artifact) return &spec;
  }
  return nullptr;
}

inline const ArtifactSpec& require_artifact_spec(const std::string& id) {
  if (id.empty()) {
    throw std::invalid_argument(
        "core deep-dive artifact lookup id must not be empty");
  }
  const ArtifactSpec* match = nullptr;
  for (const auto& spec : artifact_specs()) {
    if (spec.id != id) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "core deep-dive artifact lookup is ambiguous: " + id);
    }
    match = &spec;
  }
  if (match == nullptr) {
    throw std::out_of_range("unknown core deep-dive artifact: " + id);
  }
  return *match;
}

}  // namespace inferlab_deep_dive_core
