#include "mature_product_artifact_runner.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using inferlab_mature_product::ArtifactSpec;
using inferlab_mature_product::boolean;
using inferlab_mature_product::num;
using inferlab_mature_product::str;

ArtifactSpec kv_transport_timeline() {
  return {
      "native_kv_transport_timeline_deep_dive",
      "native KV transport timeline explainer; does not implement LMCache, Mooncake, Dynamo, NIXL, or distributed KV storage",
      {"KV transfer", "prefill/decode disaggregation", "remote KV restore", "network/storage overlap"},
      {"LMCache", "Mooncake", "Dynamo", "vLLM KV connector"},
      {
          {"remote_kv_restore", "LMCache/Mooncake KV transfer", "pd_split_crossover",
           "allow_remote_restore_when_overlap_covers_transfer",
           "Transport timing is synthetic until product replay or execute evidence exists",
           {num("kv_bytes", 805306368.0), num("sender_queue_ms", 4.0), num("network_ms", 21.0),
            num("receiver_restore_ms", 11.0), num("decode_blocked_ms", 6.0), num("overlap_ratio", 0.78)}},
          {"remote_store_tail", "Dynamo/NIXL-style KV movement", "kv_tier_decision",
           "fallback_to_recompute_when_tail_latency_exceeds_prefill",
           "Remote-store behavior remains owned by the external product",
           {num("kv_bytes", 1342177280.0), num("sender_queue_ms", 13.0), num("network_ms", 54.0),
            num("receiver_restore_ms", 19.0), num("decode_blocked_ms", 42.0), num("overlap_ratio", 0.36)}},
      }};
}

ArtifactSpec radix_prefix_cache() {
  return {
      "native_radix_prefix_cache_deep_dive",
      "native radix/prefix-cache mechanism slice; does not reimplement SGLang RadixAttention or vLLM prefix cache",
      {"radix tree", "prefix sharing", "block mapping", "eviction cost"},
      {"SGLang", "vLLM", "LMCache"},
      {
          {"shared_system_prompt", "SGLang RadixAttention", "kv_paged_attention",
           "keep_radix_node_and_share_physical_blocks",
           "Radix hit evidence needs product replay before production cache claims",
           {num("prefix_tokens", 1536.0), num("shared_physical_blocks", 48.0), num("hit_rate", 0.86),
            num("eviction_cost_ms", 5.0), num("fragmentation_score", 0.18), str("cache_policy", "radix_lru")}},
          {"rag_document_overlap", "vLLM prefix cache", "kv_tier_decision",
           "split_node_and_reuse_strict_prefix_only",
           "Non-prefix/semantic reuse is outside this native correctness boundary",
           {num("prefix_tokens", 3072.0), num("shared_physical_blocks", 64.0), num("hit_rate", 0.61),
            num("eviction_cost_ms", 14.0), num("fragmentation_score", 0.41), str("cache_policy", "strict_prefix")}},
      }};
}

ArtifactSpec kv_compression_blend_risk() {
  return {
      "native_kv_compression_blend_risk",
      "native KV reuse/compression risk classifier; does not implement compressed KV storage or semantic cache serving",
      {"strict prefix reuse", "compressed KV", "non-prefix reuse", "semantic reuse risk"},
      {"LMCache", "SGLang HiCache", "vLLM prefix cache"},
      {
          {"strict_prefix_quantized_kv", "LMCache/vLLM KV reuse", "kv_tier_decision",
           "allow_when_prefix_identity_and_scale_contract_match",
           "Compression correctness requires target model/kernel validation",
           {num("reuse_risk", 0.18), num("compression_ratio", 0.50), num("expected_restore_ms", 18.0),
            num("prefill_recompute_ms", 51.0), num("correctness_risk", 0.12), str("policy", "allow_replay_only")}},
          {"semantic_non_prefix_reuse", "semantic cache research", "release_evidence_pipeline",
           "block_release_claim_until_semantic_correctness_evidence_exists",
           "Semantic reuse is not equivalent to KV prefix reuse",
           {num("reuse_risk", 0.84), num("compression_ratio", 0.38), num("expected_restore_ms", 12.0),
            num("prefill_recompute_ms", 47.0), num("correctness_risk", 0.81), str("policy", "deny_for_runtime_claim")}},
      }};
}

