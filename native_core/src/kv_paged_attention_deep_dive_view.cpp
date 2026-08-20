#include "kv_paged_attention_deep_dive_view.hpp"

#include "native_trace_context_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::kv_paged_attention_deep_dive {
namespace {

constexpr const char* kArtifactType =
    "native_kv_paged_attention_deep_dive";
constexpr const char* kTargetName =
    "inferlab_kv_paged_attention_deep_dive";
constexpr const char* kArtifactPath =
    "results/raw/native_kv_paged_attention_deep_dive.json";
constexpr const char* kRefreshCommand =
    "./build/inferlab_kv_paged_attention_deep_dive --repo-root . --output results/raw/native_kv_paged_attention_deep_dive.json";
constexpr const char* kClaimScope =
    "native mechanism deep-dive plan; not a product benchmark";

const std::vector<std::string>& product_alignment() {
  static const std::vector<std::string> products = {
      "vLLM", "SGLang", "TensorRT-LLM", "LMCache", "Mooncake"};
  return products;
}

const std::vector<std::string>& research_basis() {
  static const std::vector<std::string> basis = {
      "Paged KV block table", "prefix cache", "Paged KV locality",
      "KV transfer versus prefill analytical crossover"};
  return basis;
}

std::vector<std::string> evidence_bridge_inputs(const Plan& plan) {
  std::vector<std::string> inputs;
  inputs.reserve(plan.records.size() + 2U);
  for (const auto& record : plan.records) {
    inputs.push_back(record.scenario.evidence_path);
  }
  inputs.push_back("results/raw/product_vllm_capability_profile.json");
  inputs.push_back("results/raw/product_sglang_capability_profile.json");
  return inputs;
}

void validate_plan_for_view(const Plan& plan) {
  if (plan.records.empty() ||
      plan.best_record_index >= plan.records.size() ||
      plan.worst_read_record_index >= plan.records.size() ||
      plan.transfer_preferred_record_count > plan.records.size()) {
    throw std::invalid_argument(
        "KV/PagedAttention artifact view received an invalid plan");
  }
  std::vector<std::string> scenario_names;
  scenario_names.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    scenario_names.push_back(record.scenario.name);
  }
  inferlab_native_trace::validate_trace_context_alignment(
      plan.trace_context,
      inferlab_native_trace::Domain::kv_paged_attention,
      scenario_names);
}

