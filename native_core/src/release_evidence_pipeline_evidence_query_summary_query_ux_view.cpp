#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {
namespace {

using evidence_query_closure_view_detail::write_bool_field;
using evidence_query_closure_view_detail::write_count_field;
using evidence_query_closure_view_detail::write_json_field;

std::string summary_pad(const int indent) {
  return inferlab_plan::json_indent(indent);
}

}  // namespace

std::string evidence_query_query_ux_summary_head_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto& navigation = projection.query_ux.navigation;
  const auto& group = navigation.repair_card_navigation.group;
  const auto& explain = navigation.explain_tree_coverage;
  const auto& blocker = navigation.blocker_drilldown;
  const auto& readiness = navigation.readiness_path;
  const std::string pad = summary_pad(indent);
  std::ostringstream out;
  write_count_field(out, pad, "evidence_query_repair_card_group_count",
                    group.group_count);
  write_count_field(out, pad, "evidence_query_repair_card_group_card_count",
                    group.card_count);
  write_count_field(out, pad,
                    "evidence_query_repair_card_group_current_repo_count",
                    group.current_repo_count);
  write_count_field(out, pad,
                    "evidence_query_repair_card_group_current_server_count",
                    group.current_server_count);
  write_count_field(out, pad,
                    "evidence_query_repair_card_group_target_machine_count",
                    group.target_machine_count);
  write_count_field(out, pad,
                    "evidence_query_repair_card_group_target_product_count",
                    group.target_product_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_group_source_location_gap_count",
      group.source_location_gap_count);
  write_count_field(out, pad, "evidence_query_explain_tree_node_count",
                    explain.node_count);
  write_count_field(out, pad,
                    "evidence_query_explain_tree_source_exact_node_count",
                    explain.source_exact_node_count);
  write_count_field(out, pad,
                    "evidence_query_explain_tree_source_gap_node_count",
                    explain.source_gap_node_count);
  write_count_field(
      out, pad, "evidence_query_explain_tree_current_repo_source_gap_count",
      explain.current_repo_source_gap_count);
  write_count_field(
      out, pad,
      "evidence_query_explain_tree_target_or_validation_source_gap_count",
      explain.target_or_validation_source_gap_count);
  write_count_field(out, pad,
                    "evidence_query_explain_tree_source_exactness_percent",
                    explain.source_exactness_percent);
  write_count_field(out, pad,
                    "evidence_query_blocker_drilldown_record_count",
                    blocker.record_count);
  write_count_field(out, pad,
                    "evidence_query_blocker_drilldown_category_count",
                    blocker.category_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_source_location_gap_count",
      blocker.source_location_gap_count);
  write_count_field(out, pad,
                    "evidence_query_blocker_drilldown_affected_claim_count",
                    blocker.affected_claim_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_exact_wiring_location_count",
      blocker.exact_wiring_location_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_current_repo_action_count",
      blocker.current_repo_action_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_current_server_action_count",
      blocker.current_server_action_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_target_machine_action_count",
      blocker.target_machine_action_count);
  write_count_field(
      out, pad,
      "evidence_query_blocker_drilldown_target_product_action_count",
      blocker.target_product_action_count);
  write_json_field(out, pad, "evidence_query_readiness_path_target",
                   inferlab_plan::json_string(readiness.target));
  write_json_field(out, pad, "evidence_query_readiness_path_state",
                   inferlab_plan::json_string(readiness.state));
  write_count_field(
      out, pad,
      "evidence_query_readiness_path_blocking_repair_card_count",
      readiness.blocking_repair_card_count);
  write_count_field(out, pad, "evidence_query_readiness_path_step_count",
                    readiness.step_count);
  write_count_field(
      out, pad, "evidence_query_readiness_path_current_repo_step_count",
      readiness.current_repo_step_count);
  write_count_field(
      out, pad,
      "evidence_query_readiness_path_target_or_validation_step_count",
      readiness.target_or_validation_step_count);
  write_count_field(
      out, pad, "evidence_query_readiness_path_source_location_gap_count",
      readiness.source_location_gap_count);
  write_count_field(
      out, pad,
      "evidence_query_readiness_path_catalog_flow_discrepancy_count",
      readiness.catalog_flow_discrepancy_count);
  write_count_field(
      out, pad,
      "evidence_query_readiness_path_fallback_removal_blocked_count",
      readiness.fallback_removal_blocked_count);
  write_count_field(
      out, pad, "evidence_query_readiness_path_field_coverage_gap_count",
      readiness.field_coverage_gap_count);
  write_json_field(
      out, pad, "evidence_query_readiness_path_actionability_status",
      inferlab_plan::json_string(readiness.actionability_status));
  write_json_field(
      out, pad, "evidence_query_readiness_path_primary_environment",
      inferlab_plan::json_string(readiness.primary_environment));
  write_json_field(out, pad, "evidence_query_readiness_path_blocker_reason",
                   inferlab_plan::json_string(readiness.blocker_reason));
  write_bool_field(out, pad,
                   "evidence_query_readiness_path_current_repo_actionable",
                   readiness.current_repo_actionable);
  write_bool_field(
      out, pad,
      "evidence_query_readiness_path_validation_or_target_required",
      readiness.validation_or_target_required);
  write_bool_field(
      out, pad,
      "evidence_query_readiness_path_source_location_metadata_required",
      readiness.source_location_metadata_required);
  write_bool_field(out, pad,
                   "evidence_query_readiness_path_next_action_present",
                   readiness.next_action_present);
  return out.str();
}

