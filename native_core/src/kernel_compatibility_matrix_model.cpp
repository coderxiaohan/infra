#include "kernel_compatibility_matrix_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace inferlab::kernel_compatibility {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

constexpr double kWeightTolerance = 1.0e-9;

void require_unit_weight_sum(const double sum, const char* name) {
  const double finite_sum = require_finite(sum, name);
  if (std::abs(finite_sum - 1.0) > kWeightTolerance) {
    throw std::invalid_argument(std::string(name) + " must sum to one");
  }
}

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "kernel compatibility assumption profile");
  require_unit_interval(assumptions.candidate_shape_coverage_weight,
                        "candidate shape coverage weight");
  require_unit_interval(assumptions.candidate_dtype_coverage_weight,
                        "candidate dtype coverage weight");
  require_unit_interval(
      assumptions.candidate_inverse_operational_risk_weight,
      "candidate inverse operational risk weight");
  const double candidate_weight_sum = internal::checked_add_finite_double(
      internal::checked_add_finite_double(
          assumptions.candidate_shape_coverage_weight,
          assumptions.candidate_dtype_coverage_weight,
          "candidate score weight subtotal"),
      assumptions.candidate_inverse_operational_risk_weight,
      "candidate score weight total");
  require_unit_weight_sum(candidate_weight_sum,
                          "candidate selection weights");

  require_unit_interval(assumptions.compatibility_shape_coverage_weight,
                        "compatibility shape coverage weight");
  require_unit_interval(assumptions.compatibility_dtype_coverage_weight,
                        "compatibility dtype coverage weight");
  require_unit_interval(
      assumptions.compatibility_inverse_operational_risk_weight,
      "compatibility inverse operational risk weight");
  require_unit_interval(
      assumptions.compatibility_correctness_contract_weight,
      "compatibility correctness contract weight");
  require_unit_interval(
      assumptions.compatibility_cuda_profile_metadata_weight,
      "compatibility CUDA profile metadata weight");
  require_unit_interval(
      assumptions.compatibility_cpu_reference_metadata_weight,
      "compatibility CPU reference metadata weight");
  const double compatibility_base_weight =
      internal::checked_add_finite_double(
      internal::checked_add_finite_double(
          assumptions.compatibility_shape_coverage_weight,
          assumptions.compatibility_dtype_coverage_weight,
          "compatibility base weight shape/dtype subtotal"),
      internal::checked_add_finite_double(
          assumptions.compatibility_inverse_operational_risk_weight,
          assumptions.compatibility_correctness_contract_weight,
          "compatibility base weight risk/correctness subtotal"),
      "compatibility base weight total");
  require_unit_weight_sum(
      internal::checked_add_finite_double(
          compatibility_base_weight,
          assumptions.compatibility_cuda_profile_metadata_weight,
          "compatibility CUDA weight total"),
      "compatibility CUDA weights");
  const double cpu_weight_total = internal::checked_add_finite_double(
      compatibility_base_weight,
      assumptions.compatibility_cpu_reference_metadata_weight,
      "compatibility CPU weight total");
  if (cpu_weight_total <= 0.0 || cpu_weight_total > 1.0) {
    throw std::invalid_argument(
        "compatibility CPU weights must be positive and no greater than one");
  }
  if (assumptions.compatibility_cpu_reference_metadata_weight >
      assumptions.compatibility_cuda_profile_metadata_weight) {
    throw std::invalid_argument(
        "CPU reference metadata weight must not exceed CUDA profile metadata weight");
  }
  const double threshold = require_unit_interval(
      assumptions.target_profile_queue_threshold,
      "target profile queue threshold");
  if (threshold <= 0.0) {
    throw std::invalid_argument(
        "target profile queue threshold must be positive");
  }
  require_unique_nonempty_strings(
      assumptions.profiling_metadata_fields,
      "kernel compatibility profiling metadata fields");
  require_unique_nonempty_strings(
      assumptions.evidence_bridge_inputs,
      "kernel compatibility evidence bridge inputs");
}

