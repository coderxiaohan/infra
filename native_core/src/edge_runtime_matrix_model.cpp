#include "edge_runtime_matrix_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "edge_runtime_input_snapshot.hpp"
#include "edge_runtime_matrix_execution.hpp"
#include "edge_runtime_matrix_internal.hpp"
#include "edge_runtime_quality_comparison.hpp"
#include "edge_runtime_resource_plan.hpp"
#include "inferlab/edge_runtime.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace inferlab::edge_runtime_matrix {
namespace {

using matrix_detail::append_unique;

constexpr std::size_t kMaximumConfigBytes = 1024U * 1024U;
constexpr std::size_t kMaximumWarmupIterations = 100U;
constexpr std::size_t kMaximumMeasurementIterations = 10000U;
constexpr std::uint64_t kMaximumSustainedDurationMs = 3600000U;
bool checked_size_product_equals(const std::size_t left,
                                 const std::size_t right,
                                 const std::size_t expected) noexcept {
  if (left != 0U &&
      right > std::numeric_limits<std::size_t>::max() / left) {
    return false;
  }
  return left * right == expected;
}

class ProtectedInputOutputAlias final : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

void reject_output_alias(const std::filesystem::path& input_path,
                         const std::filesystem::path& output_path,
                         const std::string& input_role) {
  if (!inferlab_plan::paths_alias(input_path, output_path)) return;
  throw ProtectedInputOutputAlias(
      std::string("edge runtime matrix --output must not alias ") +
      input_role);
}

const char* build_architecture() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#else
  return "unknown";
#endif
}

const char* build_system() noexcept {
#if defined(__ANDROID__)
  return "android";
#elif defined(__linux__)
  return "linux";
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
  return "ios";
#else
  return "macos";
#endif
#elif defined(_WIN32)
  return "windows";
#else
  return "unknown";
#endif
}

bool target_matches_build(const EdgeRuntimeTargetPlatform& target,
                          const std::string& system,
                          const std::string& architecture) {
  return system == target.build_system &&
         architecture == target.build_architecture;
}

std::string edge_build_directory(const std::string& target_platform) {
  if (target_platform == "linux-aarch64") {
    return "build-edge-linux-aarch64";
  }
  if (target_platform == "linux-x86_64") {
    return "build-edge-linux-x86_64";
  }
  return "build-edge-target";
}

std::string runtime_configure_command(const std::string& target_platform,
                                      const bool target_platform_matches,
                                      const RuntimeRow& row) {
  const auto* registration = find_edge_runtime_registration(row.runtime_id);
  if (registration == nullptr) {
    return "review configs/edge_runtime_matrix.example.json runtime id";
  }
  std::string command = "cmake -B " + edge_build_directory(target_platform) +
                        " -D" +
                        registration->cmake_option +
                        "=ON -DINFERLAB_ENABLE_ARM_NEON=" +
                        (target_platform == "linux-aarch64" ? "ON" : "OFF");
  if (!target_platform_matches) {
    command += " -DCMAKE_TOOLCHAIN_FILE=/path/to/" + target_platform +
               "-toolchain.cmake";
  }
  if (row.runtime_id == "llama_cpp") {
    command +=
        " -DINFERLAB_LLAMA_CPP_SOURCE_DIR=/path/to/llama.cpp";
  } else if (row.runtime_id == "executorch") {
    command +=
        " -DINFERLAB_EXECUTORCH_SOURCE_DIR=/path/to/executorch-v1.2.0"
        " -DINFERLAB_EXECUTORCH_INSTALL_DIR=/path/to/executorch-v1.2.0-install"
        " -DINFERLAB_EXECUTORCH_BUILD_REVISION_FILE=/path/to/executorch-v1.2.0-install/source-revision.txt";
  }
  return command;
}

std::string prompt_fingerprint(const std::string& prompt) {
  if (prompt.empty()) return {};
  return inferlab_plan::fnv1a64_fingerprint(prompt);
}

struct TargetRuntimeContractStatus {
  bool runtime_registered = false;
  bool version_matches = false;
  bool model_format_declared = false;
  bool backend_declared = false;
  bool backend_supported_on_target = false;

  bool supported() const noexcept {
    return runtime_registered && version_matches && model_format_declared &&
           backend_declared && backend_supported_on_target;
  }
};

TargetRuntimeContractStatus target_runtime_contract_status(
    const RuntimeRow& row,
    const EdgeRuntimeArchitecture target_architecture) {
  TargetRuntimeContractStatus status;
  const auto* registration = find_edge_runtime_registration(row.runtime_id);
  if (registration == nullptr) return status;

  status.runtime_registered = true;
  status.version_matches =
      row.runtime_version ==
      registration->declared_capabilities.runtime_version;
  status.model_format_declared =
      std::find(registration->declared_capabilities.model_formats.begin(),
                registration->declared_capabilities.model_formats.end(),
                row.model_format) !=
      registration->declared_capabilities.model_formats.end();
  status.backend_declared =
      std::find(registration->declared_capabilities.backends.begin(),
                registration->declared_capabilities.backends.end(),
                row.backend) !=
      registration->declared_capabilities.backends.end();
  const auto target_capabilities = edge_runtime_capabilities_for_architecture(
      row.runtime_id, target_architecture);
  status.backend_supported_on_target =
      status.backend_declared &&
      std::find(target_capabilities.backends.begin(),
                target_capabilities.backends.end(), row.backend) !=
          target_capabilities.backends.end();
  return status;
}

void mark_execute_preconditions_blocked(RuntimeRow* row) {
  row->execution_protocol.status = "blocked_before_execute";
  row->concurrent_execution.finish_alignment_status =
      "blocked_before_execute";
  row->concurrent_execution.status = "blocked_before_execute";
  row->concurrent_execution.provenance =
      "unavailable_execute_preconditions_blocked";
  row->load_status = "blocked_before_execute";
  row->generate_status = "blocked_before_execute";
  row->cancel_status = "blocked_before_execute";
  row->reset_status = "blocked_before_execute";
  row->unload_status = "blocked_before_execute";
  row->metrics.provenance = "unavailable_execute_preconditions_blocked";
  append_unique(&row->blockers, "execute_preconditions_not_met");
}

EdgeRuntimeInputSnapshotComparison observe_input_snapshot_stability(
    const std::filesystem::path& path,
    const EdgeRuntimeInputSnapshot& baseline, const bool before_execute,
    std::string* status) {
  if (status == nullptr) {
    throw std::invalid_argument(
        "edge runtime input stability status must not be null");
  }
  const auto current = capture_edge_runtime_input_snapshot(path);
  const auto comparison =
      compare_edge_runtime_input_snapshots(baseline, current);
  switch (comparison) {
    case EdgeRuntimeInputSnapshotComparison::kStable:
      *status = before_execute ? "stable_before_execute"
                               : "stable_through_execute_attempt";
      break;
    case EdgeRuntimeInputSnapshotComparison::kChanged:
      *status = before_execute ? "changed_after_preflight"
                               : "changed_during_execute_attempt";
      break;
    case EdgeRuntimeInputSnapshotComparison::kUnavailable:
      *status = before_execute
                    ? "metadata_unavailable_before_execute"
                    : "metadata_unavailable_after_execute_attempt";
      break;
  }
  return comparison;
}

void append_input_snapshot_blocker(
    const char* role, const bool before_execute,
    const EdgeRuntimeInputSnapshotComparison comparison, RuntimeRow* row) {
  const std::string prefix(role);
  if (comparison == EdgeRuntimeInputSnapshotComparison::kChanged) {
    append_unique(&row->blockers,
                  prefix + (before_execute ? "_changed_after_preflight"
                                           : "_changed_during_execute"));
  } else if (comparison ==
             EdgeRuntimeInputSnapshotComparison::kUnavailable) {
    append_unique(
        &row->blockers,
        prefix + (before_execute ? "_snapshot_unavailable_before_execute"
                                 : "_snapshot_unavailable_after_execute"));
  }
}

