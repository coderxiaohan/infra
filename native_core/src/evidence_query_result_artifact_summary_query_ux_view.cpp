#include "evidence_query_result_artifact_sections.hpp"

#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_repair_source_projection.hpp"
#include "evidence_query_source_location_view.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

void append_result_artifact_summary_query_ux(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& request = projection.request;
  const std::size_t blocked_count =
      request.query_artifacts.blocked_result_count;
  const auto& query_operation_guidance = request.operation_guidance;
  const auto& query_selector_resolution = request.selector_resolution;
  const auto& query_ux_repair_groups = request.repair_groups;
  const RepairCardGroup* primary_query_ux_repair_group =
      query_operation_guidance.primary_group;
  const auto& query_ux_blocker_drilldown = request.blocker_drilldown;
  const auto& query_ux_explain_tree_categories =
      query_ux_blocker_drilldown.category_counts;
  const auto& query_ux_explain_tree_stats =
      query_ux_blocker_drilldown.source_stats;
  const auto& query_ux_source_location_category_stats =
      request.source_location_coverage.stats;
  const auto& query_ux_source_exactness = request.source_exactness.stats;
  const auto& query_ux_categories =
      request.repair_collection.matching_categories;
  const auto& query_ux_locations =
      request.repair_collection.exact_wiring_locations;
  const auto& shortest_paths = request.shortest_paths;
  const auto& query_ux_source_location_focus =
      projection.source_location_focus;
  const RepairCardGroup* query_ux_source_location_first_item =
      query_ux_source_location_focus.first_item_group;
  const RepairCardGroup* query_ux_source_location_first_gap =
      query_ux_source_location_focus.first_gap_group;
  const auto& query_ux_source_location_first_item_location =
      query_ux_source_location_focus.first_item_source_location;
  const bool query_ux_source_location_first_item_present =
      query_ux_source_location_first_item != nullptr;
  const bool query_ux_source_location_first_gap_present =
      query_ux_source_location_first_gap != nullptr;
  const bool query_ux_source_location_first_item_current_repo =
      query_ux_source_location_focus.first_item_current_repo;
  out << "    \"query_ux_explain_tree_node_count\": "
      << query_ux_explain_tree_stats.node_count << ",\n";
  out << "    \"query_ux_explain_tree_source_exact_node_count\": "
      << query_ux_explain_tree_stats.source_exact_node_count << ",\n";
  out << "    \"query_ux_explain_tree_source_gap_node_count\": "
      << query_ux_explain_tree_stats.source_gap_node_count << ",\n";
  out << "    \"query_ux_explain_tree_current_repo_source_gap_count\": "
      << query_ux_explain_tree_stats.current_repo_source_gap_count << ",\n";
  out
      << "    \"query_ux_explain_tree_target_or_validation_source_gap_count\": "
      << query_ux_explain_tree_stats.target_or_validation_source_gap_count
      << ",\n";
  out << "    \"query_ux_explain_tree_source_exactness_percent\": "
      << explain_tree_source_exactness_percent(query_ux_explain_tree_stats)
      << ",\n";
  out << "    \"query_ux_blocker_drilldown_record_count\": "
      << query_ux_blocker_drilldown.blocker_record_count << ",\n";
  out << "    \"query_ux_blocker_drilldown_category_count\": "
      << query_ux_explain_tree_categories.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_source_location_gap_count\": "
      << query_ux_blocker_drilldown.source_location_gap_count << ",\n";
  out << "    \"query_ux_blocker_drilldown_affected_claim_count\": "
      << query_ux_blocker_drilldown.affected_claims.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_exact_wiring_location_count\": "
      << query_ux_blocker_drilldown.exact_wiring_locations.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_current_repo_action_count\": "
      << query_ux_blocker_drilldown.current_repo_actions.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_current_server_action_count\": "
      << query_ux_blocker_drilldown.current_server_actions.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_target_machine_action_count\": "
      << query_ux_blocker_drilldown.target_machine_actions.size() << ",\n";
  out << "    \"query_ux_blocker_drilldown_target_product_action_count\": "
      << query_ux_blocker_drilldown.target_product_actions.size() << ",\n";
  out << "    \"query_ux_repair_card_count\": " << blocked_count << ",\n";
  out << "    \"query_ux_repair_card_group_count\": "
      << query_operation_guidance.group_count << ",\n";
  out << "    \"query_ux_repair_card_group_card_count\": "
      << query_operation_guidance.repair_card_count << ",\n";
  out << "    \"query_ux_repair_card_group_current_repo_count\": "
      << query_operation_guidance.current_repo_group_count
      << ",\n";
  out << "    \"query_ux_repair_card_group_current_server_count\": "
      << query_operation_guidance.current_server_group_count
      << ",\n";
  out << "    \"query_ux_repair_card_group_target_machine_count\": "
      << query_operation_guidance.target_machine_group_count
      << ",\n";
  out << "    \"query_ux_repair_card_group_target_product_count\": "
      << query_operation_guidance.target_product_group_count
      << ",\n";
  out << "    \"query_ux_repair_card_group_source_location_gap_count\": "
      << query_operation_guidance.source_location_gap_count
      << ",\n";
  out << "    \"query_ux_repair_group_summary_present\": true,\n";
  out << "    \"query_ux_repair_group_exact_wiring_location_count\": "
      << query_operation_guidance.exact_wiring_location_count
      << ",\n";
  out << "    \"query_ux_repair_group_affected_claim_count\": "
      << query_operation_guidance.affected_claim_count
      << ",\n";
  out << "    \"query_ux_repair_group_primary_action_present\": "
      << (primary_query_ux_repair_group != nullptr &&
                  !primary_query_ux_repair_group->first_action.empty()
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_ux_repair_group_primary_source_location_present\": "
      << (primary_query_ux_repair_group != nullptr &&
                  !primary_query_ux_repair_group->exact_wiring_locations.empty()
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_ux_repair_group_primary_environment\": "
      << inferlab_plan::json_string(
             primary_query_ux_repair_group == nullptr
                 ? ""
                 : primary_query_ux_repair_group->repair_environment)
      << ",\n";
  out << "    \"query_ux_repair_group_primary_category\": "
      << inferlab_plan::json_string(
             primary_query_ux_repair_group == nullptr
                 ? ""
                 : primary_query_ux_repair_group->first_category)
      << ",\n";
  out << "    \"query_ux_repair_group_primary_artifact\": "
      << inferlab_plan::json_string(
             primary_query_ux_repair_group == nullptr
                 ? ""
                 : primary_query_ux_repair_group->first_artifact)
      << ",\n";
  out << "    \"query_ux_repair_narrative_present\": true,\n";
  out << "    \"query_ux_repair_narrative_decision\": "
      << inferlab_plan::json_string(query_operation_guidance.decision)
      << ",\n";
  out << "    \"query_ux_repair_narrative_readiness_state\": "
      << inferlab_plan::json_string(query_operation_guidance.readiness.state)
      << ",\n";
  out << "    \"query_ux_repair_narrative_step_count\": "
      << query_operation_guidance.narrative_steps.size() << ",\n";
  out << "    \"query_ux_repair_narrative_current_repo_action_count\": "
      << query_selector_resolution.current_repo_actions.size() << ",\n";
  out << "    \"query_ux_repair_narrative_target_or_validation_action_count\": "
      << query_operation_guidance.target_or_validation_step_count << ",\n";
  out << "    \"query_ux_repair_narrative_source_location_gap_count\": "
      << query_operation_guidance.source_location_gap_count << ",\n";
  out << "    \"query_ux_repair_narrative_readiness_blocker_count\": "
      << query_operation_guidance.readiness.blocking_repair_card_count
      << ",\n";
  out << "    \"query_ux_repair_narrative_affected_claim_count\": "
      << query_operation_guidance.affected_claim_count << ",\n";
  out << "    \"query_ux_repair_narrative_exact_wiring_location_count\": "
      << query_operation_guidance.exact_wiring_location_count << ",\n";
  out << "    \"query_ux_repair_group_navigation_step_count\": "
      << query_ux_repair_groups.size() << ",\n";
  out << "    \"query_ux_repair_group_navigation_current_repo_step_count\": "
      << repair_group_navigation_current_repo_step_count(
             query_ux_repair_groups)
      << ",\n";
  out
      << "    \"query_ux_repair_group_navigation_target_or_validation_step_count\": "
      << repair_group_navigation_target_or_validation_step_count(
             query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_source_location_worklist_item_count\": "
      << source_location_worklist_item_count(query_ux_repair_groups) << ",\n";
  out << "    \"query_ux_source_location_worklist_current_repo_item_count\": "
      << source_location_worklist_current_repo_item_count(
             query_ux_repair_groups)
      << ",\n";
  out
      << "    \"query_ux_source_location_worklist_target_or_validation_item_count\": "
      << source_location_worklist_target_or_validation_item_count(
             query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_source_location_worklist_gap_count\": "
      << query_operation_guidance.source_location_gap_count
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_present\": true,\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_present\": "
      << (query_ux_source_location_first_item_present ? "true" : "false")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_gap_present\": "
      << (query_ux_source_location_first_gap_present ? "true" : "false")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_current_repo_action_required\": "
      << (query_ux_source_location_first_item_current_repo ? "true" : "false")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required\": "
      << (query_ux_source_location_first_item_present &&
                  !query_ux_source_location_first_item_current_repo
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_environment\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_item_present
                 ? query_ux_source_location_first_item->repair_environment
                 : "")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_artifact\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_item_present
                 ? query_ux_source_location_first_item->first_artifact
                 : "")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_category\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_item_present
                 ? query_ux_source_location_first_item->first_category
                 : "")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_item_source_location\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_item_location)
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_gap_environment\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_gap_present
                 ? query_ux_source_location_first_gap->repair_environment
                 : "")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_first_gap_category\": "
      << inferlab_plan::json_string(
             query_ux_source_location_first_gap_present
                 ? query_ux_source_location_first_gap
                       ->first_source_gap_category
                 : "")
      << ",\n";
  out << "    \"query_ux_source_location_worklist_focus_next_action\": "
      << inferlab_plan::json_string(query_ux_source_location_focus.next_action)
      << ",\n";
  out << "    \"query_ux_source_location_category_count\": "
      << query_ux_source_location_category_stats.category_count << ",\n";
  out << "    \"query_ux_source_location_active_category_count\": "
      << query_ux_source_location_category_stats.active_category_count
      << ",\n";
  out << "    \"query_ux_source_location_exact_category_count\": "
      << query_ux_source_location_category_stats.source_exact_category_count
      << ",\n";
  out << "    \"query_ux_source_location_gap_category_count\": "
      << query_ux_source_location_category_stats.source_gap_category_count
      << ",\n";
  out << "    \"query_ux_source_location_current_repo_gap_category_count\": "
      << query_ux_source_location_category_stats
             .current_repo_source_gap_category_count
      << ",\n";
  out << "    \"query_ux_source_location_target_or_validation_gap_category_count\": "
      << query_ux_source_location_category_stats
             .target_or_validation_source_gap_category_count
      << ",\n";
  out << "    \"query_ux_source_location_gap_repair_card_count\": "
      << query_ux_source_location_category_stats.source_gap_repair_card_count
      << ",\n";
  out << "    \"query_ux_source_location_category_exactness_percent\": "
      << source_location_category_exactness_percent(
             query_ux_source_location_category_stats)
      << ",\n";
  out << "    \"query_ux_repair_card_path_rank_count\": "
      << query_operation_guidance.group_count << ",\n";
  out << "    \"query_ux_repair_card_path_current_repo_rank_count\": "
      << repair_card_path_current_repo_rank_count(query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_path_target_or_validation_rank_count\": "
      << repair_card_path_target_or_validation_rank_count(
             query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_path_source_location_gap_count\": "
      << query_operation_guidance.source_location_gap_count
      << ",\n";
  out << "    \"query_ux_repair_card_rank_readiness_link_count\": "
      << query_operation_guidance.group_count << ",\n";
  out << "    \"query_ux_repair_card_rank_readiness_current_repo_link_count\": "
      << repair_card_path_current_repo_rank_count(query_ux_repair_groups)
      << ",\n";
  out
      << "    \"query_ux_repair_card_rank_readiness_target_or_validation_link_count\": "
      << repair_card_path_target_or_validation_rank_count(
             query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_rank_readiness_report_ready_link_count\": "
      << repair_card_rank_readiness_report_link_count(query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_rank_readiness_replay_ready_link_count\": "
      << repair_card_rank_readiness_replay_link_count(query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_rank_readiness_execute_ready_link_count\": "
      << repair_card_rank_readiness_execute_link_count(query_ux_repair_groups)
      << ",\n";
  out << "    \"query_ux_repair_card_source_exactness_card_count\": "
      << query_ux_source_exactness.repair_card_count << ",\n";
  out << "    \"query_ux_repair_card_source_exactness_exact_card_count\": "
      << query_ux_source_exactness.source_exact_card_count << ",\n";
  out << "    \"query_ux_repair_card_source_exactness_gap_card_count\": "
      << query_ux_source_exactness.source_gap_card_count << ",\n";
  out << "    \"query_ux_repair_card_source_exactness_current_repo_gap_count\": "
      << query_ux_source_exactness.current_repo_source_gap_card_count
      << ",\n";
  out
      << "    \"query_ux_repair_card_source_exactness_target_or_validation_gap_count\": "
      << query_ux_source_exactness
             .target_or_validation_source_gap_card_count
      << ",\n";
  out << "    \"query_ux_repair_card_source_exactness_percent\": "
      << repair_card_source_exactness_percent(query_ux_source_exactness)
      << ",\n";
  out << "    \"query_ux_blocker_category_count\": "
      << query_ux_categories.size() << ",\n";
  out << "    \"query_ux_exact_wiring_location_count\": "
      << query_ux_locations.size() << ",\n";
  out << "    \"query_ux_report_ready_blocker_count\": "
      << (shortest_paths.size() > 0
              ? shortest_paths[0].blocking_repair_card_count
              : 0U)
      << ",\n";
  out << "    \"query_ux_replay_ready_blocker_count\": "
      << (shortest_paths.size() > 1
              ? shortest_paths[1].blocking_repair_card_count
              : 0U)
      << ",\n";
  out << "    \"query_ux_execute_ready_blocker_count\": "
      << (shortest_paths.size() > 2
              ? shortest_paths[2].blocking_repair_card_count
              : 0U)
      << ",\n";
  {
    const auto& readiness_projection = query_operation_guidance.readiness;
    const std::string& readiness_target =
        query_operation_guidance.readiness_target;
    out << "    \"query_ux_readiness_path_target\": "
        << inferlab_plan::json_string(readiness_target) << ",\n";
    out << "    \"query_ux_readiness_path_state\": "
        << inferlab_plan::json_string(readiness_projection.state)
        << ",\n";
    out << "    \"query_ux_readiness_path_blocking_repair_card_count\": "
        << readiness_projection.blocking_repair_card_count
        << ",\n";
    out << "    \"query_ux_readiness_path_step_count\": "
        << readiness_projection.path_step_count
        << ",\n";
    out << "    \"query_ux_readiness_path_current_repo_step_count\": "
        << readiness_projection.current_repo_step_count
        << ",\n";
    out << "    \"query_ux_readiness_path_target_or_validation_step_count\": "
        << readiness_projection.target_or_validation_step_count
        << ",\n";
    out << "    \"query_ux_readiness_path_source_location_gap_count\": "
        << readiness_projection.source_location_gap_count
        << ",\n";
    out << "    \"query_ux_readiness_path_catalog_flow_discrepancy_count\": "
        << readiness_projection.catalog_flow_discrepancy_count
        << ",\n";
    out << "    \"query_ux_readiness_path_fallback_removal_blocked_count\": "
        << readiness_projection.catalog_flow_fallback_removal_blocked_count
        << ",\n";
    out << "    \"query_ux_readiness_path_field_coverage_gap_count\": "
        << readiness_projection.catalog_flow_field_coverage_gap_count
        << ",\n";
    out << "    \"query_ux_readiness_path_actionability_status\": "
        << inferlab_plan::json_string(readiness_projection.actionability_status)
        << ",\n";
    out << "    \"query_ux_readiness_path_primary_environment\": "
        << inferlab_plan::json_string(readiness_projection.primary_environment)
        << ",\n";
    out << "    \"query_ux_readiness_path_blocker_reason\": "
        << inferlab_plan::json_string(readiness_projection.blocker_reason)
        << ",\n";
    out << "    \"query_ux_readiness_path_current_repo_actionable\": "
        << inferlab_plan::json_bool(readiness_projection.current_repo_actionable)
        << ",\n";
    out << "    \"query_ux_readiness_path_validation_or_target_required\": "
        << inferlab_plan::json_bool(
               readiness_projection.validation_or_target_required)
        << ",\n";
    out << "    \"query_ux_readiness_path_source_location_metadata_required\": "
        << inferlab_plan::json_bool(
               readiness_projection.source_location_metadata_required)
        << ",\n";
    out << "    \"query_ux_readiness_path_next_action_present\": "
        << inferlab_plan::json_bool(!readiness_projection.next_action.empty())
        << ",\n";
  }

}

}  // namespace inferlab_evidence_query::detail
