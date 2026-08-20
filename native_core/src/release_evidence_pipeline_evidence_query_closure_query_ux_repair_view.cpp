#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string evidence_query_repair_group_plan_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  using evidence_query_closure_view_detail::write_bool_field;
  using evidence_query_closure_view_detail::write_count_field;
  using evidence_query_closure_view_detail::write_json_field;

  const auto& query_ux = projection.query_ux;
  const auto& navigation = query_ux.navigation;
  const auto& group = navigation.repair_card_navigation.group;
  const auto& guidance = navigation.repair_guidance;
  const auto& narrative = guidance.narrative;
  const auto& repair_group = guidance.group;
  const auto& worklist = navigation.source_location.worklist;
  const auto& category = navigation.source_location.category_coverage;
  const auto& path_rank = navigation.repair_card_navigation.path_rank;
  const auto& rank_readiness =
      navigation.repair_card_navigation.rank_readiness;
  const auto& source_exactness =
      navigation.repair_card_navigation.source_exactness;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      inferlab::internal::checked_add_int(
          indent, 2,
          "release pipeline Evidence Query repair-group closure indent"));
  std::ostringstream out;
  out << pad << "{\n";
  write_json_field(out, nested, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, nested, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, nested, "repair_card_group_plan_present",
                   query_ux.objects.repair_card_group_plan);
  write_count_field(out, nested, "repair_card_group_count",
                    group.group_count);
  write_count_field(out, nested, "repair_card_group_card_count",
                    group.card_count);
  write_count_field(out, nested, "current_repo_group_count",
                    group.current_repo_count);
  write_count_field(out, nested, "current_server_group_count",
                    group.current_server_count);
  write_count_field(out, nested, "target_machine_group_count",
                    group.target_machine_count);
  write_count_field(out, nested, "target_product_group_count",
                    group.target_product_count);
  write_count_field(out, nested, "source_location_gap_count",
                    group.source_location_gap_count);
  write_bool_field(out, nested, "repair_group_summary_present",
                   repair_group.summary_present);
  write_count_field(out, nested, "exact_wiring_location_count",
                    repair_group.exact_wiring_location_count);
  write_count_field(out, nested, "affected_claim_count",
                    repair_group.affected_claim_count);
  write_bool_field(out, nested, "repair_narrative_present",
                   narrative.present);
  write_json_field(out, nested, "repair_narrative_decision",
                   inferlab_plan::json_string(narrative.decision));
  write_json_field(out, nested, "repair_narrative_readiness_state",
                   inferlab_plan::json_string(narrative.readiness_state));
  write_count_field(out, nested, "repair_narrative_step_count",
                    narrative.step_count);
  write_count_field(out, nested, "repair_narrative_current_repo_action_count",
                    narrative.current_repo_action_count);
  write_count_field(out, nested,
                    "repair_narrative_target_or_validation_action_count",
                    narrative.target_or_validation_action_count);
  write_count_field(out, nested,
                    "repair_narrative_source_location_gap_count",
                    narrative.source_location_gap_count);
  write_count_field(out, nested,
                    "repair_narrative_readiness_blocker_count",
                    narrative.readiness_blocker_count);
  write_count_field(out, nested, "repair_narrative_affected_claim_count",
                    narrative.affected_claim_count);
  write_count_field(out, nested,
                    "repair_narrative_exact_wiring_location_count",
                    narrative.exact_wiring_location_count);
  write_bool_field(out, nested, "primary_action_present",
                   repair_group.primary_action_present);
  write_bool_field(out, nested, "primary_source_location_present",
                   repair_group.primary_source_location_present);
  write_json_field(
      out, nested, "primary_repair_environment",
      repair_group.primary_environment_present
          ? inferlab_plan::json_string(repair_group.primary_environment)
          : "null");
  write_json_field(out, nested, "primary_category",
                   repair_group.primary_category_present
                       ? inferlab_plan::json_string(
                             repair_group.primary_category)
                       : "null");
  write_json_field(out, nested, "primary_artifact",
                   repair_group.primary_artifact_present
                       ? inferlab_plan::json_string(
                             repair_group.primary_artifact)
                       : "null");
  write_count_field(out, nested, "navigation_step_count",
                    repair_group.navigation_step_count);
  write_count_field(out, nested, "navigation_current_repo_step_count",
                    repair_group.navigation_current_repo_step_count);
  write_count_field(
      out, nested, "navigation_target_or_validation_step_count",
      repair_group.navigation_target_or_validation_step_count);
  write_count_field(out, nested, "source_location_worklist_item_count",
                    worklist.item_count);
  write_count_field(out, nested,
                    "source_location_worklist_current_repo_item_count",
                    worklist.current_repo_item_count);
  write_count_field(
      out, nested,
      "source_location_worklist_target_or_validation_item_count",
      worklist.target_or_validation_item_count);
  write_count_field(out, nested, "source_location_worklist_gap_count",
                    worklist.gap_count);
  write_bool_field(out, nested, "source_location_worklist_focus_present",
                   worklist.focus_present);
  write_bool_field(out, nested,
                   "source_location_worklist_focus_first_item_present",
                   worklist.first_item_present);
  write_bool_field(out, nested,
                   "source_location_worklist_focus_first_gap_present",
                   worklist.first_gap_present);
  write_bool_field(
      out, nested,
      "source_location_worklist_focus_first_item_current_repo_action_required",
      worklist.first_item_current_repo_action_required);
  write_bool_field(
      out, nested,
      "source_location_worklist_focus_first_item_target_or_validation_action_required",
      worklist.first_item_target_or_validation_action_required);
  write_json_field(
      out, nested, "source_location_worklist_focus_first_item_environment",
      inferlab_plan::json_string(worklist.first_item_environment));
  write_json_field(
      out, nested, "source_location_worklist_focus_first_item_artifact",
      inferlab_plan::json_string(worklist.first_item_artifact));
  write_json_field(
      out, nested, "source_location_worklist_focus_first_item_category",
      inferlab_plan::json_string(worklist.first_item_category));
  write_json_field(
      out, nested, "source_location_worklist_focus_first_item_source_location",
      inferlab_plan::json_string(worklist.first_item_source_location));
  write_json_field(
      out, nested, "source_location_worklist_focus_first_gap_environment",
      inferlab_plan::json_string(worklist.first_gap_environment));
  write_json_field(
      out, nested, "source_location_worklist_focus_first_gap_category",
      inferlab_plan::json_string(worklist.first_gap_category));
  write_json_field(out, nested, "source_location_worklist_focus_next_action",
                   inferlab_plan::json_string(worklist.next_action));
  write_count_field(out, nested, "source_location_category_count",
                    category.category_count);
  write_count_field(out, nested, "source_location_active_category_count",
                    category.active_category_count);
  write_count_field(out, nested, "source_location_exact_category_count",
                    category.exact_category_count);
  write_count_field(out, nested, "source_location_gap_category_count",
                    category.gap_category_count);
  write_count_field(out, nested,
                    "source_location_current_repo_gap_category_count",
                    category.current_repo_gap_category_count);
  write_count_field(
      out, nested,
      "source_location_target_or_validation_gap_category_count",
      category.target_or_validation_gap_category_count);
  write_count_field(out, nested, "source_location_gap_repair_card_count",
                    category.gap_repair_card_count);
  write_count_field(out, nested, "source_location_category_exactness_percent",
                    category.exactness_percent);
  write_count_field(out, nested, "repair_card_path_rank_count",
                    path_rank.rank_count);
  write_count_field(out, nested, "repair_card_path_current_repo_rank_count",
                    path_rank.current_repo_rank_count);
  write_count_field(
      out, nested, "repair_card_path_target_or_validation_rank_count",
      path_rank.target_or_validation_rank_count);
  write_count_field(out, nested, "repair_card_path_source_location_gap_count",
                    path_rank.source_location_gap_count);
  write_count_field(out, nested, "repair_card_rank_readiness_link_count",
                    rank_readiness.link_count);
  write_count_field(
      out, nested, "repair_card_rank_readiness_current_repo_link_count",
      rank_readiness.current_repo_link_count);
  write_count_field(
      out, nested,
      "repair_card_rank_readiness_target_or_validation_link_count",
      rank_readiness.target_or_validation_link_count);
  write_count_field(
      out, nested, "repair_card_rank_readiness_report_ready_link_count",
      rank_readiness.report_ready_link_count);
  write_count_field(
      out, nested, "repair_card_rank_readiness_replay_ready_link_count",
      rank_readiness.replay_ready_link_count);
  write_count_field(
      out, nested, "repair_card_rank_readiness_execute_ready_link_count",
      rank_readiness.execute_ready_link_count);
  write_count_field(out, nested, "repair_card_source_exactness_card_count",
                    source_exactness.card_count);
  write_count_field(out, nested,
                    "repair_card_source_exactness_exact_card_count",
                    source_exactness.exact_card_count);
  write_count_field(out, nested,
                    "repair_card_source_exactness_gap_card_count",
                    source_exactness.gap_card_count);
  write_count_field(
      out, nested, "repair_card_source_exactness_current_repo_gap_count",
      source_exactness.current_repo_gap_count);
  write_count_field(
      out, nested,
      "repair_card_source_exactness_target_or_validation_gap_count",
      source_exactness.target_or_validation_gap_count);
  write_count_field(out, nested, "repair_card_source_exactness_percent",
                    source_exactness.exactness_percent);
  write_bool_field(out, nested, "release_gate_consumed", true);
  write_json_field(out, nested, "group_plan_status",
                   inferlab_plan::json_string(
                       query_ux.repair_group_plan_status));
  write_json_field(
      out, nested, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query repair-card grouping as "
          "read-only triage metadata only; it does not execute repairs or "
          "create benchmark/replay/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