void prepare_row(const std::filesystem::path& repo_root,
                 const std::filesystem::path& model_path,
                 const std::filesystem::path& tokenizer_path,
                 const std::string& prompt,
                 const EdgeRuntimeArchitecture target_architecture,
                 const bool target_platform_matches,
                 const Mode mode,
                 EdgeRuntimeInputSnapshot* model_input_snapshot,
                 EdgeRuntimeInputSnapshot* tokenizer_input_snapshot,
                 RuntimeRow* row) {
  if (model_input_snapshot == nullptr || tokenizer_input_snapshot == nullptr ||
      row == nullptr) {
    throw std::invalid_argument(
        "edge runtime preflight snapshot and row must not be null");
  }
  const auto resolved_model =
      inferlab_plan::resolve_repo_relative_path(repo_root, model_path);
  *model_input_snapshot =
      capture_edge_runtime_input_snapshot(resolved_model);
  row->model_input_snapshot_source = model_input_snapshot->metadata_source;
  row->model_input_stability_status =
      model_input_snapshot->observed
          ? (mode == Mode::kPlan
                 ? "preflight_metadata_observed_not_executed_plan"
                 : "preflight_metadata_observed_execute_pending")
          : "preflight_metadata_unavailable";

  const auto* registration = find_edge_runtime_registration(row->runtime_id);
  row->build_option_enabled =
      registration != nullptr && registration->build_option_enabled;
  row->dependency_linked =
      registration != nullptr &&
      registration->declared_capabilities.dependency_linked;
  row->adapter_implemented =
      registration != nullptr && registration->adapter_implemented;
  row->thread_control_status =
      registration != nullptr &&
              registration->declared_capabilities
                  .supports_explicit_thread_count
          ? "explicit_request_supported_not_executed"
          : "runtime_managed_not_explicitly_controlled";
  row->model_path_label =
      inferlab_plan::redacted_path_label(model_path.generic_string());
  std::filesystem::path resolved_tokenizer;
  if (tokenizer_path.empty()) {
    *tokenizer_input_snapshot = {};
    row->tokenizer_path_label = row->runtime_id == "llama_cpp"
                                    ? "embedded_in_gguf"
                                    : "not_configured";
    row->tokenizer_path_status = row->runtime_id == "llama_cpp"
                                     ? "embedded_in_model"
                                     : "missing";
    row->tokenizer_input_snapshot_source =
        row->runtime_id == "llama_cpp" ? "embedded_in_model"
                                        : "not_configured";
    row->tokenizer_input_stability_status =
        row->runtime_id == "llama_cpp" ? "covered_by_model_snapshot"
                                        : "not_configured";
  } else {
    row->tokenizer_path_label =
        inferlab_plan::redacted_path_label(tokenizer_path.generic_string());
    resolved_tokenizer =
        inferlab_plan::resolve_repo_relative_path(repo_root, tokenizer_path);
    *tokenizer_input_snapshot =
        capture_edge_runtime_input_snapshot(resolved_tokenizer);
    row->tokenizer_input_snapshot_source =
        tokenizer_input_snapshot->metadata_source;
    row->tokenizer_input_stability_status =
        tokenizer_input_snapshot->observed
            ? (mode == Mode::kPlan
                   ? "preflight_metadata_observed_not_executed_plan"
                   : "preflight_metadata_observed_execute_pending")
            : "preflight_metadata_unavailable";
    row->tokenizer_path_status =
        inferlab_artifact::path_probe_status(resolved_tokenizer);
  }
  row->prompt_bytes = prompt.size();
  row->prompt_fingerprint = prompt_fingerprint(prompt);
  row->model_path_status =
      inferlab_artifact::path_probe_status(resolved_model);
  prepare_edge_resource_plan(
      model_input_snapshot->observed
          ? std::optional<std::uint64_t>(
                model_input_snapshot->file_size_bytes)
          : std::nullopt,
      row);
  if (row->model_path_status == "file" &&
      !model_input_snapshot->observed) {
    append_unique(&row->blockers, "model_input_snapshot_unavailable");
  }
  if (row->model_file_bytes.has_value() && *row->model_file_bytes == 0U) {
    append_unique(&row->blockers, "model_artifact_empty_or_unreadable");
  }
  if (row->estimated_resource_fit == false) {
    append_unique(&row->blockers, "estimated_memory_budget_exceeded");
  }
  if (row->mmap_requested &&
      row->mmap_strategy == "mmap_requested_runtime_unsupported") {
    append_unique(&row->blockers, "mmap_requested_but_runtime_unsupported");
  }

  const auto target_contract =
      target_runtime_contract_status(*row, target_architecture);
  row->target_runtime_contract_supported = target_contract.supported();
  if (!target_contract.runtime_registered) {
    append_unique(&row->blockers, "unsupported_runtime_id");
  } else {
    if (!target_contract.version_matches) {
      append_unique(&row->blockers, "runtime_version_pin_mismatch");
    }
    if (!target_contract.model_format_declared) {
      append_unique(&row->blockers, "runtime_model_format_mismatch");
    }
    if (!target_contract.backend_declared) {
      append_unique(&row->blockers, "runtime_backend_not_declared");
    } else if (!target_contract.backend_supported_on_target) {
      append_unique(&row->blockers,
                    "runtime_backend_not_supported_on_target");
    }
  }
  if (!row->build_option_enabled) {
    append_unique(&row->blockers, "runtime_build_option_disabled");
  }
  if (!row->dependency_linked) {
    append_unique(&row->blockers, "runtime_dependency_not_linked");
  }
  if (!row->adapter_implemented) {
    append_unique(&row->blockers, "runtime_adapter_not_implemented");
  }
  if (row->model_path_status != "file") {
    append_unique(&row->blockers, "model_artifact_not_ready");
  }
  if (row->runtime_id == "executorch" &&
      row->tokenizer_path_status != "file") {
    append_unique(&row->blockers, "tokenizer_artifact_not_ready");
  }
  if (!tokenizer_path.empty() && row->tokenizer_path_status == "file" &&
      !tokenizer_input_snapshot->observed) {
    append_unique(&row->blockers, "tokenizer_input_snapshot_unavailable");
  }
  if (!tokenizer_path.empty() && tokenizer_input_snapshot->observed &&
      tokenizer_input_snapshot->file_size_bytes == 0U) {
    append_unique(&row->blockers, "tokenizer_artifact_empty_or_unreadable");
  }
  if (prompt.empty()) {
    append_unique(&row->blockers, "generation_prompt_missing");
  }
  if (mode == Mode::kExecute &&
      row->concurrent_sessions > kMaximumConcurrentExecuteSessions) {
    append_unique(&row->blockers,
                  "concurrent_sessions_execute_limit_exceeded");
  }
  if (mode == Mode::kExecute && !target_platform_matches) {
    append_unique(&row->blockers, "target_platform_build_mismatch");
  }

  row->executed = false;
  row->execution_protocol.completed_warmup_iterations = 0U;
  row->execution_protocol.completed_measurement_iterations = 0U;
  row->execution_protocol.observed_measurement_duration_ms.reset();
  row->execution_protocol.status = mode == Mode::kPlan
                                       ? "not_run_plan"
                                       : "not_observed_execute_not_started";
  row->execution_protocol.aggregation =
      "median_warm_latency_and_throughput_max_process_memory_latest_mapping";
  row->concurrent_execution = {};
  row->concurrent_execution.requested_sessions = row->concurrent_sessions;
  row->concurrent_execution.finish_alignment_status =
      mode == Mode::kPlan ? "not_observed_plan"
                          : "not_observed_execute_not_started";
  row->concurrent_execution.status =
      mode == Mode::kPlan ? "not_run_plan"
                          : "not_observed_execute_not_started";
  row->concurrent_execution.provenance =
      mode == Mode::kPlan ? "unavailable_not_executed_plan"
                          : "unavailable_execute_not_started";
  row->thermal_before_status = mode == Mode::kPlan
                                   ? "not_run_plan"
                                   : "not_observed_execute_not_started";
  row->thermal_after_status = row->thermal_before_status;
  row->thermal_provenance = mode == Mode::kPlan
                                ? "unavailable_not_executed_plan"
                                : "unavailable_execute_not_started";
  row->observed_energy_uj.reset();
  row->energy_root_zone_count = 0U;
  row->energy_before_status = mode == Mode::kPlan
                                  ? "not_run_plan"
                                  : "not_observed_execute_not_started";
  row->energy_after_status = row->energy_before_status;
  row->energy_status = mode == Mode::kPlan
                           ? "not_run_plan"
                           : "not_observed_execute_not_started";
  row->energy_provenance = mode == Mode::kPlan
                               ? "unavailable_not_executed_plan"
                               : "unavailable_execute_not_started";
  row->capability_status = row->blockers.empty()
                               ? "execute_supported"
                               : "target_machine_pending";
  if (mode == Mode::kPlan) {
    row->load_status = "not_run_plan";
    row->generate_status = "not_run_plan";
    row->cancel_status = "not_run_plan";
    row->reset_status = "not_run_plan";
    row->unload_status = "not_run_plan";
    row->metrics.provenance = "unavailable_not_executed_plan";
  } else if (row->blockers.empty()) {
    row->load_status = "execute_preflight_ready";
    row->generate_status = "execute_preflight_ready";
    row->cancel_status = "execute_preflight_ready";
    row->reset_status = "execute_preflight_ready";
    row->unload_status = "execute_preflight_ready";
    row->metrics.provenance = "unavailable_execute_preflight_ready";
  } else {
    mark_execute_preconditions_blocked(row);
  }
}