ArtifactSpec chunked_prefill_stall_free() {
  return {
      "native_chunked_prefill_stall_free_deep_dive",
      "native chunked-prefill scheduler explainer; does not reimplement vLLM, SGLang, or Sarathi-Serve production schedulers",
      {"chunked prefill", "decode-maximal batching", "stall-free scheduling", "TTFT/TPOT trade-off"},
      {"vLLM", "SGLang", "Sarathi-Serve"},
      {
          {"balanced_chunk", "vLLM/SGLang chunked prefill", "scheduler_root_cause",
           "prefer_chunk_when_decode_stall_drops_without_p99_spike",
           "Synthetic schedule scores are not production SLA evidence",
           {num("prefill_chunk_size", 1024.0), num("decode_stall_ms", 9.0), num("ttft_delta_ms", -34.0),
            num("tpot_delta_ms", 1.7), num("stall_free_score", 0.82), num("p99_risk", 0.26)}},
          {"oversized_chunk", "Sarathi-style stall analysis", "scheduler_batching",
           "reduce_chunk_or_raise_decode_priority",
           "Chunk tuning needs target workload replay before release claims",
           {num("prefill_chunk_size", 4096.0), num("decode_stall_ms", 52.0), num("ttft_delta_ms", -61.0),
            num("tpot_delta_ms", 9.4), num("stall_free_score", 0.39), num("p99_risk", 0.68)}},
      }};
}

ArtifactSpec tenant_quota_admission() {
  return {
      "native_tenant_quota_admission_deep_dive",
      "native tenant quota/admission/fairness explainer; does not implement a production gateway or scheduler",
      {"tenant quota", "admission control", "fairness", "starvation guard", "SLO risk"},
      {"KServe/llm-d", "Dynamo", "Ray Serve", "vLLM"},
      {
          {"tenant_burst_guard", "llm-d/Dynamo routing", "scheduler_root_cause",
           "admit_with_quota_and_starvation_guard",
           "Quota decisions need product gateway evidence before enforcement claims",
           {num("tenant_quota_tokens", 8192.0), num("queue_depth", 19.0), num("fairness_score", 0.74),
            num("admission_score", 0.68), num("slo_violation_risk", 0.31), str("admission_decision", "admit_limited")}},
          {"noisy_neighbor", "Ray Serve/KServe gateway", "release_evidence_pipeline",
           "reject_or_backpressure_when_quota_and_slo_both_fail",
           "Native model explains policy only; cloud control plane remains external",
           {num("tenant_quota_tokens", 4096.0), num("queue_depth", 57.0), num("fairness_score", 0.42),
            num("admission_score", 0.29), num("slo_violation_risk", 0.77), str("admission_decision", "backpressure")}},
      }};
}

ArtifactSpec live_request_migration() {
  return {
      "native_live_request_migration_deep_dive",
      "native live request migration cost/rollback explainer; does not implement Llumnix, KV migration, or instance rescheduling",
      {"live migration", "KV migration", "overload rescue", "drain", "rollback"},
      {"Llumnix", "Mooncake", "Dynamo", "llm-d"},
      {
          {"overload_rescue", "Llumnix-style request migration", "kv_transport_timeline",
           "migrate_when_slo_rescue_exceeds_kv_move_cost",
           "Migration execution requires product/runtime evidence",
           {num("kv_bytes_to_move", 536870912.0), num("migration_ms", 43.0), num("rollback_ms", 18.0),
            num("slo_rescue_score", 0.73), num("decode_pause_ms", 12.0), str("decision", "migrate")}},
          {"drain_tail_request", "Dynamo/llm-d drain", "deployment_readiness",
           "avoid_migration_when_tail_request_can_finish_during_drain",
           "Drain semantics are target control-plane specific",
           {num("kv_bytes_to_move", 1073741824.0), num("migration_ms", 91.0), num("rollback_ms", 32.0),
            num("slo_rescue_score", 0.38), num("decode_pause_ms", 45.0), str("decision", "finish_in_place")}},
      }};
}