void validate_extension_contract(
    const ExtensionOperatorContract& contract,
    const std::string& profile_operator_name) {
  require_nonempty(contract.name,
                   "compatibility extension operator name");
  require_nonempty(contract.operator_family,
                   "compatibility extension operator family");
  require_nonempty(contract.shape_signature,
                   "compatibility extension shape signature");
  require_nonempty(contract.dtype_layout,
                   "compatibility extension dtype/layout");
  require_nonempty(contract.backend_candidates,
                   "compatibility extension backend candidates");
  require_nonempty(contract.correctness_oracle,
                   "compatibility extension correctness oracle");
  if (contract.name != profile_operator_name) {
    throw std::invalid_argument(
        "compatibility extension identity must match its profile foreign key");
  }
}

ScenarioProfile validate_profile(ScenarioProfile profile) {
  require_nonempty(profile.operator_name,
                   "kernel compatibility operator foreign key");
  require_nonempty(profile.modeled_shape_signature,
                   "kernel compatibility modeled shape signature");
  require_nonempty(profile.modeled_dtype,
                   "kernel compatibility modeled dtype");
  require_nonempty(profile.layout,
                   "kernel compatibility layout");
  require_nonempty(profile.reference_artifact,
                   "kernel compatibility reference artifact");
  require_nonempty(profile.fallback_backend,
                   "kernel compatibility fallback backend");
  profile.estimated_flops = require_positive_finite(
      profile.estimated_flops,
      "kernel compatibility estimated FLOPs");
  profile.estimated_bytes = require_positive_finite(
      profile.estimated_bytes,
      "kernel compatibility estimated bytes");
  profile.tolerance = require_positive_finite(
      profile.tolerance,
      "kernel compatibility correctness tolerance");
  if (profile.candidates.empty()) {
    throw std::invalid_argument(
        "kernel compatibility profile must have backend candidates");
  }
  for (std::size_t index = 0; index < profile.candidates.size(); ++index) {
    auto& candidate = profile.candidates[index];
    require_nonempty(candidate.backend,
                     "kernel compatibility backend candidate");
    require_nonempty(candidate.source_product,
                     "kernel compatibility candidate source product");
    (void)candidate_support_state_name(candidate.support_state);
    candidate.shape_coverage = require_unit_interval(
        candidate.shape_coverage,
        "kernel compatibility candidate shape coverage");
    candidate.dtype_coverage = require_unit_interval(
        candidate.dtype_coverage,
        "kernel compatibility candidate dtype coverage");
    candidate.operational_risk = require_unit_interval(
        candidate.operational_risk,
        "kernel compatibility candidate operational risk");
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (profile.candidates[prior].backend == candidate.backend) {
        throw std::invalid_argument(
            "kernel compatibility backend candidates must be unique per operator");
      }
    }
  }
  return profile;
}

OperatorContractProjection resolve_operator_contract(
    const std::vector<inferlab_high_value::MicrobenchOperator>&
        canonical_operators,
    const ScenarioProfile& profile) {
  const auto canonical = std::find_if(
      canonical_operators.begin(), canonical_operators.end(),
      [&profile](const inferlab_high_value::MicrobenchOperator& op) {
        return op.name == profile.operator_name;
      });
  if (canonical != canonical_operators.end()) {
    if (profile.extension_contract.has_value()) {
      throw std::invalid_argument(
          "canonical microbenchmark profile must not redeclare an extension contract");
    }
    return {canonical->name,
            canonical->operator_family,
            canonical->shape_signature,
            canonical->dtype_layout,
            canonical->backend_candidates,
            canonical->correctness_oracle,
            canonical->benchmark_case,
            canonical->nvtx_range,
            canonical->nsight_systems_command,
            canonical->nsight_compute_command,
            RegistryMembership::canonical_microbenchmark_operator};
  }
  if (!profile.extension_contract.has_value()) {
    throw std::invalid_argument(
        "non-canonical compatibility profile requires an explicit extension contract");
  }
  validate_extension_contract(*profile.extension_contract,
                              profile.operator_name);
  const auto& extension = *profile.extension_contract;
  return {extension.name,
          extension.operator_family,
          extension.shape_signature,
          extension.dtype_layout,
          extension.backend_candidates,
          extension.correctness_oracle,
          "",
          "",
          "",
          "",
          RegistryMembership::compatibility_only_extension};
}