void complete_prepared_row(const std::filesystem::path& repo_root,
                           const std::filesystem::path& model_path,
                           const std::filesystem::path& tokenizer_path,
                           const std::string& prompt, const Mode mode,
                           const EdgeRuntimeInputSnapshot& model_input_snapshot,
                           const EdgeRuntimeInputSnapshot& tokenizer_input_snapshot,
                           GeneratedTextCapture* generated_text,
                           RuntimeRow* row) {
  if (generated_text == nullptr || row == nullptr) {
    throw std::invalid_argument(
        "edge prepared row requires text capture and row");
  }
  *generated_text = {};
  generated_text->observed_bytes.reset();
  generated_text->status = mode == Mode::kPlan
                               ? "not_observed_plan"
                               : "blocked_before_execute";
  const auto resolved_model =
      inferlab_plan::resolve_repo_relative_path(repo_root, model_path);
  const auto resolved_tokenizer = tokenizer_path.empty()
                                      ? std::filesystem::path{}
                                      : inferlab_plan::resolve_repo_relative_path(
                                            repo_root, tokenizer_path);
  if (mode == Mode::kExecute && row->blockers.empty()) {
    const auto model_stability = observe_input_snapshot_stability(
        resolved_model, model_input_snapshot, true,
        &row->model_input_stability_status);
    append_input_snapshot_blocker("model_input", true, model_stability, row);
    if (!resolved_tokenizer.empty()) {
      const auto tokenizer_stability = observe_input_snapshot_stability(
          resolved_tokenizer, tokenizer_input_snapshot, true,
          &row->tokenizer_input_stability_status);
      append_input_snapshot_blocker("tokenizer_input", true,
                                    tokenizer_stability, row);
    }
    if (!row->blockers.empty()) {
      if (row->model_input_stability_status == "stable_before_execute") {
        row->model_input_stability_status =
            "stable_before_execute_attempt_not_started";
      }
      if (row->tokenizer_input_stability_status ==
          "stable_before_execute") {
        row->tokenizer_input_stability_status =
            "stable_before_execute_attempt_not_started";
      }
      row->capability_status = "target_machine_pending";
      mark_execute_preconditions_blocked(row);
    }
  }
  bool execute_attempted = false;
  if (mode == Mode::kExecute && row->blockers.empty()) {
    execute_attempted = true;
    try {
      execute_runtime_row(resolved_model, resolved_tokenizer, prompt,
                          generated_text, row);
    } catch (const std::exception&) {
      generated_text->text.clear();
      generated_text->observed_bytes.reset();
      generated_text->status = "unavailable_runtime_adapter_exception";
      row->executed = false;
      row->capability_status = "target_machine_execute_failed";
      row->execution_protocol.status = "failed_adapter_exception";
      row->concurrent_execution.finish_alignment_status =
          "failed_adapter_exception";
      row->concurrent_execution.status = "failed_adapter_exception";
      row->concurrent_execution.provenance =
          "unavailable_runtime_adapter_exception";
      row->observed_energy_uj.reset();
      row->energy_root_zone_count = 0U;
      row->energy_before_status = "unavailable_runtime_adapter_exception";
      row->energy_after_status = "unavailable_runtime_adapter_exception";
      row->energy_status = "unavailable_runtime_adapter_exception";
      row->energy_provenance = "unavailable_runtime_adapter_exception";
      if (row->execution_error_category.empty()) {
        row->execution_error_category = "runtime_adapter_exception";
      }
      if (row->load_status == "execute_preflight_ready") {
        row->load_status = "failed";
      }
      if (row->generate_status == "execute_preflight_ready") {
        row->generate_status = "failed";
      }
      if (row->cancel_status == "execute_preflight_ready") {
        row->cancel_status = "not_observed";
      }
      if (row->reset_status == "execute_preflight_ready") {
        row->reset_status = "not_observed";
      }
      if (row->unload_status == "execute_preflight_ready") {
        row->unload_status = "adapter_destructor_cleanup_only";
      }
      if (row->metrics.provenance.empty()) {
        row->metrics.provenance = "unavailable_runtime_adapter_exception";
      }
      append_unique(&row->blockers, "runtime_adapter_exception");
    }
  }
  if (execute_attempted) {
    const auto model_stability = observe_input_snapshot_stability(
        resolved_model, model_input_snapshot, false,
        &row->model_input_stability_status);
    append_input_snapshot_blocker("model_input", false, model_stability, row);
    EdgeRuntimeInputSnapshotComparison tokenizer_stability =
        EdgeRuntimeInputSnapshotComparison::kStable;
    if (!resolved_tokenizer.empty()) {
      tokenizer_stability = observe_input_snapshot_stability(
          resolved_tokenizer, tokenizer_input_snapshot, false,
          &row->tokenizer_input_stability_status);
      append_input_snapshot_blocker("tokenizer_input", false,
                                    tokenizer_stability, row);
    }
    if (model_stability != EdgeRuntimeInputSnapshotComparison::kStable ||
        tokenizer_stability != EdgeRuntimeInputSnapshotComparison::kStable) {
      generated_text->text.clear();
      generated_text->observed_bytes.reset();
      generated_text->status = "unavailable_runtime_input_stability";
      row->executed = false;
      row->capability_status = "target_machine_execute_failed";
      if (row->execution_error_category.empty()) {
        row->execution_error_category =
            "runtime_input_stability_not_proven";
      }
    }
  }
  finalize_edge_resource_observation(row);
  if (mode == Mode::kExecute && row->executed &&
      row->observed_resource_status ==
          "unavailable_working_set_metric_missing") {
    append_unique(&row->blockers, "observed_memory_metric_missing");
  }
  if (row->observed_resource_fit == false) {
    append_unique(&row->blockers, "observed_memory_budget_exceeded");
  }
}

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON

std::string required_string(const nlohmann::json& value,
                            const char* field) {
  if (!value.contains(field) || !value.at(field).is_string()) {
    throw std::invalid_argument(std::string("edge runtime row requires string ") +
                                field);
  }
  const std::string result = value.at(field).get<std::string>();
  if (result.empty()) {
    throw std::invalid_argument(std::string("edge runtime row field is empty: ") +
                                field);
  }
  return result;
}

std::size_t required_positive_size(const nlohmann::json& value,
                                   const char* field) {
  if (!value.contains(field) || !value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge runtime row requires unsigned integer ") + field);
  }
  const auto result = value.at(field).get<std::uint64_t>();
  if (result == 0U || result >
                          static_cast<std::uint64_t>(
                              std::numeric_limits<std::size_t>::max())) {
    throw std::invalid_argument(
        std::string("edge runtime row field must be positive: ") + field);
  }
  return static_cast<std::size_t>(result);
}

std::size_t optional_non_negative_size(const nlohmann::json& value,
                                       const char* field,
                                       const std::size_t fallback) {
  if (!value.contains(field)) return fallback;
  if (!value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge runtime row requires unsigned integer ") + field);
  }
  const auto result = value.at(field).get<std::uint64_t>();
  if (result > static_cast<std::uint64_t>(
                   std::numeric_limits<std::size_t>::max())) {
    throw std::invalid_argument(
        std::string("edge runtime row field exceeds size range: ") + field);
  }
  return static_cast<std::size_t>(result);
}

std::uint64_t optional_non_negative_u64(const nlohmann::json& value,
                                        const char* field,
                                        const std::uint64_t fallback) {
  if (!value.contains(field)) return fallback;
  if (!value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge runtime row requires unsigned integer ") + field);
  }
  return value.at(field).get<std::uint64_t>();
}

std::uint64_t required_non_negative_u64(const nlohmann::json& value,
                                        const char* field) {
  if (!value.contains(field) || !value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge runtime row requires unsigned integer ") + field);
  }
  const auto result = value.at(field).get<std::uint64_t>();
  if (result >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    throw std::invalid_argument(
        std::string("edge runtime row field exceeds byte range: ") + field);
  }
  return result;
}

std::uint64_t required_u64(const nlohmann::json& value,
                           const char* field) {
  if (!value.contains(field) || !value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge runtime row requires unsigned integer ") + field);
  }
  return value.at(field).get<std::uint64_t>();
}

int required_positive_int(const nlohmann::json& value, const char* field) {
  const auto result = required_positive_size(value, field);
  if (result > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(
        std::string("edge runtime row field exceeds int range: ") + field);
  }
  return static_cast<int>(result);
}

bool required_bool(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_boolean()) {
    throw std::invalid_argument(std::string("edge runtime row requires bool ") +
                                field);
  }
  return value.at(field).get<bool>();
}

float required_finite_float(const nlohmann::json& value,
                            const char* field, const double lower_bound,
                            const bool lower_bound_inclusive,
                            const double upper_bound) {
  if (!value.contains(field) || !value.at(field).is_number()) {
    throw std::invalid_argument(std::string("edge runtime row requires number ") +
                                field);
  }
  const double result = value.at(field).get<double>();
  if (!std::isfinite(result) ||
      result < -static_cast<double>(std::numeric_limits<float>::max()) ||
      result > static_cast<double>(std::numeric_limits<float>::max()) ||
      (lower_bound_inclusive ? result < lower_bound
                             : result <= lower_bound) ||
      result > upper_bound) {
    throw std::invalid_argument(
        std::string("edge runtime row field is outside allowed range: ") +
        field);
  }
  const float narrowed = static_cast<float>(result);
  if (!std::isfinite(narrowed) || (result != 0.0 && narrowed == 0.0F)) {
    throw std::invalid_argument(
        std::string("edge runtime row field loses value in float conversion: ") +
        field);
  }
  return narrowed;
}

void reject_output_aliases_runtime_inputs(
    const std::filesystem::path& repo_root,
    const nlohmann::json& runtime_rows,
    const std::filesystem::path& output_path) {
  if (output_path.empty() || !runtime_rows.is_array()) return;
  for (const auto& row : runtime_rows) {
    if (!row.is_object()) continue;
    for (const char* field : {"model_path", "tokenizer_path"}) {
      if (!row.contains(field) || !row.at(field).is_string()) continue;
      const std::filesystem::path configured_path(
          row.at(field).get<std::string>());
      if (configured_path.empty()) continue;
      const auto resolved_input = inferlab_plan::resolve_repo_relative_path(
          repo_root, configured_path);
      const std::string input_role = std::string("runtime ") + field;
      reject_output_alias(resolved_input, output_path, input_role);
    }
  }
}

