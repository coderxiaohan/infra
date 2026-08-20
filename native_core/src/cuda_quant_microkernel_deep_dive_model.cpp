#include "cuda_quant_microkernel_deep_dive_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace inferlab::cuda_quant_microkernel {
namespace {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "CUDA/quant assumption profile");
  require_positive_finite(
      assumptions.memory_bound_intensity_threshold,
      "CUDA/quant memory-bound intensity threshold");
  require_positive_finite(
      assumptions.compute_bound_intensity_threshold,
      "CUDA/quant compute-bound intensity threshold");
  if (assumptions.compute_bound_intensity_threshold <=
      assumptions.memory_bound_intensity_threshold) {
    throw std::invalid_argument(
        "CUDA/quant compute-bound threshold must exceed the memory-bound threshold");
  }
  require_positive_finite(
      assumptions.intensity_priority_reference,
      "CUDA/quant intensity priority reference");
  require_unit_interval(
      assumptions.static_contract_score_weight,
      "CUDA/quant static-contract score weight");
  require_unit_interval(
      assumptions.intensity_priority_score_weight,
      "CUDA/quant intensity-priority score weight");
  const double total_weight = internal::checked_add_finite_double(
      assumptions.static_contract_score_weight,
      assumptions.intensity_priority_score_weight,
      "CUDA/quant analytical priority weight total");
  if (total_weight < 1.0 - 1e-9 || total_weight > 1.0 + 1e-9) {
    throw std::invalid_argument(
        "CUDA/quant analytical priority weights must sum to one");
  }
  require_unique_nonempty_strings(
      assumptions.profiling_metadata_fields,
      "CUDA/quant profiling metadata fields");
  require_unique_nonempty_strings(
      assumptions.additional_evidence_inputs,
      "CUDA/quant evidence inputs");
}

AnalyticalProfile validate_profile(AnalyticalProfile profile) {
  require_nonempty(profile.canonical_operator,
                   "CUDA/quant canonical operator");
  require_nonempty(profile.legacy_scenario_name,
                   "CUDA/quant legacy scenario name");
  require_nonempty(profile.modeled_shape_signature,
                   "CUDA/quant modeled shape signature");
  require_nonempty(profile.reference_artifact,
                   "CUDA/quant reference artifact");
  require_nonempty(profile.fallback_backend,
                   "CUDA/quant fallback backend");
  profile.estimated_flops = require_positive_finite(
      profile.estimated_flops, "CUDA/quant estimated FLOPs");
  profile.estimated_bytes = require_positive_finite(
      profile.estimated_bytes, "CUDA/quant estimated bytes");
  profile.correctness_tolerance = require_positive_finite(
      profile.correctness_tolerance,
      "CUDA/quant correctness tolerance");
  return profile;
}

const AnalyticalProfile& require_profile_for_operator(
    const std::vector<AnalyticalProfile>& profiles,
    const std::string& canonical_operator_name) {
  const auto match = std::find_if(
      profiles.begin(), profiles.end(),
      [&canonical_operator_name](const AnalyticalProfile& profile) {
        return profile.canonical_operator == canonical_operator_name;
      });
  if (match == profiles.end()) {
    throw std::invalid_argument(
        "CUDA/quant analytical profiles must cover every canonical microbenchmark operator");
  }
  return *match;
}

std::string tolerance_text(const double tolerance) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << tolerance;
  return out.str();
}

BottleneckModel classify_bottleneck(
    const Assumptions& assumptions,
    const double arithmetic_intensity) {
  if (arithmetic_intensity <
      assumptions.memory_bound_intensity_threshold) {
    return BottleneckModel::memory_bandwidth_or_gather_locality;
  }
  if (arithmetic_intensity <
      assumptions.compute_bound_intensity_threshold) {
    return BottleneckModel::mixed_memory_compute;
  }
  return BottleneckModel::compute_or_launch_latency;
}

std::string optimization_focus(
    const AnalyticalProfile& profile,
    const BottleneckModel bottleneck) {
  if (profile.canonical_operator == "int4_dequant_matvec") {
    return "fuse_int4_unpack_scale_and_matvec_then_capture_group_metadata";
  }
  if (profile.canonical_operator == "paged_kv_gather") {
    return "coalesce_block_table_loads_and_measure_gather_replay_locality";
  }
  if (profile.canonical_operator == "decode_attention") {
    return "compare_native_decode_attention_metadata_against_flashinfer_replay";
  }
  if (bottleneck ==
      BottleneckModel::memory_bandwidth_or_gather_locality) {
    return "reduce_memory_traffic_and_validate_vectorized_loads";
  }
  return "capture_kernel_time_occupancy_and_numerical_drift_before_optimization_claims";
}

