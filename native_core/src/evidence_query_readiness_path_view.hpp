#pragma once

#include "evidence_query_operation_guidance_projection.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string readiness_path_actionability_focus_json(
    const ReadinessPathActionabilityProjection& projection,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(projection.present) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(projection.actionability_status) << ",\n";
  out << nested << "\"primary_environment\": "
      << inferlab_plan::json_string(projection.primary_environment) << ",\n";
  out << nested << "\"blocker_reason\": "
      << inferlab_plan::json_string(projection.blocker_reason) << ",\n";
  out << nested << "\"current_repo_actionable\": "
      << inferlab_plan::json_bool(projection.current_repo_actionable) << ",\n";
  out << nested << "\"validation_or_target_required\": "
      << inferlab_plan::json_bool(projection.validation_or_target_required)
      << ",\n";
  out << nested << "\"source_location_metadata_required\": "
      << inferlab_plan::json_bool(
             projection.source_location_metadata_required)
      << ",\n";
  out << nested << "\"blocking_repair_card_count\": "
      << projection.blocking_repair_card_count << ",\n";
  out << nested << "\"current_repo_step_count\": "
      << projection.current_repo_step_count << ",\n";
  out << nested << "\"target_or_validation_step_count\": "
      << projection.target_or_validation_step_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << projection.source_location_gap_count << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(projection.next_action) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "readiness-path actionability focus is read-only release triage metadata; it does not execute repairs, refresh artifacts, or create replay/execute evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string readiness_path_drilldown_json(
    const OperationGuidanceProjection& guidance,
    const QueryOperation& operation,
    const SelectorResolution& resolution,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::string& target = guidance.readiness_target;
  const RepairPathSummary* path = guidance.readiness_path;
  const ReadinessPathActionabilityProjection& projection = guidance.readiness;
  const std::vector<std::string>& navigation_steps =
      guidance.action_plan_steps;
  const std::vector<std::string> empty_path_steps;
  const std::vector<std::string>& path_steps =
      path == nullptr ? empty_path_steps : path->steps;
  const std::string first_action =
      !resolution.first_action.empty() ? resolution.first_action
                                       : projection.first_path_step;

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(projection.present) << ",\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"target\": " << inferlab_plan::json_string(target)
      << ",\n";
  out << nested << "\"state\": "
      << inferlab_plan::json_string(projection.state) << ",\n";
  out << nested << "\"blocking_repair_card_count\": "
      << projection.blocking_repair_card_count << ",\n";
  out << nested << "\"path_step_count\": " << projection.path_step_count
      << ",\n";
  out << nested << "\"navigation_step_count\": "
      << navigation_steps.size() << ",\n";
  out << nested << "\"current_repo_step_count\": "
      << projection.current_repo_step_count << ",\n";
  out << nested << "\"target_or_validation_step_count\": "
      << projection.target_or_validation_step_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << projection.source_location_gap_count << ",\n";
  out << nested << "\"catalog_flow_discrepancy_count\": "
      << projection.catalog_flow_discrepancy_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_fallback_removal_blocked_count\": "
      << projection.catalog_flow_fallback_removal_blocked_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_parity_refresh_required_count\": "
      << projection.catalog_flow_parity_refresh_required_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_current_repo_action_count\": "
      << projection.catalog_flow_current_repo_action_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_target_machine_action_count\": "
      << projection.catalog_flow_target_machine_action_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_repair_card_count\": "
      << projection.catalog_flow_field_coverage_repair_card_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_field_gap_count\": "
      << projection.catalog_flow_field_coverage_gap_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_parity_refresh_required_count\": "
      << projection.catalog_flow_field_coverage_parity_refresh_required_count
      << ",\n";
  out << nested << "\"selector_resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"selector_focus\": "
      << selector_focus_json(resolution, indent + 2) << ",\n";
  out << nested << "\"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << ",\n";
  out << nested << "\"first_artifact\": "
      << inferlab_plan::json_string(resolution.first_artifact) << ",\n";
  out << nested << "\"first_category\": "
      << inferlab_plan::json_string(resolution.first_category) << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(first_action) << ",\n";
  out << nested << "\"first_path_step\": "
      << inferlab_plan::json_string(projection.first_path_step) << ",\n";
  out << nested << "\"actionability_focus\": "
      << readiness_path_actionability_focus_json(projection, indent + 2)
      << ",\n";
  out << nested << "\"path_steps\": "
      << inferlab_plan::json_string_array(path_steps, indent + 2)
      << ",\n";
  out << nested << "\"navigation_steps\": "
      << inferlab_plan::json_string_array(navigation_steps, indent + 2)
      << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(resolution.current_repo_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_server_actions\": "
      << inferlab_plan::json_string_array(resolution.current_server_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(resolution.target_machine_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_product_actions\": "
      << inferlab_plan::json_string_array(resolution.target_product_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"path_command\": "
      << inferlab_plan::json_string(
             "inferlab_evidence_query --operation path --target " + target)
      << ",\n";
  out << nested << "\"suggest_repair_command\": "
      << inferlab_plan::json_string(
             resolution.selector.empty()
                 ? "inferlab_evidence_query --operation suggest_repair"
                 : "inferlab_evidence_query --operation suggest_repair --selector " +
                       resolution.selector)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "readiness-path drilldown is read-only operator guidance derived from existing repair-path, selector, and catalog-flow evidence; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