ArtifactSpec quant_correctness_contract() {
  return {
      "native_quant_correctness_contract",
      "native quantization correctness contract; does not implement TensorRT-LLM, FlashInfer, Marlin, CUTLASS, or product quant kernels",
      {"INT4", "INT8", "FP8", "FP4", "NVFP4", "scale/range/tolerance"},
      {"TensorRT-LLM", "FlashInfer", "vLLM", "llama.cpp"},
      {
          {"int4_weight_only_decode", "vLLM/FlashInfer quant backend", "cuda_quant_microkernel",
           "allow_after_reference_tolerance_and_fallback_are_recorded",
           "Native contract metadata is not CUDA timing evidence",
           {num("max_abs_error_tolerance", 0.035), num("scale_granularity", 128.0), num("range_clip_rate", 0.004),
            num("fallback_risk", 0.19), str("dtype", "int4_weight_only"), str("reference", "cpu_dequant_matvec")}},
          {"fp8_kv_cache", "TensorRT-LLM/vLLM KV quant", "kernel_compatibility_matrix",
           "require_scale_capture_and_attention_reference_before_claim",
           "FP8/FP4 behavior must be validated on target GPU",
           {num("max_abs_error_tolerance", 0.060), num("scale_granularity", 64.0), num("range_clip_rate", 0.012),
            num("fallback_risk", 0.34), str("dtype", "fp8_kv"), str("reference", "fp32_attention")}},
      }};
}

ArtifactSpec paged_kv_gather_kernel_contract() {
  return {
      "native_paged_kv_gather_kernel_contract",
      "native Paged KV gather kernel contract; does not replace FlashInfer or vLLM PagedAttention kernels",
      {"block table lookup", "paged KV gather", "coalescing", "decode attention bytes", "fallback"},
      {"FlashInfer", "vLLM", "SGLang"},
      {
          {"decode_block_table_gather", "FlashInfer paged KV", "kv_paged_attention",
           "use_fused_gather_when_layout_and_dtype_match",
           "Coalescing estimates need CUDA profiling before performance claims",
           {num("logical_tokens", 4096.0), num("physical_blocks", 128.0), num("estimated_bytes", 67108864.0),
            num("coalescing_risk", 0.28), num("fallback_risk", 0.16), str("layout", "paged_bshd")}},
          {"fragmented_long_context", "vLLM PagedAttention", "cuda_quant_microkernel",
           "fallback_to_reference_gather_when_block_order_is_fragmented",
           "Native contract explains eligibility only",
           {num("logical_tokens", 16384.0), num("physical_blocks", 612.0), num("estimated_bytes", 268435456.0),
            num("coalescing_risk", 0.67), num("fallback_risk", 0.49), str("layout", "fragmented_blocks")}},
      }};
}

ArtifactSpec flashinfer_replay_alignment() {
  return {
      "native_flashinfer_replay_alignment",
      "native FlashInfer replay alignment bridge; does not execute or copy FlashInfer kernels",
      {"kernel availability", "shape coverage", "fallback reason", "paged/ragged KV"},
      {"FlashInfer"},
      {
          {"decode_attention_replay", "product_flashinfer_kernel_profile", "kernel_compatibility_matrix",
           "align_shape_dtype_layout_before_kernel_claim",
           "Profile/replay alignment is not kernel execution by InferLab",
           {num("shape_coverage", 0.82), num("dtype_coverage", 0.74), num("fallback_rate", 0.11),
            num("required_metadata_present", 0.93), str("operator", "decode_attention"), str("mode", "replay")}},
          {"paged_append_attention", "FlashInfer paged/ragged APIs", "paged_kv_gather_kernel_contract",
           "record_missing_layout_metadata_as_release_blocker",
           "Missing product profile evidence blocks benchmark conclusions",
           {num("shape_coverage", 0.65), num("dtype_coverage", 0.58), num("fallback_rate", 0.29),
            num("required_metadata_present", 0.61), str("operator", "append_attention"), str("mode", "dry_run")}},
      }};
}