double candidate_selection_score(const Assumptions& assumptions,
                                 const BackendCandidateProfile& candidate) {
  double score = finite_product(
      candidate.shape_coverage,
      assumptions.candidate_shape_coverage_weight,
      "candidate shape score component");
  score = internal::checked_add_finite_double(
      score,
      finite_product(
          candidate.dtype_coverage,
          assumptions.candidate_dtype_coverage_weight,
          "candidate dtype score component"),
      "candidate shape/dtype score subtotal");
  score = internal::checked_add_finite_double(
      score,
      finite_product(
          1.0 - candidate.operational_risk,
          assumptions.candidate_inverse_operational_risk_weight,
          "candidate inverse-risk score component"),
      "candidate selection score");
  return require_unit_interval(score,
                               "candidate selection score");
}

double compatibility_score(const Assumptions& assumptions,
                           const BackendCandidateProfile& selected,
                           const bool requires_cuda) {
  double score = finite_product(
      selected.shape_coverage,
      assumptions.compatibility_shape_coverage_weight,
      "compatibility shape score component");
  score = internal::checked_add_finite_double(
      score,
      finite_product(
          selected.dtype_coverage,
          assumptions.compatibility_dtype_coverage_weight,
          "compatibility dtype score component"),
      "compatibility shape/dtype score subtotal");
  score = internal::checked_add_finite_double(
      score,
      finite_product(
          1.0 - selected.operational_risk,
          assumptions.compatibility_inverse_operational_risk_weight,
          "compatibility inverse-risk score component"),
      "compatibility coverage/risk score subtotal");
  score = internal::checked_add_finite_double(
      score,
      assumptions.compatibility_correctness_contract_weight,
      "compatibility correctness score subtotal");
  score = internal::checked_add_finite_double(
      score,
      requires_cuda
          ? assumptions.compatibility_cuda_profile_metadata_weight
          : assumptions.compatibility_cpu_reference_metadata_weight,
      "kernel compatibility score");
  return require_unit_interval(score,
                               "kernel compatibility score");
}

Result analyze_profile(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>&
        canonical_operators,
    const ScenarioProfile& input_profile) {
  Result result;
  result.scenario = validate_profile(input_profile);
  result.operator_contract = resolve_operator_contract(
      canonical_operators, result.scenario);
  result.candidates.reserve(result.scenario.candidates.size());
  for (const auto& candidate : result.scenario.candidates) {
    result.candidates.push_back(
        {candidate, candidate_selection_score(assumptions, candidate)});
  }
  result.selected_candidate_index = 0U;
  for (std::size_t index = 1U; index < result.candidates.size(); ++index) {
    if (result.candidates[index].selection_score >
        result.candidates[result.selected_candidate_index]
            .selection_score) {
      result.selected_candidate_index = index;
    }
  }
  const auto& selected =
      result.candidates[result.selected_candidate_index].profile;
  result.arithmetic_intensity = require_finite(
      result.scenario.estimated_flops /
          result.scenario.estimated_bytes,
      "kernel compatibility arithmetic intensity");
  result.compatibility_score = compatibility_score(
      assumptions, selected, result.scenario.requires_cuda);
  result.correctness_contract = {
      result.scenario.reference_artifact,
      result.operator_contract.correctness_oracle,
      result.scenario.tolerance,
      true,
      false};
  result.requires_target_machine_validation = true;
  result.ranking_applicable =
      result.operator_contract.registry_membership ==
      RegistryMembership::canonical_microbenchmark_operator;
  if (!result.ranking_applicable) {
    result.target_profile_queue_eligible = false;
    result.fallback_policy =
        FallbackPolicy::extension_registration_and_fallback_required;
    result.release_gate =
        ReleaseGate::register_extension_before_target_machine_profile_queue;
  } else if (result.compatibility_score >=
             assumptions.target_profile_queue_threshold) {
    result.target_profile_queue_eligible = true;
    result.fallback_policy = FallbackPolicy::
        fallback_only_when_target_backend_missing_or_shape_unsupported;
    result.release_gate = ReleaseGate::
        candidate_backend_can_enter_target_machine_profile_queue;
  } else {
    result.target_profile_queue_eligible = false;
    result.fallback_policy = FallbackPolicy::
        fallback_required_until_target_profile_and_correctness_exist;
    result.release_gate =
        ReleaseGate::keep_fallback_and_block_performance_claims;
  }
  return result;
}

}  // namespace