void parse_config(const std::filesystem::path& repo_root,
                  const std::string& text, const Mode mode,
                  const std::filesystem::path& output_path, Matrix* matrix) {
  const auto document = nlohmann::json::parse(text);
  if (!document.is_object() ||
      !document.contains("schema_version") ||
      !document.at("schema_version").is_number_unsigned() ||
      !document.contains("target_platform") ||
      !document.at("target_platform").is_string() ||
      !document.contains("runtime_rows") ||
      !document.at("runtime_rows").is_array()) {
    throw std::invalid_argument(
        "edge runtime config requires schema_version, target_platform and "
        "runtime_rows");
  }
  if (document.at("runtime_rows").size() >
      kMaximumEdgeRuntimeMatrixRows) {
    throw std::invalid_argument(
        "edge runtime config exceeds bounded runtime row count");
  }
  reject_output_aliases_runtime_inputs(
      repo_root, document.at("runtime_rows"), output_path);
  const auto schema_version =
      document.at("schema_version").get<std::uint64_t>();
  matrix->config_schema_version = schema_version;
  if (schema_version != 1U && schema_version != 2U &&
      schema_version != 3U && schema_version != 4U) {
    throw std::invalid_argument(
        "edge runtime config schema_version must be 1, 2, 3 or 4");
  }
  matrix->target_platform =
      document.at("target_platform").get<std::string>();
  const auto* target_spec =
      find_edge_runtime_target_platform(matrix->target_platform);
  if (target_spec == nullptr) {
    throw std::invalid_argument(
        "edge runtime target_platform is not a canonical supported value");
  }
  const bool platform_matches =
      target_matches_build(*target_spec, matrix->build_system,
                           matrix->build_architecture);
  matrix->target_platform_match_status =
      platform_matches ? "matches_compiled_architecture"
                       : "mismatch_compiled_architecture";

  std::unordered_set<std::string> row_ids;
  struct ParsedRuntimeRow {
    RuntimeRow row;
    std::filesystem::path model_path;
    std::filesystem::path tokenizer_path;
    std::string prompt;
    EdgeRuntimeInputSnapshot model_input_snapshot;
    EdgeRuntimeInputSnapshot tokenizer_input_snapshot;
    GeneratedTextCapture generated_text;
  };
  std::vector<ParsedRuntimeRow> parsed_rows;
  std::string reference_prompt;
  bool reference_prompt_set = false;
  bool prompt_identity_mismatch = false;
  for (const auto& value : document.at("runtime_rows")) {
    if (!value.is_object()) {
      throw std::invalid_argument("edge runtime rows must be objects");
    }
    RuntimeRow row;
    row.row_id = required_string(value, "row_id");
    row.runtime_id = required_string(value, "runtime");
    row.runtime_version = required_string(value, "runtime_version");
    row.model_format = required_string(value, "model_format");
    row.backend = required_string(value, "backend");
    row.logical_model_id = required_string(value, "logical_model_id");
    row.checkpoint_revision = required_string(value, "checkpoint_revision");
    row.tokenizer_id = required_string(value, "tokenizer_id");
    row.conversion_provenance =
        schema_version >= 2U
            ? required_string(value, "conversion_provenance")
            : "unspecified_legacy_schema_v1";
    row.quantization = required_string(value, "quantization");
    std::string prompt;
    if (schema_version >= 2U) {
      prompt = required_string(value, "prompt");
      row.seed = required_u64(value, "seed");
      row.temperature = required_finite_float(
          value, "temperature", 0.0, true,
          static_cast<double>(std::numeric_limits<float>::max()));
      row.top_p = required_finite_float(value, "top_p", 0.0, false, 1.0);
    }
    if (!reference_prompt_set) {
      reference_prompt = prompt;
      reference_prompt_set = true;
    } else if (prompt != reference_prompt) {
      prompt_identity_mismatch = true;
    }
    row.thread_count = required_positive_size(value, "thread_count");
    if (value.contains("target_physical_core_count")) {
      row.target_physical_core_count =
          required_positive_size(value, "target_physical_core_count");
    }
    row.context_tokens = required_positive_size(value, "context_tokens");
    row.batch_size = required_positive_size(value, "batch_size");
    row.concurrent_sessions =
        required_positive_size(value, "concurrent_sessions");
    row.maximum_output_tokens =
        required_positive_size(value, "maximum_output_tokens");
    if (schema_version >= 3U) {
      if (!value.contains("warmup_iterations") ||
          !value.contains("minimum_measurement_iterations") ||
          !value.contains("minimum_sustained_duration_ms") ||
          !value.contains("maximum_measurement_iterations")) {
        throw std::invalid_argument(
            "edge runtime schema v3 rows require the complete execution "
            "protocol");
      }
      row.execution_protocol.warmup_iterations = optional_non_negative_size(
          value, "warmup_iterations", 0U);
      row.execution_protocol.minimum_measurement_iterations =
          optional_non_negative_size(
              value, "minimum_measurement_iterations", 1U);
      row.execution_protocol.minimum_sustained_duration_ms =
          optional_non_negative_u64(
              value, "minimum_sustained_duration_ms", 0U);
      row.execution_protocol.maximum_measurement_iterations =
          optional_non_negative_size(
              value, "maximum_measurement_iterations",
              row.execution_protocol.minimum_measurement_iterations);
    }
    if (schema_version >= 4U) {
      if (!value.contains("maximum_generation_duration_ms")) {
        throw std::invalid_argument(
            "edge runtime schema v4 rows require a cooperative generation "
            "deadline");
      }
      row.execution_protocol.maximum_generation_duration_ms =
          optional_non_negative_u64(
              value, "maximum_generation_duration_ms",
              kDefaultEdgeGenerationDurationMs);
    }
    if (row.execution_protocol.warmup_iterations >
            kMaximumWarmupIterations ||
        row.execution_protocol.minimum_measurement_iterations == 0U ||
        row.execution_protocol.minimum_measurement_iterations >
            kMaximumMeasurementIterations ||
        row.execution_protocol.maximum_measurement_iterations <
            row.execution_protocol.minimum_measurement_iterations ||
        row.execution_protocol.maximum_measurement_iterations >
            kMaximumMeasurementIterations ||
        row.execution_protocol.minimum_sustained_duration_ms >
            kMaximumSustainedDurationMs ||
        row.execution_protocol.maximum_generation_duration_ms == 0U ||
        row.execution_protocol.maximum_generation_duration_ms >
            kMaximumEdgeGenerationDurationMs) {
      throw std::invalid_argument(
          "edge runtime execution protocol exceeds bounded limits");
    }
    if (row.thread_count >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        row.context_tokens >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        row.batch_size >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        row.concurrent_sessions >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      throw std::invalid_argument(
          "edge runtime thread, context, batch or session field exceeds int range");
    }
    row.layers = required_positive_int(value, "layers");
    row.kv_heads = required_positive_int(value, "kv_heads");
    row.head_dim = required_positive_int(value, "head_dim");
    row.kv_dtype = required_string(value, "kv_dtype");
    row.mmap_requested = required_bool(value, "mmap_requested");
    row.runtime_workspace_budget_bytes = required_non_negative_u64(
        value, "runtime_workspace_budget_bytes");
    row.system_memory_budget_bytes =
        required_non_negative_u64(value, "system_memory_budget_bytes");
    row.system_reserve_bytes =
        required_non_negative_u64(value, "system_reserve_bytes");
    if (row.system_memory_budget_bytes == 0U ||
        row.system_reserve_bytes >= row.system_memory_budget_bytes) {
      throw std::invalid_argument(
          "edge system memory budget must be positive and exceed reserve");
    }
    if (schema_version >= 2U) {
      EdgeGenerationRequest request;
      request.prompt = prompt;
      request.maximum_output_tokens = row.maximum_output_tokens;
      request.context_tokens = row.context_tokens;
      request.batch_size = row.batch_size;
      request.thread_count = row.thread_count;
      request.maximum_generation_duration_ms =
          row.execution_protocol.maximum_generation_duration_ms;
      request.seed = row.seed;
      request.temperature = row.temperature;
      request.top_p = row.top_p;
      validate_edge_generation_request(request);
    }
    const std::filesystem::path model_path(
        required_string(value, "model_path"));
    std::filesystem::path tokenizer_path;
    if (value.contains("tokenizer_path")) {
      tokenizer_path = required_string(value, "tokenizer_path");
    }
    if (!row_ids.insert(row.row_id).second) {
      throw std::invalid_argument("duplicate edge runtime row_id: " +
                                  row.row_id);
    }
    parsed_rows.push_back(
        {std::move(row), model_path, tokenizer_path, std::move(prompt), {},
         {}, {}});
  }
  if (parsed_rows.empty()) {
    throw std::invalid_argument("edge runtime config has no runtime rows");
  }
  if (prompt_identity_mismatch) {
    append_unique(&matrix->comparison_identity_gaps,
                  "prompt_text_identity_mismatch");
  }
  matrix->runtime_rows.reserve(parsed_rows.size());
  matrix->quality_comparison.observations.reserve(parsed_rows.size());
  for (auto& parsed : parsed_rows) {
    prepare_row(repo_root, parsed.model_path, parsed.tokenizer_path,
                parsed.prompt, target_spec->runtime_architecture,
                platform_matches, mode, &parsed.model_input_snapshot,
                &parsed.tokenizer_input_snapshot, &parsed.row);
  }
  std::size_t retained_generated_text_bytes = 0U;
  for (auto& parsed : parsed_rows) {
    complete_prepared_row(repo_root, parsed.model_path,
                          parsed.tokenizer_path, parsed.prompt, mode,
                          parsed.model_input_snapshot,
                          parsed.tokenizer_input_snapshot,
                          &parsed.generated_text, &parsed.row);
    GeneratedTextQualityObservation observation;
    observation.row_id = parsed.row.row_id;
    observation.generated_text_bytes = parsed.generated_text.observed_bytes;
    observation.capture_status = parsed.generated_text.status;
    if (observation.capture_status == "observed_bounded_callback_text" &&
        observation.generated_text_bytes.has_value()) {
      if (*observation.generated_text_bytes !=
          parsed.generated_text.text.size()) {
        throw std::logic_error(
            "edge generated text byte count does not match retained text");
      }
      if (parsed.generated_text.text.size() >
          kMaximumGeneratedTextMatrixEvidenceBytes -
              retained_generated_text_bytes) {
        observation.capture_status =
            "unavailable_generated_text_matrix_budget_exceeded";
        parsed.generated_text.text.clear();
      } else {
        retained_generated_text_bytes += parsed.generated_text.text.size();
        observation.generated_text_fingerprint =
            inferlab_plan::fnv1a64_fingerprint(parsed.generated_text.text);
        observation.retained_text_for_comparison =
            std::move(parsed.generated_text.text);
      }
    }
    matrix->quality_comparison.observations.push_back(
        std::move(observation));
    matrix->runtime_rows.push_back(std::move(parsed.row));
  }
}

#endif

