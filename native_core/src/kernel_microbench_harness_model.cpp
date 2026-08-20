#include "kernel_microbench_harness_model.hpp"

#include "checked_index.hpp"
#include "catalog_derived_views.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace inferlab::kernel_microbench {
namespace {

constexpr int kDeterministicSeed = 20260617;

template <typename Shape>
CaseSpec make_case(const inferlab_high_value::MicrobenchOperator& op,
                   const CaseProfile profile, Shape shape,
                   const double absolute_tolerance,
                   const double relative_tolerance) {
  CaseSpec result;
  result.operator_contract = &op;
  result.case_id = op.name + "." + case_profile_name(profile);
  result.operator_name = op.name;
  result.profile = profile;
  result.shape = std::move(shape);
  result.benchmark_name = op.benchmark_case + "/" +
                          case_profile_name(profile);
  result.absolute_tolerance = absolute_tolerance;
  result.relative_tolerance = relative_tolerance;
  result.deterministic_seed = kDeterministicSeed;
  return result;
}

const inferlab_high_value::MicrobenchOperator& require_operator(
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators,
    const std::string& name) {
  const inferlab_high_value::MicrobenchOperator* match = nullptr;
  for (const auto& op : operators) {
    if (op.name != name) continue;
    if (match != nullptr) {
      throw std::logic_error("duplicate canonical microbenchmark operator: " +
                             name);
    }
    match = &op;
  }
  if (match == nullptr) {
    throw std::logic_error("missing canonical microbenchmark operator: " +
                           name);
  }
  return *match;
}

const inferlab_high_value::DependencySpec& require_dependency(
    const std::vector<inferlab_high_value::DependencySpec>& dependencies,
    const std::string& name) {
  const inferlab_high_value::DependencySpec* match = nullptr;
  for (const auto& dependency : dependencies) {
    if (dependency.name != name) continue;
    if (match != nullptr) {
      throw std::logic_error("duplicate dependency specification: " + name);
    }
    match = &dependency;
  }
  if (match == nullptr) {
    throw std::logic_error("missing dependency specification: " + name);
  }
  return *match;
}

std::size_t case_output_item_count(const CaseShape& shape) {
  return std::visit(
      [](const auto& typed_shape) -> std::size_t {
        using T = std::decay_t<decltype(typed_shape)>;
        if constexpr (std::is_same_v<T, RmsNormShape>) {
          return internal::checked_product_size(
              typed_shape.rows, typed_shape.hidden,
              "RMSNorm microbenchmark output");
        } else if constexpr (std::is_same_v<T, RopeShape>) {
          return internal::checked_product_size(
              typed_shape.sequence, typed_shape.head_dimension,
              "RoPE microbenchmark output");
        } else if constexpr (std::is_same_v<T, RowSoftmaxShape>) {
          return internal::checked_product_size(
              typed_shape.rows, typed_shape.columns,
              "row softmax microbenchmark output");
        } else if constexpr (std::is_same_v<T, Int4DequantMatvecShape>) {
          if (typed_shape.output_features <= 0 ||
              typed_shape.input_features <= 0 ||
              typed_shape.group_size <= 0) {
            throw std::invalid_argument(
                "INT4 dequant matvec microbenchmark dimensions must be positive");
          }
          return static_cast<std::size_t>(typed_shape.output_features);
        } else if constexpr (std::is_same_v<T, PagedKvGatherShape>) {
          return internal::checked_product_size(
              typed_shape.tokens, typed_shape.heads,
              typed_shape.head_dimension,
              "Paged KV gather microbenchmark output");
        } else {
          return internal::checked_product_size(
              typed_shape.heads, typed_shape.head_dimension,
              "decode attention microbenchmark output");
        }
      },
      shape);
}

void append_operator_cases(
    std::vector<CaseSpec>& cases,
    const inferlab_high_value::MicrobenchOperator& op) {
  constexpr double kFloatAbsoluteTolerance = 1.0e-4;
  constexpr double kFloatRelativeTolerance = 1.0e-4;
  constexpr double kInt4AbsoluteTolerance = 1.0e-2;
  constexpr double kInt4RelativeTolerance = 1.0e-2;

  if (op.name == "rmsnorm") {
    cases.push_back(make_case(op, CaseProfile::smoke,
                              RmsNormShape{1, 1024},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              RmsNormShape{8, 4096},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              RmsNormShape{32, 8192},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    return;
  }
  if (op.name == "rope") {
    cases.push_back(make_case(op, CaseProfile::smoke, RopeShape{128, 64},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              RopeShape{2048, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              RopeShape{8192, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    return;
  }
  if (op.name == "row_softmax") {
    cases.push_back(make_case(op, CaseProfile::smoke,
                              RowSoftmaxShape{1, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              RowSoftmaxShape{32, 2048},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              RowSoftmaxShape{128, 8192},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    return;
  }
  if (op.name == "int4_dequant_matvec") {
    cases.push_back(make_case(op, CaseProfile::smoke,
                              Int4DequantMatvecShape{1024, 4096, 32},
                              kInt4AbsoluteTolerance,
                              kInt4RelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              Int4DequantMatvecShape{1024, 4096, 128},
                              kInt4AbsoluteTolerance,
                              kInt4RelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              Int4DequantMatvecShape{4096, 8192, 128},
                              kInt4AbsoluteTolerance,
                              kInt4RelativeTolerance));
    return;
  }
  if (op.name == "paged_kv_gather") {
    cases.push_back(make_case(op, CaseProfile::smoke,
                              PagedKvGatherShape{128, 16, 8, 64},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              PagedKvGatherShape{2048, 16, 16, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              PagedKvGatherShape{8192, 32, 32, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    return;
  }
  if (op.name == "decode_attention") {
    cases.push_back(make_case(op, CaseProfile::smoke,
                              DecodeAttentionShape{1, 128, 64},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::serving_like,
                              DecodeAttentionShape{16, 2048, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    cases.push_back(make_case(op, CaseProfile::stress,
                              DecodeAttentionShape{64, 8192, 128},
                              kFloatAbsoluteTolerance,
                              kFloatRelativeTolerance));
    return;
  }
  throw std::logic_error(
      "canonical microbenchmark operator lacks an execution fixture: " +
      op.name);
}

std::vector<CaseSpec> build_case_fixture(
    const std::vector<inferlab_high_value::MicrobenchOperator>& operators) {
  std::vector<CaseSpec> cases;
  for (const auto& op : operators) append_operator_cases(cases, op);
  for (auto& benchmark_case : cases) {
    if (benchmark_case.operator_name == "int4_dequant_matvec") {
      benchmark_case.execution_dtype_layout =
          "INT4 packed weights with fp32 scales and fp32 activation";
    } else if (benchmark_case.operator_name == "paged_kv_gather") {
      benchmark_case.execution_dtype_layout =
          "fp32 paged KV block payload and integer block table";
    } else {
      benchmark_case.execution_dtype_layout = "fp32 contiguous runtime input";
    }
    benchmark_case.output_item_count =
        case_output_item_count(benchmark_case.shape);
    if (benchmark_case.operator_contract == nullptr ||
        benchmark_case.operator_contract->name !=
            benchmark_case.operator_name ||
        benchmark_case.case_id.empty() || benchmark_case.operator_name.empty() ||
        benchmark_case.benchmark_name.empty() ||
        benchmark_case.execution_dtype_layout.empty() ||
        benchmark_case.deterministic_seed <= 0 ||
        benchmark_case.output_item_count == 0U ||
        !std::isfinite(benchmark_case.absolute_tolerance) ||
        benchmark_case.absolute_tolerance <= 0.0 ||
        !std::isfinite(benchmark_case.relative_tolerance) ||
        benchmark_case.relative_tolerance <= 0.0) {
      throw std::logic_error(
          "invalid microbenchmark execution fixture record");
    }
  }
  for (std::size_t index = 0; index < cases.size(); ++index) {
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (cases[index].case_id == cases[prior].case_id ||
          cases[index].benchmark_name == cases[prior].benchmark_name) {
        throw std::logic_error(
            "duplicate microbenchmark execution fixture identity");
      }
    }
  }
  for (const auto& op : operators) {
    std::size_t count = 0;
    bool smoke = false;
    bool serving_like = false;
    bool stress = false;
    for (const auto& benchmark_case : cases) {
      if (benchmark_case.operator_name != op.name) continue;
      count = internal::checked_add_size(
          count, 1U, "microbenchmark fixture operator case count");
      smoke = smoke || benchmark_case.profile == CaseProfile::smoke;
      serving_like = serving_like ||
                     benchmark_case.profile == CaseProfile::serving_like;
      stress = stress || benchmark_case.profile == CaseProfile::stress;
    }
    if (count != 3U || !smoke || !serving_like || !stress) {
      throw std::logic_error(
          "microbenchmark fixture must cover three unique profiles for " +
          op.name);
    }
  }
  return cases;
}

const inferlab_high_value::MicrobenchOperator& operator_for_case(
    const HarnessPlan& plan, const CaseSpec& benchmark_case) {
  for (const auto* op : plan.selected_operators) {
    if (op != nullptr && op->name == benchmark_case.operator_name) return *op;
  }
  throw std::logic_error(
      "selected microbenchmark case has no selected canonical operator: " +
      benchmark_case.case_id);
}

std::string execution_command(const HarnessPlan& plan,
                              const std::string& operator_name,
                              const std::string& profile_name,
                              const ExecutionBackend backend) {
  if (plan.artifact == nullptr) {
    throw std::logic_error("microbenchmark plan has no catalog artifact");
  }
  if (operator_name.empty() || profile_name.empty()) {
    throw std::logic_error(
        "microbenchmark execution command selector must not be empty");
  }
  return "./build/" + plan.artifact->target +
         " --repo-root . --execute --operator " + operator_name +
         " --case-profile " + profile_name + " --backend " +
         execution_backend_name(backend) + " --output results/raw/" +
         plan.artifact->artifact + " --format json";
}

std::string case_execution_command(const HarnessPlan& plan,
                                   const CaseSpec& benchmark_case) {
  return execution_command(plan, benchmark_case.operator_name,
                           case_profile_name(benchmark_case.profile),
                           plan.options.backend);
}

std::string selected_execution_command(const HarnessPlan& plan) {
  return execution_command(
      plan, plan.options.selected_operator,
      plan.options.all_profiles
          ? std::string("all")
          : std::string(case_profile_name(plan.options.selected_profile)),
      plan.options.backend);
}

std::string target_gpu_execution_command(const HarnessPlan& plan) {
  return execution_command(
      plan, "all",
      plan.options.all_profiles
          ? std::string("all")
          : std::string(case_profile_name(plan.options.selected_profile)),
      ExecutionBackend::cuda_runtime);
}

void validate_result_alignment(const HarnessPlan& plan) {
  if (plan.correctness_results.size() != plan.selected_cases.size() ||
      plan.timing_results.size() != plan.selected_cases.size() ||
      plan.profile_metadata.size() != plan.selected_cases.size() ||
      plan.execution_commands.size() != plan.selected_cases.size()) {
    throw std::logic_error(
        "microbenchmark result collections must align with selected cases");
  }
  for (std::size_t index = 0; index < plan.selected_cases.size(); ++index) {
    const auto& benchmark_case = plan.selected_cases[index];
    if (plan.correctness_results[index].case_id != benchmark_case.case_id ||
        plan.correctness_results[index].operator_name !=
            benchmark_case.operator_name ||
        plan.correctness_results[index].oracle.empty() ||
        plan.correctness_results[index].output_item_count !=
            benchmark_case.output_item_count ||
        plan.timing_results[index].case_id != benchmark_case.case_id ||
        plan.timing_results[index].operator_name !=
            benchmark_case.operator_name ||
        plan.profile_metadata[index].case_id != benchmark_case.case_id ||
        plan.profile_metadata[index].operator_name !=
            benchmark_case.operator_name ||
        plan.execution_commands[index].case_id != benchmark_case.case_id ||
        plan.execution_commands[index].operator_name !=
            benchmark_case.operator_name) {
      throw std::logic_error(
          "microbenchmark result identity does not match selected case");
    }
  }
}

void push_unique(std::vector<std::string>& values, std::string value) {
  if (value.empty()) return;
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

}  // namespace

const char* execution_backend_name(const ExecutionBackend backend) {
  switch (backend) {
    case ExecutionBackend::cuda_runtime:
      return "cuda_runtime";
    case ExecutionBackend::cpu_reference:
      return "cpu_reference";
  }
  throw std::logic_error("unknown microbenchmark execution backend");
}

const char* case_profile_name(const CaseProfile profile) {
  switch (profile) {
    case CaseProfile::smoke:
      return "smoke";
    case CaseProfile::serving_like:
      return "serving_like";
    case CaseProfile::stress:
      return "stress";
  }
  throw std::logic_error("unknown microbenchmark case profile");
}

const char* correctness_state_name(const CorrectnessState state) {
  switch (state) {
    case CorrectnessState::not_requested:
      return "not_requested";
    case CorrectnessState::backend_unavailable:
      return "backend_unavailable";
    case CorrectnessState::reference_generated:
      return "reference_generated";
    case CorrectnessState::passed:
      return "passed";
    case CorrectnessState::failed:
      return "failed";
    case CorrectnessState::runtime_error:
      return "runtime_error";
  }
  throw std::logic_error("unknown microbenchmark correctness state");
}

const char* timing_state_name(const TimingState state) {
  switch (state) {
    case TimingState::not_requested:
      return "not_requested";
    case TimingState::blocked_by_correctness:
      return "blocked_by_correctness";
    case TimingState::benchmark_unavailable:
      return "benchmark_unavailable";
    case TimingState::completed:
      return "completed";
    case TimingState::runtime_error:
      return "runtime_error";
  }
  throw std::logic_error("unknown microbenchmark timing state");
}

const char* harness_decision_name(const HarnessDecision decision) {
  switch (decision) {
    case HarnessDecision::manifest_ready:
      return "manifest_ready";
    case HarnessDecision::execute_blocked:
      return "execute_blocked";
    case HarnessDecision::cpu_reference_executed:
      return "cpu_reference_executed";
    case HarnessDecision::correctness_verified_timing_blocked:
      return "correctness_verified_timing_blocked";
    case HarnessDecision::correctness_failed:
      return "correctness_failed";
    case HarnessDecision::timing_failed:
      return "timing_failed";
    case HarnessDecision::execute_verified:
      return "execute_verified";
  }
  throw std::logic_error("unknown microbenchmark harness decision");
}

ExecutionBackend parse_execution_backend(const std::string& value) {
  if (value == "cuda_runtime") return ExecutionBackend::cuda_runtime;
  if (value == "cpu_reference") return ExecutionBackend::cpu_reference;
  throw std::invalid_argument(
      "unsupported --backend value: " + value +
      "; expected cuda_runtime or cpu_reference");
}

CaseProfile parse_case_profile(const std::string& value) {
  if (value == "smoke") return CaseProfile::smoke;
  if (value == "serving_like") return CaseProfile::serving_like;
  if (value == "stress") return CaseProfile::stress;
  throw std::invalid_argument(
      "unsupported --case-profile value: " + value +
      "; expected smoke, serving_like, stress, or all");
}

HarnessPlan build_harness_plan(const HarnessOptions& options,
                               const BuildCapabilities& capabilities) {
  if (options.selected_operator.empty()) {
    throw std::invalid_argument("--operator must not be empty");
  }
  if (!std::isfinite(options.benchmark_min_time_ms) ||
      options.benchmark_min_time_ms <= 0.0) {
    throw std::invalid_argument(
        "--benchmark-min-time-ms must be finite and positive");
  }
  if (options.benchmark_repetitions <= 0) {
    throw std::invalid_argument("--benchmark-repetitions must be positive");
  }

  HarnessPlan plan;
  plan.artifact = &inferlab_high_value::require_managed_artifact(
      "kernel_microbench_harness");
  plan.artifact_type = inferlab_catalog::registry_artifact_type(*plan.artifact);
  plan.options = options;
  plan.capabilities = capabilities;
  plan.generated_at = inferlab_plan::current_utc_timestamp();

  const auto& dependencies = inferlab_high_value::dependency_specs();
  plan.dependencies = {
      &require_dependency(dependencies, "google/benchmark"),
      &require_dependency(dependencies, "CUDA NVTX"),
      &require_dependency(dependencies, "CUDA Toolkit")};

  const auto& operators = inferlab_high_value::microbench_operators();
  if (options.selected_operator == "all") {
    for (const auto& op : operators) plan.selected_operators.push_back(&op);
  } else {
    plan.selected_operators.push_back(
        &require_operator(operators, options.selected_operator));
  }

  plan.available_cases = build_case_fixture(operators);
  for (const auto& benchmark_case : plan.available_cases) {
    bool operator_selected = false;
    for (const auto* op : plan.selected_operators) {
      operator_selected = operator_selected ||
                          (op != nullptr &&
                           op->name == benchmark_case.operator_name);
    }
    const bool profile_selected =
        options.all_profiles ||
        benchmark_case.profile == options.selected_profile;
    if (operator_selected && profile_selected) {
      plan.selected_cases.push_back(benchmark_case);
    }
  }
  if (plan.selected_cases.empty()) {
    throw std::logic_error(
        "microbenchmark selector resolved to no execution cases");
  }

  plan.failure_classes = {
      {"shape_mismatch",
       "candidate output shape or layout does not match the CPU oracle"},
      {"non_finite_output",
       "candidate or CPU oracle output contains a non-finite value"},
      {"numerical_mismatch",
       "candidate error exceeds the typed case tolerance"},
      {"backend_unavailable",
       "the selected execution backend was not compiled into the target"},
      {"kernel_runtime_error",
       "candidate runtime or kernel launch raised an error"},
      {"benchmark_unavailable",
       "correctness is available but google/benchmark is not compiled"},
      {"profiling_missing",
       "timing exists but no external Nsight report was captured"}};

  for (const auto& benchmark_case : plan.selected_cases) {
    const auto& op = operator_for_case(plan, benchmark_case);
    const std::string execute = case_execution_command(plan, benchmark_case);
    const std::string nsight_systems =
        "nsys profile --trace=cuda,nvtx " + execute;
    const std::string nsight_compute = "ncu --set full " + execute;
    const std::string configure =
        plan.options.backend == ExecutionBackend::cuda_runtime
            ? "cmake -B build -DINFERLAB_ENABLE_CUDA=ON "
              "-DINFERLAB_ENABLE_GOOGLE_BENCHMARK=ON "
              "-DINFERLAB_ENABLE_NVTX=ON"
            : "cmake -B build -DINFERLAB_ENABLE_GOOGLE_BENCHMARK=ON";
    CorrectnessResult correctness;
    correctness.case_id = benchmark_case.case_id;
    correctness.operator_name = benchmark_case.operator_name;
    correctness.oracle = op.correctness_oracle;
    correctness.output_item_count = benchmark_case.output_item_count;
    plan.correctness_results.push_back(std::move(correctness));
    plan.timing_results.push_back(
        {benchmark_case.case_id, benchmark_case.operator_name});
    plan.profile_metadata.push_back(
        {benchmark_case.case_id,
         benchmark_case.operator_name,
         op.nvtx_range,
         nsight_systems,
         nsight_compute});
    plan.execution_commands.push_back(
        {benchmark_case.case_id,
         benchmark_case.operator_name,
         configure,
         execute,
         nsight_systems,
         nsight_compute});
  }

  finalize_harness_plan(plan);
  return plan;
}

void finalize_harness_plan(HarnessPlan& plan) {
  if (plan.artifact == nullptr || plan.selected_operators.empty() ||
      plan.selected_cases.empty()) {
    throw std::logic_error("microbenchmark plan is incomplete");
  }
  validate_result_alignment(plan);

  HarnessSummary summary;
  summary.operator_count = inferlab_high_value::microbench_operators().size();
  summary.selected_operator_count = plan.selected_operators.size();
  summary.available_case_count = plan.available_cases.size();
  summary.selected_case_count = plan.selected_cases.size();

  plan.blockers.clear();
  plan.not_run.clear();
  plan.recommended_next_commands.clear();
  plan.can_claim.clear();
  plan.cannot_claim.clear();
  plan.requires_execute_before_claim.clear();
  if (plan.options.execute_requested &&
      plan.options.backend == ExecutionBackend::cuda_runtime &&
      !plan.capabilities.cuda_runtime_compiled) {
    push_unique(plan.blockers, "cuda_runtime_not_compiled");
  }
  if (plan.options.execute_requested &&
      !plan.capabilities.google_benchmark_compiled) {
    push_unique(plan.blockers, "google_benchmark_not_compiled");
  }

  for (const auto& result : plan.correctness_results) {
    if (result.attempted) {
      summary.correctness_attempt_count = internal::checked_add_size(
          summary.correctness_attempt_count, 1U,
          "microbenchmark correctness attempt count");
    }
    if (result.comparison_performed) {
      summary.correctness_comparison_count = internal::checked_add_size(
          summary.correctness_comparison_count, 1U,
          "microbenchmark correctness comparison count");
    }
    if (result.reference_generated) {
      summary.reference_generation_count = internal::checked_add_size(
          summary.reference_generation_count, 1U,
          "microbenchmark reference generation count");
    }
    if (result.state == CorrectnessState::passed) {
      summary.correctness_pass_count = internal::checked_add_size(
          summary.correctness_pass_count, 1U,
          "microbenchmark correctness pass count");
    } else if (result.state == CorrectnessState::failed ||
               result.state == CorrectnessState::runtime_error) {
      summary.correctness_failure_count = internal::checked_add_size(
          summary.correctness_failure_count, 1U,
          "microbenchmark correctness failure count");
      push_unique(plan.blockers,
                  "correctness_not_satisfied:" + result.case_id);
    }
  }

  for (const auto& result : plan.timing_results) {
    if (result.attempted) {
      summary.timing_attempt_count = internal::checked_add_size(
          summary.timing_attempt_count, 1U,
          "microbenchmark timing attempt count");
    }
    if (result.state == TimingState::completed) {
      summary.timing_result_count = internal::checked_add_size(
          summary.timing_result_count, 1U,
          "microbenchmark timing result count");
    } else if (result.state == TimingState::runtime_error) {
      summary.timing_failure_count = internal::checked_add_size(
          summary.timing_failure_count, 1U,
          "microbenchmark timing failure count");
      push_unique(plan.blockers, "timing_failed:" + result.case_id);
    } else if (plan.options.execute_requested &&
               result.state == TimingState::blocked_by_correctness) {
      push_unique(plan.blockers,
                  "timing_blocked_by_correctness:" + result.case_id);
    }
  }

  plan.executed = summary.correctness_attempt_count != 0U ||
                  summary.timing_attempt_count != 0U;
  plan.mode = plan.executed ? "execute" : "dry_run";
  plan.replayed = false;

  if (!plan.options.execute_requested) {
    plan.decision = HarnessDecision::manifest_ready;
    plan.evidence_confidence = "schema_only";
    plan.claim_scope =
        "typed microbenchmark execution manifest only; no correctness, timing, "
        "profiling, GPU performance, product behavior, or release evidence";
    plan.not_run = {"kernel correctness", "google/benchmark timing",
                    "CUDA execution", "NVTX ranges", "Nsight Systems",
                    "Nsight Compute"};
  } else if (!plan.executed) {
    plan.decision = HarnessDecision::execute_blocked;
    plan.evidence_confidence = "execute_blocked";
    plan.claim_scope =
        "execution request and blockers only; no kernel execution or timing "
        "evidence was produced";
    plan.not_run = {"kernel correctness", "google/benchmark timing",
                    "NVTX ranges", "Nsight Systems", "Nsight Compute"};
  } else if (summary.correctness_failure_count != 0U) {
    plan.decision = HarnessDecision::correctness_failed;
    plan.evidence_confidence = "execution_failed";
    plan.claim_scope =
        "kernel execution failure evidence only; failed correctness blocks all "
        "timing and performance claims";
    plan.not_run = {"valid kernel timing", "Nsight Systems",
                    "Nsight Compute"};
  } else if (summary.timing_failure_count != 0U) {
    plan.decision = HarnessDecision::timing_failed;
    plan.evidence_confidence = "execution_failed";
    plan.claim_scope =
        "correctness and timing failure evidence for the selected local "
        "microkernel cases only; no performance gain or product claim";
    plan.not_run = {"valid timing for failed cases", "Nsight Systems",
                    "Nsight Compute"};
  } else if (plan.options.backend == ExecutionBackend::cpu_reference &&
             summary.reference_generation_count ==
                 plan.selected_cases.size() &&
             summary.timing_result_count == plan.selected_cases.size()) {
    plan.decision = HarnessDecision::cpu_reference_executed;
    plan.evidence_confidence = "cpu_reference_executed";
    plan.claim_scope =
        "CPU reference baseline execution and timing for selected cases only; "
        "not candidate correctness, CUDA timing, GPU performance, product "
        "behavior, or release evidence";
    plan.not_run = {"CUDA candidate correctness", "CUDA timing",
                    "NVTX ranges", "Nsight Systems", "Nsight Compute"};
  } else if (summary.timing_result_count != plan.selected_cases.size()) {
    plan.decision = HarnessDecision::correctness_verified_timing_blocked;
    plan.evidence_confidence =
        plan.options.backend == ExecutionBackend::cuda_runtime
            ? "correctness_verified"
            : "cpu_reference_executed";
    plan.claim_scope =
        "selected-case correctness or CPU reference execution only; timing "
        "remains incomplete and no performance gain, product, or release claim "
        "is allowed";
    plan.not_run = {"complete google/benchmark timing", "Nsight Systems",
                    "Nsight Compute"};
  } else {
    plan.decision = HarnessDecision::execute_verified;
    plan.evidence_confidence = "execute_verified";
    plan.claim_scope =
        "selected-case CUDA correctness and google/benchmark timing from this "
        "binary execution only; no cross-product speedup, production SLO, or "
        "release claim";
    plan.not_run = {"Nsight Systems report capture",
                    "Nsight Compute report capture", "product benchmark"};
  }
  if (plan.executed &&
      plan.options.backend == ExecutionBackend::cuda_runtime &&
      !plan.capabilities.nvtx_compiled) {
    push_unique(plan.not_run, "NVTX ranges");
  }

  plan.can_claim.push_back(
      "the C++17 microbenchmark fixture, CPU oracle, optional CUDA runner, "
      "google/benchmark reporter, and profiling hooks are implemented");
  if (plan.executed) {
    plan.can_claim.push_back(plan.claim_scope);
  } else {
    plan.can_claim.push_back(
        "the selected manifest and execution prerequisites were projected");
  }
  plan.cannot_claim = {
      "unselected shapes, operators, backends, devices, products, or workloads",
      "a speedup against FlashInfer, TensorRT-LLM, vLLM, SGLang, or any other product",
      "production throughput, latency, cost, SLO, or release readiness"};
  if (!plan.executed) {
    plan.cannot_claim.push_back(
        "kernel correctness, timing, throughput, occupancy, or bandwidth");
  }
  plan.requires_execute_before_claim = {
      "run the selected correctness oracle and candidate backend",
      "allow timing only after the selected case satisfies correctness",
      "capture Nsight reports separately before making profile-derived claims",
      "refresh downstream contract, freshness, summary, Prometheus, and final-report consumers"};

  plan.status = harness_decision_name(plan.decision);
  plan.target_gpu_execute_verified =
      plan.options.backend == ExecutionBackend::cuda_runtime &&
      plan.decision == HarnessDecision::execute_verified &&
      summary.selected_operator_count == summary.operator_count;
  if (plan.decision == HarnessDecision::execute_verified &&
      !plan.target_gpu_execute_verified) {
    push_unique(plan.blockers,
                "target_gpu_gate_requires_all_canonical_operators");
  }
  if (plan.decision == HarnessDecision::cpu_reference_executed) {
    push_unique(plan.blockers,
                "target_gpu_gate_requires_cuda_runtime_execution");
  }
  summary.blocker_count = plan.blockers.size();
  plan.summary = summary;

  push_unique(plan.recommended_next_commands, plan.artifact->command);
  if (!plan.execution_commands.empty()) {
    if ((plan.options.backend == ExecutionBackend::cuda_runtime &&
         !plan.capabilities.cuda_runtime_compiled) ||
        !plan.capabilities.google_benchmark_compiled) {
      push_unique(plan.recommended_next_commands,
                  plan.execution_commands.front().configure);
    }
    if (!plan.options.execute_requested) {
      push_unique(plan.recommended_next_commands,
                  selected_execution_command(plan));
    }
  }
  if (!plan.target_gpu_execute_verified &&
      (plan.options.backend == ExecutionBackend::cpu_reference ||
       !plan.capabilities.cuda_runtime_compiled ||
       !plan.capabilities.google_benchmark_compiled)) {
    push_unique(
        plan.recommended_next_commands,
        "cmake -B build -DINFERLAB_ENABLE_CUDA=ON "
        "-DINFERLAB_ENABLE_GOOGLE_BENCHMARK=ON "
        "-DINFERLAB_ENABLE_NVTX=ON");
  }
  if (!plan.target_gpu_execute_verified) {
    push_unique(plan.recommended_next_commands,
                target_gpu_execution_command(plan));
  }
}

}  // namespace inferlab::kernel_microbench