const char* candidate_support_state_name(
    const CandidateSupportState state) {
  switch (state) {
    case CandidateSupportState::available_reference:
      return "available_reference";
    case CandidateSupportState::requires_target_gpu_profile:
      return "requires_target_gpu_profile";
    case CandidateSupportState::requires_product_replay:
      return "requires_product_replay";
    case CandidateSupportState::requires_kernel_capability_probe:
      return "requires_kernel_capability_probe";
    case CandidateSupportState::prototype_only:
      return "prototype_only";
    case CandidateSupportState::requires_engine_metadata:
      return "requires_engine_metadata";
    case CandidateSupportState::requires_shape_matrix:
      return "requires_shape_matrix";
  }
  throw std::logic_error("unknown kernel candidate support state");
}

const char* registry_membership_name(
    const RegistryMembership membership) {
  switch (membership) {
    case RegistryMembership::canonical_microbenchmark_operator:
      return "canonical_microbenchmark_operator";
    case RegistryMembership::compatibility_only_extension:
      return "compatibility_only_extension";
  }
  throw std::logic_error("unknown kernel operator registry membership");
}

const char* fallback_policy_name(const FallbackPolicy policy) {
  switch (policy) {
    case FallbackPolicy::
        fallback_only_when_target_backend_missing_or_shape_unsupported:
      return "fallback_only_when_target_backend_missing_or_shape_unsupported";
    case FallbackPolicy::
        fallback_required_until_target_profile_and_correctness_exist:
      return "fallback_required_until_target_gpu_profile_and_correctness_contract_exist";
    case FallbackPolicy::extension_registration_and_fallback_required:
      return "extension_registration_and_fallback_required_before_profile_queue";
  }
  throw std::logic_error("unknown kernel compatibility fallback policy");
}

const char* release_gate_name(const ReleaseGate gate) {
  switch (gate) {
    case ReleaseGate::
        candidate_backend_can_enter_target_machine_profile_queue:
      return "candidate_backend_can_enter_target_machine_profile_queue";
    case ReleaseGate::keep_fallback_and_block_performance_claims:
      return "keep_fallback_and_block_performance_claims";
    case ReleaseGate::
        register_extension_before_target_machine_profile_queue:
      return "register_extension_before_target_machine_profile_queue";
  }
  throw std::logic_error("unknown kernel compatibility release gate");
}

const char* matrix_decision_name(const MatrixDecision decision) {
  switch (decision) {
    case MatrixDecision::target_machine_validation_required:
      return "target_machine_validation_required";
    case MatrixDecision::
        extension_registry_and_target_machine_validation_required:
      return "extension_registry_and_target_machine_validation_required";
  }
  throw std::logic_error("unknown kernel compatibility matrix decision");
}

Assumptions default_assumptions() {
  return {
      "compiled_kernel_compatibility_analytical_fixture_v2",
      0.45,
      0.35,
      0.20,
      0.36,
      0.30,
      0.18,
      0.10,
      0.06,
      0.04,
      0.74,
      {"operator", "shape_signature", "dtype", "layout",
       "backend", "elapsed_ms", "bytes", "flops",
       "fallback_reason", "product_version"},
      {"results/raw/native_cuda_quant_microkernel_deep_dive.json",
       "results/raw/product_flashinfer_kernel_profile.json",
       "results/raw/product_tensorrtllm_runtime_profile.json",
       "results/raw/product_ktransformers_hybrid_profile.json",
       "results/raw/gguf_probe.json"}};
}

