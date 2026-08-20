#pragma once

#include "high_value_artifact_catalog.hpp"
#include "native_trace_context.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::cuda_quant_microkernel {

enum class BottleneckModel {
  memory_bandwidth_or_gather_locality,
  mixed_memory_compute,
  compute_or_launch_latency,
};

const char* bottleneck_model_name(BottleneckModel model);

struct Assumptions {
  std::string profile_name =
      "compiled_cuda_quant_microkernel_analytical_fixture_v1";
  double memory_bound_intensity_threshold = 2.0;
  double compute_bound_intensity_threshold = 8.0;
  double intensity_priority_reference = 8.0;
  double static_contract_score_weight = 0.70;
  double intensity_priority_score_weight = 0.30;
  std::vector<std::string> profiling_metadata_fields;
  std::vector<std::string> additional_evidence_inputs;
};

struct AnalyticalProfile {
  std::string canonical_operator;
  std::string legacy_scenario_name;
  std::string modeled_shape_signature;
  std::string reference_artifact;
  std::string fallback_backend;
  double estimated_flops = 0.0;
  double estimated_bytes = 0.0;
  double correctness_tolerance = 0.0;
};

struct Result {
  AnalyticalProfile profile;
  inferlab_high_value::MicrobenchOperator operator_contract;
  double arithmetic_intensity = 0.0;
  std::size_t static_contract_check_count = 0;
  std::size_t static_contract_pass_count = 0;
  double static_contract_coverage_score = 0.0;
  double analytical_priority_score = 0.0;
  BottleneckModel bottleneck_model =
      BottleneckModel::memory_bandwidth_or_gather_locality;
  bool requires_target_gpu_validation = true;
  bool ranking_applicable = false;
  std::string correctness_contract;
  std::string fallback_contract;
  std::string optimization_focus;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Result> records;
  std::vector<std::string> evidence_bridge_inputs;
  inferlab_native_trace::Projection trace_context;
  std::size_t comparable_record_count = 0;
  std::size_t highest_intensity_record_index = 0;
  std::size_t highest_priority_record_index = 0;
};

Assumptions default_assumptions();
std::vector<AnalyticalProfile> default_profiles();
Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators,
    const std::vector<AnalyticalProfile>& profiles);
Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators,
    const std::vector<AnalyticalProfile>& profiles,
    inferlab_native_trace::Source trace_source);

}  // namespace inferlab::cuda_quant_microkernel