void summarize(Matrix* matrix) {
  matrix->summary.runtime_row_count = matrix->runtime_rows.size();
  for (const auto& row : matrix->runtime_rows) {
    if (row.dependency_linked) ++matrix->summary.dependency_linked_count;
    if (row.adapter_implemented) ++matrix->summary.adapter_implemented_count;
    if (row.model_path_status == "file" &&
        row.model_file_bytes.value_or(0U) > 0U) {
      ++matrix->summary.model_present_count;
    }
    if (row.executed) ++matrix->summary.executed_row_count;
    for (const auto& blocker : row.blockers) {
      append_unique(&matrix->blockers, row.row_id + ":" + blocker);
    }
    append_unique(&matrix->recommended_next_commands,
                  runtime_configure_command(
                      matrix->target_platform,
                      matrix->target_platform_match_status ==
                          "matches_compiled_architecture",
                      row));
  }
  matrix->comparison_identity_status = "not_comparable_single_or_empty";
  if (matrix->runtime_rows.size() > 1U) {
    const auto& reference = matrix->runtime_rows.front();
    for (std::size_t index = 1U; index < matrix->runtime_rows.size(); ++index) {
      const auto& candidate = matrix->runtime_rows[index];
      if (candidate.logical_model_id != reference.logical_model_id) {
        append_unique(&matrix->comparison_identity_gaps,
                      "logical_model_id_mismatch");
      }
      if (candidate.checkpoint_revision != reference.checkpoint_revision) {
        append_unique(&matrix->comparison_identity_gaps,
                      "checkpoint_revision_mismatch");
      }
      if (candidate.tokenizer_id != reference.tokenizer_id) {
        append_unique(&matrix->comparison_identity_gaps,
                      "tokenizer_identity_mismatch");
      }
      if (candidate.seed != reference.seed ||
          candidate.temperature != reference.temperature ||
          candidate.top_p != reference.top_p) {
        append_unique(&matrix->comparison_identity_gaps,
                      "sampling_configuration_mismatch");
      }
      if (candidate.context_tokens != reference.context_tokens) {
        append_unique(&matrix->comparison_identity_gaps,
                      "context_tokens_mismatch");
      }
      if (candidate.layers != reference.layers) {
        append_unique(&matrix->comparison_identity_gaps,
                      "kv_layers_mismatch");
      }
      if (candidate.kv_heads != reference.kv_heads) {
        append_unique(&matrix->comparison_identity_gaps,
                      "kv_heads_mismatch");
      }
      if (candidate.head_dim != reference.head_dim) {
        append_unique(&matrix->comparison_identity_gaps,
                      "kv_head_dim_mismatch");
      }
      if (candidate.kv_dtype != reference.kv_dtype) {
        append_unique(&matrix->comparison_identity_gaps,
                      "kv_dtype_mismatch");
      }
      if (candidate.batch_size != reference.batch_size) {
        append_unique(&matrix->comparison_identity_gaps,
                      "batch_size_mismatch");
      }
      if (candidate.concurrent_sessions != reference.concurrent_sessions) {
        append_unique(&matrix->comparison_identity_gaps,
                      "concurrent_sessions_mismatch");
      }
      if (candidate.thread_count != reference.thread_count) {
        append_unique(&matrix->comparison_identity_gaps,
                      "thread_count_mismatch");
      }
      if (candidate.target_physical_core_count !=
          reference.target_physical_core_count) {
        append_unique(&matrix->comparison_identity_gaps,
                      "target_physical_core_count_mismatch");
      }
      if (candidate.system_memory_budget_bytes !=
          reference.system_memory_budget_bytes) {
        append_unique(&matrix->comparison_identity_gaps,
                      "system_memory_budget_mismatch");
      }
      if (candidate.system_reserve_bytes != reference.system_reserve_bytes) {
        append_unique(&matrix->comparison_identity_gaps,
                      "system_reserve_mismatch");
      }
      if (candidate.maximum_output_tokens !=
          reference.maximum_output_tokens) {
        append_unique(&matrix->comparison_identity_gaps,
                      "maximum_output_tokens_mismatch");
      }
      if (candidate.execution_protocol.warmup_iterations !=
              reference.execution_protocol.warmup_iterations ||
          candidate.execution_protocol.minimum_measurement_iterations !=
              reference.execution_protocol.minimum_measurement_iterations ||
          candidate.execution_protocol.minimum_sustained_duration_ms !=
              reference.execution_protocol.minimum_sustained_duration_ms ||
          candidate.execution_protocol.maximum_measurement_iterations !=
              reference.execution_protocol.maximum_measurement_iterations ||
          candidate.execution_protocol.maximum_generation_duration_ms !=
              reference.execution_protocol.maximum_generation_duration_ms) {
        append_unique(&matrix->comparison_identity_gaps,
                      "execution_protocol_mismatch");
      }
    }
    matrix->comparison_identity_status =
        matrix->comparison_identity_gaps.empty()
            ? "logical_identity_aligned_quality_comparison_allowed"
            : "not_comparable_identity_gap";
    for (const auto& gap : matrix->comparison_identity_gaps) {
      append_unique(&matrix->blockers, "comparison_identity:" + gap);
    }
  }
  derive_quality_comparison(matrix);
  derive_edge_deployment_recommendation(matrix);
  if (matrix->mode == Mode::kExecute &&
      matrix->deployment_recommendation.observation_alignment_status !=
          "aligned_for_configured_workload_performance_ranking") {
    for (const auto& gap :
         matrix->deployment_recommendation.observation_alignment_gaps) {
      append_unique(&matrix->blockers,
                    "deployment_observation:" + gap);
    }
  }
  matrix->summary.blocker_count = matrix->blockers.size();
  if (matrix->mode == Mode::kExecute) {
    matrix->summary.decision =
        matrix->blockers.empty() && !matrix->runtime_rows.empty() &&
                matrix->summary.executed_row_count ==
                    matrix->summary.runtime_row_count
            ? "execute_ready"
            : "blocked";
    matrix->summary.evidence_level =
        matrix->summary.decision == "execute_ready" ? "execute_supported"
                                                     : "stale_or_blocked";
  } else {
    matrix->summary.decision = matrix->blockers.empty()
                                   ? "dependency_ready"
                                   : "target_machine_pending";
    matrix->summary.evidence_level = matrix->blockers.empty()
                                         ? "dependency_ready"
                                         : "dry_run_ready";
  }
  const std::string build_directory =
      edge_build_directory(matrix->target_platform);
  append_unique(&matrix->recommended_next_commands,
                "cmake --build " + build_directory + " --target "
                "inferlab_edge_runtime_matrix");
  append_unique(&matrix->recommended_next_commands,
                "./" + build_directory +
                "/inferlab_edge_runtime_matrix --repo-root . "
                "--config configs/edge_runtime_matrix.example.json --mode "
                "plan --output results/raw/native_edge_runtime_matrix.json");
  append_unique(&matrix->recommended_next_commands,
                "./" + build_directory +
                "/inferlab_edge_runtime_matrix --repo-root . "
                "--config configs/edge_runtime_matrix.example.json --mode "
                "execute --output results/raw/native_edge_runtime_matrix.json");
}

void validate_dynamic_input_snapshot_blocker(
    const RuntimeRow& row, const char* role, const std::string& status) {
  std::string suffix;
  if (status == "changed_after_preflight") {
    suffix = "_changed_after_preflight";
  } else if (status == "metadata_unavailable_before_execute") {
    suffix = "_snapshot_unavailable_before_execute";
  } else if (status == "changed_during_execute_attempt") {
    suffix = "_changed_during_execute";
  } else if (status == "metadata_unavailable_after_execute_attempt") {
    suffix = "_snapshot_unavailable_after_execute";
  } else {
    return;
  }
  const std::string blocker = std::string(role) + suffix;
  if (std::find(row.blockers.begin(), row.blockers.end(), blocker) ==
      row.blockers.end()) {
    throw std::logic_error("edge runtime input blocker projection drift");
  }
}

void validate_runtime_input_stability(const RuntimeRow& row,
                                      const Mode mode) {
  static const std::unordered_set<std::string> input_stability_statuses = {
      "preflight_metadata_observed_not_executed_plan",
      "preflight_metadata_observed_execute_pending",
      "preflight_metadata_unavailable",
      "covered_by_model_snapshot",
      "not_configured",
      "stable_before_execute_attempt_not_started",
      "changed_after_preflight",
      "metadata_unavailable_before_execute",
      "stable_through_execute_attempt",
      "changed_during_execute_attempt",
      "metadata_unavailable_after_execute_attempt"};
  const auto source_is_supported = [](const std::string& source) {
    return is_known_edge_runtime_input_snapshot_source(source) ||
           source == "embedded_in_model" || source == "not_configured";
  };
  if (!source_is_supported(row.model_input_snapshot_source) ||
      !source_is_supported(row.tokenizer_input_snapshot_source) ||
      input_stability_statuses.find(row.model_input_stability_status) ==
          input_stability_statuses.end() ||
      input_stability_statuses.find(row.tokenizer_input_stability_status) ==
          input_stability_statuses.end()) {
    throw std::logic_error("edge runtime input stability vocabulary drift");
  }

  const auto baseline_status_is_consistent =
      [](const std::string& source, const std::string& status) {
        const bool unavailable =
            status == "preflight_metadata_unavailable";
        return is_observed_edge_runtime_input_snapshot_source(source)
                   ? !unavailable
                   : unavailable;
      };
  if (!baseline_status_is_consistent(row.model_input_snapshot_source,
                                     row.model_input_stability_status)) {
    throw std::logic_error("edge model input snapshot status drift");
  }
  const bool tokenizer_embedded =
      row.tokenizer_path_status == "embedded_in_model";
  const bool tokenizer_not_configured =
      row.tokenizer_input_snapshot_source == "not_configured";
  if ((tokenizer_embedded !=
       (row.tokenizer_input_snapshot_source == "embedded_in_model" &&
        row.tokenizer_input_stability_status ==
            "covered_by_model_snapshot")) ||
      (tokenizer_not_configured !=
       (row.tokenizer_path_status == "missing" &&
        row.tokenizer_input_stability_status == "not_configured"))) {
    throw std::logic_error("edge tokenizer input snapshot status drift");
  }
  if (!tokenizer_embedded && !tokenizer_not_configured &&
      !baseline_status_is_consistent(
          row.tokenizer_input_snapshot_source,
          row.tokenizer_input_stability_status)) {
    throw std::logic_error("edge tokenizer metadata status drift");
  }
  if (mode == Mode::kPlan &&
      ((row.model_input_stability_status !=
            "preflight_metadata_observed_not_executed_plan" &&
        row.model_input_stability_status !=
            "preflight_metadata_unavailable") ||
       (!tokenizer_embedded && !tokenizer_not_configured &&
        row.tokenizer_input_stability_status !=
            "preflight_metadata_observed_not_executed_plan" &&
        row.tokenizer_input_stability_status !=
            "preflight_metadata_unavailable"))) {
    throw std::logic_error("planned edge input claims execution stability");
  }
  if (row.executed &&
      (row.model_input_stability_status !=
           "stable_through_execute_attempt" ||
       (row.tokenizer_input_stability_status !=
            "stable_through_execute_attempt" &&
        row.tokenizer_input_stability_status !=
            "covered_by_model_snapshot"))) {
    throw std::logic_error("executed edge row lacks stable runtime inputs");
  }
  validate_dynamic_input_snapshot_blocker(
      row, "model_input", row.model_input_stability_status);
  validate_dynamic_input_snapshot_blocker(
      row, "tokenizer_input", row.tokenizer_input_stability_status);
}

}  // namespace

