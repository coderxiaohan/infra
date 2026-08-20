#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace inferlab_mature_product {

enum class ArtifactFamily {
  p2,
  borrowed_gap,
};

inline constexpr char kP2CanonicalTarget[] =
    "inferlab_mature_product_p2_artifacts";
inline constexpr char kBorrowedGapCanonicalTarget[] =
    "inferlab_mature_product_borrowed_gap_artifacts";

struct ArtifactDispatchSpec {
  std::string_view artifact_type;
  ArtifactFamily family;
  std::string_view legacy_bundle_name;
  std::string_view report_path;
};

inline constexpr std::array<ArtifactDispatchSpec, 19> kArtifactDispatchSpecs{{
    {"native_streaming_backpressure_deep_dive", ArtifactFamily::p2,
     "streaming_backpressure", "reports/75_streaming_backpressure_deep_dive.md"},
    {"native_structured_output_parser_contract", ArtifactFamily::p2,
     "structured_output_parser_contract",
     "reports/76_structured_output_parser_contract.md"},
    {"native_autoscaler_cost_slo_deep_dive", ArtifactFamily::p2,
     "autoscaler_cost_slo", "reports/77_autoscaler_cost_slo_deep_dive.md"},
    {"native_lora_adapter_cache_deep_dive", ArtifactFamily::p2,
     "lora_adapter_cache", "reports/78_lora_adapter_cache_deep_dive.md"},
    {"native_grammar_backend_matrix", ArtifactFamily::p2,
     "grammar_backend_matrix", "reports/79_grammar_backend_matrix.md"},
    {"native_edge_compiler_artifact_matrix", ArtifactFamily::p2,
     "edge_compiler_artifact_matrix",
     "reports/80_edge_compiler_artifact_matrix.md"},
    {"native_kv_transport_timeline_deep_dive", ArtifactFamily::borrowed_gap,
     "kv_transport_timeline", "reports/81_kv_transport_timeline_deep_dive.md"},
    {"native_radix_prefix_cache_deep_dive", ArtifactFamily::borrowed_gap,
     "radix_prefix_cache", "reports/82_radix_prefix_cache_deep_dive.md"},
    {"native_kv_compression_blend_risk", ArtifactFamily::borrowed_gap,
     "kv_compression_blend_risk", "reports/83_kv_compression_blend_risk.md"},
    {"native_chunked_prefill_stall_free_deep_dive",
     ArtifactFamily::borrowed_gap, "chunked_prefill_stall_free",
     "reports/84_chunked_prefill_stall_free_deep_dive.md"},
    {"native_tenant_quota_admission_deep_dive",
     ArtifactFamily::borrowed_gap, "tenant_quota_admission",
     "reports/85_tenant_quota_admission_deep_dive.md"},
    {"native_live_request_migration_deep_dive",
     ArtifactFamily::borrowed_gap, "live_request_migration",
     "reports/86_live_request_migration_deep_dive.md"},
    {"native_quant_correctness_contract", ArtifactFamily::borrowed_gap,
     "quant_correctness_contract", "reports/87_quant_correctness_contract.md"},
    {"native_paged_kv_gather_kernel_contract", ArtifactFamily::borrowed_gap,
     "paged_kv_gather_kernel_contract",
     "reports/88_paged_kv_gather_kernel_contract.md"},
    {"native_flashinfer_replay_alignment", ArtifactFamily::borrowed_gap,
     "flashinfer_replay_alignment", "reports/89_flashinfer_replay_alignment.md"},
    {"native_trtllm_engine_quant_alignment", ArtifactFamily::borrowed_gap,
     "trtllm_engine_quant_alignment",
     "reports/90_trtllm_engine_quant_alignment.md"},
    {"native_gateway_policy_matrix", ArtifactFamily::borrowed_gap,
     "gateway_policy_matrix", "reports/91_gateway_policy_matrix.md"},
    {"native_deployment_readiness_matrix", ArtifactFamily::borrowed_gap,
     "deployment_readiness_matrix", "reports/92_deployment_readiness_matrix.md"},
    {"native_openai_api_conformance_matrix", ArtifactFamily::borrowed_gap,
     "openai_api_conformance_matrix",
     "reports/93_openai_api_conformance_matrix.md"},
}};

inline constexpr std::string_view canonical_target(
    const ArtifactFamily family) {
  switch (family) {
    case ArtifactFamily::p2:
      return kP2CanonicalTarget;
    case ArtifactFamily::borrowed_gap:
      return kBorrowedGapCanonicalTarget;
  }
  throw std::invalid_argument("unsupported mature-product artifact family");
}

inline constexpr bool is_safe_dispatch_token(const std::string_view value) {
  if (value.empty()) return false;
  for (const char ch : value) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') {
      continue;
    }
    return false;
  }
  return true;
}