ArtifactSpec trtllm_engine_quant_alignment() {
  return {
      "native_trtllm_engine_quant_alignment",
      "native TensorRT-LLM engine/quant metadata alignment; does not build engines or run Triton",
      {"engine metadata", "in-flight batching", "paged KV", "FP8/FP4/NVFP4", "Triton layout"},
      {"TensorRT-LLM", "Triton"},
      {
          {"fp8_engine_metadata", "TensorRT-LLM engine profile", "quant_correctness_contract",
           "allow_release_only_when_engine_precision_and_scale_are_captured",
           "Engine build/runtime remains external product evidence",
           {num("engine_metadata_completeness", 0.78), num("precision_contract_present", 0.85),
            num("paged_kv_contract_present", 0.72), num("release_blocker_count", 2.0),
            str("precision", "fp8"), str("deployment", "triton")}},
          {"nvfp4_missing_scale", "TensorRT-LLM low precision", "release_evidence_pipeline",
           "block_claim_until_scale_and_tolerance_artifacts_exist",
           "NVFP4 correctness and timing require target GPU validation",
           {num("engine_metadata_completeness", 0.52), num("precision_contract_present", 0.39),
            num("paged_kv_contract_present", 0.68), num("release_blocker_count", 4.0),
            str("precision", "nvfp4"), str("deployment", "triton")}},
      }};
}

ArtifactSpec gateway_policy_matrix() {
  return {
      "native_gateway_policy_matrix",
      "native gateway routing policy matrix; does not implement Dynamo, KServe/llm-d, Ray Serve, or AIBrix control planes",
      {"prefix-aware routing", "LoRA-aware routing", "tenant-aware routing", "queue-aware routing"},
      {"Dynamo", "KServe/llm-d", "Ray Serve", "AIBrix"},
      {
          {"prefix_lora_route", "llm-d/Dynamo gateway", "product_replay_alignment",
           "route_to_replica_with_prefix_and_adapter_affinity",
           "Routing decisions need product control-plane evidence before execute claims",
           {num("prefix_affinity_score", 0.79), num("adapter_affinity_score", 0.73), num("queue_penalty", 0.22),
            num("tenant_quota_risk", 0.18), str("selected_policy", "prefix_lora_queue_aware")}},
          {"tenant_queue_route", "Ray Serve/KServe gateway", "tenant_quota_admission",
           "prefer_slo_route_when_queue_depth_exceeds_affinity_gain",
           "Native matrix is a policy explainer only",
           {num("prefix_affinity_score", 0.44), num("adapter_affinity_score", 0.28), num("queue_penalty", 0.69),
            num("tenant_quota_risk", 0.54), str("selected_policy", "slo_queue_aware")}},
      }};
}

ArtifactSpec deployment_readiness_matrix() {
  return {
      "native_deployment_readiness_matrix",
      "native deployment readiness matrix; does not run kubectl, helm, docker, endpoints, or Kubernetes controllers",
      {"KServe", "llm-d", "Dynamo", "Ray Serve", "TGI", "vLLM", "deployment readiness"},
      {"KServe/llm-d", "Dynamo", "Ray Serve", "TGI", "vLLM"},
      {
          {"kserve_llmd_pd_split", "KServe/llm-d deployment", "release_evidence_pipeline",
           "ready_for_target_cluster_replay_after_crd_and_metrics_capture",
           "Current machine does not validate Kubernetes deployment",
           {num("manifest_completeness", 0.83), num("metrics_evidence_present", 0.46),
            num("rollback_readiness", 0.62), num("release_blocker_count", 3.0),
            str("deployment_mode", "kubernetes_pd_split")}},
          {"ray_tgi_single_cluster", "Ray Serve/TGI deployment", "autoscaler_cost_slo",
           "require_endpoint_health_and_prometheus_replay_before_release",
           "Endpoint availability remains target-environment evidence",
           {num("manifest_completeness", 0.71), num("metrics_evidence_present", 0.38),
            num("rollback_readiness", 0.55), num("release_blocker_count", 4.0),
            str("deployment_mode", "single_cluster_serving")}},
      }};
}