Mode parse_mode(const std::string& value) {
  if (value == "plan") return Mode::kPlan;
  if (value == "execute") return Mode::kExecute;
  throw std::invalid_argument("edge runtime mode must be plan or execute");
}

const char* mode_name(const Mode mode) noexcept {
  switch (mode) {
    case Mode::kPlan:
      return "plan";
    case Mode::kExecute:
      return "execute";
  }
  return "unknown";
}

Matrix build_matrix(const std::filesystem::path& repo_root,
                    const std::filesystem::path& config_path,
                    const Mode mode,
                    const std::filesystem::path& output_path) {
  reject_output_alias(config_path, output_path, "configuration input");
  Matrix matrix;
  matrix.mode = mode;
  matrix.build_system = build_system();
  matrix.build_architecture = build_architecture();
  matrix.config_path_label =
      inferlab_plan::redacted_path_label(config_path.generic_string());
  matrix.delivery_contract = {
      {"bin", "lib", "models", "configs", "licenses"},
      {"schema_version", "architecture", "abi", "runtime_versions",
       "required_shared_libraries", "model_identities", "checksums",
       "entry_commands", "license_index"},
      "edge-package",
      "contract_ready_target_package_not_built",
  };
  matrix.isa_capabilities = detect_edge_isa_capabilities();
  const std::string selected_backend =
      matrix.isa_capabilities.neon_compiled &&
              matrix.isa_capabilities.neon_runtime_available
          ? "arm_neon"
          : "scalar";
  const std::string selection_status = selected_backend == "arm_neon"
                                           ? "runtime_capability_selected"
                                           : "deterministic_scalar_fallback";
  for (const char* operator_id :
       {"int4_dequant_gemv", "rms_norm", "rope"}) {
    matrix.kernel_capabilities.push_back(
        {operator_id, true, matrix.isa_capabilities.neon_compiled,
         matrix.isa_capabilities.neon_runtime_available, selected_backend,
         selection_status});
  }

  const auto read_result = inferlab_artifact::read_text_with_status(
      config_path, kMaximumConfigBytes);
  matrix.config_status = read_result.status;
  if (!read_result.readable || read_result.empty) {
    append_unique(&matrix.blockers, "config:" + read_result.status);
    matrix.target_platform = "unknown";
    matrix.target_platform_match_status = "unavailable_config_not_readable";
    summarize(&matrix);
    validate_matrix(matrix);
    return matrix;
  }

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
  try {
    parse_config(repo_root, read_result.text, mode, output_path, &matrix);
    matrix.config_status = "parsed";
  } catch (const ProtectedInputOutputAlias&) {
    throw;
  } catch (const std::exception&) {
    matrix.runtime_rows.clear();
    matrix.quality_comparison = {};
    matrix.target_platform = "unknown";
    matrix.target_platform_match_status = "unavailable_config_malformed";
    matrix.config_status = "malformed";
    append_unique(&matrix.blockers, "config:malformed");
  }
#else
  if (!output_path.empty()) {
    throw std::runtime_error(
        "edge runtime matrix cannot verify output/input separation without the structured JSON parser");
  }
  append_unique(&matrix.blockers, "config:structured_json_parser_unavailable");
  matrix.config_status = "structured_json_parser_unavailable";
  matrix.target_platform = "unknown";
  matrix.target_platform_match_status =
      "unavailable_structured_json_parser";
#endif
  summarize(&matrix);
  validate_matrix(matrix);
  return matrix;
}

