#include "release_evidence_pipeline_repair_link_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string snapshot_diff_release_link_json(
    const ReleasePipelineSnapshotDiffReleaseLinkProjection& projection,
    const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline snapshot link nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const auto& snapshot_diff_signals = projection.snapshot_diff;

  std::ostringstream out;
  out << "{\n";
  out << nested << "\"source\": "
      << inferlab_plan::json_string(projection.artifact.artifact) << ",\n";
  out << nested << "\"linked_repair_category\": "
      << inferlab_plan::json_string(projection.group_spec.key)
      << ",\n";
  out << nested << "\"linked_group_id\": "
      << inferlab_plan::json_string(projection.group_spec.id)
      << ",\n";
  out << nested << "\"link_status\": "
      << inferlab_plan::json_string(projection.link_status) << ",\n";
  out << nested << "\"release_blocker_reason\": "
      << inferlab_plan::json_string(projection.release_blocker_reason)
      << ",\n";
  out << nested << "\"snapshot_diff_query_present\": "
      << (snapshot_diff_signals.query_present ? "true" : "false") << ",\n";
  out << nested << "\"release_repair_path_count\": "
      << projection.repair_path_count << ",\n";
  out << nested << "\"linked_group_plan_step\": "
      << projection.group_plan_step << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << projection.source_location_gap_count
      << ",\n";
  out << nested << "\"evidence_counter_count\": "
      << projection.evidence_counter_count
      << ",\n";
  out << nested << "\"added_deep_path_count\": "
      << snapshot_diff_signals.added_deep_path_count << ",\n";
  out << nested << "\"removed_deep_path_count\": "
      << snapshot_diff_signals.removed_deep_path_count << ",\n";
  out << nested << "\"changed_deep_value_count\": "
      << snapshot_diff_signals.changed_deep_value_count << ",\n";
  out << nested << "\"deep_semantic_truncated\": "
      << (snapshot_diff_signals.deep_semantic_truncated ? "true" : "false")
      << ",\n";
  out << nested << "\"deep_semantic_decision\": "
      << (snapshot_diff_signals.deep_semantic_decision.empty()
              ? "null"
              : inferlab_plan::json_string(
                    snapshot_diff_signals.deep_semantic_decision))
      << ",\n";
  out << nested << "\"query_focus_present\": "
      << (snapshot_diff_signals.release_focus_present ? "true" : "false")
      << ",\n";
  out << nested << "\"query_focus_link_status\": "
      << (snapshot_diff_signals.release_focus_link_status.empty()
              ? "null"
              : inferlab_plan::json_string(
                    snapshot_diff_signals.release_focus_link_status))
      << ",\n";
  out << nested << "\"query_focus_blocker_reason\": "
      << (snapshot_diff_signals.release_focus_blocker_reason.empty()
              ? "null"
              : inferlab_plan::json_string(
                    snapshot_diff_signals.release_focus_blocker_reason))
      << ",\n";
  out << nested << "\"query_focus_review_required\": "
      << (snapshot_diff_signals.release_focus_review_required ? "true"
                                                              : "false")
      << ",\n";
  out << nested << "\"query_focus_current_repo_action_count\": "
      << snapshot_diff_signals.release_focus_current_repo_action_count
      << ",\n";
  out << nested << "\"query_focus_changed_deep_path_count\": "
      << snapshot_diff_signals.release_focus_changed_deep_path_count << ",\n";
  out << nested << "\"linked_repair_path_id\": "
      << (projection.first_repair_path_present
              ? inferlab_plan::json_string(projection.first_repair_path_id)
              : "null")
      << ",\n";
  out << nested << "\"first_source_location\": "
      << (projection.first_source_location.empty()
              ? "null"
              : inferlab_plan::json_string(projection.first_source_location))
      << ",\n";
  out << nested << "\"recommended_action\": "
      << inferlab_plan::json_string(projection.recommended_action)
      << ",\n";
  out << nested << "\"command\": "
      << (projection.command.empty()
              ? "null"
              : inferlab_plan::json_string(projection.command))
      << ",\n";
  out << nested << "\"current_repo_action_required\": "
      << inferlab_plan::json_bool(projection.current_repo_action_required)
      << ",\n";
  out << nested << "\"target_or_validation_action_required\": "
      << inferlab_plan::json_bool(
             projection.target_or_validation_action_required)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"snapshot-to-release link explains how existing Evidence Query snapshot diff metadata maps to an existing release repair path; it does not execute diff commands, refresh artifacts, or prove replay/execute readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string blocker_repair_card_link_json(
    const ReleasePipelineBlockerRepairCardLinkProjection& projection,
    const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline blocker link nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"source\": "
      << inferlab_plan::json_string(
             projection.root_cause.artifact.artifact)
      << ",\n";
  out << nested << "\"direct_artifact_present\": "
      << (projection.root_cause.source_present ? "true" : "false")
      << ",\n";
  out << nested << "\"root_cause_repair_cards_present\": "
      << (projection.root_cause_repair_cards_present ? "true" : "false")
      << ",\n";
  out << nested << "\"link_status\": "
      << inferlab_plan::json_string(projection.link_status)
      << ",\n";
  out << nested << "\"root_cause_repair_card_count\": "
      << projection.root_cause_repair_card_count << ",\n";
  out << nested << "\"root_cause_current_repo_repair_card_count\": "
      << projection.root_cause_current_repo_repair_card_count
      << ",\n";
  out << nested << "\"root_cause_target_product_replay_repair_card_count\": "
      << projection.root_cause_target_product_replay_repair_card_count
      << ",\n";
  out << nested << "\"root_cause_target_gpu_execute_repair_card_count\": "
      << projection.root_cause_target_gpu_execute_repair_card_count
      << ",\n";
  out << nested << "\"root_cause_target_machine_repair_card_count\": "
      << projection.root_cause_target_machine_repair_card_count
      << ",\n";
  out << nested << "\"root_cause_repair_environment_split_status\": "
      << inferlab_plan::json_string(
             projection.root_cause_repair_environment_split_status)
      << ",\n";
  out << nested << "\"root_cause_repair_environment_split_schema_gap\": "
      << (projection.root_cause_repair_environment_split_schema_gap
              ? "true"
              : "false")
      << ",\n";
  out << nested << "\"root_cause_repair_environment_legacy_fallback_used\": "
      << (projection.root_cause_repair_environment_legacy_fallback_used
              ? "true"
              : "false")
      << ",\n";
  out << nested << "\"root_cause_flow_check_repair_card_count\": "
      << projection.root_cause_flow_check_repair_card_count
      << ",\n";
  out << nested << "\"total_release_repair_path_count\": "
      << projection.total_release_repair_path_count << ",\n";
  out << nested << "\"release_repair_path_count\": "
      << projection.release_repair_path_count << ",\n";
  out << nested << "\"release_current_repo_repair_path_count\": "
      << projection.release_current_repo_repair_path_count << ",\n";
  out << nested << "\"release_target_or_validation_repair_path_count\": "
      << projection.release_target_or_validation_repair_path_count << ",\n";
  out << nested << "\"repair_path_source_location_gap_count\": "
      << projection.repair_path_source_location_gap_count << ",\n";
  out << nested << "\"linked_root_cause_to_release_path_count\": "
      << projection.linked_root_cause_to_release_path_count << ",\n";
  out << nested << "\"linked_root_cause_repair_card_count\": "
      << projection.linked_root_cause_repair_card_count << ",\n";
  out << nested << "\"unmatched_release_repair_path_count\": "
      << projection.unmatched_release_repair_path_count << ",\n";
  out << nested << "\"unmatched_root_cause_repair_card_count\": "
      << projection.unmatched_root_cause_repair_card_count << ",\n";
  out << nested << "\"first_release_repair_path_id\": "
      << (projection.first_release_repair_path_present
              ? inferlab_plan::json_string(
                    projection.first_release_repair_path_id)
              : "null")
      << ",\n";
  out << nested << "\"first_release_repair_path_category\": "
      << (projection.first_release_repair_path_present
              ? inferlab_plan::json_string(
                    projection.first_release_repair_path_category)
              : "null")
      << ",\n";
  out << nested << "\"first_release_repair_environment\": "
      << (projection.first_release_repair_path_present
              ? inferlab_plan::json_string(
                    projection.first_release_repair_environment)
              : "null")
      << ",\n";
  out << nested << "\"first_release_repair_source_location\": "
      << (projection.first_release_repair_source_location.empty()
              ? "null"
              : inferlab_plan::json_string(
                    projection.first_release_repair_source_location))
      << ",\n";
  out << nested << "\"recommended_action\": "
      << inferlab_plan::json_string(projection.recommended_action)
      << ",\n";
  out << nested << "\"current_repo_action_required\": "
      << (projection.current_repo_action_required ? "true" : "false")
      << ",\n";
  out << nested << "\"target_or_validation_action_required\": "
      << (projection.target_or_validation_action_required ? "true"
                                                          : "false")
      << ",\n";
  out << nested << "\"upstream_source_contract_issue_count\": "
      << projection.upstream_source_contract_issue_count << ",\n";
  out << nested << "\"link_contract_issue_count\": "
      << projection.link_contract_issue_count << ",\n";
  out << nested << "\"source_contract_issue_count\": "
      << projection.source_contract_issue_count
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"blocker-to-repair-card link correlates existing root-cause repair cards with release repair paths; it does not execute repairs, refresh artifacts, or prove replay/execute readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string blocker_repair_card_link_summary_fields_json(
    const ReleasePipelineBlockerRepairCardLinkProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "\"blocker_repair_card_link_count\": "
      << (projection.link_present ? 1 : 0) << ",\n";
  out << pad << "\"blocker_repair_card_linked_count\": "
      << projection.linked_root_cause_to_release_path_count << ",\n";
  out << pad << "\"blocker_repair_card_linked_root_cause_count\": "
      << projection.linked_root_cause_repair_card_count << ",\n";
  out << pad << "\"blocker_repair_card_unmatched_release_path_count\": "
      << projection.unmatched_release_repair_path_count << ",\n";
  out << pad << "\"blocker_repair_card_unmatched_root_cause_count\": "
      << projection.unmatched_root_cause_repair_card_count << ",\n";
  out << pad << "\"blocker_repair_card_current_repo_action_count\": "
      << projection.release_current_repo_repair_path_count << ",\n";
  out << pad
      << "\"blocker_repair_card_target_or_validation_action_count\": "
      << projection.release_target_or_validation_repair_path_count << ",\n";
  out << pad << "\"blocker_repair_card_link_status\": "
      << inferlab_plan::json_string(projection.link_status) << ",\n";
  out << pad << "\"blocker_repair_card_upstream_contract_issue_count\": "
      << projection.upstream_source_contract_issue_count << ",\n";
  out << pad << "\"blocker_repair_card_link_contract_issue_count\": "
      << projection.link_contract_issue_count << ",\n";
  out << pad << "\"blocker_repair_card_source_contract_issue_count\": "
      << projection.source_contract_issue_count << ",\n";
  return out.str();
}

}  // namespace inferlab_release_pipeline