std::vector<ScenarioProfile> default_profiles() {
  using State = CandidateSupportState;
  return {
      {"rmsnorm",
       "batch=16,hidden=4096",
       "fp16/fp32_accum",
       "row_major",
       "results/raw/decoder_probe.json",
       "cpu_reference_rmsnorm",
       16.0 * 4096.0 * 4.0,
       16.0 * 4096.0 * 6.0,
       1.0e-4,
       false,
       {{"native_cpu_reference", "InferLab", State::available_reference,
         1.00, 0.90, 0.10},
        {"tensorrt_llm_plugin", "TensorRT-LLM",
         State::requires_target_gpu_profile, 0.82, 0.88, 0.22}},
       std::nullopt},
      {"rope",
       "tokens=8192,heads=32,head_dim=128",
       "fp16",
       "interleaved_or_neox",
       "results/raw/attention_probe.json",
       "cpu_reference_rope",
       8192.0 * 32.0 * 128.0 * 6.0,
       8192.0 * 32.0 * 128.0 * 4.0,
       1.0e-4,
       false,
       {{"native_cpu_reference", "InferLab", State::available_reference,
         1.00, 0.88, 0.12},
        {"vllm_sglang_backend", "vLLM/SGLang",
         State::requires_product_replay, 0.84, 0.86, 0.20}},
       std::nullopt},
      {"row_softmax",
       "rows=4096,cols=4096,causal=true",
       "fp16/fp32_accum",
       "row_major",
       "results/raw/attention_probe.json",
       "cpu_reference_softmax",
       4096.0 * 4096.0 * 5.0,
       4096.0 * 4096.0 * 4.0,
       2.0e-4,
       true,
       {{"flashinfer_softmax_or_attention", "FlashInfer",
         State::requires_kernel_capability_probe, 0.78, 0.82, 0.26},
        {"native_cuda_skeleton", "InferLab", State::prototype_only,
         0.42, 0.62, 0.55}},
       std::nullopt},
      {"int4_dequant_matvec",
       "out=4096,in=4096,group=128",
       "int4_to_fp16",
       "grouped_weight_only",
       "results/raw/quantization_probe.json",
       "cpu_reference_int4_dequant",
       4096.0 * 4096.0 * 2.0,
       4096.0 * 4096.0 * 0.75,
       3.0e-3,
       true,
       {{"marlin_int4", "Marlin/CUTLASS",
         State::requires_target_gpu_profile, 0.72, 0.76, 0.32},
        {"tensorrt_llm_quant", "TensorRT-LLM",
         State::requires_engine_metadata, 0.80, 0.88, 0.25}},
       std::nullopt},
      {"paged_kv_gather",
       "tokens=32768,heads=32,head_dim=128,page=16",
       "fp16",
       "block_table_paged",
       "results/raw/paged_kv_locality_probe.json",
       "cpu_reference_paged_kv",
       32768.0 * 32.0 * 128.0,
       32768.0 * 32.0 * 128.0 * 2.0,
       1.0e-4,
       true,
       {{"flashinfer_paged_kv", "FlashInfer",
         State::requires_shape_matrix, 0.82, 0.86, 0.24},
        {"vllm_paged_attention", "vLLM",
         State::requires_product_replay, 0.90, 0.84, 0.20}},
       std::nullopt},
      {"decode_attention",
       "batch=64,kv_tokens=8192,heads=32,head_dim=128",
       "fp16",
       "paged_or_ragged_kv",
       "results/raw/attention_probe.json",
       "cpu_reference_attention",
       64.0 * 8192.0 * 32.0 * 128.0 * 2.0,
       64.0 * 8192.0 * 32.0 * 128.0 * 4.0,
       2.0e-4,
       true,
       {{"flashinfer_decode", "FlashInfer",
         State::requires_kernel_capability_probe, 0.86, 0.84, 0.23},
        {"tensorrt_llm_attention", "TensorRT-LLM",
         State::requires_engine_metadata, 0.74, 0.82, 0.28}},
       std::nullopt},
      {"prefill_attention",
       "batch=8,seqlen=8192,heads=32,head_dim=128",
       "fp16/bf16",
       "ragged_or_paged_kv",
       "results/raw/attention_probe.json",
       "cpu_reference_attention",
       8.0 * 8192.0 * 8192.0 * 32.0 * 128.0 * 2.0,
       8.0 * 8192.0 * 32.0 * 128.0 * 6.0,
       2.0e-4,
       true,
       {{"flashinfer_prefill", "FlashInfer",
         State::requires_shape_matrix, 0.82, 0.84, 0.24},
        {"vllm_attention_backend", "vLLM",
         State::requires_product_replay, 0.78, 0.82, 0.25}},
       ExtensionOperatorContract{
           "prefill_attention",
           "prefill_attention",
           "batch={1,8}, seqlen={128,2048,8192}, head_dim={64,128}",
           "fp16/bf16 ragged or paged KV",
           "FlashInfer prefill and vLLM attention backend candidates",
           "native_core attention reference for prefill"}},
  };
}

Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>&
        canonical_operators,
    const std::vector<ScenarioProfile>& profiles) {
  validate_assumptions(assumptions);
  inferlab_high_value::validate_microbench_operator_registry(
      canonical_operators);
  if (profiles.empty()) {
    throw std::invalid_argument(
        "kernel compatibility profiles must not be empty");
  }
  for (std::size_t index = 0; index < profiles.size(); ++index) {
    require_nonempty(profiles[index].operator_name,
                     "kernel compatibility profile operator");
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (profiles[prior].operator_name == profiles[index].operator_name) {
        throw std::invalid_argument(
            "kernel compatibility profile operator identities must be unique");
      }
    }
  }
  for (const auto& canonical : canonical_operators) {
    const int count = internal::checked_count_if(
        profiles.begin(), profiles.end(),
        [&canonical](const ScenarioProfile& profile) {
          return profile.operator_name == canonical.name;
        },
        "kernel compatibility canonical profile match count");
    if (count != 1) {
      throw std::invalid_argument(
          "kernel compatibility profiles must cover each canonical microbenchmark operator exactly once");
    }
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.records.reserve(profiles.size());
  bool highest_initialized = false;
  for (const auto& profile : profiles) {
    plan.records.push_back(analyze_profile(
        assumptions, canonical_operators, profile));
    const std::size_t index = plan.records.size() - 1U;
    const auto& result = plan.records[index];
    if (result.operator_contract.registry_membership ==
        RegistryMembership::canonical_microbenchmark_operator) {
      plan.canonical_operator_count = internal::checked_add_size(
          plan.canonical_operator_count, 1U,
          "kernel compatibility canonical operator count");
    } else {
      plan.extension_operator_count = internal::checked_add_size(
          plan.extension_operator_count, 1U,
          "kernel compatibility extension operator count");
    }
    if (result.ranking_applicable) {
      plan.comparable_record_count = internal::checked_add_size(
          plan.comparable_record_count, 1U,
          "kernel compatibility comparable record count");
      if (!highest_initialized ||
          result.compatibility_score >
              plan.records[plan.highest_compatibility_record_index]
                  .compatibility_score) {
        plan.highest_compatibility_record_index = index;
        highest_initialized = true;
      }
    }
    if (result.target_profile_queue_eligible) {
      plan.profile_queue_eligible_count = internal::checked_add_size(
          plan.profile_queue_eligible_count, 1U,
          "kernel compatibility profile queue eligible count");
    }
    if (result.requires_target_machine_validation) {
      plan.target_machine_validation_required_count =
          internal::checked_add_size(
              plan.target_machine_validation_required_count, 1U,
              "kernel compatibility target validation count");
    }
  }
  if (!highest_initialized || plan.comparable_record_count == 0U) {
    throw std::invalid_argument(
        "kernel compatibility matrix requires at least one canonical comparable record");
  }
  if (plan.canonical_operator_count != canonical_operators.size()) {
    throw std::invalid_argument(
        "kernel compatibility canonical profile count drift");
  }
  const std::size_t classified_count = internal::checked_add_size(
      plan.canonical_operator_count, plan.extension_operator_count,
      "kernel compatibility classified operator count");
  if (classified_count != plan.records.size()) {
    throw std::invalid_argument(
        "kernel compatibility operator membership count drift");
  }
  plan.decision = plan.extension_operator_count == 0U
                      ? MatrixDecision::target_machine_validation_required
                      : MatrixDecision::
                            extension_registry_and_target_machine_validation_required;
  return plan;
}

}  // namespace inferlab::kernel_compatibility
