#include "cuda_quant_microkernel_deep_dive_view.hpp"

#include "checked_index.hpp"
#include "native_trace_context_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::cuda_quant_microkernel {
namespace {

constexpr const char* kArtifactType =
    "native_cuda_quant_microkernel_deep_dive";
constexpr const char* kArtifactPath =
    "results/raw/native_cuda_quant_microkernel_deep_dive.json";
constexpr const char* kTargetName =
    "inferlab_cuda_quant_microkernel_deep_dive";
constexpr const char* kRefreshCommand =
    "./build/inferlab_cuda_quant_microkernel_deep_dive --repo-root . --output results/raw/native_cuda_quant_microkernel_deep_dive.json";
constexpr const char* kClaimScope =
    "compiled CUDA/quant microkernel metadata and analytical-priority fixture only; not CUDA correctness, measured timing, observed arithmetic intensity, kernel readiness, product backend support, execute, benchmark, or release evidence";

std::string joined_metadata_fields(
    const std::vector<std::string>& fields) {
  if (fields.empty()) {
    throw std::invalid_argument(
        "CUDA/quant view requires profiling metadata fields");
  }
  std::ostringstream out;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    if (index != 0U) {
      out << ", ";
    }
    out << fields[index];
  }
  return out.str();
}

std::string assumptions_json(const Assumptions& assumptions,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "CUDA/quant assumptions JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"compute_bound_intensity_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.compute_bound_intensity_threshold)
      << ",\n";
  out << nested << "\"intensity_priority_reference\": "
      << inferlab_plan::fixed6(
             assumptions.intensity_priority_reference)
      << ",\n";
  out << nested << "\"intensity_priority_score_weight\": "
      << inferlab_plan::fixed6(
             assumptions.intensity_priority_score_weight)
      << ",\n";
  out << nested << "\"memory_bound_intensity_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.memory_bound_intensity_threshold)
      << ",\n";
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name)
      << ",\n";
  out << nested << "\"profiling_metadata_fields\": "
      << inferlab_plan::json_string_array(
             assumptions.profiling_metadata_fields, nested_indent)
      << ",\n";
  out << nested << "\"static_contract_score_weight\": "
      << inferlab_plan::fixed6(
             assumptions.static_contract_score_weight)
      << ",\n";
  out << nested << "\"score_scope\": "
      << inferlab_plan::json_string(
             "static_contract_coverage_plus_modeled_intensity_priority_not_runtime_readiness")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string operator_contract_json(
    const inferlab_high_value::MicrobenchOperator& contract,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "CUDA/quant operator contract JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend_candidates\": "
      << inferlab_plan::json_string(contract.backend_candidates)
      << ",\n";
  out << nested << "\"benchmark_case\": "
      << inferlab_plan::json_string(contract.benchmark_case)
      << ",\n";
  out << nested << "\"correctness_oracle\": "
      << inferlab_plan::json_string(contract.correctness_oracle)
      << ",\n";
  out << nested << "\"dtype_layout\": "
      << inferlab_plan::json_string(contract.dtype_layout)
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(contract.name) << ",\n";
  out << nested << "\"operator_family\": "
      << inferlab_plan::json_string(contract.operator_family)
      << ",\n";
  out << nested << "\"nsight_compute_command\": "
      << inferlab_plan::json_string(
             contract.nsight_compute_command)
      << ",\n";
  out << nested << "\"nsight_systems_command\": "
      << inferlab_plan::json_string(
             contract.nsight_systems_command)
      << ",\n";
  out << nested << "\"nvtx_range\": "
      << inferlab_plan::json_string(contract.nvtx_range)
      << ",\n";
  out << nested << "\"shape_signature\": "
      << inferlab_plan::json_string(contract.shape_signature)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const Result& result, const int indent) {
  const auto& profile = result.profile;
  const auto& contract = result.operator_contract;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "CUDA/quant scenario JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"benchmark_case\": "
      << inferlab_plan::json_string(contract.benchmark_case)
      << ",\n";
  out << nested << "\"canonical_operator\": "
      << inferlab_plan::json_string(contract.name) << ",\n";
  out << nested << "\"canonical_shape_matrix\": "
      << inferlab_plan::json_string(contract.shape_signature)
      << ",\n";
  out << nested << "\"correctness_oracle\": "
      << inferlab_plan::json_string(contract.correctness_oracle)
      << ",\n";
  out << nested << "\"correctness_tolerance\": "
      << inferlab_plan::fixed6(profile.correctness_tolerance)
      << ",\n";
  out << nested << "\"dtype\": "
      << inferlab_plan::json_string(contract.dtype_layout)
      << ",\n";
  out << nested << "\"dtype_layout\": "
      << inferlab_plan::json_string(contract.dtype_layout)
      << ",\n";
  out << nested << "\"estimated_bytes\": "
      << inferlab_plan::fixed6(profile.estimated_bytes) << ",\n";
  out << nested << "\"estimated_flops\": "
      << inferlab_plan::fixed6(profile.estimated_flops) << ",\n";
  out << nested << "\"estimate_basis\": "
      << inferlab_plan::json_string(
             "compiled_operator_specific_analytical_fixture_not_profile_measurement")
      << ",\n";
  out << nested << "\"fallback_backend\": "
      << inferlab_plan::json_string(profile.fallback_backend)
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(
             profile.legacy_scenario_name)
      << ",\n";
  out << nested << "\"nvtx_range\": "
      << inferlab_plan::json_string(contract.nvtx_range)
      << ",\n";
  out << nested << "\"operator_family\": "
      << inferlab_plan::json_string(contract.operator_family)
      << ",\n";
  out << nested << "\"product_alignment\": "
      << inferlab_plan::json_string(contract.backend_candidates)
      << ",\n";
  out << nested << "\"reference_artifact\": "
      << inferlab_plan::json_string(profile.reference_artifact)
      << ",\n";
  out << nested << "\"requires_cuda\": "
      << inferlab_plan::json_bool(
             result.requires_target_gpu_validation)
      << ",\n";
  out << nested << "\"requires_target_gpu_validation\": "
      << inferlab_plan::json_bool(
             result.requires_target_gpu_validation)
      << ",\n";
  out << nested << "\"shape_signature\": "
      << inferlab_plan::json_string(
             profile.modeled_shape_signature)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string result_json(const Plan& plan,
                        const Result& result,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "CUDA/quant result JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested
      << "\"analytical_priority_score\": "
      << inferlab_plan::fixed6(result.analytical_priority_score)
      << ",\n";
  out << nested
      << "\"arithmetic_intensity_flops_per_byte\": "
      << inferlab_plan::fixed6(result.arithmetic_intensity)
      << ",\n";
  out << nested << "\"arithmetic_intensity_scope\": "
      << inferlab_plan::json_string(
             "estimated_flops_divided_by_estimated_bytes_not_observed_hardware_counter")
      << ",\n";
  out << nested << "\"bottleneck_model\": "
      << inferlab_plan::json_string(
             bottleneck_model_name(result.bottleneck_model))
      << ",\n";
  out << nested << "\"correctness_contract\": "
      << inferlab_plan::json_string(result.correctness_contract)
      << ",\n";
  out << nested << "\"fallback_contract\": "
      << inferlab_plan::json_string(result.fallback_contract)
      << ",\n";
  out << nested << "\"microkernel_readiness_score\": "
      << inferlab_plan::fixed6(result.analytical_priority_score)
      << ",\n";
  out << nested << "\"microkernel_readiness_score_scope\": "
      << inferlab_plan::json_string(
             "legacy_alias_of_compiled_analytical_priority_not_runtime_readiness")
      << ",\n";
  out << nested << "\"migration_command\": "
      << inferlab_plan::json_string(kRefreshCommand) << ",\n";
  out << nested << "\"operator_contract\": "
      << operator_contract_json(
             result.operator_contract, nested_indent)
      << ",\n";
  out << nested << "\"optimization_focus\": "
      << inferlab_plan::json_string(result.optimization_focus)
      << ",\n";
  out << nested << "\"profiling_metadata_fields\": "
      << inferlab_plan::json_string_array(
             plan.assumptions.profiling_metadata_fields,
             nested_indent)
      << ",\n";
  out << nested << "\"profiling_metadata_required\": "
      << inferlab_plan::json_string(joined_metadata_fields(
             plan.assumptions.profiling_metadata_fields))
      << ",\n";
  out << nested << "\"ranking_applicable\": "
      << inferlab_plan::json_bool(result.ranking_applicable)
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result, nested_indent) << ",\n";
  out << nested << "\"static_contract_coverage_score\": "
      << inferlab_plan::fixed6(
             result.static_contract_coverage_score)
      << ",\n";
  out << nested << "\"static_contract_check_count\": "
      << result.static_contract_check_count << ",\n";
  out << nested << "\"static_contract_pass_count\": "
      << result.static_contract_pass_count
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  if (plan.records.empty() ||
      plan.comparable_record_count == 0U ||
      plan.comparable_record_count > plan.records.size() ||
      plan.highest_intensity_record_index >= plan.records.size() ||
      plan.highest_priority_record_index >= plan.records.size() ||
      !plan.records[plan.highest_intensity_record_index]
           .ranking_applicable ||
      !plan.records[plan.highest_priority_record_index]
           .ranking_applicable ||
      plan.evidence_bridge_inputs.empty()) {
    throw std::invalid_argument(
        "CUDA/quant view requires complete comparable records and evidence inputs");
  }
  std::vector<std::string> operator_names;
  operator_names.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    operator_names.push_back(record.profile.canonical_operator);
  }
  inferlab_native_trace::validate_trace_context_alignment(
      plan.trace_context,
      inferlab_native_trace::Domain::cuda_quant_microkernel,
      operator_names);
  const auto& highest_intensity =
      plan.records[plan.highest_intensity_record_index];
  const auto& highest_priority =
      plan.records[plan.highest_priority_record_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(kArtifactType) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope,
      {"FlashInfer", "TensorRT-LLM", "CUTLASS", "Marlin",
       "vLLM", "SGLang"},
      kTargetName, kArtifactPath, kRefreshCommand);
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"deep_dive_type\": "
      << inferlab_plan::json_string(
             "cuda_quant_microkernel")
      << ",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"canonical_operator_registry\": "
      << inferlab_plan::json_string(
             "inferlab_high_value::microbench_operators")
      << ",\n";
  out << "  \"fixture_basis\": "
      << inferlab_plan::json_string(
             "compiled_operator_profiles_joined_to_validated_canonical_microbenchmark_registry")
      << ",\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(
             {"RMSNorm", "RoPE", "row softmax",
              "INT4 dequant matvec", "Paged KV gather",
              "decode attention"},
             2)
      << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(
             {"FlashInfer", "TensorRT-LLM", "CUTLASS",
              "Marlin", "vLLM", "SGLang"},
             2)
      << ",\n";
  out << "  \"model_assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"highest_arithmetic_intensity_scenario\": "
      << inferlab_plan::json_string(
             highest_intensity.profile.legacy_scenario_name)
      << ",\n";
  out << "    \"highest_arithmetic_intensity_flops_per_byte\": "
      << inferlab_plan::fixed6(
             highest_intensity.arithmetic_intensity)
      << ",\n";
  out << "    \"highest_arithmetic_intensity_scope\": "
      << inferlab_plan::json_string(
             "highest_compiled_modeled_record_only_not_hardware_or_global_kernel_ranking")
      << ",\n";
  out << "    \"highest_analytical_priority_scenario\": "
      << inferlab_plan::json_string(
             highest_priority.profile.legacy_scenario_name)
      << ",\n";
  out << "    \"highest_analytical_priority_score\": "
      << inferlab_plan::fixed6(
             highest_priority.analytical_priority_score)
      << ",\n";
  out << "    \"most_ready_scenario\": "
      << inferlab_plan::json_string(
             highest_priority.profile.legacy_scenario_name)
      << ",\n";
  out << "    \"most_ready_score\": "
      << inferlab_plan::fixed6(
             highest_priority.analytical_priority_score)
      << ",\n";
  out << "    \"most_ready_scope\": "
      << inferlab_plan::json_string(
             "legacy_alias_of_highest_compiled_analytical_priority_not_runtime_readiness")
      << ",\n";
  out << "    \"scenario_count\": "
      << plan.records.size() << ",\n";
  out << "    \"canonical_operator_count\": "
      << plan.records.size() << ",\n";
  out << "    \"comparable_scenario_count\": "
      << plan.comparable_record_count << "\n";
  out << "  },\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(
             plan.evidence_bridge_inputs, 2)
      << ",\n";
  out << "  \"native_trace_context\": "
      << inferlab_native_trace::trace_context_projection_json(
             plan.trace_context, 2)
      << ",\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0;
       index < plan.records.size(); ++index) {
    out << result_json(plan, plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << ",\n";
  out << "  \"recommended_next_commands\": [\n";
  out << "    " << inferlab_plan::json_string(kRefreshCommand)
      << "\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::cuda_quant_microkernel
