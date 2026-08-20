#pragma once

#include "checked_index.hpp"
#include "evidence_query_artifact_projection.hpp"
#include "evidence_query_operation_catalog.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_snapshot_diff.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline bool operation_target_required(const QueryOperation& operation) {
  return operation.operation == "path";
}

inline bool operation_snapshot_inputs_required(
    const QueryOperation& operation) {
  return operation.operation == "diff_snapshots";
}

inline std::string operation_current_repo_action(
    const QueryOperation& operation) {
  const auto* spec = find_evidence_query_operation_spec(operation.operation,
                                                        operation.target);
  if (spec != nullptr) return spec->operation_current_repo_action;
  return "use query output as read-only evidence; refresh upstream artifacts separately";
}

inline std::string operation_target_machine_action(
    const QueryOperation& operation) {
  const auto* spec = find_evidence_query_operation_spec(operation.operation,
                                                        operation.target);
  if (spec != nullptr) return spec->operation_target_machine_action;
  return "run CMake/native/product replay/execute validation only when validation is explicitly re-enabled";
}

inline std::string selected_operation_result_kind(
    const QueryOperation& operation) {
  if (operation.operation == "list_artifacts") return "artifact_list";
  if (operation.operation == "show_artifact") return "artifact_detail";
  if (operation.operation == "explain_blocker") {
    return "blocker_explanation";
  }
  if (operation.operation == "suggest_repair") return "repair_suggestion";
  if (operation.operation == "repair_group_summary") {
    return "repair_group_summary";
  }
  if (operation.operation == "diff_snapshots") return "snapshot_diff";
  if (operation.operation == "path") return "readiness_path";
  return "warehouse_query_results";
}

inline bool selected_operation_input_ready(
    const QueryOperation& operation,
    const SnapshotDiffSignals& snapshot_diff) {
  if (operation_target_required(operation)) {
    return inferlab_evidence::is_readiness_target(operation.target);
  }
  if (operation_snapshot_inputs_required(operation)) {
    return snapshot_diff.before_configured && snapshot_diff.after_configured;
  }
  return true;
}

inline std::string selected_action_plan_target(
    const QueryOperation& operation) {
  if (operation.operation == "path" &&
      inferlab_evidence::is_readiness_target(operation.target)) {
    return operation.target;
  }
  return "report_ready";
}

inline const RepairPathSummary* selected_action_plan_path(
    const std::vector<RepairPathSummary>& shortest_paths,
    const QueryOperation& operation) {
  return find_repair_path(shortest_paths,
                          selected_action_plan_target(operation));
}

inline std::string selector_action_plan_decision(
    const SelectorResolution& resolution) {
  if (resolution.resolution_status == "selector_not_found" ||
      resolution.resolution_status == "target_invalid" ||
      resolution.resolution_status == "snapshot_inputs_missing") {
    return "selector_input_required";
  }
  if (resolution.resolution_status == "selector_ambiguous" ||
      resolution.ambiguous) {
    return "selector_narrowing_required";
  }
  if (resolution.repair_card_count == 0U) {
    return "no_selector_repair_card";
  }
  if (!resolution.current_repo_actions.empty()) {
    return "current_repo_repair_ready";
  }
  if (!resolution.current_server_actions.empty()) {
    return "validation_refresh_required";
  }
  if (!resolution.target_machine_actions.empty()) {
    return "target_machine_action_required";
  }
  if (!resolution.target_product_actions.empty()) {
    return "target_product_or_replay_required";
  }
  return "inspect_selector";
}

inline std::size_t selector_action_plan_target_or_validation_step_count(
    const SelectorResolution& resolution) {
  return inferlab::internal::checked_sum_size(
      {resolution.current_server_actions.size(),
       resolution.target_machine_actions.size(),
       resolution.target_product_actions.size()},
      "selector target-or-validation step count");
}

inline std::size_t readiness_path_current_repo_step_count(
    const RepairPathSummary* path,
    const SelectorResolution& resolution) {
  if (path == nullptr) return resolution.current_repo_actions.size();
  return inferlab::internal::checked_sum_size(
      {resolution.current_repo_actions.size(),
       path->catalog_flow_discrepancy_count,
       path->catalog_flow_surface_consumer_fallback_removal_blocked_count,
       path->catalog_flow_surface_consumer_current_repo_action_count,
       path->catalog_flow_consumer_field_coverage_repair_card_count},
      "readiness-path current-repo step count");
}

inline std::size_t readiness_path_target_or_validation_step_count(
    const RepairPathSummary* path,
    const SelectorResolution& resolution) {
  const std::size_t selector_count =
      selector_action_plan_target_or_validation_step_count(resolution);
  if (path == nullptr) return selector_count;
  return inferlab::internal::checked_sum_size(
      {selector_count,
       path->catalog_flow_surface_consumer_parity_refresh_required_count,
       path->catalog_flow_surface_consumer_target_machine_action_count,
       path->catalog_flow_consumer_field_coverage_parity_refresh_required_count},
      "readiness-path target-or-validation step count");
}