void validate_matrix(const Matrix& matrix) {
  if ((matrix.mode != Mode::kPlan && matrix.mode != Mode::kExecute) ||
      matrix.target_platform.empty() || matrix.build_system.empty() ||
      matrix.build_architecture.empty() ||
      matrix.target_platform_match_status.empty() ||
      matrix.config_path_label.empty() ||
      matrix.config_status.empty() || matrix.kernel_capabilities.size() != 3U ||
      matrix.delivery_contract.required_directories.empty() ||
      matrix.delivery_contract.manifest_required_fields.empty() ||
      matrix.delivery_contract.package_root.empty() ||
      matrix.delivery_contract.status.empty() ||
      matrix.comparison_identity_status.empty() ||
      matrix.runtime_rows.size() > kMaximumEdgeRuntimeMatrixRows ||
      matrix.deployment_recommendation.status.empty() ||
      matrix.deployment_recommendation.criterion.empty() ||
      matrix.summary.runtime_row_count != matrix.runtime_rows.size() ||
      matrix.summary.blocker_count != matrix.blockers.size() ||
      matrix.summary.evidence_level.empty() || matrix.summary.decision.empty()) {
    throw std::logic_error("edge runtime matrix projection is inconsistent");
  }
  const EdgeRuntimeTargetPlatform* parsed_target_spec = nullptr;
  if (matrix.config_status == "parsed") {
    if (matrix.config_schema_version < 1U ||
        matrix.config_schema_version > 4U) {
      throw std::logic_error("parsed edge config schema version drift");
    }
    parsed_target_spec =
        find_edge_runtime_target_platform(matrix.target_platform);
    if (parsed_target_spec == nullptr) {
      throw std::logic_error(
          "parsed edge runtime matrix has unknown target platform");
    }
    const bool platform_matches = target_matches_build(
        *parsed_target_spec, matrix.build_system, matrix.build_architecture);
    const std::string expected_platform_status =
        platform_matches ? "matches_compiled_architecture"
                         : "mismatch_compiled_architecture";
    if (matrix.target_platform_match_status != expected_platform_status) {
      throw std::logic_error("edge target platform match status drift");
    }
  }
  if ((matrix.isa_capabilities.neon_runtime_available &&
       !matrix.isa_capabilities.neon_compiled) ||
      (matrix.isa_capabilities.sve_runtime_available &&
       !matrix.isa_capabilities.sve_compiled)) {
    throw std::logic_error("edge ISA capability availability drift");
  }
  const std::unordered_set<std::string> expected_kernel_ids = {
      "int4_dequant_gemv", "rms_norm", "rope"};
  std::unordered_set<std::string> kernel_ids;
  const bool neon_selected = matrix.isa_capabilities.neon_compiled &&
                             matrix.isa_capabilities.neon_runtime_available;
  const std::string expected_backend = neon_selected ? "arm_neon" : "scalar";
  const std::string expected_selection_status =
      neon_selected ? "runtime_capability_selected"
                    : "deterministic_scalar_fallback";
  for (const auto& capability : matrix.kernel_capabilities) {
    if (capability.operator_id.empty() ||
        expected_kernel_ids.find(capability.operator_id) ==
            expected_kernel_ids.end() ||
        !kernel_ids.insert(capability.operator_id).second ||
        !capability.scalar_available ||
        capability.neon_compiled != matrix.isa_capabilities.neon_compiled ||
        capability.neon_runtime_available !=
            matrix.isa_capabilities.neon_runtime_available ||
        capability.selected_backend != expected_backend ||
        capability.selection_status != expected_selection_status) {
      throw std::logic_error("edge kernel capability projection drift");
    }
  }
  if (kernel_ids != expected_kernel_ids) {
    throw std::logic_error("edge kernel capability inventory drift");
  }
  std::unordered_set<std::string> row_ids;
  std::size_t executed_count = 0U;
  std::size_t dependency_linked_count = 0U;
  std::size_t adapter_implemented_count = 0U;
  std::size_t model_present_count = 0U;
  for (const auto& row : matrix.runtime_rows) {
    if (row.row_id.empty() || row.runtime_id.empty() ||
        row.runtime_version.empty() || row.model_format.empty() ||
        row.backend.empty() || row.logical_model_id.empty() ||
        row.checkpoint_revision.empty() || row.tokenizer_id.empty() ||
        row.quantization.empty() || row.model_path_label.empty() ||
        row.model_path_status.empty() ||
        row.model_input_snapshot_source.empty() ||
        row.model_input_stability_status.empty() ||
        row.tokenizer_path_label.empty() ||
        row.tokenizer_path_status.empty() ||
        row.tokenizer_input_snapshot_source.empty() ||
        row.tokenizer_input_stability_status.empty() ||
        row.kv_dtype.empty() || row.resource_plan_status.empty() ||
        row.thread_count == 0U || row.context_tokens == 0U ||
        row.batch_size == 0U || row.concurrent_sessions == 0U ||
        row.maximum_output_tokens == 0U || row.layers <= 0 ||
        row.execution_protocol.minimum_measurement_iterations == 0U ||
        row.execution_protocol.minimum_measurement_iterations >
            kMaximumMeasurementIterations ||
        row.execution_protocol.maximum_measurement_iterations <
            row.execution_protocol.minimum_measurement_iterations ||
        row.execution_protocol.maximum_measurement_iterations >
            kMaximumMeasurementIterations ||
        row.execution_protocol.warmup_iterations >
            kMaximumWarmupIterations ||
        row.execution_protocol.minimum_sustained_duration_ms >
            kMaximumSustainedDurationMs ||
        row.execution_protocol.maximum_generation_duration_ms == 0U ||
        row.execution_protocol.maximum_generation_duration_ms >
            kMaximumEdgeGenerationDurationMs ||
        row.execution_protocol.completed_warmup_iterations >
            row.execution_protocol.warmup_iterations ||
        row.execution_protocol.completed_measurement_iterations >
            row.execution_protocol.maximum_measurement_iterations ||
        (row.execution_protocol.observed_measurement_duration_ms.has_value() &&
         (!std::isfinite(
              *row.execution_protocol.observed_measurement_duration_ms) ||
          *row.execution_protocol.observed_measurement_duration_ms < 0.0)) ||
        row.execution_protocol.status.empty() ||
        row.execution_protocol.aggregation !=
            "median_warm_latency_and_throughput_max_process_memory_latest_mapping" ||
        row.concurrent_execution.requested_sessions !=
            row.concurrent_sessions ||
        row.concurrent_execution.loaded_sessions > row.concurrent_sessions ||
        row.concurrent_execution.warmed_sessions > row.concurrent_sessions ||
        row.concurrent_execution.started_sessions > row.concurrent_sessions ||
        row.concurrent_execution.completed_sessions >
            row.concurrent_sessions ||
        row.concurrent_execution.finish_alignment_status.empty() ||
        row.concurrent_execution.status.empty() ||
        row.concurrent_execution.provenance.empty() ||
        row.kv_heads <= 0 || row.head_dim <= 0 ||
        row.system_memory_budget_bytes == 0U ||
        row.system_reserve_bytes >= row.system_memory_budget_bytes ||
        row.recommended_thread_count == 0U ||
        row.thread_recommendation_status.empty() ||
        row.mmap_strategy.empty() || row.observed_resource_status.empty() ||
        row.conversion_provenance.empty() ||
        row.thermal_before_status.empty() ||
        row.thermal_after_status.empty() || row.thermal_provenance.empty() ||
        row.energy_before_status.empty() || row.energy_after_status.empty() ||
        row.energy_status.empty() || row.energy_provenance.empty() ||
        row.thread_control_status.empty() ||
        row.capability_status.empty() || row.metrics.provenance.empty() ||
        row.load_status.empty() || row.generate_status.empty() ||
        row.cancel_status.empty() || row.reset_status.empty() ||
        row.unload_status.empty() ||
        !row_ids.insert(row.row_id).second) {
      throw std::logic_error("edge runtime row projection is inconsistent");
    }
    validate_runtime_input_stability(row, matrix.mode);
    const std::unordered_set<std::string> protocol_statuses = {
        "not_run_plan",
        "not_observed_execute_not_started",
        "blocked_before_execute",
        "failed_before_measurement",
        "cold_probe_running",
        "failed_cold_probe",
        "warmup_running",
        "failed_warmup",
        "measurement_running",
        "failed_measurement",
        "failed_sustained_duration",
        "observed",
        "failed_adapter_exception"};
    if (protocol_statuses.find(row.execution_protocol.status) ==
        protocol_statuses.end()) {
      throw std::logic_error("edge execution protocol status drift");
    }
    if (matrix.mode == Mode::kPlan &&
        (row.execution_protocol.status != "not_run_plan" ||
         row.execution_protocol.completed_warmup_iterations != 0U ||
         row.execution_protocol.completed_measurement_iterations != 0U ||
         row.execution_protocol.observed_measurement_duration_ms.has_value())) {
      throw std::logic_error("planned edge execution protocol is observed");
    }
    if (row.execution_protocol.status == "observed" &&
        (row.execution_protocol.completed_warmup_iterations !=
             row.execution_protocol.warmup_iterations ||
         row.execution_protocol.completed_measurement_iterations <
             row.execution_protocol.minimum_measurement_iterations ||
         !row.execution_protocol.observed_measurement_duration_ms.has_value() ||
         *row.execution_protocol.observed_measurement_duration_ms <
             static_cast<double>(row.execution_protocol
                                     .minimum_sustained_duration_ms))) {
      throw std::logic_error("observed edge execution protocol is incomplete");
    }
    const auto valid_positive_optional = [](const auto& value) {
      return !value.has_value() ||
             (std::isfinite(*value) && *value > 0.0);
    };
    const auto valid_non_negative_optional = [](const auto& value) {
      return !value.has_value() ||
             (std::isfinite(*value) && *value >= 0.0);
    };
    if (!valid_positive_optional(row.concurrent_execution.wall_time_ms) ||
        !valid_non_negative_optional(
            row.concurrent_execution.median_warm_ttft_ms) ||
        !valid_positive_optional(row.concurrent_execution.median_tpot_ms) ||
        !valid_positive_optional(
            row.concurrent_execution
                .aggregate_generated_tokens_per_second)) {
      throw std::logic_error("edge concurrent metric projection is invalid");
    }
    if ((row.concurrent_execution.peak_rss_bytes.has_value() &&
         *row.concurrent_execution.peak_rss_bytes == 0U) ||
        (row.concurrent_execution.peak_pss_bytes.has_value() &&
         *row.concurrent_execution.peak_pss_bytes == 0U)) {
      throw std::logic_error(
          "edge concurrent process working-set metric is invalid");
    }
    const std::unordered_set<std::string> concurrent_statuses = {
        "not_run_plan",
        "not_observed_execute_not_started",
        "blocked_before_execute",
        "failed_adapter_exception",
        "not_run_primary_runtime_failed",
        "not_run_serial_protocol_failed",
        "not_required_single_session",
        "loading_sessions",
        "warming_sessions",
        "failed_session_factory",
        "failed_session_capability",
        "failed_session_load",
        "failed_session_warmup",
        "failed_session_warmup_reset",
        "failed_thread_start",
        "failed_session_generation",
        "failed_session_reset",
        "failed_session_metric_projection",
        "observed"};
    if (concurrent_statuses.find(row.concurrent_execution.status) ==
        concurrent_statuses.end()) {
      throw std::logic_error("edge concurrent execution status drift");
    }
    if (matrix.mode == Mode::kPlan &&
        (row.concurrent_execution.status != "not_run_plan" ||
         row.concurrent_execution.loaded_sessions != 0U ||
         row.concurrent_execution.warmed_sessions != 0U ||
         row.concurrent_execution.started_sessions != 0U ||
         row.concurrent_execution.completed_sessions != 0U ||
         row.concurrent_execution.total_prompt_tokens != 0U ||
         row.concurrent_execution.total_generated_tokens != 0U ||
         row.concurrent_execution.total_callback_count != 0U ||
         row.concurrent_execution.wall_time_ms.has_value() ||
         row.concurrent_execution.median_warm_ttft_ms.has_value() ||
         row.concurrent_execution.median_tpot_ms.has_value() ||
         row.concurrent_execution.aggregate_generated_tokens_per_second
             .has_value() ||
         row.concurrent_execution.peak_rss_bytes.has_value() ||
         row.concurrent_execution.peak_pss_bytes.has_value() ||
         row.concurrent_execution.mapped_model_bytes.has_value())) {
      throw std::logic_error("planned edge concurrency is observed");
    }
    if (row.concurrent_execution.status == "observed" &&
        (row.concurrent_sessions <= 1U ||
         row.concurrent_execution.loaded_sessions !=
             row.concurrent_sessions ||
         row.concurrent_execution.warmed_sessions !=
             row.concurrent_sessions ||
         row.concurrent_execution.started_sessions !=
             row.concurrent_sessions ||
         row.concurrent_execution.completed_sessions !=
             row.concurrent_sessions ||
         row.concurrent_execution.finish_alignment_status !=
             "aligned_with_primary_serial_observation" ||
         !row.concurrent_execution.wall_time_ms.has_value() ||
         !checked_size_product_equals(
             row.observed_prompt_tokens, row.concurrent_sessions,
             row.concurrent_execution.total_prompt_tokens) ||
         !checked_size_product_equals(
             row.observed_generated_tokens, row.concurrent_sessions,
             row.concurrent_execution.total_generated_tokens) ||
         row.concurrent_execution.total_callback_count !=
             row.concurrent_execution.total_generated_tokens ||
         (row.observed_generated_tokens > 0U &&
          (!row.concurrent_execution.median_warm_ttft_ms.has_value() ||
           !row.concurrent_execution.aggregate_generated_tokens_per_second
                .has_value())))) {
      throw std::logic_error("observed edge concurrency is incomplete");
    }
    if (row.concurrent_execution.status ==
            "not_required_single_session" &&
        (row.concurrent_sessions != 1U ||
         row.concurrent_execution.loaded_sessions != 0U ||
         row.concurrent_execution.warmed_sessions != 0U ||
         row.concurrent_execution.started_sessions != 0U ||
         row.concurrent_execution.completed_sessions != 0U ||
         row.concurrent_execution.total_prompt_tokens != 0U ||
         row.concurrent_execution.total_generated_tokens != 0U ||
         row.concurrent_execution.total_callback_count != 0U ||
         row.concurrent_execution.wall_time_ms.has_value() ||
         row.concurrent_execution.median_warm_ttft_ms.has_value() ||
         row.concurrent_execution.median_tpot_ms.has_value() ||
         row.concurrent_execution.aggregate_generated_tokens_per_second
             .has_value() ||
         row.concurrent_execution.peak_rss_bytes.has_value() ||
         row.concurrent_execution.peak_pss_bytes.has_value() ||
         row.concurrent_execution.mapped_model_bytes.has_value() ||
         row.concurrent_execution.finish_alignment_status !=
             "not_applicable_single_session" ||
         row.concurrent_execution.provenance !=
             "serial_execution_protocol")) {
      throw std::logic_error(
          "single-session row carries concurrent execution evidence");
    }
    const std::unordered_set<std::string> thermal_statuses = {
        "not_run_plan",
        "not_observed_execute_not_started",
        "observed_linux_thermal_zone_max",
        "unavailable_thermal_sysfs_probe_error",
        "unavailable_thermal_sysfs_missing",
        "unavailable_no_readable_thermal_zone",
        "unavailable_thermal_sysfs_exception",
        "unavailable_thermal_zone_limit_reached",
        "unavailable_non_linux",
        "unavailable_mixed_thermal_provenance"};
    const auto thermal_observation_consistent =
        [&thermal_statuses](const auto& temperature, const std::size_t count,
                            const std::string& status) {
      if (thermal_statuses.find(status) == thermal_statuses.end()) {
        return false;
      }
      return temperature.has_value()
                 ? std::isfinite(*temperature) && *temperature >= -273.15 &&
                       *temperature <= 1000.0 && count > 0U &&
                       status == "observed_linux_thermal_zone_max"
                 : count == 0U &&
                       status != "observed_linux_thermal_zone_max";
    };
    if (!thermal_observation_consistent(
            row.thermal_before_c, row.thermal_sensor_count_before,
            row.thermal_before_status) ||
        !thermal_observation_consistent(
            row.thermal_after_c, row.thermal_sensor_count_after,
            row.thermal_after_status) ||
        ((row.thermal_before_c.has_value() ||
          row.thermal_after_c.has_value()) &&
         row.thermal_provenance !=
             "linux_sysfs_thermal_zone_temp_point_in_time") ||
        (row.thermal_provenance ==
             "unavailable_mixed_thermal_provenance" &&
         (row.thermal_before_status !=
              "unavailable_mixed_thermal_provenance" ||
          row.thermal_after_status !=
              "unavailable_mixed_thermal_provenance"))) {
      throw std::logic_error("edge thermal observation projection drift");
    }
    const std::unordered_set<std::string> energy_statuses = {
        "not_run_plan",
        "not_observed_execute_not_started",
        "unavailable_runtime_adapter_exception",
        "unavailable_powercap_snapshot",
        "unavailable_powercap_zone_set_changed",
        "unavailable_powercap_counter_range_changed",
        "unavailable_powercap_counter_wrapped_or_reset",
        "unavailable_powercap_duration_invalid",
        "unavailable_powercap_wrap_bound_overflow",
        "unavailable_powercap_wrap_ambiguous",
        "unavailable_powercap_counter_rate_exceeded",
        "unavailable_powercap_delta_overflow",
        "unavailable_powercap_interval_exception",
        "observed_linux_powercap_lifecycle_delta"};
    const std::unordered_set<std::string> energy_snapshot_statuses = {
        "not_run_plan",
        "not_observed_execute_not_started",
        "unavailable_runtime_adapter_exception",
        "observed_linux_powercap_root_zones",
        "unavailable_no_readable_powercap_zone",
        "unavailable_powercap_sysfs_probe_error",
        "unavailable_powercap_counter_malformed",
        "unavailable_powercap_power_bound_missing",
        "unavailable_powercap_inventory_limit_reached",
        "unavailable_powercap_sysfs_exception",
        "unavailable_powercap_sysfs_missing",
        "unavailable_powercap_zone_identity_duplicate",
        "unavailable_powercap_root_zone_limit_reached",
        "unavailable_non_linux"};
    const bool energy_observed = row.observed_energy_uj.has_value();
    const bool energy_snapshots_observed =
        row.energy_before_status == "observed_linux_powercap_root_zones" &&
        row.energy_after_status == "observed_linux_powercap_root_zones";
    const bool energy_status_requires_observed_snapshots =
        row.energy_status == "observed_linux_powercap_lifecycle_delta" ||
        row.energy_status == "unavailable_powercap_zone_set_changed" ||
        row.energy_status == "unavailable_powercap_counter_range_changed" ||
        row.energy_status ==
            "unavailable_powercap_counter_wrapped_or_reset" ||
        row.energy_status == "unavailable_powercap_duration_invalid" ||
        row.energy_status == "unavailable_powercap_wrap_bound_overflow" ||
        row.energy_status == "unavailable_powercap_wrap_ambiguous" ||
        row.energy_status ==
            "unavailable_powercap_counter_rate_exceeded" ||
        row.energy_status == "unavailable_powercap_delta_overflow";
    const bool energy_interval_provenance_valid =
        row.energy_provenance ==
            "linux_sysfs_powercap_root_zone_energy_uj_lifecycle_delta" ||
        (row.energy_status == "unavailable_powercap_snapshot" &&
         row.energy_before_status == "unavailable_non_linux" &&
         row.energy_after_status == "unavailable_non_linux" &&
         row.energy_provenance == "unavailable_non_linux");
    if (energy_statuses.find(row.energy_status) == energy_statuses.end() ||
        energy_snapshot_statuses.find(row.energy_before_status) ==
            energy_snapshot_statuses.end() ||
        energy_snapshot_statuses.find(row.energy_after_status) ==
            energy_snapshot_statuses.end() ||
        (energy_observed
             ? row.energy_root_zone_count == 0U ||
                   row.energy_status !=
                       "observed_linux_powercap_lifecycle_delta" ||
                   row.energy_before_status !=
                       "observed_linux_powercap_root_zones" ||
                   row.energy_after_status !=
                       "observed_linux_powercap_root_zones" ||
                   row.energy_provenance !=
                       "linux_sysfs_powercap_root_zone_energy_uj_lifecycle_delta"
             : row.energy_root_zone_count != 0U ||
                   row.energy_status ==
                       "observed_linux_powercap_lifecycle_delta") ||
        (energy_status_requires_observed_snapshots &&
         !energy_snapshots_observed) ||
        ((energy_status_requires_observed_snapshots ||
          row.energy_status == "unavailable_powercap_snapshot" ||
          row.energy_status == "unavailable_powercap_interval_exception") &&
         !energy_interval_provenance_valid) ||
        (row.energy_status == "unavailable_powercap_snapshot" &&
         energy_snapshots_observed) ||
        (row.energy_status == "not_observed_execute_not_started" &&
         (row.energy_before_status !=
              "not_observed_execute_not_started" ||
          row.energy_after_status !=
              "not_observed_execute_not_started" ||
          row.energy_provenance != "unavailable_execute_not_started")) ||
        (row.energy_status == "unavailable_runtime_adapter_exception" &&
         (row.energy_before_status !=
              "unavailable_runtime_adapter_exception" ||
          row.energy_after_status !=
              "unavailable_runtime_adapter_exception" ||
          row.energy_provenance !=
              "unavailable_runtime_adapter_exception")) ||
        (matrix.mode == Mode::kPlan &&
         (energy_observed || row.energy_root_zone_count != 0U ||
          row.energy_before_status != "not_run_plan" ||
          row.energy_after_status != "not_run_plan" ||
          row.energy_status != "not_run_plan" ||
          row.energy_provenance != "unavailable_not_executed_plan"))) {
      throw std::logic_error("edge energy observation projection drift");
    }
    const bool has_observed_metric =
        row.metrics.load_time_ms.has_value() ||
        row.metrics.cold_ttft_ms.has_value() ||
        row.metrics.warm_ttft_ms.has_value() ||
        row.metrics.tpot_ms.has_value() ||
        row.metrics.prompt_tokens_per_second.has_value() ||
        row.metrics.decode_tokens_per_second.has_value() ||
        row.metrics.peak_rss_bytes.has_value() ||
        row.metrics.peak_pss_bytes.has_value() ||
        row.metrics.mapped_model_bytes.has_value() ||
        row.metrics.kv_cache_bytes.has_value() ||
        row.metrics.runtime_workspace_bytes.has_value();
    if (has_observed_metric &&
        row.metrics.provenance.rfind("unavailable_", 0U) == 0U) {
      throw std::logic_error(
          "edge runtime row carries metrics with unavailable provenance");
    }
    validate_edge_resource_row(row);
    if (parsed_target_spec == nullptr ||
        row.target_runtime_contract_supported !=
            target_runtime_contract_status(
                row, parsed_target_spec->runtime_architecture)
                .supported()) {
      throw std::logic_error(
          "edge target runtime contract projection drift");
    }
    if (row.executed &&
        (row.load_status != "ready" || row.generate_status != "ready" ||
         row.cancel_status != "observed_public_cancel_during_generation" ||
         row.reset_status != "ready_after_generation_and_cancel_probe" ||
         row.unload_status != "completed" || row.finish_reason.empty() ||
         row.token_callback_count != row.observed_generated_tokens ||
         row.execution_protocol.status != "observed" ||
         (row.concurrent_sessions == 1U
              ? row.concurrent_execution.status !=
                    "not_required_single_session"
              : row.concurrent_execution.status != "observed") ||
         !row.execution_error_category.empty())) {
      throw std::logic_error(
          "executed edge runtime row has inconsistent observation fields");
    }
    if (row.dependency_linked) ++dependency_linked_count;
    if (row.adapter_implemented) ++adapter_implemented_count;
    if (row.model_path_status == "file" &&
        row.model_file_bytes.value_or(0U) > 0U) {
      ++model_present_count;
    }
    if (row.executed) ++executed_count;
  }
  validate_quality_comparison(matrix);
  validate_edge_deployment_recommendation(matrix);
  if (executed_count != matrix.summary.executed_row_count ||
      dependency_linked_count != matrix.summary.dependency_linked_count ||
      adapter_implemented_count !=
          matrix.summary.adapter_implemented_count ||
      model_present_count != matrix.summary.model_present_count) {
    throw std::logic_error("edge runtime summary count drift");
  }
  const bool execute_ready =
      matrix.mode == Mode::kExecute && matrix.blockers.empty() &&
      !matrix.runtime_rows.empty() &&
      executed_count == matrix.runtime_rows.size();
  const std::string expected_decision =
      matrix.mode == Mode::kExecute
          ? (execute_ready ? "execute_ready" : "blocked")
          : (matrix.blockers.empty() ? "dependency_ready"
                                     : "target_machine_pending");
  const std::string expected_evidence =
      matrix.mode == Mode::kExecute
          ? (execute_ready ? "execute_supported" : "stale_or_blocked")
          : (matrix.blockers.empty() ? "dependency_ready" : "dry_run_ready");
  if (matrix.summary.decision != expected_decision ||
      matrix.summary.evidence_level != expected_evidence) {
    throw std::logic_error("edge runtime summary decision drift");
  }
}

}  // namespace inferlab::edge_runtime_matrix