std::size_t static_contract_pass_count(
    const inferlab_high_value::MicrobenchOperator& operator_contract,
    const AnalyticalProfile& profile) {
  const std::array<bool, 14> checks = {
      !operator_contract.name.empty(),
      !operator_contract.operator_family.empty(),
      !operator_contract.shape_signature.empty(),
      !operator_contract.dtype_layout.empty(),
      !operator_contract.backend_candidates.empty(),
      !operator_contract.correctness_oracle.empty(),
      !operator_contract.benchmark_case.empty(),
      !operator_contract.nvtx_range.empty(),
      !operator_contract.nsight_systems_command.empty(),
      !operator_contract.nsight_compute_command.empty(),
      !profile.modeled_shape_signature.empty(),
      !profile.reference_artifact.empty(),
      !profile.fallback_backend.empty(),
      profile.correctness_tolerance > 0.0};
  std::size_t passed = 0U;
  for (const bool check : checks) {
    if (check) {
      passed = internal::checked_add_size(
          passed, 1U,
          "CUDA/quant static contract pass count");
    }
  }
  return passed;
}

Result estimate_result(
    const Assumptions& assumptions,
    const inferlab_high_value::MicrobenchOperator& operator_contract,
    const AnalyticalProfile& input_profile) {
  const AnalyticalProfile profile = validate_profile(input_profile);
  if (operator_contract.name != profile.canonical_operator) {
    throw std::invalid_argument(
        "CUDA/quant profile and canonical operator identity must match");
  }

  Result result;
  result.profile = profile;
  result.operator_contract = operator_contract;
  result.arithmetic_intensity = require_finite(
      profile.estimated_flops / profile.estimated_bytes,
      "CUDA/quant arithmetic intensity");
  result.static_contract_check_count = 14U;
  result.static_contract_pass_count =
      static_contract_pass_count(operator_contract, profile);
  result.static_contract_coverage_score = require_finite(
      static_cast<double>(result.static_contract_pass_count) /
          static_cast<double>(result.static_contract_check_count),
      "CUDA/quant static contract coverage score");
  if (result.static_contract_pass_count !=
      result.static_contract_check_count) {
    throw std::invalid_argument(
        "CUDA/quant static operator contract must be complete");
  }
  const double intensity_priority = clamp_unit_interval(require_finite(
      result.arithmetic_intensity /
          assumptions.intensity_priority_reference,
      "CUDA/quant normalized intensity priority"));
  const double static_component = finite_product(
      result.static_contract_coverage_score,
      assumptions.static_contract_score_weight,
      "CUDA/quant static-contract score component");
  const double intensity_component = finite_product(
      intensity_priority,
      assumptions.intensity_priority_score_weight,
      "CUDA/quant intensity-priority score component");
  result.analytical_priority_score = clamp_unit_interval(
      internal::checked_add_finite_double(
          static_component, intensity_component,
          "CUDA/quant analytical priority score"));
  result.bottleneck_model =
      classify_bottleneck(assumptions, result.arithmetic_intensity);
  result.requires_target_gpu_validation = true;
  result.ranking_applicable = true;
  result.correctness_contract =
      "compare_" + operator_contract.correctness_oracle + "_and_" +
      profile.reference_artifact + "_with_abs_or_rel_tolerance_" +
      tolerance_text(profile.correctness_tolerance);
  result.fallback_contract =
      "fallback_to_" + profile.fallback_backend +
      "_when_cuda_or_product_kernel_is_unavailable";
  result.optimization_focus =
      optimization_focus(profile, result.bottleneck_model);
  return result;
}

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

}  // namespace

const char* bottleneck_model_name(const BottleneckModel model) {
  switch (model) {
    case BottleneckModel::memory_bandwidth_or_gather_locality:
      return "memory_bandwidth_or_gather_locality";
    case BottleneckModel::mixed_memory_compute:
      return "mixed_memory_compute";
    case BottleneckModel::compute_or_launch_latency:
      return "compute_or_launch_latency";
  }
  throw std::invalid_argument("unknown CUDA/quant bottleneck model");
}

Assumptions default_assumptions() {
  Assumptions assumptions;
  assumptions.profiling_metadata_fields = {
      "kernel_name", "dtype", "shape", "launch_count", "elapsed_ms",
      "bytes", "flops", "occupancy_or_reason_unavailable"};
  assumptions.additional_evidence_inputs = {
      "results/raw/cuda_profile_matrix.json",
      "results/raw/matrix_ops_profile.json",
      "results/raw/attention_probe.json",
      "results/raw/quantization_probe.json",
      "results/raw/product_flashinfer_kernel_profile.json"};
  return assumptions;
}