std::string evidence_query_query_ux_repair_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto& navigation = projection.query_ux.navigation;
  const auto& narrative = navigation.repair_guidance.narrative;
  const auto& group = navigation.repair_guidance.group;
  const auto& worklist = navigation.source_location.worklist;
  const auto& category = navigation.source_location.category_coverage;
  const std::string pad = summary_pad(indent);
  std::ostringstream out;
  write_bool_field(out, pad, "evidence_query_repair_group_summary_present",
                   group.summary_present);
  write_count_field(
      out, pad, "evidence_query_repair_group_exact_wiring_location_count",
      group.exact_wiring_location_count);
  write_count_field(out, pad,
                    "evidence_query_repair_group_affected_claim_count",
                    group.affected_claim_count);
  write_bool_field(out, pad, "evidence_query_repair_narrative_present",
                   narrative.present);
  write_json_field(out, pad, "evidence_query_repair_narrative_decision",
                   inferlab_plan::json_string(narrative.decision));
  write_json_field(
      out, pad, "evidence_query_repair_narrative_readiness_state",
      inferlab_plan::json_string(narrative.readiness_state));
  write_count_field(out, pad, "evidence_query_repair_narrative_step_count",
                    narrative.step_count);
  write_count_field(
      out, pad, "evidence_query_repair_narrative_current_repo_action_count",
      narrative.current_repo_action_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_narrative_target_or_validation_action_count",
      narrative.target_or_validation_action_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_narrative_source_location_gap_count",
      narrative.source_location_gap_count);
  write_count_field(
      out, pad, "evidence_query_repair_narrative_readiness_blocker_count",
      narrative.readiness_blocker_count);
  write_count_field(
      out, pad, "evidence_query_repair_narrative_affected_claim_count",
      narrative.affected_claim_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_narrative_exact_wiring_location_count",
      narrative.exact_wiring_location_count);
  write_bool_field(out, pad,
                   "evidence_query_repair_group_primary_action_present",
                   group.primary_action_present);
  write_bool_field(
      out, pad,
      "evidence_query_repair_group_primary_source_location_present",
      group.primary_source_location_present);
  write_json_field(
      out, pad, "evidence_query_repair_group_primary_environment",
      group.primary_environment_present
          ? inferlab_plan::json_string(group.primary_environment)
          : "null");
  write_json_field(out, pad, "evidence_query_repair_group_primary_category",
                   group.primary_category_present
                       ? inferlab_plan::json_string(group.primary_category)
                       : "null");
  write_json_field(out, pad, "evidence_query_repair_group_primary_artifact",
                   group.primary_artifact_present
                       ? inferlab_plan::json_string(group.primary_artifact)
                       : "null");
  write_count_field(
      out, pad, "evidence_query_repair_group_navigation_step_count",
      group.navigation_step_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_group_navigation_current_repo_step_count",
      group.navigation_current_repo_step_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_group_navigation_target_or_validation_step_count",
      group.navigation_target_or_validation_step_count);
  write_count_field(out, pad,
                    "evidence_query_source_location_worklist_item_count",
                    worklist.item_count);
  write_count_field(
      out, pad,
      "evidence_query_source_location_worklist_current_repo_item_count",
      worklist.current_repo_item_count);
  write_count_field(
      out, pad,
      "evidence_query_source_location_worklist_target_or_validation_item_count",
      worklist.target_or_validation_item_count);
  write_count_field(out, pad,
                    "evidence_query_source_location_worklist_gap_count",
                    worklist.gap_count);
  write_bool_field(out, pad,
                   "evidence_query_source_location_worklist_focus_present",
                   worklist.focus_present);
  write_bool_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_present",
      worklist.first_item_present);
  write_bool_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_gap_present",
      worklist.first_gap_present);
  write_bool_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required",
      worklist.first_item_current_repo_action_required);
  write_bool_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required",
      worklist.first_item_target_or_validation_action_required);
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_environment",
      inferlab_plan::json_string(worklist.first_item_environment));
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_artifact",
      inferlab_plan::json_string(worklist.first_item_artifact));
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_category",
      inferlab_plan::json_string(worklist.first_item_category));
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_item_source_location",
      inferlab_plan::json_string(worklist.first_item_source_location));
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_gap_environment",
      inferlab_plan::json_string(worklist.first_gap_environment));
  write_json_field(
      out, pad,
      "evidence_query_source_location_worklist_focus_first_gap_category",
      inferlab_plan::json_string(worklist.first_gap_category));
  write_json_field(
      out, pad, "evidence_query_source_location_worklist_focus_next_action",
      inferlab_plan::json_string(worklist.next_action));
  write_count_field(out, pad, "evidence_query_source_location_category_count",
                    category.category_count);
  write_count_field(
      out, pad, "evidence_query_source_location_active_category_count",
      category.active_category_count);
  write_count_field(out, pad,
                    "evidence_query_source_location_exact_category_count",
                    category.exact_category_count);
  write_count_field(out, pad,
                    "evidence_query_source_location_gap_category_count",
                    category.gap_category_count);
  write_count_field(
      out, pad,
      "evidence_query_source_location_current_repo_gap_category_count",
      category.current_repo_gap_category_count);
  write_count_field(
      out, pad,
      "evidence_query_source_location_target_or_validation_gap_category_count",
      category.target_or_validation_gap_category_count);
  write_count_field(out, pad,
                    "evidence_query_source_location_gap_repair_card_count",
                    category.gap_repair_card_count);
  write_count_field(
      out, pad, "evidence_query_source_location_category_exactness_percent",
      category.exactness_percent);
  return out.str();
}

