#include "product_replay_alignment_fixture.hpp"

#include "advanced_serving_internal.hpp"
#include "artifact_contract_model.hpp"
#include "deep_dive_core_artifact_registry.hpp"
#include "deep_dive_artifact_identity.hpp"
#include "high_value_artifact_catalog.hpp"
#include "mature_product_artifact_dispatch.hpp"
#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::product_replay_alignment {
namespace {

constexpr const char* kResultsPrefix = "results/raw/";
NativeArtifactSelector core(std::string role,
                            std::string foreign_key,
                            std::string family = "native_deep_dive") {
  return {std::move(role), NativeIdentitySource::core_deep_dive_registry,
          std::move(foreign_key), std::move(family)};
}

NativeArtifactSelector wiring(std::string role,
                              std::string foreign_key,
                              std::string family) {
  return {std::move(role), NativeIdentitySource::deep_dive_wiring_registry,
          std::move(foreign_key), std::move(family)};
}

NativeArtifactSelector mature_product_dispatch(std::string role,
                                               std::string foreign_key,
                                               std::string family) {
  return {std::move(role),
          NativeIdentitySource::mature_product_dispatch_contract,
          std::move(foreign_key), std::move(family)};
}

std::string results_filename(const std::string& path) {
  if (path.rfind(kResultsPrefix, 0U) != 0U ||
      path.size() <= std::char_traits<char>::length(kResultsPrefix)) {
    throw std::logic_error(
        "product replay alignment artifact is not results/raw scoped: " +
        path);
  }
  const std::string filename =
      path.substr(std::char_traits<char>::length(kResultsPrefix));
  if (filename.find('/') != std::string::npos ||
      filename.find('\\') != std::string::npos ||
      filename.find("..") != std::string::npos) {
    throw std::logic_error(
        "product replay alignment artifact filename is unsafe: " + path);
  }
  return filename;
}

std::string artifact_type_from_filename(const std::string& artifact) {
  if (artifact.size() <= 5U ||
      artifact.compare(artifact.size() - 5U, 5U, ".json") != 0 ||
      artifact.find('/') != std::string::npos ||
      artifact.find('\\') != std::string::npos ||
      artifact.find("..") != std::string::npos) {
    throw std::logic_error(
        "product replay alignment artifact filename is invalid: " +
        artifact);
  }
  return artifact.substr(0U, artifact.size() - 5U);
}

bool contains_string(const std::vector<std::string>& values,
                     const std::string& expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

void validate_rules(const std::vector<AlignmentRuleSpec>& rules) {
  constexpr std::size_t kExpectedRuleCount = 7U;
  if (rules.size() != kExpectedRuleCount) {
    throw std::logic_error("product replay alignment rule count drift");
  }

  std::vector<std::string> covered_profiles;
  std::vector<NativeArtifactIdentity> resolved_identities;
  for (std::size_t i = 0U; i < rules.size(); ++i) {
    const auto& rule = rules[i];
    if (rule.name.empty() || rule.alignment_type.empty() ||
        rule.product_profile_ids.empty() || rule.native_artifacts.empty() ||
        rule.replay_signals.empty() || rule.native_explainers.empty() ||
        rule.claim_boundary.empty() ||
        rule.next_action_when_missing.empty()) {
      throw std::logic_error(
          "product replay alignment rule is incomplete: " + rule.name);
    }
    advanced_serving_internal::require_unique_nonempty_strings(
        rule.product_profile_ids, "product replay alignment profile ids");
    advanced_serving_internal::require_unique_nonempty_strings(
        rule.replay_signals, "product replay alignment replay signals");
    advanced_serving_internal::require_unique_nonempty_strings(
        rule.native_explainers,
        "product replay alignment native explainers");

    for (std::size_t prior = 0U; prior < i; ++prior) {
      if (rules[prior].name == rule.name ||
          rules[prior].alignment_type == rule.alignment_type) {
        throw std::logic_error(
            "product replay alignment rule identity is duplicated: " +
            rule.name);
      }
    }

    for (const auto& profile_id : rule.product_profile_ids) {
      (void)mature_product_profiles::require_product_profile(profile_id);
      if (!contains_string(covered_profiles, profile_id)) {
        covered_profiles.push_back(profile_id);
      }
    }

    std::vector<std::string> roles;
    std::vector<std::string> artifacts;
    for (const auto& selector : rule.native_artifacts) {
      if (selector.role.empty() || selector.foreign_key.empty() ||
          selector.family.empty()) {
        throw std::logic_error(
            "product replay alignment native selector is incomplete");
      }
      if (contains_string(roles, selector.role)) {
        throw std::logic_error(
            "product replay alignment native role is duplicated: " +
            selector.role);
      }
      roles.push_back(selector.role);

      const auto identity = resolve_native_artifact(selector);
      if (contains_string(artifacts, identity.artifact)) {
        throw std::logic_error(
            "product replay alignment rule repeats a physical artifact: " +
            identity.artifact);
      }
      artifacts.push_back(identity.artifact);

      const auto prior = std::find_if(
          resolved_identities.begin(), resolved_identities.end(),
          [&](const auto& candidate) {
            return candidate.artifact == identity.artifact;
          });
      if (prior == resolved_identities.end()) {
        resolved_identities.push_back(identity);
      } else if (prior->canonical_id != identity.canonical_id ||
                 prior->role != identity.role ||
                 prior->source != identity.source ||
                 prior->family != identity.family ||
                 prior->expected_artifact_type !=
                     identity.expected_artifact_type ||
                 prior->identity_owner != identity.identity_owner ||
                 prior->producer_target != identity.producer_target ||
                 prior->refresh_command != identity.refresh_command ||
                 prior->claim_boundary != identity.claim_boundary ||
                 prior->required_fields != identity.required_fields ||
                 prior->required_metrics != identity.required_metrics) {
        throw std::logic_error(
            "product replay alignment physical artifact identity drifts "
            "between rules: " +
            identity.artifact);
      }
    }
  }

  const std::size_t default_profile_count =
      mature_product_profiles::product_profile_count_for_scope(
          mature_product_profiles::
              ProductProfileScope::default_required);
  if (covered_profiles.size() != default_profile_count) {
    throw std::logic_error(
        "product replay alignment does not cover the default mature-product "
        "scope");
  }
  for (const auto& profile : mature_product_profiles::product_profiles()) {
    if (!mature_product_profiles::profile_is_default_required(profile)) {
      if (contains_string(covered_profiles, profile.id)) {
        throw std::logic_error(
            "product replay alignment includes an optional profile in the "
            "default scope: " + profile.id);
      }
      continue;
    }
    if (!contains_string(covered_profiles, profile.id)) {
      throw std::logic_error(
          "product replay alignment profile coverage is missing: " +
          profile.id);
    }
  }
}

}  // namespace

const char* native_identity_source_name(const NativeIdentitySource source) {
  switch (source) {
    case NativeIdentitySource::core_deep_dive_registry:
      return "core_deep_dive_registry";
    case NativeIdentitySource::deep_dive_wiring_registry:
      return "deep_dive_wiring_registry";
    case NativeIdentitySource::high_value_catalog:
      return "high_value_artifact_catalog";
    case NativeIdentitySource::mature_product_dispatch_contract:
      return "mature_product_dispatch_contract";
  }
  return "unknown";
}

NativeArtifactIdentity resolve_native_artifact(
    const NativeArtifactSelector& selector) {
  NativeArtifactIdentity identity;
  identity.role = selector.role;
  identity.source = selector.source;
  identity.family = selector.family;
  switch (selector.source) {
    case NativeIdentitySource::core_deep_dive_registry: {
      const auto& source =
          inferlab_deep_dive_core::require_artifact_spec(
              selector.foreign_key);
      identity.canonical_id = source.id;
      identity.artifact = source.artifact;
      identity.expected_artifact_type =
          artifact_type_from_filename(source.artifact);
      identity.identity_owner = "deep_dive_core_artifact_registry";
      identity.producer_target = source.target;
      identity.refresh_command = source.command;
      identity.claim_boundary = source.claim_boundary;
      identity.required_fields = source.required_fields;
      identity.required_metrics = source.required_metrics;
      break;
    }
    case NativeIdentitySource::deep_dive_wiring_registry: {
      const auto source = inferlab_deep_dive::require_artifact_identity(
          selector.foreign_key);
      identity.canonical_id = source.contract.name;
      identity.artifact = results_filename(source.contract.artifact);
      identity.expected_artifact_type =
          artifact_type_from_filename(identity.artifact);
      if (identity.expected_artifact_type != source.contract.name) {
        throw std::logic_error(
            "product replay alignment General Contract artifact type "
            "drift: " +
            source.contract.name);
      }
      identity.identity_owner =
          "deep_dive_artifact_wiring_registry";
      identity.producer_target = source.wiring.target;
      identity.refresh_command = source.wiring.command;
      identity.claim_boundary = source.contract.claim_boundary;
      identity.required_fields = source.contract.required_fields;
      identity.required_metrics = source.contract.required_metrics;
      break;
    }
    case NativeIdentitySource::high_value_catalog: {
      const auto& source =
          inferlab_high_value::require_managed_artifact(
              selector.foreign_key);
      identity.canonical_id = source.id;
      identity.artifact = source.artifact;
      identity.expected_artifact_type =
          artifact_type_from_filename(source.artifact);
      identity.identity_owner = "high_value_artifact_catalog";
      identity.producer_target = source.target;
      identity.refresh_command = source.command;
      identity.claim_boundary = source.claim_boundary;
      identity.required_fields = source.required_fields;
      identity.required_metrics = source.required_metrics;
      break;
    }
    case NativeIdentitySource::mature_product_dispatch_contract: {
      const auto* dispatch =
          inferlab_mature_product::find_exact(selector.foreign_key);
      if (dispatch == nullptr) {
        throw std::logic_error(
            "product replay alignment mature-product dispatch foreign key "
            "is unknown: " +
            selector.foreign_key);
      }
      const auto& contract =
          inferlab_artifact_contract::require_contract_extension(
              selector.foreign_key);
      identity.canonical_id = std::string(dispatch->artifact_type);
      identity.artifact =
          inferlab_mature_product::artifact_filename(*dispatch);
      identity.expected_artifact_type = identity.canonical_id;
      if (contract.name != identity.canonical_id ||
          results_filename(contract.artifact) != identity.artifact ||
          contract.source != inferlab_artifact_contract::ContractSource::
                                 MatureProductDispatch) {
        throw std::logic_error(
            "product replay alignment mature-product dispatch and General "
            "Contract identity drift: " +
            selector.foreign_key);
      }
      identity.identity_owner =
          "mature_product_artifact_dispatch_plus_artifact_contract";
      identity.producer_target =
          std::string(inferlab_mature_product::canonical_target(
              dispatch->family));
      identity.refresh_command =
          inferlab_mature_product::refresh_command(dispatch->artifact_type);
      identity.claim_boundary = contract.claim_boundary;
      identity.required_fields = contract.required_fields;
      identity.required_metrics = contract.required_metrics;
      break;
    }
  }
  if (identity.canonical_id.empty() || identity.artifact.empty() ||
      identity.expected_artifact_type.empty() ||
      identity.identity_owner.empty() || identity.producer_target.empty() ||
      identity.refresh_command.empty() ||
      identity.claim_boundary.empty() || identity.required_fields.empty() ||
      identity.required_metrics.empty()) {
    throw std::logic_error(
        "product replay alignment resolved native identity is incomplete: " +
        selector.foreign_key);
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      identity.required_fields,
      "product replay alignment native required fields");
  advanced_serving_internal::require_unique_nonempty_strings(
      identity.required_metrics,
      "product replay alignment native required metrics");
  return identity;
}

const std::vector<AlignmentRuleSpec>& alignment_rules() {
  static const std::vector<AlignmentRuleSpec> rules = {
      {"vllm_sglang_kv_scheduler_replay",
       "product_replay_to_kv_and_scheduler_deep_dive",
       {"vllm", "sglang"},
       {core("kv_paged_attention", "kv_paged_attention_deep_dive"),
        wiring("kv_lifecycle", "native_kv_lifecycle_deep_dive",
               "native_deep_dive"),
        core("scheduler_batching", "scheduler_batching_deep_dive"),
        wiring("kv_tier_decision", "native_kv_tier_decision_deep_dive",
               "native_deep_dive"),
        wiring("scheduler_root_cause",
               "native_scheduler_root_cause_deep_dive",
               "native_deep_dive")},
       {"paged_attention_events", "prefix_cache_hits",
        "batch_merge_split", "ttft_tpot_p99"},
       {"logical_to_physical_samples", "logical_to_physical_blocks",
        "read_amplification", "prefix_sharing_rate", "lifecycle_score",
        "prefill_chunk_timeline", "decode_stall_ms",
        "slo_violation_risk", "selected_tier", "root_cause"},
       "vLLM/SGLang own production scheduling and PagedAttention; InferLab only aligns replay evidence to native mechanism slices",
       "collect vLLM/SGLang replay evidence and refresh the native KV and scheduler deep-dive artifacts"},
      {"tensorrt_flashinfer_kernel_replay",
       "product_replay_to_cuda_quant_microkernel_deep_dive",
       {"tensorrtllm"},
       {core("cuda_quant_microkernel", "cuda_quant_microkernel_deep_dive"),
        wiring("kernel_compatibility",
               "native_kernel_compatibility_matrix",
               "native_kernel_contract")},
       {"kernel_elapsed_ms", "operator_name", "dtype", "shape",
        "fallback_reason"},
       {"arithmetic_intensity_flops_per_byte", "bottleneck_model",
        "correctness_contract", "fallback_contract",
        "profiling_metadata_required", "selected_backend", "release_gate"},
       "TensorRT-LLM owns its optimized runtime and kernels; InferLab only aligns profile metadata and correctness contracts",
       "collect TensorRT-LLM kernel replay/profile evidence and refresh the CUDA/Quant microkernel deep-dive artifact"},
      {"product_replay_semantic_events_bridge",
       "product_replay_to_semantic_events",
       {"vllm", "sglang", "tensorrtllm"},
       {wiring("semantic_events", "product_replay_semantic_events",
               "product_replay_semantics")},
       {"request_arrival", "prefill_start", "decode_step", "kv_hit",
        "route_decision", "fallback"},
       {"event_type", "native_alignment", "observed_signal",
        "sample_replay_present", "semantic_event_decision"},
       "Semantic events explain replay evidence only; the canonical release pipeline consumes their linkage without converting replay into execute claims",
       "refresh product replay semantic events after importing product evidence, then refresh the release pipeline"},
      {"streaming_parser_control_plane_p2_bridge",
       "product_replay_to_p2_user_visible_and_control_plane_deep_dives",
       {"vllm", "sglang", "tensorrtllm"},
       {mature_product_dispatch(
            "streaming_backpressure",
            "native_streaming_backpressure_deep_dive", "native_deep_dive"),
        mature_product_dispatch(
            "structured_parser", "native_structured_output_parser_contract",
            "native_parser_contract"),
        mature_product_dispatch(
            "autoscaler_cost_slo", "native_autoscaler_cost_slo_deep_dive",
            "native_deep_dive"),
        mature_product_dispatch(
            "lora_adapter_cache", "native_lora_adapter_cache_deep_dive",
            "native_deep_dive"),
        mature_product_dispatch(
            "grammar_backend", "native_grammar_backend_matrix",
            "native_parser_contract"),
        mature_product_dispatch(
            "edge_compiler", "native_edge_compiler_artifact_matrix",
            "native_edge_contract")},
       {"streaming_latency", "tool_call", "autoscale_event",
        "lora_adapter", "grammar_backend", "edge_runtime"},
       {"first_event_latency_ms", "parser_policy", "slo_violation_risk",
        "cache_hit_rate", "schema_coverage", "artifact_completeness"},
       "P2 bridges explain user-visible serving, parser, control-plane, LoRA, grammar, and edge evidence only; mature product runtimes remain external",
       "collect product replay evidence and refresh P2 native artifacts before release claims"},
      {"kv_cache_transport_and_radix_bridge",
       "product_replay_to_kv_transport_radix_and_reuse_risk_deep_dives",
       {"sglang", "vllm"},
       {mature_product_dispatch(
            "kv_transport_timeline",
            "native_kv_transport_timeline_deep_dive", "native_deep_dive"),
        wiring("kv_lifecycle", "native_kv_lifecycle_deep_dive",
               "native_deep_dive"),
        mature_product_dispatch(
            "radix_prefix_cache", "native_radix_prefix_cache_deep_dive",
            "native_deep_dive"),
        mature_product_dispatch(
            "kv_compression_blend_risk",
            "native_kv_compression_blend_risk", "native_deep_dive"),
        wiring("kv_tier_decision", "native_kv_tier_decision_deep_dive",
               "native_deep_dive")},
       {"kv_transfer_bytes", "remote_kv_hit", "prefix_cache_hit",
        "cache_eviction", "reuse_policy"},
       {"kv_bytes", "overlap_ratio", "hit_rate", "reuse_risk",
        "correctness_risk", "selected_tier", "lifecycle_score",
        "logical_to_physical_blocks", "tier_decisions"},
       "KV transport, radix cache, and compression/reuse risk are native explanation slices only; product KV stores and cache engines remain external",
       "collect KV/cache replay evidence and refresh native KV transport, radix, and risk artifacts"},
      {"kernel_quant_product_alignment_bridge",
       "product_replay_to_quant_paged_kv_flashinfer_and_trtllm_contracts",
       {"tensorrtllm", "vllm"},
       {mature_product_dispatch(
            "quant_correctness", "native_quant_correctness_contract",
            "native_kernel_contract"),
        mature_product_dispatch(
            "paged_kv_gather", "native_paged_kv_gather_kernel_contract",
            "native_kernel_contract"),
        mature_product_dispatch(
            "flashinfer_alignment", "native_flashinfer_replay_alignment",
            "native_kernel_contract"),
        mature_product_dispatch(
            "trtllm_engine_quant",
            "native_trtllm_engine_quant_alignment",
            "native_kernel_contract")},
       {"operator_name", "dtype", "shape", "fallback_reason",
        "engine_precision", "kernel_backend"},
       {"max_abs_error_tolerance", "coalescing_risk", "shape_coverage",
        "engine_metadata_completeness", "release_blocker_count"},
       "Kernel and quant bridges align product metadata to native contracts only; product kernel execution remains external",
       "collect TensorRT-LLM/vLLM replay or execute evidence and refresh the native FlashInfer compatibility and kernel contracts; the optional FlashInfer product profile is not a default release input"},
      {"control_plane_api_readiness_bridge",
       "product_replay_to_gateway_deployment_and_openai_api_matrices",
       {"vllm", "sglang", "tensorrtllm"},
       {mature_product_dispatch(
            "gateway_policy", "native_gateway_policy_matrix",
            "native_evidence_contract"),
        mature_product_dispatch(
            "deployment_readiness", "native_deployment_readiness_matrix",
            "native_evidence_contract"),
        mature_product_dispatch(
            "openai_api_conformance",
            "native_openai_api_conformance_matrix",
            "native_parser_contract"),
        mature_product_dispatch(
            "tenant_quota_admission",
            "native_tenant_quota_admission_deep_dive",
            "native_deep_dive")},
       {"routing_decision", "autoscale_event", "endpoint_health",
        "api_error", "tool_call", "tenant_quota"},
       {"selected_policy", "manifest_completeness", "surface_coverage",
        "admission_decision", "slo_violation_risk"},
       "Control-plane and API matrices explain readiness only; Kubernetes/Ray/product endpoints remain external",
       "collect control-plane and endpoint replay evidence before release claims"},
  };
  static const bool validated = [] {
    validate_rules(rules);
    return true;
  }();
  (void)validated;
  return rules;
}

}  // namespace inferlab::product_replay_alignment