ArtifactSpec openai_api_conformance_matrix() {
  return {
      "native_openai_api_conformance_matrix",
      "native OpenAI-compatible API conformance matrix; does not execute product endpoints or model responses",
      {"chat completions", "responses API", "streaming", "tool calls", "error code", "structured output"},
      {"vLLM", "SGLang", "TGI", "Ray Serve", "LMDeploy"},
      {
          {"streaming_tool_call", "OpenAI-compatible product servers", "structured_output_parser_contract",
           "require_sse_parser_and_tool_schema_before_endpoint_claim",
           "API fixture conformance is not model behavior evidence",
           {num("surface_coverage", 0.78), num("streaming_case_coverage", 0.81),
            num("tool_call_case_coverage", 0.69), num("error_mapping_coverage", 0.74),
            str("api_surface", "chat_completions_stream_tool")}},
          {"responses_json_schema", "vLLM/SGLang/TGI API", "grammar_backend_matrix",
           "block_release_until_product_endpoint_replay_exists",
           "Responses API compatibility must be confirmed against target product versions",
           {num("surface_coverage", 0.57), num("streaming_case_coverage", 0.64),
            num("tool_call_case_coverage", 0.48), num("error_mapping_coverage", 0.52),
            str("api_surface", "responses_json_schema")}},
      }};
}

ArtifactSpec spec_for(const std::string& requested) {
  const std::string_view artifact_type =
      inferlab_mature_product::resolve_selector(
          requested, inferlab_mature_product::ArtifactFamily::borrowed_gap)
          .artifact_type;
  if (artifact_type == "native_radix_prefix_cache_deep_dive") {
    return radix_prefix_cache();
  }
  if (artifact_type == "native_kv_compression_blend_risk") {
    return kv_compression_blend_risk();
  }
  if (artifact_type == "native_chunked_prefill_stall_free_deep_dive") {
    return chunked_prefill_stall_free();
  }
  if (artifact_type == "native_tenant_quota_admission_deep_dive") {
    return tenant_quota_admission();
  }
  if (artifact_type == "native_live_request_migration_deep_dive") {
    return live_request_migration();
  }
  if (artifact_type == "native_quant_correctness_contract") {
    return quant_correctness_contract();
  }
  if (artifact_type == "native_paged_kv_gather_kernel_contract") {
    return paged_kv_gather_kernel_contract();
  }
  if (artifact_type == "native_flashinfer_replay_alignment") {
    return flashinfer_replay_alignment();
  }
  if (artifact_type == "native_trtllm_engine_quant_alignment") {
    return trtllm_engine_quant_alignment();
  }
  if (artifact_type == "native_gateway_policy_matrix") {
    return gateway_policy_matrix();
  }
  if (artifact_type == "native_deployment_readiness_matrix") {
    return deployment_readiness_matrix();
  }
  if (artifact_type == "native_openai_api_conformance_matrix") {
    return openai_api_conformance_matrix();
  }
  if (artifact_type == "native_kv_transport_timeline_deep_dive") {
    return kv_transport_timeline();
  }
  throw std::logic_error("resolved borrowed-gap artifact has no builder");
}

}  // namespace

int main(int argc, char** argv) {
  return inferlab_mature_product::run_artifact_dispatcher(
      argc, argv, inferlab_mature_product::ArtifactFamily::borrowed_gap,
      spec_for);
}
