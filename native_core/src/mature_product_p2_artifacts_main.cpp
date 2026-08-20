#include "mature_product_artifact_runner.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using inferlab_mature_product::ArtifactSpec;
using inferlab_mature_product::boolean;
using inferlab_mature_product::num;
using inferlab_mature_product::str;

ArtifactSpec streaming_backpressure() {
  return {
      "native_streaming_backpressure_deep_dive",
      "native streaming/backpressure mechanism slice; does not execute TGI, vLLM, SGLang, Ray Serve, or endpoints",
      {"SSE streaming", "client cancel", "slow receiver", "backpressure", "slot cleanup"},
      {"TGI", "vLLM", "SGLang", "Ray Serve"},
      {
          {"slow_client_cancel", "TGI/Ray Serve SSE", "scheduler_root_cause",
           "enable_cancel_cleanup_and_backpressure_gate",
           "Explains streaming pressure only; product stream runtime remains external",
           {num("first_event_latency_ms", 145.0), num("inter_token_gap_ms", 42.0),
            num("client_receive_lag_ms", 210.0), num("cancel_cleanup_ms", 18.0),
            num("slot_release_risk", 0.64), str("streaming_policy", "cancel_reclaims_decode_slot")}},
          {"high_flush_frequency", "vLLM/SGLang OpenAI server", "scheduler_batching",
           "batch_flush_interval_before_claiming_user_visible_latency",
           "Flush timing requires endpoint evidence before user-experience claims",
           {num("first_event_latency_ms", 118.0), num("inter_token_gap_ms", 27.0),
            num("flush_events_per_second", 9200.0), num("server_backpressure_score", 0.58),
            num("slot_release_risk", 0.22), str("streaming_policy", "bounded_flush_rate")}},
          {"network_jitter_tail", "OpenAI-compatible product endpoint", "release_evidence_pipeline",
           "keep_replay_only_until_network_trace_exists",
           "Network jitter is not product kernel evidence",
           {num("first_event_latency_ms", 230.0), num("inter_token_gap_ms", 68.0),
            num("client_receive_lag_ms", 390.0), num("timeout_risk_score", 0.71),
            num("slot_release_risk", 0.36), str("streaming_policy", "slow_receiver_backpressure")}},
      }};
}

ArtifactSpec structured_output_parser_contract() {
  return {
      "native_structured_output_parser_contract",
      "native parser contract and fixture taxonomy; does not execute models or grammar backends",
      {"tool calling", "reasoning parser", "JSON schema", "stream fragment recovery", "grammar constraints"},
      {"vLLM", "SGLang", "XGrammar", "llguidance", "Outlines"},
      {
          {"tool_json_strict", "vLLM/SGLang structured output", "product_replay_semantic_events",
           "allow_with_schema_and_required_argument_gate",
           "Parser fixture success is not model correctness",
           {num("json_parse_success_rate", 0.98), num("schema_valid_rate", 0.95),
            num("required_args_missing_rate", 0.03), num("stream_fragment_recovery_rate", 0.91),
            str("parser_policy", "strict_schema_then_repair")}},
          {"reasoning_tool_mixed", "reasoning/tool parser products", "release_evidence_pipeline",
           "require_span_taxonomy_before_release_claim",
           "Reasoning span classification needs product replay or endpoint evidence",
           {num("json_parse_success_rate", 0.88), num("schema_valid_rate", 0.81),
            num("required_args_missing_rate", 0.11), num("reasoning_span_conflict_rate", 0.17),
            str("parser_policy", "separate_reasoning_tool_final_spans")}},
          {"streaming_partial_json", "TGI/vLLM streaming", "streaming_backpressure",
           "buffer_partial_json_until_schema_boundary",
           "Streaming parser behavior is replay-only unless endpoint traces exist",
           {num("json_parse_success_rate", 0.76), num("schema_valid_rate", 0.70),
            num("stream_fragment_recovery_rate", 0.84), num("retry_required_rate", 0.19),
            str("parser_policy", "incremental_json_state_machine")}},
      }};
}