std::string evidence_query_query_ux_navigation_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto& navigation = projection.query_ux.navigation;
  const auto& path_rank = navigation.repair_card_navigation.path_rank;
  const auto& readiness = navigation.repair_card_navigation.rank_readiness;
  const auto& exactness = navigation.repair_card_navigation.source_exactness;
  const std::string pad = summary_pad(indent);
  std::ostringstream out;
  write_count_field(out, pad, "evidence_query_repair_card_path_rank_count",
                    path_rank.rank_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_path_current_repo_rank_count",
      path_rank.current_repo_rank_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_path_target_or_validation_rank_count",
      path_rank.target_or_validation_rank_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_path_source_location_gap_count",
      path_rank.source_location_gap_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_rank_readiness_link_count",
      readiness.link_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_rank_readiness_current_repo_link_count",
      readiness.current_repo_link_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_rank_readiness_target_or_validation_link_count",
      readiness.target_or_validation_link_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_rank_readiness_report_ready_link_count",
      readiness.report_ready_link_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_rank_readiness_replay_ready_link_count",
      readiness.replay_ready_link_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_rank_readiness_execute_ready_link_count",
      readiness.execute_ready_link_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_source_exactness_card_count",
      exactness.card_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_source_exactness_exact_card_count",
      exactness.exact_card_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_source_exactness_gap_card_count",
      exactness.gap_card_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_source_exactness_current_repo_gap_count",
      exactness.current_repo_gap_count);
  write_count_field(
      out, pad,
      "evidence_query_repair_card_source_exactness_target_or_validation_gap_count",
      exactness.target_or_validation_gap_count);
  write_count_field(
      out, pad, "evidence_query_repair_card_source_exactness_percent",
      exactness.exactness_percent);
  return out.str();
}

}  // namespace inferlab_release_pipeline
