#include "kernel_microbench_harness_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace inferlab::kernel_microbench {
namespace {

std::string bool_json(const bool value) { return value ? "true" : "false"; }

std::string optional_double_json(const std::optional<double>& value) {
  return value.has_value() ? inferlab_plan::fixed6(*value) : "null";
}

std::string optional_string_json(const std::string& value) {
  return value.empty() ? "null" : inferlab_plan::json_string(value);
}

template <typename T, typename Renderer>
std::string object_array_json(const std::vector<T>& values, const int indent,
                              Renderer renderer) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int item_indent = internal::checked_add_int(
      indent, 2, "kernel microbenchmark JSON array indent");
  std::ostringstream out;
  out << "[";
  if (!values.empty()) out << "\n";
  for (std::size_t index = 0; index < values.size(); ++index) {
    out << renderer(values[index], item_indent)
        << (index == values.size() - 1U ? "\n" : ",\n");
  }
  if (!values.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string shape_json(const CaseShape& shape, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel microbenchmark shape JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  return std::visit(
      [&pad, &nested](const auto& typed_shape) {
        using T = std::decay_t<decltype(typed_shape)>;
        std::ostringstream out;
        out << pad << "{\n";
        if constexpr (std::is_same_v<T, RmsNormShape>) {
          out << nested << "\"hidden\": " << typed_shape.hidden << ",\n";
          out << nested << "\"kind\": \"rmsnorm\",\n";
          out << nested << "\"rows\": " << typed_shape.rows << "\n";
        } else if constexpr (std::is_same_v<T, RopeShape>) {
          out << nested << "\"head_dimension\": "
              << typed_shape.head_dimension << ",\n";
          out << nested << "\"kind\": \"rope\",\n";
          out << nested << "\"sequence\": " << typed_shape.sequence
              << "\n";
        } else if constexpr (std::is_same_v<T, RowSoftmaxShape>) {
          out << nested << "\"columns\": " << typed_shape.columns
              << ",\n";
          out << nested << "\"kind\": \"row_softmax\",\n";
          out << nested << "\"rows\": " << typed_shape.rows << "\n";
        } else if constexpr (
            std::is_same_v<T, Int4DequantMatvecShape>) {
          out << nested << "\"group_size\": " << typed_shape.group_size
              << ",\n";
          out << nested << "\"input_features\": "
              << typed_shape.input_features << ",\n";
          out << nested << "\"kind\": \"int4_dequant_matvec\",\n";
          out << nested << "\"output_features\": "
              << typed_shape.output_features << "\n";
        } else if constexpr (std::is_same_v<T, PagedKvGatherShape>) {
          out << nested << "\"block_size\": " << typed_shape.block_size
              << ",\n";
          out << nested << "\"head_dimension\": "
              << typed_shape.head_dimension << ",\n";
          out << nested << "\"heads\": " << typed_shape.heads << ",\n";
          out << nested << "\"kind\": \"paged_kv_gather\",\n";
          out << nested << "\"tokens\": " << typed_shape.tokens << "\n";
        } else {
          out << nested << "\"context\": " << typed_shape.context
              << ",\n";
          out << nested << "\"head_dimension\": "
              << typed_shape.head_dimension << ",\n";
          out << nested << "\"heads\": " << typed_shape.heads << ",\n";
          out << nested << "\"kind\": \"decode_attention\"\n";
        }
        out << pad << "}";
        return out.str();
      },
      shape);
}

std::string operator_json(
    const inferlab_high_value::MicrobenchOperator* op, const int indent) {
  if (op == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark view received a null operator");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel microbenchmark operator JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend_candidates\": "
      << inferlab_plan::json_string(op->backend_candidates) << ",\n";
  out << nested << "\"benchmark_case\": "
      << inferlab_plan::json_string(op->benchmark_case) << ",\n";
  out << nested << "\"correctness_oracle\": "
      << inferlab_plan::json_string(op->correctness_oracle) << ",\n";
  out << nested << "\"dtype_layout\": "
      << inferlab_plan::json_string(op->dtype_layout) << ",\n";
  out << nested << "\"nvtx_range\": "
      << inferlab_plan::json_string(op->nvtx_range) << ",\n";
  out << nested << "\"operator\": " << inferlab_plan::json_string(op->name)
      << ",\n";
  out << nested << "\"operator_family\": "
      << inferlab_plan::json_string(op->operator_family) << ",\n";
  out << nested << "\"requires_target_gpu_validation\": true,\n";
  out << nested << "\"shape_signature\": "
      << inferlab_plan::json_string(op->shape_signature) << "\n";
  out << pad << "}";
  return out.str();
}

std::string case_json(const CaseSpec& benchmark_case, const int indent) {
  if (benchmark_case.operator_contract == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark case has no canonical operator contract");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "kernel microbenchmark case JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"benchmark_name\": "
      << inferlab_plan::json_string(benchmark_case.benchmark_name) << ",\n";
  out << nested << "\"case_id\": "
      << inferlab_plan::json_string(benchmark_case.case_id) << ",\n";
  out << nested << "\"dtype_layout\": "
      << inferlab_plan::json_string(benchmark_case.execution_dtype_layout)
      << ",\n";
  out << nested << "\"framework\": \"google/benchmark\",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(benchmark_case.operator_name) << ",\n";
  out << nested << "\"output_items_per_iteration\": "
      << benchmark_case.output_item_count << ",\n";
  out << nested << "\"profile\": "
      << inferlab_plan::json_string(
             case_profile_name(benchmark_case.profile))
      << ",\n";
  out << nested << "\"shape\": "
      << shape_json(benchmark_case.shape, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

std::string profiling_command_json(
    const inferlab_high_value::MicrobenchOperator* op, const int indent) {
  if (op == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark profiling command has no operator");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark profiling indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"nsight_compute\": "
      << inferlab_plan::json_string(op->nsight_compute_command) << ",\n";
  out << nested << "\"nsight_systems\": "
      << inferlab_plan::json_string(op->nsight_systems_command) << ",\n";
  out << nested << "\"nvtx_range\": "
      << inferlab_plan::json_string(op->nvtx_range) << ",\n";
  out << nested << "\"operator\": " << inferlab_plan::json_string(op->name)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string correctness_contract_json(const CaseSpec& benchmark_case,
                                      const int indent) {
  if (benchmark_case.operator_contract == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark correctness contract has no operator");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark correctness indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"absolute_tolerance\": "
      << inferlab_plan::fixed6(benchmark_case.absolute_tolerance) << ",\n";
  out << nested << "\"case_id\": "
      << inferlab_plan::json_string(benchmark_case.case_id) << ",\n";
  out << nested << "\"comparison\": "
      << inferlab_plan::json_string(
             "abs(candidate-reference) <= absolute + relative * abs(reference)")
      << ",\n";
  out << nested << "\"nan_allowed\": false,\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(benchmark_case.operator_name) << ",\n";
  out << nested << "\"oracle\": "
      << inferlab_plan::json_string(
             benchmark_case.operator_contract->correctness_oracle)
      << ",\n";
  out << nested << "\"relative_tolerance\": "
      << inferlab_plan::fixed6(benchmark_case.relative_tolerance) << ",\n";
  out << nested << "\"timing_claim_allowed_before_correctness\": false\n";
  out << pad << "}";
  return out.str();
}

std::string input_generator_json(const CaseSpec& benchmark_case,
                                 const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark generator indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"case_id\": "
      << inferlab_plan::json_string(benchmark_case.case_id) << ",\n";
  out << nested << "\"generator\": \"deterministic_bounded_pattern\",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(benchmark_case.operator_name) << ",\n";
  out << nested << "\"seed\": " << benchmark_case.deterministic_seed << ",\n";
  out << nested << "\"shape\": "
      << shape_json(benchmark_case.shape,
                    internal::checked_add_int(
                        indent, 2,
                        "kernel microbenchmark nested generator shape"))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string reference_output_json(const CorrectnessResult& result,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark reference indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"case_id\": " << inferlab_plan::json_string(result.case_id)
      << ",\n";
  out << nested << "\"generated_in_memory\": "
      << bool_json(result.reference_generated) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(result.operator_name) << ",\n";
  out << nested << "\"reference\": "
      << inferlab_plan::json_string(result.oracle) << ",\n";
  out << nested << "\"output_item_count\": " << result.output_item_count
      << ",\n";
  out << nested << "\"reference_artifact\": null,\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(correctness_state_name(result.state))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string tolerance_policy_json(const CaseSpec& benchmark_case,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark tolerance indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"absolute_tolerance\": "
      << inferlab_plan::fixed6(benchmark_case.absolute_tolerance) << ",\n";
  out << nested << "\"case_id\": "
      << inferlab_plan::json_string(benchmark_case.case_id) << ",\n";
  out << nested << "\"nan_allowed\": false,\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(benchmark_case.operator_name) << ",\n";
  out << nested << "\"relative_tolerance\": "
      << inferlab_plan::fixed6(benchmark_case.relative_tolerance) << ",\n";
  out << nested << "\"timing_allowed_before_pass\": false\n";
  out << pad << "}";
  return out.str();
}

std::string failure_class_json(const FailureClass& failure,
                               const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark failure indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"class\": " << inferlab_plan::json_string(failure.name)
      << ",\n";
  out << nested << "\"meaning\": "
      << inferlab_plan::json_string(failure.meaning) << "\n";
  out << pad << "}";
  return out.str();
}

std::string shape_matrix_json(const CaseSpec& benchmark_case,
                              const int indent) {
  if (benchmark_case.operator_contract == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark shape matrix has no operator contract");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark shape matrix indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"case_id\": "
      << inferlab_plan::json_string(benchmark_case.case_id) << ",\n";
  out << nested << "\"current_artifact_status\": \"fixture_defined\",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(benchmark_case.operator_name) << ",\n";
  out << nested << "\"primary_shape\": "
      << inferlab_plan::json_string(
             benchmark_case.operator_contract->shape_signature)
      << ",\n";
  out << nested << "\"profile\": "
      << inferlab_plan::json_string(
             case_profile_name(benchmark_case.profile))
      << ",\n";
  out << nested << "\"shape\": "
      << shape_json(benchmark_case.shape,
                    internal::checked_add_int(
                        indent, 2,
                        "kernel microbenchmark matrix nested shape"))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string execution_command_json(const ExecutionCommand& command,
                                   const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark command indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"case_id\": " << inferlab_plan::json_string(command.case_id)
      << ",\n";
  out << nested << "\"configure\": "
      << inferlab_plan::json_string(command.configure) << ",\n";
  out << nested << "\"correctness\": "
      << inferlab_plan::json_string(command.correctness_and_timing) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(command.operator_name) << ",\n";
  out << nested << "\"profile_nsight_compute\": "
      << inferlab_plan::json_string(command.nsight_compute) << ",\n";
  out << nested << "\"profile_nsight_systems\": "
      << inferlab_plan::json_string(command.nsight_systems) << ",\n";
  out << nested << "\"timing\": "
      << inferlab_plan::json_string(command.correctness_and_timing) << "\n";
  out << pad << "}";
  return out.str();
}

std::string correctness_result_json(const CorrectnessResult& result,
                                    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark result indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"attempted\": " << bool_json(result.attempted) << ",\n";
  out << nested << "\"case_id\": " << inferlab_plan::json_string(result.case_id)
      << ",\n";
  out << nested << "\"comparison_performed\": "
      << bool_json(result.comparison_performed) << ",\n";
  out << nested << "\"error\": " << optional_string_json(result.error)
      << ",\n";
  out << nested << "\"max_abs_error\": "
      << optional_double_json(result.max_absolute_error) << ",\n";
  out << nested << "\"max_rel_error\": "
      << optional_double_json(result.max_relative_error) << ",\n";
  out << nested << "\"mismatch_count\": " << result.mismatch_count << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(result.operator_name) << ",\n";
  out << nested << "\"oracle\": "
      << inferlab_plan::json_string(result.oracle) << ",\n";
  out << nested << "\"output_item_count\": " << result.output_item_count
      << ",\n";
  out << nested << "\"passed\": " << bool_json(result.passed) << ",\n";
  out << nested << "\"reference_generated\": "
      << bool_json(result.reference_generated) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(correctness_state_name(result.state))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string timing_result_json(const TimingResult& result, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark timing indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"attempted\": " << bool_json(result.attempted) << ",\n";
  out << nested << "\"case_id\": " << inferlab_plan::json_string(result.case_id)
      << ",\n";
  out << nested << "\"error\": " << optional_string_json(result.error)
      << ",\n";
  out << nested << "\"latency_us_p50\": "
      << optional_double_json(result.latency_us_p50) << ",\n";
  out << nested << "\"latency_us_p99\": "
      << optional_double_json(result.latency_us_p99) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(result.operator_name) << ",\n";
  out << nested << "\"reported_run_count\": "
      << result.reported_run_count << ",\n";
  out << nested << "\"source\": "
      << optional_string_json(result.timing_source) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(timing_state_name(result.state)) << ",\n";
  out << nested << "\"throughput_items_per_s\": "
      << optional_double_json(result.throughput_items_per_second) << "\n";
  out << pad << "}";
  return out.str();
}

std::string profile_metadata_json(const ProfileMetadata& profile,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark profile indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"case_id\": " << inferlab_plan::json_string(profile.case_id)
      << ",\n";
  out << nested << "\"nsight_compute_command\": "
      << inferlab_plan::json_string(profile.nsight_compute_command) << ",\n";
  out << nested << "\"nsight_report_captured\": "
      << bool_json(profile.nsight_report_captured) << ",\n";
  out << nested << "\"nsight_systems_command\": "
      << inferlab_plan::json_string(profile.nsight_systems_command) << ",\n";
  out << nested << "\"nvtx_range\": "
      << inferlab_plan::json_string(profile.nvtx_range) << ",\n";
  out << nested << "\"nvtx_range_emitted\": "
      << bool_json(profile.nvtx_range_emitted) << ",\n";
  out << nested << "\"operator\": "
      << inferlab_plan::json_string(profile.operator_name) << ",\n";
  out << nested << "\"profile_artifact\": null\n";
  out << pad << "}";
  return out.str();
}

bool dependency_compiled(const HarnessPlan& plan,
                         const std::string& dependency_name) {
  if (dependency_name == "google/benchmark") {
    return plan.capabilities.google_benchmark_compiled;
  }
  if (dependency_name == "CUDA NVTX") {
    return plan.capabilities.nvtx_compiled;
  }
  if (dependency_name == "CUDA Toolkit") {
    return plan.capabilities.cuda_runtime_compiled;
  }
  throw std::logic_error(
      "unknown kernel microbenchmark dependency projection: " +
      dependency_name);
}

std::string dependency_source_json(
    const inferlab_high_value::DependencySpec* dependency, const int indent) {
  if (dependency == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark view received a null dependency");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark source indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(dependency->name) << ",\n";
  out << nested << "\"purpose\": "
      << inferlab_plan::json_string(dependency->engineering_value) << ",\n";
  out << nested << "\"url\": "
      << inferlab_plan::json_string(dependency->source) << ",\n";
  out << nested << "\"version\": "
      << inferlab_plan::json_string(dependency->pinned_version) << "\n";
  out << pad << "}";
  return out.str();
}

std::string dependency_runtime_json(
    const HarnessPlan& plan,
    const inferlab_high_value::DependencySpec* dependency,
    const int indent) {
  if (dependency == nullptr) {
    throw std::logic_error(
        "kernel microbenchmark runtime received a null dependency");
  }
  const bool compiled = dependency_compiled(plan, dependency->name);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark dependency indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"compiled\": " << bool_json(compiled) << ",\n";
  out << nested << "\"cmake_option\": "
      << inferlab_plan::json_string(dependency->cmake_option) << ",\n";
  out << nested << "\"fallback\": "
      << inferlab_plan::json_string(dependency->fallback) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(dependency->name) << ",\n";
  out << nested << "\"optional\": " << bool_json(dependency->optional)
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(compiled ? "compiled" : "not_compiled")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string summary_json(const HarnessPlan& plan, const int indent) {
  const auto& summary = plan.summary;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2,
                                "kernel microbenchmark summary indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"available_case_count\": "
      << summary.available_case_count << ",\n";
  out << nested << "\"benchmark_case_count\": "
      << summary.selected_case_count << ",\n";
  out << nested << "\"blocker_count\": " << summary.blocker_count << ",\n";
  out << nested << "\"correctness_comparison_count\": "
      << summary.correctness_comparison_count << ",\n";
  out << nested << "\"correctness_failure_count\": "
      << summary.correctness_failure_count << ",\n";
  out << nested << "\"correctness_pass_count\": "
      << summary.correctness_pass_count << ",\n";
  out << nested << "\"correctness_result_count\": "
      << summary.correctness_attempt_count << ",\n";
  out << nested << "\"cuda_enabled\": "
      << bool_json(plan.capabilities.cuda_runtime_compiled) << ",\n";
  out << nested << "\"evidence_confidence\": "
      << inferlab_plan::json_string(plan.evidence_confidence) << ",\n";
  out << nested << "\"execute_requested\": "
      << bool_json(plan.options.execute_requested) << ",\n";
  out << nested << "\"execution_backend\": "
      << inferlab_plan::json_string(
             execution_backend_name(plan.options.backend))
      << ",\n";
  out << nested << "\"google_benchmark_enabled\": "
      << bool_json(plan.capabilities.google_benchmark_compiled) << ",\n";
  out << nested << "\"microbench_decision\": "
      << inferlab_plan::json_string(harness_decision_name(plan.decision))
      << ",\n";
  out << nested << "\"nvtx_enabled\": "
      << bool_json(plan.capabilities.nvtx_compiled) << ",\n";
  out << nested << "\"operator_count\": " << summary.operator_count
      << ",\n";
  out << nested << "\"reference_generation_count\": "
      << summary.reference_generation_count << ",\n";
  out << nested << "\"selected_case_count\": "
      << summary.selected_case_count << ",\n";
  out << nested << "\"selected_operator\": "
      << inferlab_plan::json_string(plan.options.selected_operator) << ",\n";
  out << nested << "\"selected_operator_count\": "
      << summary.selected_operator_count << ",\n";
  out << nested << "\"selected_profile\": "
      << inferlab_plan::json_string(
             plan.options.all_profiles
                 ? "all"
                 : case_profile_name(plan.options.selected_profile))
      << ",\n";
  out << nested << "\"target_gpu_execute_verified\": "
      << bool_json(plan.target_gpu_execute_verified) << ",\n";
  out << nested << "\"timing_attempt_count\": "
      << summary.timing_attempt_count << ",\n";
  out << nested << "\"timing_failure_count\": "
      << summary.timing_failure_count << ",\n";
  out << nested << "\"timing_result_count\": "
      << summary.timing_result_count << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string harness_artifact_json(const HarnessPlan& plan) {
  if (plan.artifact == nullptr || plan.artifact_type.empty() ||
      plan.generated_at.empty() || plan.claim_scope.empty() ||
      plan.status != harness_decision_name(plan.decision) ||
      plan.summary.selected_case_count != plan.selected_cases.size() ||
      plan.summary.blocker_count != plan.blockers.size()) {
    throw std::logic_error(
        "kernel microbenchmark artifact projection received an invalid plan");
  }

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"schema\": {\n";
  out << "    \"name\": " << inferlab_plan::json_string(plan.artifact_type)
      << ",\n";
  out << "    \"version\": \"v1\"\n";
  out << "  },\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(plan.artifact_type) << ",\n";
  out << "  \"mode\": " << inferlab_plan::json_string(plan.mode) << ",\n";
  out << "  \"executed\": " << bool_json(plan.executed) << ",\n";
  out << "  \"replayed\": " << bool_json(plan.replayed) << ",\n";
  out << "  \"status\": " << inferlab_plan::json_string(plan.status)
      << ",\n";
  out << "  \"target_gpu_execute_verified\": "
      << bool_json(plan.target_gpu_execute_verified) << ",\n";
  out << "  \"generated_at\": "
      << inferlab_plan::json_string(plan.generated_at) << ",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(plan.claim_scope) << ",\n";
  out << "  \"claim_scope_detail\": {\n";
  out << "    \"can_claim\": "
      << inferlab_plan::json_string_array(plan.can_claim, 4) << ",\n";
  out << "    \"cannot_claim\": "
      << inferlab_plan::json_string_array(plan.cannot_claim, 4) << ",\n";
  out << "    \"requires_execute_before_claim\": "
      << inferlab_plan::json_string_array(
             plan.requires_execute_before_claim, 4)
      << "\n";
  out << "  },\n";
  out << "  \"environment\": {\n";
  out << "    \"cuda_build_enabled\": "
      << bool_json(plan.capabilities.cuda_runtime_compiled) << ",\n";
  out << "    \"google_benchmark_enabled\": "
      << bool_json(plan.capabilities.google_benchmark_compiled) << ",\n";
  out << "    \"gpu_runtime_probe_performed\": "
      << bool_json(plan.executed &&
                   plan.options.backend == ExecutionBackend::cuda_runtime)
      << ",\n";
  out << "    \"nvtx_enabled\": "
      << bool_json(plan.capabilities.nvtx_compiled) << ",\n";
  out << "    \"requires_target_gpu_validation\": true\n";
  out << "  },\n";
  out << "  \"official_sources\": "
      << object_array_json(
             plan.dependencies, 2,
             [](const auto* dependency, const int indent) {
               return dependency_source_json(dependency, indent);
             })
      << ",\n";
  out << "  \"refresh_commands\": "
      << inferlab_plan::json_string_array({plan.artifact->command}, 2)
      << ",\n";
  out << "  \"migration_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << ",\n";
  out << "  \"cli_context\": {\n";
  out << "    \"benchmark_min_time_ms\": "
      << inferlab_plan::fixed6(plan.options.benchmark_min_time_ms) << ",\n";
  out << "    \"benchmark_repetitions\": "
      << plan.options.benchmark_repetitions << ",\n";
  out << "    \"output_path\": "
      << optional_string_json(plan.output_path_label) << ",\n";
  out << "    \"repo_root\": "
      << optional_string_json(plan.repo_root_label) << "\n";
  out << "  },\n";
  out << "  \"dependency_runtime\": "
      << object_array_json(
             plan.dependencies, 2,
             [&plan](const auto* dependency, const int indent) {
               return dependency_runtime_json(plan, dependency, indent);
             })
      << ",\n";
  out << "  \"summary\": " << summary_json(plan, 2) << ",\n";
  out << "  \"target_execution_contract\": {\n";
  out << "    \"backend_flag\": \"--backend <cuda_runtime|cpu_reference>\",\n";
  out << "    \"benchmark_min_time_ms\": "
      << inferlab_plan::fixed6(plan.options.benchmark_min_time_ms) << ",\n";
  out << "    \"benchmark_repetitions\": "
      << plan.options.benchmark_repetitions << ",\n";
  out << "    \"case_profile_flag\": \"--case-profile <smoke|serving_like|stress|all>\",\n";
  out << "    \"correctness_before_timing\": true,\n";
  out << "    \"entrypoint\": "
      << inferlab_plan::json_string(plan.artifact->target) << ",\n";
  out << "    \"execute_flag\": \"--execute\",\n";
  out << "    \"operator_flag\": \"--operator <all|rmsnorm|rope|row_softmax|int4_dequant_matvec|paged_kv_gather|decode_attention>\",\n";
  out << "    \"profile_requires_external_nsight_capture\": true,\n";
  out << "    \"timing_requires_google_benchmark\": true\n";
  out << "  },\n";
  out << "  \"operators\": "
      << object_array_json(plan.selected_operators, 2, operator_json) << ",\n";
  out << "  \"benchmark_cases\": "
      << object_array_json(plan.selected_cases, 2, case_json) << ",\n";
  out << "  \"profiling_commands\": "
      << object_array_json(plan.selected_operators, 2, profiling_command_json)
      << ",\n";
  out << "  \"correctness_contracts\": "
      << object_array_json(plan.selected_cases, 2, correctness_contract_json)
      << ",\n";
  out << "  \"input_generators\": "
      << object_array_json(plan.selected_cases, 2, input_generator_json)
      << ",\n";
  out << "  \"reference_outputs\": "
      << object_array_json(plan.correctness_results, 2, reference_output_json)
      << ",\n";
  out << "  \"tolerance_policies\": "
      << object_array_json(plan.selected_cases, 2, tolerance_policy_json)
      << ",\n";
  out << "  \"failure_classification\": "
      << object_array_json(plan.failure_classes, 2, failure_class_json)
      << ",\n";
  out << "  \"shape_matrix\": "
      << object_array_json(plan.available_cases, 2, shape_matrix_json) << ",\n";
  out << "  \"target_machine_execution_matrix\": "
      << object_array_json(plan.execution_commands, 2, execution_command_json)
      << ",\n";
  out << "  \"correctness_results\": "
      << object_array_json(plan.correctness_results, 2,
                           correctness_result_json)
      << ",\n";
  out << "  \"timing_results\": "
      << object_array_json(plan.timing_results, 2, timing_result_json) << ",\n";
  out << "  \"profile_metadata\": "
      << object_array_json(plan.profile_metadata, 2, profile_metadata_json)
      << ",\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(plan.blockers, 2) << ",\n";
  out << "  \"not_run\": "
      << inferlab_plan::json_string_array(plan.not_run, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kernel_microbench
