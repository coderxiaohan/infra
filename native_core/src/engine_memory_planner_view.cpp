#include "engine_memory_planner_view.hpp"

#include "release_kv_budget_profile_view.hpp"

#include "advanced_serving_internal.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::engine_memory_planner {
namespace {

constexpr const char* kArtifactType = "native_engine_memory_planner";
constexpr const char* kArtifactPath =
    "results/raw/native_engine_memory_planner.json";
constexpr const char* kTargetName = "inferlab_engine_memory_planner";
constexpr const char* kRefreshCommand =
    "./build/inferlab_engine_memory_planner --repo-root . --output results/raw/native_engine_memory_planner.json";
constexpr const char* kClaimScope =
    "compiled analytical engine-memory fixture only; not observed allocator or CUDA memory, runtime admission, product backend capacity, OOM reproduction, benchmark, measured performance, execute/replay, or release-readiness evidence";

using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

const std::vector<std::string>& product_alignment() {
  static const std::vector<std::string> products = {
      "vLLM", "SGLang", "TensorRT-LLM", "FlashInfer",
      "LMCache", "Mooncake"};
  return products;
}

void validate_record_for_projection(const Record& record) {
  if (record.profile.name.empty() || record.profile.parameter_count <= 0 ||
      record.active_sequence_count <= 0 ||
      record.sequence_length_tokens <= 0 ||
      record.activation_live_tokens < 0 ||
      record.max_tokens_by_kv_budget < 0 ||
      record.max_concurrency_by_memory < 0) {
    throw std::invalid_argument(
        "engine memory view received an incomplete record");
  }
  numeric_format_name(record.profile.weight_format);
  numeric_format_name(record.profile.kv_format);
  recommendation_name(record.recommendation);
  require_unit_interval(record.weight_metadata_ratio,
                        "engine memory weight metadata ratio");
  require_non_negative_finite(record.raw_weight_bytes,
                              "engine memory raw weight bytes");
  require_positive_finite(record.weight_bytes,
                          "engine memory weight bytes");
  require_positive_finite(record.kv_cache_bytes,
                          "engine memory KV cache bytes");
  require_non_negative_finite(record.activation_bytes,
                              "engine memory activation bytes");
  require_positive_finite(record.io_tensor_bytes,
                          "engine memory I/O tensor bytes");
  require_non_negative_finite(record.lora_adapter_bytes,
                              "engine memory LoRA bytes");
  require_non_negative_finite(record.cuda_graph_workspace_bytes,
                              "engine memory CUDA graph workspace bytes");
  require_positive_finite(record.fixed_memory_bytes,
                          "engine memory fixed bytes");
  require_positive_finite(record.configured_variable_bytes,
                          "engine memory configured variable bytes");
  require_non_negative_finite(record.fragmentation_bytes,
                              "engine memory fragmentation bytes");
  require_positive_finite(record.total_required_bytes,
                          "engine memory total required bytes");
  require_finite(record.available_after_weights_bytes,
                 "engine memory available-after-weights bytes");
  require_positive_finite(record.usable_before_fragmentation_bytes,
                          "engine memory usable pre-fragmentation bytes");
  require_non_negative_finite(record.kv_budget_bytes,
                              "engine memory KV budget bytes");
  require_positive_finite(record.aggregate_kv_token_bytes,
                          "engine memory aggregate KV token bytes");
  require_positive_finite(record.one_concurrency_kv_bytes,
                          "engine memory one-concurrency KV bytes");
  require_positive_finite(record.one_concurrency_variable_bytes,
                          "engine memory one-concurrency variable bytes");
  require_non_negative_finite(record.kv_headroom_streams,
                              "engine memory KV headroom streams");
  require_positive_finite(record.memory_utilization,
                          "engine memory utilization");
  require_non_negative_finite(record.memory_pressure_over_target,
                              "engine memory pressure over target");
  require_unit_interval(record.oom_risk_score,
                        "engine memory normalized OOM risk");
}

void validate_plan_for_projection(const Plan& plan) {
  if (plan.records.empty() ||
      plan.comparable_record_count != plan.records.size() ||
      plan.oom_profile_count > plan.records.size() ||
      plan.capacity_review_profile_count > plan.records.size() ||
      plan.max_kv_record_index >= plan.records.size() ||
      plan.max_risk_record_index >= plan.records.size()) {
    throw std::invalid_argument(
        "engine memory view requires a complete valid plan");
  }
  require_positive_finite(plan.gpu_memory_gib,
                          "engine memory plan GPU capacity");
  plan_decision_name(plan.decision);
  for (const auto& record : plan.records) {
    validate_record_for_projection(record);
  }
}

std::string assumptions_json(const Assumptions& assumptions,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"activation_format\": "
      << inferlab_plan::json_string(
             numeric_format_name(assumptions.activation_format)) << ",\n";
  out << nested << "\"activation_live_token_cap\": "
      << assumptions.activation_live_token_cap << ",\n";
  out << nested << "\"activation_residency_ratio\": "
      << inferlab_plan::fixed6(
             assumptions.activation_residency_ratio) << ",\n";
  out << nested << "\"binary_bytes_per_gib\": 1073741824,\n";
  out << nested << "\"full_risk_memory_utilization\": "
      << inferlab_plan::fixed6(
             assumptions.full_risk_memory_utilization) << ",\n";
  out << nested << "\"io_format\": "
      << inferlab_plan::json_string(
             numeric_format_name(assumptions.io_format)) << ",\n";
  out << nested << "\"io_hidden_copy_count\": "
      << assumptions.io_hidden_copy_count << ",\n";
  out << nested << "\"io_logits_copy_count\": "
      << assumptions.io_logits_copy_count << ",\n";
  out << nested << "\"kv_storage_mapping\": "
      << inferlab_plan::json_string(
             "fp8/int8 use canonical eight-bit storage; fp4/int4/nvfp4 use canonical four-bit packed storage")
      << ",\n";
  out << nested << "\"minimum_kv_headroom_stream_multiplier\": "
      << inferlab_plan::fixed6(
             assumptions.minimum_kv_headroom_stream_multiplier) << ",\n";
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << ",\n";
  out << nested << "\"standard_weight_metadata_ratio\": "
      << inferlab_plan::fixed6(
             assumptions.standard_weight_metadata_ratio) << ",\n";
  out << nested << "\"sub_byte_weight_metadata_ratio\": "
      << inferlab_plan::fixed6(
             assumptions.sub_byte_weight_metadata_ratio) << ",\n";
  out << nested << "\"target_memory_utilization\": "
      << inferlab_plan::fixed6(
             assumptions.target_memory_utilization) << "\n";
  out << pad << "}";
  return out.str();
}

std::string profile_json(const Profile& profile, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const double parameters_b =
      static_cast<double>(profile.parameter_count) / 1000000000.0;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"batch_size\": " << profile.batch_size << ",\n";
  out << nested << "\"cuda_graph_workspace_gb\": "
      << inferlab_plan::fixed6(profile.cuda_graph_workspace_gib) << ",\n";
  out << nested << "\"cuda_graph_workspace_gib\": "
      << inferlab_plan::fixed6(profile.cuda_graph_workspace_gib) << ",\n";
  out << nested << "\"fragmentation_ratio\": "
      << inferlab_plan::fixed6(profile.fragmentation_ratio) << ",\n";
  out << nested << "\"prefill_chunk_tokens\": "
      << profile.prefill_chunk_tokens << ",\n";
  out << nested << "\"gpu_memory_gb\": "
      << inferlab_plan::fixed6(profile.gpu_memory_gib) << ",\n";
  out << nested << "\"gpu_memory_gib\": "
      << inferlab_plan::fixed6(profile.gpu_memory_gib) << ",\n";
  out << nested << "\"head_dim\": " << profile.head_dim << ",\n";
  out << nested << "\"hidden_size\": " << profile.hidden_size << ",\n";
  out << nested << "\"kv_heads\": " << profile.kv_heads << ",\n";
  out << nested << "\"kv_precision\": "
      << inferlab_plan::json_string(
             numeric_format_name(profile.kv_format)) << ",\n";
  out << nested << "\"layers\": " << profile.layers << ",\n";
  out << nested << "\"lora_adapter_gb\": "
      << inferlab_plan::fixed6(profile.lora_adapter_gib) << ",\n";
  out << nested << "\"lora_adapter_gib\": "
      << inferlab_plan::fixed6(profile.lora_adapter_gib) << ",\n";
  out << nested << "\"max_concurrency\": "
      << profile.max_concurrency << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(profile.name) << ",\n";
  out << nested << "\"output_tokens\": "
      << profile.output_tokens << ",\n";
  out << nested << "\"parameter_count\": "
      << profile.parameter_count << ",\n";
  out << nested << "\"parameters_b\": "
      << inferlab_plan::fixed6(parameters_b) << ",\n";
  out << nested << "\"prompt_tokens\": "
      << profile.prompt_tokens << ",\n";
  out << nested << "\"vocabulary_size\": "
      << profile.vocabulary_size << ",\n";
  out << nested << "\"weight_precision\": "
      << inferlab_plan::json_string(
             numeric_format_name(profile.weight_format)) << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const Record& record, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"activation_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(record.activation_bytes))
      << ",\n";
  out << nested << "\"activation_live_tokens\": "
      << record.activation_live_tokens << ",\n";
  out << nested << "\"active_sequence_count\": "
      << record.active_sequence_count << ",\n";
  out << nested << "\"available_after_weights_gb\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.available_after_weights_bytes)) << ",\n";
  out << nested << "\"available_after_weights_scope\": "
      << inferlab_plan::json_string(
             "GPU capacity minus weights, LoRA, and CUDA graph workspace before variable tensors and fragmentation")
      << ",\n";
  out << nested << "\"cuda_graph_workspace_gb\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.cuda_graph_workspace_bytes)) << ",\n";
  out << nested << "\"exceeds_gpu_memory\": "
      << inferlab_plan::json_bool(record.exceeds_gpu_memory) << ",\n";
  out << nested << "\"fragmentation_gb\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.fragmentation_bytes)) << ",\n";
  out << nested << "\"io_tensor_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(record.io_tensor_bytes))
      << ",\n";
  out << nested << "\"kv_budget_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(record.kv_budget_bytes))
      << ",\n";
  out << nested << "\"kv_budget_scope\": "
      << inferlab_plan::json_string(
             "pre-fragmentation usable capacity after fixed memory plus configured activation and I/O tensors")
      << ",\n";
  out << nested << "\"kv_cache_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(record.kv_cache_bytes))
      << ",\n";
  out << nested << "\"kv_headroom_streams\": "
      << inferlab_plan::fixed6(record.kv_headroom_streams) << ",\n";
  out << nested << "\"lora_adapter_gb\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.lora_adapter_bytes)) << ",\n";
  out << nested << "\"max_concurrency_by_memory\": "
      << record.max_concurrency_by_memory << ",\n";
  out << nested << "\"max_tokens_by_kv_budget\": "
      << record.max_tokens_by_kv_budget << ",\n";
  out << nested << "\"max_tokens_scope\": "
      << inferlab_plan::json_string("aggregate_sequence_tokens")
      << ",\n";
  out << nested << "\"memory_pressure_over_target\": "
      << inferlab_plan::fixed6(record.memory_pressure_over_target)
      << ",\n";
  out << nested << "\"memory_utilization\": "
      << inferlab_plan::fixed6(record.memory_utilization) << ",\n";
  out << nested << "\"oom_risk_score\": "
      << inferlab_plan::fixed6(record.oom_risk_score) << ",\n";
  out << nested << "\"oom_risk_score_scope\": "
      << inferlab_plan::json_string(
             "legacy normalized analytical pressure capped at one; use memory_pressure_over_target for the uncapped multiple")
      << ",\n";
  out << nested << "\"profile\": "
      << profile_json(record.profile, indent + 2) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommendation_name(record.recommendation)) << ",\n";
  out << nested << "\"sequence_length_tokens\": "
      << record.sequence_length_tokens << ",\n";
  out << nested << "\"total_required_gb\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.total_required_bytes)) << ",\n";
  out << nested << "\"usable_before_fragmentation_gib\": "
      << inferlab_plan::fixed6(
             bytes_to_gib(record.usable_before_fragmentation_bytes))
      << ",\n";
  out << nested << "\"weight_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(record.weight_bytes))
      << ",\n";
  out << nested << "\"weight_metadata_ratio\": "
      << inferlab_plan::fixed6(record.weight_metadata_ratio) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan_for_projection(plan);
  const Record& max_kv = plan.records[plan.max_kv_record_index];
  const Record& max_risk = plan.records[plan.max_risk_record_index];
  const auto kv_budget_profile = release_kv_budget_binding(plan);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope, product_alignment(), kTargetName,
      kArtifactPath, kRefreshCommand);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"fixture_basis\": \"compiled_default_profiles\",\n";
  out << "  \"metric_evidence\": \"modeled_not_measured\",\n";
  out << "  \"reference_config\": \"configs/engine_memory.example.json\",\n";
  out << "  \"reference_config_consumed\": false,\n";
  out << "  \"unit_basis\": \"binary_GiB_with_legacy_gb_field_names\",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"capacity_model\": \"engine_memory_planner_model.hpp/.cpp\",\n";
  out << "    \"kv_storage_estimator\": \"inferlab::estimate_kv_cache\",\n";
  out << "    \"artifact_view\": \"engine_memory_planner_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"release_kv_budget_profile\": "
      << inferlab_release_kv_budget::binding_json(kv_budget_profile, 2)
      << ",\n";
  out << "  \"assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"capacity_review_profile_count\": "
      << plan.capacity_review_profile_count << ",\n";
  out << "    \"decision\": "
      << inferlab_plan::json_string(plan_decision_name(plan.decision))
      << ",\n";
  out << "    \"gpu_memory_gb\": "
      << inferlab_plan::fixed6(plan.gpu_memory_gib) << ",\n";
  out << "    \"gpu_memory_gib\": "
      << inferlab_plan::fixed6(plan.gpu_memory_gib) << ",\n";
  out << "    \"max_kv_profile\": "
      << inferlab_plan::json_string(max_kv.profile.name) << ",\n";
  out << "    \"max_kv_cache_gb\": "
      << inferlab_plan::fixed6(bytes_to_gib(max_kv.kv_cache_bytes))
      << ",\n";
  out << "    \"max_risk_profile\": "
      << inferlab_plan::json_string(max_risk.profile.name) << ",\n";
  out << "    \"max_oom_risk_score\": "
      << inferlab_plan::fixed6(max_risk.oom_risk_score) << ",\n";
  out << "    \"max_memory_pressure_over_target\": "
      << inferlab_plan::fixed6(
             max_risk.memory_pressure_over_target) << ",\n";
  out << "    \"oom_profile_count\": "
      << plan.oom_profile_count << ",\n";
  out << "    \"profile_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], 4)
        << (index + 1 == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::engine_memory_planner