std::string assumptions_json(const Assumptions& assumptions,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"bytes_per_gib\": "
      << inferlab_plan::fixed6(assumptions.bytes_per_gib) << ",\n";
  out << nested << "\"external_fragmentation_scope\": "
      << inferlab_plan::json_string(
             "compiled_free_tail_tokens_over_reserved_plus_free_tail")
      << ",\n";
  out << nested << "\"fixture_name\": "
      << inferlab_plan::json_string(assumptions.fixture_name) << ",\n";
  out << nested << "\"high_read_amplification_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.high_read_amplification_threshold)
      << ",\n";
  out << nested << "\"max_kv_bytes_per_token\": "
      << assumptions.max_kv_bytes_per_token << ",\n";
  out << nested << "\"max_physical_blocks\": "
      << assumptions.max_physical_blocks << ",\n";
  out << nested << "\"max_request_tokens\": "
      << assumptions.max_request_tokens << ",\n";
  out << nested << "\"prefix_sharing_bonus\": "
      << inferlab_plan::fixed6(assumptions.prefix_sharing_bonus) << ",\n";
  out << nested << "\"read_amplification_scope\": "
      << inferlab_plan::json_string(
             "canonical_locality_touched_block_capacity_over_logical_elements")
      << ",\n";
  out << nested << "\"replay_comparison_score_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.replay_comparison_score_threshold)
      << ",\n";
  out << nested << "\"require_block_aligned_shared_prefix\": "
      << inferlab_plan::json_bool(
             assumptions.require_block_aligned_shared_prefix)
      << ",\n";
  out << nested << "\"score_weights\": {\n";
  out << nested << "  \"external_fragmentation\": "
      << inferlab_plan::fixed6(
             assumptions.external_fragmentation_weight)
      << ",\n";
  out << nested << "  \"internal_fragmentation\": "
      << inferlab_plan::fixed6(
             assumptions.internal_fragmentation_weight)
      << ",\n";
  out << nested << "  \"prefix_sharing\": "
      << inferlab_plan::fixed6(assumptions.prefix_sharing_weight)
      << ",\n";
  out << nested << "  \"read_efficiency\": "
      << inferlab_plan::fixed6(assumptions.read_efficiency_weight)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"transfer_payload_scope\": "
      << inferlab_plan::json_string(
             "scenario_live_tokens_are_transfer_payload_tokens_not_resident_request_tokens")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string mapping_json(const MappingSample& sample, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"block_offset\": " << sample.block_offset << ",\n";
  out << nested << "\"logical_token\": " << sample.logical_token << ",\n";
  out << nested << "\"physical_block\": " << sample.physical_block
      << ",\n";
  out << nested << "\"shared_prefix\": "
      << inferlab_plan::json_bool(sample.shared_prefix) << "\n";
  out << pad << "}";
  return out.str();
}

std::string mapping_array_json_impl(
    const std::vector<MappingSample>& samples, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < samples.size(); ++index) {
    out << mapping_json(samples[index], indent + 2)
        << (index + 1 == samples.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string scenario_json(const Scenario& scenario, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dense_read_tokens\": "
      << inferlab_plan::fixed6(scenario.dense_read_tokens) << ",\n";
  out << nested << "\"evidence_path\": "
      << inferlab_plan::json_string(scenario.evidence_path) << ",\n";
  out << nested << "\"free_tail_tokens\": "
      << scenario.free_tail_tokens << ",\n";
  out << nested << "\"kv_bytes_per_token\": "
      << inferlab_plan::fixed6(
             static_cast<double>(scenario.kv_bytes_per_token))
      << ",\n";
  out << nested << "\"live_tokens\": " << scenario.live_tokens << ",\n";
  out << nested << "\"live_tokens_scope\": "
      << inferlab_plan::json_string("transfer_payload_tokens") << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"page_size_tokens\": "
      << scenario.page_size_tokens << ",\n";
  out << nested << "\"prefill_ms_per_token\": "
      << inferlab_plan::fixed6(scenario.prefill_ms_per_token) << ",\n";
  out << nested << "\"request_tokens\": "
      << scenario.request_tokens << ",\n";
  out << nested << "\"shared_prefix_tokens\": "
      << scenario.shared_prefix_tokens << ",\n";
  out << nested << "\"transfer_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.transfer_bandwidth_gbps) << ",\n";
  out << nested << "\"transfer_setup_ms\": "
      << inferlab_plan::fixed6(scenario.transfer_setup_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string result_json(const Result& result, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"block_table_entries\": "
      << result.block_table_entries << ",\n";
  out << nested << "\"deep_dive_score\": "
      << inferlab_plan::fixed6(result.deep_dive_score) << ",\n";
  out << nested << "\"dense_kv_read_tokens\": "
      << inferlab_plan::fixed6(result.dense_kv_read_tokens) << ",\n";
  out << nested << "\"external_fragmentation_ratio\": "
      << inferlab_plan::fixed6(result.external_fragmentation_ratio)
      << ",\n";
  out << nested << "\"internal_fragmentation_ratio\": "
      << inferlab_plan::fixed6(result.internal_fragmentation_ratio)
      << ",\n";
  out << nested << "\"kv_live_bytes\": "
      << inferlab_plan::fixed6(static_cast<double>(result.kv_live_bytes))
      << ",\n";
  out << nested << "\"kv_live_bytes_scope\": "
      << inferlab_plan::json_string("transfer_payload_bytes") << ",\n";
  out << nested << "\"logical_to_physical_samples\": "
      << mapping_array_json_impl(result.mapping_samples, indent + 2)
      << ",\n";
  out << nested << "\"paged_kv_read_tokens\": "
      << inferlab_plan::fixed6(result.paged_kv_read_tokens) << ",\n";
  out << nested << "\"paged_to_dense_read_ratio\": "
      << inferlab_plan::fixed6(result.paged_to_dense_read_ratio) << ",\n";
  out << nested << "\"physical_blocks\": "
      << result.physical_blocks << ",\n";
  out << nested << "\"prefix_sharing_rate\": "
      << inferlab_plan::fixed6(result.prefix_sharing_rate) << ",\n";
  out << nested << "\"prefill_recompute_ms\": "
      << inferlab_plan::fixed6(result.prefill_recompute_ms) << ",\n";
  out << nested << "\"read_amplification\": "
      << inferlab_plan::fixed6(result.read_amplification) << ",\n";
  out << nested << "\"read_amplification_scope\": "
      << inferlab_plan::json_string(
             "touched_block_capacity_over_logical_token_reads")
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommendation_name(result.recommendation))
      << ",\n";
  out << nested << "\"reserved_bytes\": " << result.reserved_bytes
      << ",\n";
  out << nested << "\"reserved_tokens\": " << result.reserved_tokens
      << ",\n";
  out << nested << "\"resident_kv_bytes\": "
      << result.resident_kv_bytes << ",\n";
  out << nested << "\"resident_tokens\": " << result.resident_tokens
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, indent + 2) << ",\n";
  out << nested << "\"transfer_ms\": "
      << inferlab_plan::fixed6(result.transfer_ms) << ",\n";
  out << nested << "\"transfer_payload_gib\": "
      << inferlab_plan::fixed6(result.transfer_payload_gib) << ",\n";
  out << nested << "\"transfer_vs_prefill_decision\": "
      << inferlab_plan::json_string(
             transfer_decision_name(result.transfer_vs_prefill_decision))
      << ",\n";
  out << nested << "\"transfer_vs_prefill_delta_ms\": "
      << inferlab_plan::fixed6(result.transfer_vs_prefill_delta_ms) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string mapping_samples_json(
    const std::vector<MappingSample>& samples, const int indent) {
  if (samples.empty()) {
    throw std::invalid_argument(
        "KV/PagedAttention mapping sample view requires records");
  }
  return mapping_array_json_impl(samples, indent);
}

std::string artifact_json(const Plan& plan) {
  validate_plan_for_view(plan);
  const Result& best = plan.records[plan.best_record_index];
  const Result& worst_read = plan.records[plan.worst_read_record_index];
  const auto inputs = evidence_bridge_inputs(plan);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope, product_alignment(), kTargetName,
      kArtifactPath, kRefreshCommand);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"deep_dive_type\": \"kv_paged_attention\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"claim_boundary_detail\": "
      << inferlab_plan::json_string(
             "compiled native Paged KV allocation/locality composition only; does not implement or benchmark a mature-product PagedAttention runtime, import replay logs, execute CUDA kernels, or prove release readiness")
      << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"fixture_basis\": "
      << inferlab_plan::json_string(plan.assumptions.fixture_name) << ",\n";
  out << "  \"metric_evidence\": \"modeled_not_measured\",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"allocation_and_token_mapping\": \"inferlab::PagedKVCacheManager\",\n";
  out << "    \"block_locality_and_read_amplification\": \"inferlab::analyze_block_table_locality\",\n";
  out << "    \"transfer_serialization\": \"runtime_communication_internal::transfer_milliseconds\",\n";
  out << "    \"artifact_composition\": \"kv_paged_attention_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"artifact_view\": \"kv_paged_attention_deep_dive_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(research_basis(), 2) << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(product_alignment(), 2) << ",\n";
  out << "  \"assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_scenario\": "
      << inferlab_plan::json_string(best.scenario.name) << ",\n";
  out << "    \"best_deep_dive_score\": "
      << inferlab_plan::fixed6(best.deep_dive_score) << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << ",\n";
  out << "    \"transfer_preferred_scenario_count\": "
      << plan.transfer_preferred_record_count << ",\n";
  out << "    \"worst_read_amplification\": "
      << inferlab_plan::fixed6(worst_read.read_amplification) << ",\n";
  out << "    \"worst_read_amplification_scenario\": "
      << inferlab_plan::json_string(worst_read.scenario.name) << "\n";
  out << "  },\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(inputs, 2) << ",\n";
  out << "  \"native_trace_context\": "
      << inferlab_native_trace::trace_context_projection_json(
             plan.trace_context, 2)
      << ",\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << result_json(plan.records[index], 4)
        << (index + 1 == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kv_paged_attention_deep_dive