std::vector<AnalyticalProfile> default_profiles() {
  return {
      {"rmsnorm", "rmsnorm_vector_fused", "batch=16, hidden=4096",
       "results/raw/decoder_probe.json", "cpu_reference_rmsnorm",
       16.0 * 4096.0 * 4.0, 16.0 * 4096.0 * 6.0, 1.0e-4},
      {"rope", "rope_interleaved",
       "tokens=8192, heads=32, head_dim=128",
       "results/raw/attention_probe.json", "cpu_reference_rope",
       8192.0 * 32.0 * 128.0 * 6.0,
       8192.0 * 32.0 * 128.0 * 4.0, 1.0e-4},
      {"row_softmax", "row_softmax_masked",
       "rows=4096, cols=4096",
       "results/raw/attention_probe.json", "cpu_reference_softmax",
       4096.0 * 4096.0 * 5.0, 4096.0 * 4096.0 * 4.0,
       2.0e-4},
      {"int4_dequant_matvec", "int4_dequant_matvec",
       "out=4096, in=4096, group=128",
       "results/raw/quantization_probe.json",
       "cpu_reference_int4_dequant", 4096.0 * 4096.0 * 2.0,
       4096.0 * 4096.0 * 0.75, 3.0e-3},
      {"paged_kv_gather", "paged_kv_gather",
       "tokens=32768, heads=32, head_dim=128, page=16",
       "results/raw/paged_kv_locality_probe.json",
       "cpu_reference_paged_kv", 32768.0 * 32.0 * 128.0,
       32768.0 * 32.0 * 128.0 * 2.0, 1.0e-4},
      {"decode_attention", "decode_attention_single_token",
       "batch=64, kv_tokens=8192, heads=32, head_dim=128",
       "results/raw/attention_probe.json",
       "cpu_reference_attention",
       64.0 * 8192.0 * 32.0 * 128.0 * 2.0,
       64.0 * 8192.0 * 32.0 * 128.0 * 4.0, 2.0e-4},
  };
}

Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators,
    const std::vector<AnalyticalProfile>& input_profiles,
    inferlab_native_trace::Source trace_source) {
  validate_assumptions(assumptions);
  inferlab_high_value::validate_microbench_operator_registry(operators);
  if (input_profiles.size() != operators.size()) {
    throw std::invalid_argument(
        "CUDA/quant analytical profiles must match canonical operator count");
  }

  std::vector<AnalyticalProfile> profiles;
  profiles.reserve(input_profiles.size());
  for (const auto& input_profile : input_profiles) {
    AnalyticalProfile profile = validate_profile(input_profile);
    for (const auto& existing : profiles) {
      if (existing.canonical_operator ==
              profile.canonical_operator ||
          existing.legacy_scenario_name ==
              profile.legacy_scenario_name) {
        throw std::invalid_argument(
            "CUDA/quant profile identities must be unique");
      }
    }
    profiles.push_back(std::move(profile));
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.evidence_bridge_inputs =
      assumptions.additional_evidence_inputs;
  std::vector<std::string> operator_names;
  operator_names.reserve(operators.size());
  bool has_comparable_record = false;
  for (const auto& operator_contract : operators) {
    operator_names.push_back(operator_contract.name);
    const auto& profile =
        require_profile_for_operator(profiles, operator_contract.name);
    Result record =
        estimate_result(assumptions, operator_contract, profile);
    append_unique(plan.evidence_bridge_inputs,
                  record.profile.reference_artifact);

    const std::size_t record_index = plan.records.size();
    plan.records.push_back(std::move(record));
    const auto& appended = plan.records.back();
    if (!appended.ranking_applicable) {
      continue;
    }
    plan.comparable_record_count = internal::checked_add_size(
        plan.comparable_record_count, 1U,
        "CUDA/quant comparable record count");
    if (!has_comparable_record ||
        appended.arithmetic_intensity >
            plan.records[plan.highest_intensity_record_index]
                .arithmetic_intensity) {
      plan.highest_intensity_record_index = record_index;
    }
    if (!has_comparable_record ||
        appended.analytical_priority_score >
            plan.records[plan.highest_priority_record_index]
                .analytical_priority_score) {
      plan.highest_priority_record_index = record_index;
    }
    has_comparable_record = true;
  }
  if (!has_comparable_record || plan.comparable_record_count == 0U) {
    throw std::invalid_argument(
        "CUDA/quant plan requires comparable analytical records");
  }
  plan.trace_context = inferlab_native_trace::bind_trace_context(
      inferlab_native_trace::Domain::cuda_quant_microkernel,
      operator_names, std::move(trace_source));
  return plan;
}

Plan build_plan(
    const Assumptions& assumptions,
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators,
    const std::vector<AnalyticalProfile>& profiles) {
  return build_plan(assumptions, operators, profiles,
                    inferlab_native_trace::Source{});
}

}  // namespace inferlab::cuda_quant_microkernel