inline constexpr bool starts_with(const std::string_view value,
                                  const std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

inline constexpr bool ends_with(const std::string_view value,
                                const std::string_view suffix) {
  return value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size()) == suffix;
}

inline constexpr std::string_view compatibility_selector(
    const std::string_view artifact_type) {
  constexpr std::string_view prefix = "native_";
  return starts_with(artifact_type, prefix)
             ? artifact_type.substr(prefix.size())
             : artifact_type;
}

inline constexpr std::size_t family_spec_count(const ArtifactFamily family) {
  std::size_t count = 0;
  for (const auto& spec : kArtifactDispatchSpecs) {
    if (spec.family == family) ++count;
  }
  return count;
}

inline constexpr bool dispatch_registry_is_valid() {
  if (!is_safe_dispatch_token(kP2CanonicalTarget) ||
      !is_safe_dispatch_token(kBorrowedGapCanonicalTarget) ||
      family_spec_count(ArtifactFamily::p2) != 6 ||
      family_spec_count(ArtifactFamily::borrowed_gap) != 13) {
    return false;
  }
  for (std::size_t i = 0; i < kArtifactDispatchSpecs.size(); ++i) {
    const auto& spec = kArtifactDispatchSpecs[i];
    const std::string_view compatibility =
        compatibility_selector(spec.artifact_type);
    if (!starts_with(spec.artifact_type, "native_") ||
        !is_safe_dispatch_token(spec.artifact_type) ||
        !is_safe_dispatch_token(compatibility) ||
        !is_safe_dispatch_token(spec.legacy_bundle_name) ||
        !starts_with(spec.report_path, "reports/") ||
        !ends_with(spec.report_path, ".md")) {
      return false;
    }
    for (std::size_t j = i + 1; j < kArtifactDispatchSpecs.size(); ++j) {
      if (spec.artifact_type == kArtifactDispatchSpecs[j].artifact_type ||
          spec.legacy_bundle_name ==
              kArtifactDispatchSpecs[j].legacy_bundle_name ||
          spec.report_path == kArtifactDispatchSpecs[j].report_path ||
          compatibility ==
              compatibility_selector(kArtifactDispatchSpecs[j].artifact_type)) {
        return false;
      }
    }
  }
  return true;
}

static_assert(dispatch_registry_is_valid(),
              "mature-product artifact dispatch registry is invalid");

inline const ArtifactDispatchSpec* find_exact(
    const std::string_view artifact_type) {
  for (const auto& spec : kArtifactDispatchSpecs) {
    if (spec.artifact_type == artifact_type) return &spec;
  }
  return nullptr;
}

inline std::string_view selector_basename(const std::string_view requested) {
  const std::size_t separator = requested.find_last_of("/\\");
  return separator == std::string_view::npos
             ? requested
             : requested.substr(separator + 1);
}

inline const ArtifactDispatchSpec& resolve_selector(
    const std::string_view requested,
    const ArtifactFamily family) {
  (void)canonical_target(family);
  const std::string_view selector = selector_basename(requested);
  if (selector.empty()) {
    throw std::invalid_argument(
        "mature-product artifact selector must not be empty");
  }
  for (const auto& spec : kArtifactDispatchSpecs) {
    if (spec.family != family) continue;
    const std::string_view compatibility =
        compatibility_selector(spec.artifact_type);
    const std::string compatibility_executable =
        "inferlab_" + std::string(compatibility);
    if (selector == spec.artifact_type || selector == compatibility ||
        selector == compatibility_executable) {
      return spec;
    }
  }
  throw std::invalid_argument(
      "unsupported mature-product artifact selector for " +
      std::string(canonical_target(family)));
}

inline std::string refresh_command(const std::string_view artifact_type) {
  const ArtifactDispatchSpec* spec = find_exact(artifact_type);
  if (spec == nullptr) {
    throw std::invalid_argument(
        "cannot build mature-product refresh command for unknown artifact: " +
        std::string(artifact_type));
  }
  return "./build/" + std::string(canonical_target(spec->family)) +
         " --repo-root . --artifact " + std::string(spec->artifact_type) +
         " --output results/raw/" + std::string(spec->artifact_type) +
         ".json";
}

inline std::string artifact_filename(const ArtifactDispatchSpec& spec) {
  return std::string(spec.artifact_type) + ".json";
}

inline std::string source_path(const ArtifactDispatchSpec& spec) {
  switch (spec.family) {
    case ArtifactFamily::p2:
      return "native_core/src/mature_product_p2_artifacts_main.cpp";
    case ArtifactFamily::borrowed_gap:
      return "native_core/src/mature_product_borrowed_gap_artifacts_main.cpp";
  }
  throw std::invalid_argument("unsupported mature-product artifact family");
}

inline std::string benchmark_matrix_entry(
    const ArtifactDispatchSpec& spec) {
  return std::string(spec.artifact_type) + "_refresh";
}

}  // namespace inferlab_mature_product