ArtifactSpec autoscaler_cost_slo() {
  return {
      "native_autoscaler_cost_slo_deep_dive",
      "native autoscaler cost/SLO model; does not execute Ray, KServe, llm-d, or AIBrix",
      {"queue depth autoscaling", "scale-to-zero", "cold start", "GPU-hour cost", "SLO risk"},
      {"Ray Serve", "KServe/llm-d", "AIBrix", "Dynamo"},
      {
          {"burst_scale_up", "Ray Serve autoscaling", "scheduler_root_cause",
           "scale_up_with_cold_start_risk_visible",
           "Autoscaler model is not cluster execution evidence",
           {num("current_replicas", 4), num("desired_replicas", 9),
            num("queue_pressure", 0.82), num("slo_violation_risk", 0.69),
            num("gpu_hour_cost_delta", 16.0), num("cold_start_risk", 0.50)}},
          {"scale_to_zero_idle", "llm-d/KServe scale-to-zero", "release_evidence_pipeline",
           "allow_scale_to_zero_only_with_warmup_budget",
           "Scale-to-zero savings require target cluster evidence",
           {num("current_replicas", 2), num("desired_replicas", 0),
            num("queue_pressure", 0.0), num("slo_violation_risk", 0.0),
            num("gpu_hour_cost_delta", -7.2), num("cold_start_risk", 0.72)}},
          {"drain_down_tail", "KServe drain", "product_replay_semantic_events",
           "drain_before_scale_down_when_active_decode_exists",
           "Replica drain behavior remains product control-plane owned",
           {num("current_replicas", 12), num("desired_replicas", 5),
            num("queue_pressure", 0.24), num("slo_violation_risk", 0.18),
            num("gpu_hour_cost_delta", -25.2), num("drain_time_s", 45.0)}},
      }};
}

ArtifactSpec lora_adapter_cache() {
  return {
      "native_lora_adapter_cache_deep_dive",
      "native LoRA adapter cache model; does not implement Punica, S-LoRA, vLLM LoRA, or product kernels",
      {"multi-LoRA serving", "adapter cache", "rank batching", "tenant isolation", "KV sharing boundary"},
      {"vLLM", "Punica", "S-LoRA", "AIBrix", "llm-d"},
      {
          {"hot_adapter_cache", "vLLM LoRA adapters", "kernel_compatibility_matrix",
           "pack_hot_adapters_and_keep_kv_isolated_by_adapter",
           "Adapter cache model does not prove product LoRA throughput",
           {num("cache_hit_rate", 0.91), num("evictions_per_minute", 4.0),
            num("adapter_memory_mb", 1536.0), num("load_penalty_p99_ms", 22.0),
            num("mixed_batch_fragmentation_score", 0.25), boolean("kv_share_allowed_across_adapters", false)}},
          {"long_tail_adapters", "Punica/S-LoRA style serving", "autoscaler_cost_slo",
           "increase_cache_slots_or_bucket_by_rank",
           "Rank batching and adapter paging remain external product/kernel capabilities",
           {num("cache_hit_rate", 0.52), num("evictions_per_minute", 41.0),
            num("adapter_memory_mb", 4096.0), num("load_penalty_p99_ms", 180.0),
            num("mixed_batch_fragmentation_score", 0.62), num("tenant_isolation_risk", 0.34)}},
          {"tenant_adapter_burst", "AIBrix/llm-d LoRA control plane", "release_evidence_pipeline",
           "require_registry_lineage_and_adapter_quota",
           "Adapter registry semantics need product/control-plane evidence",
           {num("cache_hit_rate", 0.67), num("evictions_per_minute", 18.0),
            num("adapter_memory_mb", 2816.0), num("load_penalty_p99_ms", 95.0),
            num("tenant_isolation_risk", 0.58), str("cache_policy", "tenant_rank_bucket_lru")}},
      }};
}

