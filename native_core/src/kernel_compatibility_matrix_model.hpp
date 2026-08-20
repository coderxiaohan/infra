#pragma once

#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inferlab::kernel_compatibility {

enum class CandidateSupportState {
  available_reference,
  requires_target_gpu_profile,
  requires_product_replay,
  requires_kernel_capability_probe,
  prototype_only,
  requires_engine_metadata,
  requires_shape_matrix,
};

enum class RegistryMembership {
  canonical_microbenchmark_operator,
  compatibility_only_extension,
};

enum class FallbackPolicy {
  fallback_only_when_target_backend_missing_or_shape_unsupported,
  fallback_required_until_target_profile_and_correctness_exist,
  extension_registration_and_fallback_required,
};

enum class ReleaseGate {
  candidate_backend_can_enter_target_machine_profile_queue,
  keep_fallback_and_block_performance_claims,
  register_extension_before_target_machine_profile_queue,
};

enum class MatrixDecision {
  target_machine_validation_required,
  extension_registry_and_target_machine_validation_required,
};

const char* candidate_support_state_name(CandidateSupportState state);
const char* registry_membership_name(RegistryMembership membership);
const char* fallback_policy_name(FallbackPolicy policy);
const char* release_gate_name(ReleaseGate gate);
const char* matrix_decision_name(MatrixDecision decision);

struct Assumptions {
  std::string profile_name;
  double candidate_shape_coverage_weight = 0.0;
  double candidate_dtype_coverage_weight = 0.0;
  double candidate_inverse_operational_risk_weight = 0.0;
  double compatibility_shape_coverage_weight = 0.0;
  double compatibility_dtype_coverage_weight = 0.0;
  double compatibility_inverse_operational_risk_weight = 0.0;
  double compatibility_correctness_contract_weight = 0.0;
  double compatibility_cuda_profile_metadata_weight = 0.0;
  double compatibility_cpu_reference_metadata_weight = 0.0;
  double target_profile_queue_threshold = 0.0;
  std::vector<std::string> profiling_metadata_fields;
  std::vector<std::string> evidence_bridge_inputs;
};

struct BackendCandidateProfile {
  std::string backend;
  std::string source_product;
  CandidateSupportState support_state =
      CandidateSupportState::prototype_only;
  double shape_coverage = 0.0;
  double dtype_coverage = 0.0;
  double operational_risk = 0.0;
};

struct ExtensionOperatorContract {
  std::string name;
  std::string operator_family;
  std::string shape_signature;
  std::string dtype_layout;
  std::string backend_candidates;
  std::string correctness_oracle;
};

struct ScenarioProfile {
  std::string operator_name;
  std::string modeled_shape_signature;
  std::string modeled_dtype;
  std::string layout;
  std::string reference_artifact;
  std::string fallback_backend;
  double estimated_flops = 0.0;
  double estimated_bytes = 0.0;
  double tolerance = 0.0;
  bool requires_cuda = false;
  std::vector<BackendCandidateProfile> candidates;
  std::optional<ExtensionOperatorContract> extension_contract;
};

struct OperatorContractProjection {
  std::string name;
  std::string operator_family;
  std::string shape_signature;
  std::string dtype_layout;
  std::string backend_candidates;
  std::string correctness_oracle;
  std::string benchmark_case;
  std::string nvtx_range;
  std::string nsight_systems_command;
  std::string nsight_compute_command;
  RegistryMembership registry_membership =
      RegistryMembership::compatibility_only_extension;
};

struct CandidateResult {
  BackendCandidateProfile profile;
  double selection_score = 0.0;
};

struct CorrectnessContract {
  std::string reference_artifact;
  std::string correctness_oracle;
  double tolerance = 0.0;
  bool absolute_or_relative_comparison = true;
  bool timing_claim_allowed_before_pass = false;
};

struct Result {
  ScenarioProfile scenario;
  OperatorContractProjection operator_contract;
  std::vector<CandidateResult> candidates;
  std::size_t selected_candidate_index = 0;
  CorrectnessContract correctness_contract;
  double arithmetic_intensity = 0.0;
  double compatibility_score = 0.0;
  bool target_profile_queue_eligible = false;
  bool ranking_applicable = false;
  bool requires_target_machine_validation = true;
  FallbackPolicy fallback_policy =
      FallbackPolicy::fallback_required_until_target_profile_and_correctness_exist;
  ReleaseGate release_gate =
      ReleaseGate::keep_fallback_and_block_performance_claims;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Result> records;
  std::size_t canonical_operator_count = 0;
  std::size_t extension_operator_count = 0;
  std::size_t comparable_record_count = 0;
  std::size_t profile_queue_eligible_count = 0;
  std::size_t target_machine_validation_required_count = 0;
  std::size_t highest_compatibility_record_index = 0;
  MatrixDecision decision =
      MatrixDecision::target_machine_validation_required;
};

Assumptions default_assumptions();
std::vector<ScenarioProfile> default_profiles();
Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>&
        canonical_operators,
    const std::vector<ScenarioProfile>& profiles);

}  // namespace inferlab::kernel_compatibility