inline std::size_t readiness_path_source_location_gap_count(
    const RepairPathSummary* path,
    const SelectorResolution& resolution) {
  if (path == nullptr) return resolution.source_location_gap_count;
  return inferlab::internal::checked_add_size(
      resolution.source_location_gap_count,
      path->catalog_flow_consumer_field_coverage_field_gap_count,
      "readiness-path source-location gap count");
}

struct ReadinessPathActionabilityProjection {
  bool present = false;
  std::string state = "unknown";
  std::size_t blocking_repair_card_count = 0;
  std::size_t path_step_count = 0;
  std::size_t current_repo_step_count = 0;
  std::size_t target_or_validation_step_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t catalog_flow_discrepancy_count = 0;
  std::size_t catalog_flow_fallback_removal_blocked_count = 0;
  std::size_t catalog_flow_parity_refresh_required_count = 0;
  std::size_t catalog_flow_current_repo_action_count = 0;
  std::size_t catalog_flow_target_machine_action_count = 0;
  std::size_t catalog_flow_field_coverage_repair_card_count = 0;
  std::size_t catalog_flow_field_coverage_gap_count = 0;
  std::size_t catalog_flow_field_coverage_parity_refresh_required_count = 0;
  std::string actionability_status = "missing_path";
  std::string primary_environment = "unknown";
  std::string blocker_reason = "readiness_path_missing";
  std::string next_action;
  std::string first_path_step;
  bool current_repo_actionable = false;
  bool validation_or_target_required = false;
  bool source_location_metadata_required = false;
};

inline ReadinessPathActionabilityProjection
readiness_path_actionability_projection(
    const RepairPathSummary* path,
    const SelectorResolution& resolution,
    const std::string& target) {
  ReadinessPathActionabilityProjection projection;
  projection.present = path != nullptr;
  projection.current_repo_step_count =
      readiness_path_current_repo_step_count(path, resolution);
  projection.target_or_validation_step_count =
      readiness_path_target_or_validation_step_count(path, resolution);
  projection.source_location_gap_count =
      readiness_path_source_location_gap_count(path, resolution);

  if (path != nullptr) {
    projection.state = path->state;
    projection.blocking_repair_card_count =
        path->blocking_repair_card_count;
    projection.path_step_count = path->steps.size();
    projection.catalog_flow_discrepancy_count =
        path->catalog_flow_discrepancy_count;
    projection.catalog_flow_fallback_removal_blocked_count =
        path->catalog_flow_surface_consumer_fallback_removal_blocked_count;
    projection.catalog_flow_parity_refresh_required_count =
        path->catalog_flow_surface_consumer_parity_refresh_required_count;
    projection.catalog_flow_current_repo_action_count =
        path->catalog_flow_surface_consumer_current_repo_action_count;
    projection.catalog_flow_target_machine_action_count =
        path->catalog_flow_surface_consumer_target_machine_action_count;
    projection.catalog_flow_field_coverage_repair_card_count =
        path->catalog_flow_consumer_field_coverage_repair_card_count;
    projection.catalog_flow_field_coverage_gap_count =
        path->catalog_flow_consumer_field_coverage_field_gap_count;
    projection.catalog_flow_field_coverage_parity_refresh_required_count =
        path->catalog_flow_consumer_field_coverage_parity_refresh_required_count;
    if (!path->steps.empty()) projection.first_path_step = path->steps.front();
  }

  const bool ready = path != nullptr && path->state == "ready" &&
                     path->blocking_repair_card_count == 0U;
  if (path == nullptr) {
    projection.actionability_status = "missing_path";
  } else if (ready) {
    projection.actionability_status = "ready";
  } else if (projection.current_repo_step_count > 0U) {
    projection.actionability_status = "current_repo_action_required";
  } else if (projection.source_location_gap_count > 0U) {
    projection.actionability_status = "source_location_metadata_required";
  } else if (projection.target_or_validation_step_count > 0U) {
    projection.actionability_status =
        "validation_or_target_machine_required";
  } else if (path->blocking_repair_card_count > 0U) {
    projection.actionability_status = "repair_cards_pending_triage";
  } else {
    projection.actionability_status = "unknown";
  }

  if (path == nullptr) {
    projection.primary_environment = "unknown";
  } else if (projection.current_repo_step_count > 0U) {
    projection.primary_environment = "current_repo_fixable";
  } else if (!resolution.current_server_actions.empty() ||
             projection.catalog_flow_parity_refresh_required_count > 0U ||
             projection
                     .catalog_flow_field_coverage_parity_refresh_required_count >
                 0U) {
    projection.primary_environment =
        "current_server_when_validation_enabled";
  } else if (!resolution.target_machine_actions.empty() ||
             projection.catalog_flow_target_machine_action_count > 0U) {
    projection.primary_environment = "target_gpu_or_execute_machine";
  } else if (!resolution.target_product_actions.empty()) {
    projection.primary_environment = "target_product_or_replay_evidence";
  } else if (ready) {
    projection.primary_environment = "none";
  } else {
    projection.primary_environment = "unknown";
  }

  if (path == nullptr) {
    projection.blocker_reason = "readiness_path_missing";
  } else if (ready) {
    projection.blocker_reason = "none";
  } else if (resolution.source_location_gap_count > 0U) {
    projection.blocker_reason = "source_location_gap";
  } else if (projection.catalog_flow_fallback_removal_blocked_count > 0U) {
    projection.blocker_reason = "catalog_fallback_parity_pending";
  } else if (projection.catalog_flow_field_coverage_gap_count > 0U) {
    projection.blocker_reason = "catalog_field_coverage_gap";
  } else if (projection.current_repo_step_count > 0U) {
    projection.blocker_reason = "current_repo_repairs_pending";
  } else if (projection.target_or_validation_step_count > 0U) {
    projection.blocker_reason = "target_or_validation_evidence_pending";
  } else if (path->blocking_repair_card_count > 0U) {
    projection.blocker_reason = "repair_cards_pending";
  } else {
    projection.blocker_reason = "unknown";
  }

  if (path == nullptr) {
    projection.next_action =
        "refresh native_evidence_query_result.json with path " + target +
        " once validation is allowed";
  } else if (projection.current_repo_step_count > 0U) {
    if (!resolution.first_action.empty()) {
      projection.next_action = resolution.first_action;
    } else if (!projection.first_path_step.empty()) {
      projection.next_action = projection.first_path_step;
    } else {
      projection.next_action =
          "inspect current_repo repair cards and close source-level wiring gaps";
    }
  } else if (projection.source_location_gap_count > 0U) {
    projection.next_action =
        "add exact wiring locations for repair cards before claiming the path is operator-actionable";
  } else if (!resolution.current_server_actions.empty()) {
    projection.next_action = resolution.current_server_actions.front();
  } else if (!resolution.target_machine_actions.empty()) {
    projection.next_action = resolution.target_machine_actions.front();
  } else if (!resolution.target_product_actions.empty()) {
    projection.next_action = resolution.target_product_actions.front();
  } else if (projection.target_or_validation_step_count > 0U) {
    projection.next_action =
        "refresh validation/replay/execute artifacts on an allowed validation or target machine";
  } else if (ready) {
    projection.next_action =
        "preserve dry-run/replay/execute claim boundaries when publishing the report";
  } else if (projection.first_path_step.empty()) {
    projection.next_action =
        "inspect blocker repair cards for the selected readiness target";
  } else {
    projection.next_action = projection.first_path_step;
  }

  projection.current_repo_actionable =
      projection.primary_environment == "current_repo_fixable";
  projection.validation_or_target_required =
      projection.primary_environment ==
          "current_server_when_validation_enabled" ||
      projection.primary_environment == "target_gpu_or_execute_machine" ||
      projection.primary_environment == "target_product_or_replay_evidence";
  projection.source_location_metadata_required =
      projection.source_location_gap_count > 0U;
  return projection;
}