ArtifactSpec grammar_backend_matrix() {
  return {
      "native_grammar_backend_matrix",
      "native grammar backend contract; does not execute XGrammar, llguidance, Outlines, vLLM, or SGLang",
      {"structured output grammar", "token mask", "schema compilation cache", "tool-call compatibility"},
      {"XGrammar", "llguidance", "Outlines", "vLLM", "SGLang"},
      {
          {"json_schema_mask", "XGrammar", "structured_output_parser_contract",
           "prefer_compiled_mask_with_cache",
           "Grammar backend timing needs target runtime evidence",
           {num("schema_coverage", 0.92), num("compile_cache_hit_rate", 0.84),
            num("mask_cost_ms", 1.8), num("fallback_rate", 0.04),
            str("supported_surface", "json_schema_tool_call")}},
          {"regex_choice", "llguidance/Outlines", "kernel_compatibility_matrix",
           "use_for_small_schema_and_low_qps",
           "Regex grammar coverage is not full tool-calling correctness",
           {num("schema_coverage", 0.71), num("compile_cache_hit_rate", 0.62),
            num("mask_cost_ms", 0.9), num("fallback_rate", 0.16),
            str("supported_surface", "regex_choice_enum")}},
          {"tool_call_nested_schema", "vLLM/SGLang grammar backend", "product_replay_semantic_events",
           "require_parser_contract_and_backend_fallback",
           "Nested tool schemas need replay fixtures before release claims",
           {num("schema_coverage", 0.78), num("compile_cache_hit_rate", 0.69),
            num("mask_cost_ms", 2.7), num("fallback_rate", 0.13),
            str("supported_surface", "nested_tool_call_json")}},
      }};
}

ArtifactSpec edge_compiler_matrix() {
  return {
      "native_edge_compiler_artifact_matrix",
      "native edge compiler artifact readiness matrix; does not run MLC, WebLLM, llama.cpp, Android, or device benchmarks",
      {"edge compiler", "WebGPU", "Android ABI", "GGUF", "device memory", "artifact layout"},
      {"MLC LLM", "WebLLM", "llama.cpp", "Android NDK"},
      {
          {"android_gguf_cpu", "llama.cpp Android", "release_evidence_pipeline",
           "ready_for_device_fixture_import_not_perf_claim",
           "Device throughput requires real Android execution evidence",
           {num("artifact_completeness", 0.82), num("device_memory_risk", 0.44),
            num("abi_readiness", 0.90), num("backend_compatibility", 0.76),
            str("artifact_layout", "apk_native_lib_model_external")}},
          {"webgpu_compile", "MLC LLM/WebLLM", "kernel_compatibility_matrix",
           "require_compile_log_and_browser_backend_matrix",
           "WebGPU compiler/runtime behavior remains external",
           {num("artifact_completeness", 0.70), num("device_memory_risk", 0.58),
            num("abi_readiness", 0.62), num("backend_compatibility", 0.68),
            str("artifact_layout", "wasm_webgpu_tokenizer_model_cache")}},
          {"ios_android_mlc", "MLC LLM mobile", "autoscaler_cost_slo",
           "keep_as_migration_plan_until_device_artifacts_exist",
           "Mobile compiler artifacts need target device validation",
           {num("artifact_completeness", 0.64), num("device_memory_risk", 0.66),
            num("abi_readiness", 0.72), num("backend_compatibility", 0.61),
            str("artifact_layout", "compiled_lib_params_tokenizer")}},
      }};
}

ArtifactSpec spec_for(const std::string& requested) {
  const std::string_view artifact_type =
      inferlab_mature_product::resolve_selector(
          requested, inferlab_mature_product::ArtifactFamily::p2)
          .artifact_type;
  if (artifact_type == "native_structured_output_parser_contract") {
    return structured_output_parser_contract();
  }
  if (artifact_type == "native_autoscaler_cost_slo_deep_dive") {
    return autoscaler_cost_slo();
  }
  if (artifact_type == "native_lora_adapter_cache_deep_dive") {
    return lora_adapter_cache();
  }
  if (artifact_type == "native_grammar_backend_matrix") {
    return grammar_backend_matrix();
  }
  if (artifact_type == "native_edge_compiler_artifact_matrix") {
    return edge_compiler_matrix();
  }
  if (artifact_type == "native_streaming_backpressure_deep_dive") {
    return streaming_backpressure();
  }
  throw std::logic_error("resolved P2 artifact has no builder");
}

}  // namespace

int main(int argc, char** argv) {
  return inferlab_mature_product::run_artifact_dispatcher(
      argc, argv, inferlab_mature_product::ArtifactFamily::p2, spec_for);
}
