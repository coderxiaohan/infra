#include "kernel_compatibility_matrix_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace inferlab::kernel_compatibility {
namespace {

constexpr const char* kArtifactType =
    "native_kernel_compatibility_matrix";
constexpr const char* kArtifactPath =
    "results/raw/native_kernel_compatibility_matrix.json";
constexpr const char* kTargetName =
    "inferlab_kernel_compatibility_matrix";
constexpr const char* kRefreshCommand =
    "./build/inferlab_kernel_compatibility_matrix --repo-root . --output results/raw/native_kernel_compatibility_matrix.json";
constexpr const char* kClaimScope =
    "compiled kernel compatibility profiles, canonical-registry joins, candidate selection, fallback contracts, and target-machine migration guidance only; not observed backend support, CUDA correctness, timing, profiling, product replay, execute, benchmark, performance ranking, or release evidence";

const std::vector<std::string>& product_alignment() {
  static const std::vector<std::string> products = {
      "FlashInfer", "TensorRT-LLM", "vLLM", "SGLang",
      "KTransformers", "llama.cpp", "CUTLASS", "Marlin"};
  return products;
}

const std::vector<std::string>& research_basis() {
  static const std::vector<std::string> basis = {
      "FlashInfer paged/ragged attention",
      "TensorRT-LLM engine metadata",
      "CUTLASS/Marlin low-bit kernels",
      "llama.cpp quant references",
      "InferLab CPU references and CUDA skeletons"};
  return basis;
}

std::string joined_metadata_fields(
    const std::vector<std::string>& fields) {
  if (fields.empty()) {
    throw std::invalid_argument(
        "kernel compatibility view requires profiling metadata fields");
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
      indent, 2, "kernel compatibility assumptions JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"candidate_score_weights\": {\n";
  out << nested << "  \"dtype_coverage\": "
      << inferlab_plan::fixed6(
             assumptions.candidate_dtype_coverage_weight)
      << ",\n";
  out << nested << "  \"inverse_operational_risk\": "
      << inferlab_plan::fixed6(
             assumptions.candidate_inverse_operational_risk_weight)
      << ",\n";
  out << nested << "  \"shape_coverage\": "
      << inferlab_plan::fixed6(
             assumptions.candidate_shape_coverage_weight)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"compatibility_score_weights\": {\n";
  out << nested << "  \"correctness_contract\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_correctness_contract_weight)
      << ",\n";
  out << nested << "  \"cpu_reference_metadata\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_cpu_reference_metadata_weight)
      << ",\n";
  out << nested << "  \"cuda_profile_metadata\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_cuda_profile_metadata_weight)
      << ",\n";
  out << nested << "  \"dtype_coverage\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_dtype_coverage_weight)
      << ",\n";
  out << nested << "  \"inverse_operational_risk\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_inverse_operational_risk_weight)
      << ",\n";
  out << nested << "  \"shape_coverage\": "
      << inferlab_plan::fixed6(
             assumptions.compatibility_shape_coverage_weight)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << ",\n";
  out << nested << "\"profiling_metadata_fields\": "
      << inferlab_plan::json_string_array(
             assumptions.profiling_metadata_fields, nested_indent)
      << ",\n";
  out << nested << "\"score_scope\": "
      << inferlab_plan::json_string(
             "compiled fixture coverage and risk assumptions only; not runtime compatibility or readiness")
      << ",\n";
  out << nested << "\"target_profile_queue_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.target_profile_queue_threshold)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string candidate_json(const CandidateResult& candidate,
                           const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility candidate JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(candidate.profile.backend) << ",\n";
  out << nested << "\"candidate_selection_score\": "
      << inferlab_plan::fixed6(candidate.selection_score) << ",\n";
  out << nested << "\"candidate_selection_score_scope\": "
      << inferlab_plan::json_string(
             "compiled shape/dtype/risk fixture only")
      << ",\n";
  out << nested << "\"dtype_coverage\": "
      << inferlab_plan::fixed6(candidate.profile.dtype_coverage)
      << ",\n";
  out << nested << "\"operational_risk\": "
      << inferlab_plan::fixed6(candidate.profile.operational_risk)
      << ",\n";
  out << nested << "\"shape_coverage\": "
      << inferlab_plan::fixed6(candidate.profile.shape_coverage)
      << ",\n";
  out << nested << "\"source_product\": "
      << inferlab_plan::json_string(candidate.profile.source_product)
      << ",\n";
  out << nested << "\"support_state\": "
      << inferlab_plan::json_string(candidate_support_state_name(
             candidate.profile.support_state))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string candidates_json(const std::vector<CandidateResult>& candidates,
                            const int indent) {
  if (candidates.empty()) {
    throw std::invalid_argument(
        "kernel compatibility view requires backend candidates");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility candidate array indent");
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    out << candidate_json(candidates[index], nested_indent)
        << (index + 1U == candidates.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string optional_operator_metadata_json(const std::string& value) {
  return value.empty() ? "null" : inferlab_plan::json_string(value);
}

std::string operator_contract_json(
    const OperatorContractProjection& contract,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility operator contract JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const bool canonical =
      contract.registry_membership ==
      RegistryMembership::canonical_microbenchmark_operator;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend_candidates\": "
      << inferlab_plan::json_string(contract.backend_candidates) << ",\n";
  out << nested << "\"benchmark_case\": "
      << optional_operator_metadata_json(contract.benchmark_case) << ",\n";
  out << nested << "\"canonical_registry_member\": "
      << inferlab_plan::json_bool(canonical) << ",\n";
  out << nested << "\"correctness_oracle\": "
      << inferlab_plan::json_string(contract.correctness_oracle) << ",\n";
  out << nested << "\"dtype_layout\": "
      << inferlab_plan::json_string(contract.dtype_layout) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(contract.name) << ",\n";
  out << nested << "\"nsight_compute_command\": "
      << optional_operator_metadata_json(contract.nsight_compute_command)
      << ",\n";
  out << nested << "\"nsight_systems_command\": "
      << optional_operator_metadata_json(contract.nsight_systems_command)
      << ",\n";
  out << nested << "\"nvtx_range\": "
      << optional_operator_metadata_json(contract.nvtx_range) << ",\n";
  out << nested << "\"operator_family\": "
      << inferlab_plan::json_string(contract.operator_family) << ",\n";
  out << nested << "\"registry_membership\": "
      << inferlab_plan::json_string(
             registry_membership_name(contract.registry_membership))
      << ",\n";
  out << nested << "\"registry_owner\": "
      << inferlab_plan::json_string(
             canonical
                 ? "inferlab_high_value::microbench_operators"
                 : "kernel_compatibility_matrix compatibility-only extension profile")
      << ",\n";
  out << nested << "\"shape_signature\": "
      << inferlab_plan::json_string(contract.shape_signature) << "\n";
  out << pad << "}";
  return out.str();
}

std::string correctness_contract_json(
    const CorrectnessContract& contract,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility correctness JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"absolute_or_relative_comparison\": "
      << inferlab_plan::json_bool(
             contract.absolute_or_relative_comparison)
      << ",\n";
  out << nested << "\"correctness_oracle\": "
      << inferlab_plan::json_string(contract.correctness_oracle) << ",\n";
  out << nested << "\"reference_artifact\": "
      << inferlab_plan::json_string(contract.reference_artifact) << ",\n";
  out << nested << "\"timing_claim_allowed_before_pass\": "
      << inferlab_plan::json_bool(
             contract.timing_claim_allowed_before_pass)
      << ",\n";
  out << nested << "\"tolerance\": "
      << inferlab_plan::fixed6(contract.tolerance) << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const Result& result, const int indent) {
  const auto& scenario = result.scenario;
  const auto& contract = result.operator_contract;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility scenario JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend_candidates\": "
      << candidates_json(result.candidates, nested_indent) << ",\n";
  out << nested << "\"canonical_operator_contract\": "
      << operator_contract_json(contract, nested_indent) << ",\n";
  out << nested << "\"correctness_reference\": "
      << inferlab_plan::json_string(scenario.reference_artifact) << ",\n";
  out << nested << "\"dtype\": "
      << inferlab_plan::json_string(scenario.modeled_dtype) << ",\n";
  out << nested << "\"estimated_bytes\": "
      << inferlab_plan::fixed6(scenario.estimated_bytes) << ",\n";
  out << nested << "\"estimated_flops\": "
      << inferlab_plan::fixed6(scenario.estimated_flops) << ",\n";
  out << nested << "\"estimate_basis\": "
      << inferlab_plan::json_string(
             "compiled operator-specific analytical fixture; not measured counters")
      << ",\n";
  out << nested << "\"fallback_backend\": "
      << inferlab_plan::json_string(scenario.fallback_backend) << ",\n";
  out << nested << "\"layout\": "
      << inferlab_plan::json_string(scenario.layout) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(scenario.operator_name) << ",\n";
  out << nested << "\"operator_family\": "
      << inferlab_plan::json_string(contract.operator_family) << ",\n";
  out << nested << "\"registry_membership\": "
      << inferlab_plan::json_string(
             registry_membership_name(contract.registry_membership))
      << ",\n";
  out << nested << "\"requires_cuda\": "
      << inferlab_plan::json_bool(scenario.requires_cuda) << ",\n";
  out << nested << "\"shape_signature\": "
      << inferlab_plan::json_string(scenario.modeled_shape_signature)
      << ",\n";
  out << nested << "\"tolerance\": "
      << inferlab_plan::fixed6(scenario.tolerance) << "\n";
  out << pad << "}";
  return out.str();
}

std::string legacy_correctness_contract(
    const CorrectnessContract& contract) {
  return "compare_against_" + contract.reference_artifact +
         "_with_abs_or_rel_tolerance_" +
         inferlab_plan::fixed6(contract.tolerance);
}

std::string result_json(const Plan& plan,
                        const Result& result,
                        const int indent) {
  if (result.selected_candidate_index >= result.candidates.size()) {
    throw std::invalid_argument(
        "kernel compatibility selected candidate index is invalid");
  }
  const auto& selected =
      result.candidates[result.selected_candidate_index];
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel compatibility result JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"arithmetic_intensity_flops_per_byte\": "
      << inferlab_plan::fixed6(result.arithmetic_intensity) << ",\n";
  out << nested << "\"arithmetic_intensity_scope\": "
      << inferlab_plan::json_string(
             "estimated_flops_divided_by_estimated_bytes; not observed hardware counters")
      << ",\n";
  out << nested << "\"compatibility_score\": "
      << inferlab_plan::fixed6(result.compatibility_score) << ",\n";
  out << nested << "\"compatibility_score_scope\": "
      << inferlab_plan::json_string(
             "compiled shape/dtype/risk/correctness metadata fixture; not runtime compatibility")
      << ",\n";
  out << nested << "\"correctness_contract\": "
      << inferlab_plan::json_string(
             legacy_correctness_contract(result.correctness_contract))
      << ",\n";
  out << nested << "\"correctness_contract_detail\": "
      << correctness_contract_json(result.correctness_contract,
                                   nested_indent)
      << ",\n";
  out << nested << "\"fallback_backend\": "
      << inferlab_plan::json_string(result.scenario.fallback_backend)
      << ",\n";
  out << nested << "\"fallback_reason\": "
      << inferlab_plan::json_string(
             fallback_policy_name(result.fallback_policy))
      << ",\n";
  out << nested << "\"migration_command\": "
      << inferlab_plan::json_string(kRefreshCommand) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(result.scenario.operator_name)
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
      << inferlab_plan::json_bool(result.ranking_applicable) << ",\n";
  out << nested << "\"release_gate\": "
      << inferlab_plan::json_string(
             release_gate_name(result.release_gate))
      << ",\n";
  out << nested << "\"requires_target_machine_validation\": "
      << inferlab_plan::json_bool(
             result.requires_target_machine_validation)
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result, nested_indent) << ",\n";
  out << nested << "\"selected_backend\": "
      << candidate_json(selected, nested_indent) << ",\n";
  out << nested << "\"target_profile_queue_eligible\": "
      << inferlab_plan::json_bool(
             result.target_profile_queue_eligible)
      << "\n";
  out << pad << "}";
  return out.str();
}

void validate_plan_for_projection(const Plan& plan) {
  if (plan.records.empty() || plan.comparable_record_count == 0U ||
      plan.highest_compatibility_record_index >= plan.records.size() ||
      plan.canonical_operator_count == 0U ||
      plan.target_machine_validation_required_count >
          plan.records.size() ||
      plan.profile_queue_eligible_count > plan.canonical_operator_count ||
      plan.assumptions.profiling_metadata_fields.empty() ||
      plan.assumptions.evidence_bridge_inputs.empty()) {
    throw std::invalid_argument(
        "kernel compatibility view requires a complete typed plan");
  }
  const std::size_t classified_count = internal::checked_add_size(
      plan.canonical_operator_count, plan.extension_operator_count,
      "kernel compatibility projection classified count");
  if (classified_count != plan.records.size() ||
      plan.comparable_record_count != plan.canonical_operator_count ||
      !plan.records[plan.highest_compatibility_record_index]
           .ranking_applicable) {
    throw std::invalid_argument(
        "kernel compatibility view received inconsistent plan counts or ranking index");
  }
  for (const auto& result : plan.records) {
    if (result.candidates.empty() ||
        result.selected_candidate_index >= result.candidates.size()) {
      throw std::invalid_argument(
          "kernel compatibility view received an invalid candidate projection");
    }
  }
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan_for_projection(plan);
  const auto& highest =
      plan.records[plan.highest_compatibility_record_index];
  const auto& selected =
      highest.candidates[highest.selected_candidate_index];

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
  out << "  \"deep_dive_type\": \"kernel_compatibility_matrix\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"fixture_basis\": "
      << inferlab_plan::json_string(
             "compiled analytical profiles joined to the validated canonical microbenchmark registry; explicit extensions are not harness operators")
      << ",\n";
  out << "  \"canonical_operator_registry\": "
      << inferlab_plan::json_string(
             "inferlab_high_value::microbench_operators")
      << ",\n";
  out << "  \"extension_policy\": "
      << inferlab_plan::json_string(
             "compatibility-only extensions remain non-rankable and cannot enter the target profile queue until canonical registry and harness lifecycle are added")
      << ",\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(research_basis(), 2) << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(product_alignment(), 2)
      << ",\n";
  out << "  \"model_assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_operator\": "
      << inferlab_plan::json_string(highest.scenario.operator_name)
      << ",\n";
  out << "    \"best_selected_backend\": "
      << inferlab_plan::json_string(selected.profile.backend) << ",\n";
  out << "    \"best_compatibility_score\": "
      << inferlab_plan::fixed6(highest.compatibility_score) << ",\n";
  out << "    \"best_compatibility_score_scope\": "
      << inferlab_plan::json_string(
             "stable first highest canonical compiled fixture record only; not a cross-operator performance ranking")
      << ",\n";
  out << "    \"canonical_operator_count\": "
      << plan.canonical_operator_count << ",\n";
  out << "    \"comparable_record_count\": "
      << plan.comparable_record_count << ",\n";
  out << "    \"decision\": "
      << inferlab_plan::json_string(
             matrix_decision_name(plan.decision))
      << ",\n";
  out << "    \"extension_operator_count\": "
      << plan.extension_operator_count << ",\n";
  out << "    \"operator_count\": " << plan.records.size() << ",\n";
  out << "    \"profile_queue_eligible_count\": "
      << plan.profile_queue_eligible_count << ",\n";
  out << "    \"target_machine_validation_required_count\": "
      << plan.target_machine_validation_required_count << "\n";
  out << "  },\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(
             plan.assumptions.evidence_bridge_inputs, 2)
      << ",\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << result_json(plan, plan.records[index], 4)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << ",\n";
  out << "  \"recommended_next_commands\": [\n";
  out << "    " << inferlab_plan::json_string(kRefreshCommand) << "\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kernel_compatibility
