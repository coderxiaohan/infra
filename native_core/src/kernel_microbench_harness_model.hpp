#pragma once

#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace inferlab::kernel_microbench {

enum class ExecutionBackend {
  cuda_runtime,
  cpu_reference,
};

enum class CaseProfile {
  smoke,
  serving_like,
  stress,
};

enum class CorrectnessState {
  not_requested,
  backend_unavailable,
  reference_generated,
  passed,
  failed,
  runtime_error,
};

enum class TimingState {
  not_requested,
  blocked_by_correctness,
  benchmark_unavailable,
  completed,
  runtime_error,
};

enum class HarnessDecision {
  manifest_ready,
  execute_blocked,
  cpu_reference_executed,
  correctness_verified_timing_blocked,
  correctness_failed,
  timing_failed,
  execute_verified,
};

const char* execution_backend_name(ExecutionBackend backend);
const char* case_profile_name(CaseProfile profile);
const char* correctness_state_name(CorrectnessState state);
const char* timing_state_name(TimingState state);
const char* harness_decision_name(HarnessDecision decision);

ExecutionBackend parse_execution_backend(const std::string& value);
CaseProfile parse_case_profile(const std::string& value);

struct RmsNormShape {
  int rows = 0;
  int hidden = 0;
};

struct RopeShape {
  int sequence = 0;
  int head_dimension = 0;
};

struct RowSoftmaxShape {
  int rows = 0;
  int columns = 0;
};

struct Int4DequantMatvecShape {
  int output_features = 0;
  int input_features = 0;
  int group_size = 0;
};

struct PagedKvGatherShape {
  int tokens = 0;
  int block_size = 0;
  int heads = 0;
  int head_dimension = 0;
};

struct DecodeAttentionShape {
  int heads = 0;
  int context = 0;
  int head_dimension = 0;
};

using CaseShape =
    std::variant<RmsNormShape, RopeShape, RowSoftmaxShape,
                 Int4DequantMatvecShape, PagedKvGatherShape,
                 DecodeAttentionShape>;

struct CaseSpec {
  const inferlab_high_value::MicrobenchOperator* operator_contract = nullptr;
  std::string case_id;
  std::string operator_name;
  CaseProfile profile = CaseProfile::smoke;
  CaseShape shape;
  std::string benchmark_name;
  std::string execution_dtype_layout;
  double absolute_tolerance = 0.0;
  double relative_tolerance = 0.0;
  std::size_t output_item_count = 0;
  int deterministic_seed = 0;
};

struct BuildCapabilities {
  bool google_benchmark_compiled = false;
  bool cuda_runtime_compiled = false;
  bool nvtx_compiled = false;
};

struct HarnessOptions {
  std::string selected_operator = "all";
  CaseProfile selected_profile = CaseProfile::smoke;
  bool all_profiles = false;
  ExecutionBackend backend = ExecutionBackend::cuda_runtime;
  bool execute_requested = false;
  double benchmark_min_time_ms = 20.0;
  int benchmark_repetitions = 5;
};

struct CorrectnessResult {
  std::string case_id;
  std::string operator_name;
  std::string oracle;
  CorrectnessState state = CorrectnessState::not_requested;
  bool attempted = false;
  bool reference_generated = false;
  bool comparison_performed = false;
  bool passed = false;
  std::optional<double> max_absolute_error;
  std::optional<double> max_relative_error;
  std::size_t mismatch_count = 0;
  std::size_t output_item_count = 0;
  std::string error;
};

struct TimingResult {
  std::string case_id;
  std::string operator_name;
  TimingState state = TimingState::not_requested;
  bool attempted = false;
  std::size_t reported_run_count = 0;
  std::optional<double> latency_us_p50;
  std::optional<double> latency_us_p99;
  std::optional<double> throughput_items_per_second;
  std::string timing_source;
  std::string error;
};

struct ProfileMetadata {
  std::string case_id;
  std::string operator_name;
  std::string nvtx_range;
  std::string nsight_systems_command;
  std::string nsight_compute_command;
  bool nvtx_range_emitted = false;
  bool nsight_report_captured = false;
};

struct FailureClass {
  std::string name;
  std::string meaning;
};

struct ExecutionCommand {
  std::string case_id;
  std::string operator_name;
  std::string configure;
  std::string correctness_and_timing;
  std::string nsight_systems;
  std::string nsight_compute;
};

struct HarnessSummary {
  std::size_t operator_count = 0;
  std::size_t selected_operator_count = 0;
  std::size_t available_case_count = 0;
  std::size_t selected_case_count = 0;
  std::size_t correctness_attempt_count = 0;
  std::size_t correctness_comparison_count = 0;
  std::size_t correctness_pass_count = 0;
  std::size_t correctness_failure_count = 0;
  std::size_t reference_generation_count = 0;
  std::size_t timing_attempt_count = 0;
  std::size_t timing_result_count = 0;
  std::size_t timing_failure_count = 0;
  std::size_t blocker_count = 0;
};

struct HarnessPlan {
  const inferlab_high_value::ManagedArtifact* artifact = nullptr;
  std::vector<const inferlab_high_value::DependencySpec*> dependencies;
  std::vector<const inferlab_high_value::MicrobenchOperator*>
      selected_operators;
  std::vector<CaseSpec> available_cases;
  std::vector<CaseSpec> selected_cases;
  HarnessOptions options;
  BuildCapabilities capabilities;
  std::vector<CorrectnessResult> correctness_results;
  std::vector<TimingResult> timing_results;
  std::vector<ProfileMetadata> profile_metadata;
  std::vector<FailureClass> failure_classes;
  std::vector<ExecutionCommand> execution_commands;
  std::vector<std::string> blockers;
  std::vector<std::string> not_run;
  std::vector<std::string> recommended_next_commands;
  std::vector<std::string> can_claim;
  std::vector<std::string> cannot_claim;
  std::vector<std::string> requires_execute_before_claim;
  HarnessSummary summary;
  HarnessDecision decision = HarnessDecision::manifest_ready;
  std::string artifact_type;
  std::string generated_at;
  std::string mode = "dry_run";
  std::string status = "manifest_ready";
  std::string evidence_confidence = "schema_only";
  std::string claim_scope;
  std::string repo_root_label;
  std::string output_path_label;
  bool executed = false;
  bool replayed = false;
  bool target_gpu_execute_verified = false;
};

HarnessPlan build_harness_plan(const HarnessOptions& options,
                               const BuildCapabilities& capabilities);

void finalize_harness_plan(HarnessPlan& plan);

}  // namespace inferlab::kernel_microbench