inline std::size_t selected_operation_payload_count(
    const QueryArtifactProjection& artifact_projection,
    const QueryOperation& operation,
    const std::size_t repair_card_count,
    const std::size_t repair_group_count,
    const SnapshotDiffSignals& snapshot_diff) {
  if (operation.operation == "show_artifact") {
    return artifact_projection.show_artifact.selection.selected == nullptr
               ? 0U
               : 1U;
  }
  if (operation.operation == "explain_blocker" ||
      operation.operation == "suggest_repair") {
    return repair_card_count;
  }
  if (operation.operation == "repair_group_summary") {
    return repair_group_count;
  }
  if (operation.operation == "diff_snapshots") {
    return snapshot_diff.configured ? 1U : 0U;
  }
  if (operation.operation == "path") {
    return inferlab_evidence::is_readiness_target(operation.target) ? 1U : 0U;
  }

  return artifact_projection.matched_records.size();
}

struct SelectedOperationProjection {
  std::string result_kind;
  bool input_ready = false;
  std::size_t payload_count = 0;
};

inline SelectedOperationProjection selected_operation_projection(
    const QueryArtifactProjection& artifact_projection,
    const QueryOperation& operation,
    const std::size_t repair_card_count,
    const std::size_t repair_group_count,
    const SnapshotDiffSignals& snapshot_diff) {
  SelectedOperationProjection projection;
  projection.result_kind = selected_operation_result_kind(operation);
  projection.input_ready =
      selected_operation_input_ready(operation, snapshot_diff);
  projection.payload_count = selected_operation_payload_count(
      artifact_projection, operation, repair_card_count, repair_group_count,
      snapshot_diff);
  return projection;
}

}  // namespace inferlab_evidence_query
