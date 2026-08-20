#include "evidence_query_text_view.hpp"

#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_operation_guidance_view.hpp"
#include "evidence_query_operation_catalog.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_repair_source_projection.hpp"
#include "evidence_query_selected_operation_text_view.hpp"
#include "evidence_query_selector_view.hpp"
#include "evidence_query_view_context.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_evidence_query::detail {

std::string evidence_query_text_overview(
    const EvidenceQueryRequest& request) {
  const auto& query = request.query;
  const auto& operation = request.query_operation;
  const auto& artifacts = request.query_artifacts;
  const auto& release = request.release_sources;
  const auto& groups = request.repair_groups;
  const auto& guidance = request.operation_guidance;
  const auto& blocker_drilldown = request.blocker_drilldown;
  const auto& blocker_stats = blocker_drilldown.source_stats;
  const auto& source_location_stats = request.source_location_coverage.stats;
  const auto& source_exactness = request.source_exactness.stats;
  const RepairCardGroup* primary_group = guidance.primary_group;
  const EvidenceQueryOperationViewContext operation_context{
      artifacts,
      operation,
      query,
      request.shortest_paths,
      groups,
      guidance,
      request.selector_resolution,
      blocker_drilldown,
      request.snapshot_diff};

  std::ostringstream out;
  out << "InferLab Evidence Query\n";
  out << "query: " << query << "\n";
  out << "operation: " << operation.operation
      << " | label=" << operation.human_label
      << " | selector="
      << (operation.selector.empty() ? "<none>" : operation.selector)
      << " | target="
      << (operation.target.empty() ? "<none>" : operation.target) << "\n";
  const auto& operation_names = evidence_query_operation_names();
  out << "supported_operations: ";
  for (std::size_t index = 0; index < operation_names.size(); ++index) {
    if (index != 0U) out << ", ";
    out << operation_names[index];
  }
  out << "\n";
  const auto operation_summary = evidence_query_operation_catalog_summary();
  out << "operation_catalog: supported="
      << operation_summary.supported_operation_count
      << " | read_only=" << operation_summary.read_only_operation_count
      << " | snapshot_required="
      << operation_summary.snapshot_required_operation_count
      << " | selector_recommended="
      << operation_summary.selector_recommended_operation_count
      << " | target_required="
      << operation_summary.target_required_operation_count
      << " | executes_commands=false\n";
  out << "selected_operation_result: kind="
      << request.selected_operation.result_kind
      << " | input_ready="
      << (request.selected_operation.input_ready ? "true" : "false")
      << " | payload_count=" << request.selected_operation.payload_count
      << " | read_only=true\n";
  out << selector_resolution_text(request.selector_resolution);
  out << selector_action_plan_text(guidance, request.selector_resolution);
  out << selected_operation_text_details(
      operation_context, request.selected_operation, request.catalog_flow);
  out << "artifact_filter: "
      << (request.artifact_filter.empty() ? "<none>"
                                          : request.artifact_filter)
      << "\n";
  out << "blocker_filter: "
      << (request.blocker_filter.empty() ? "<none>"
                                         : request.blocker_filter)
      << "\n";
  out << "explain_tree: nodes=" << blocker_stats.node_count
      << " | source_exact_nodes=" << blocker_stats.source_exact_node_count
      << " | source_gap_nodes=" << blocker_stats.source_gap_node_count
      << " | current_repo_source_gaps="
      << blocker_stats.current_repo_source_gap_count
      << " | target_or_validation_source_gaps="
      << blocker_stats.target_or_validation_source_gap_count
      << " | source_exactness_percent="
      << explain_tree_source_exactness_percent(blocker_stats) << "\n";
  out << "source_location_categories: categories="
      << source_location_stats.category_count
      << " | active=" << source_location_stats.active_category_count
      << " | source_exact="
      << source_location_stats.source_exact_category_count
      << " | source_gaps="
      << source_location_stats.source_gap_category_count
      << " | current_repo_gaps="
      << source_location_stats.current_repo_source_gap_category_count
      << " | target_or_validation_gaps="
      << source_location_stats.target_or_validation_source_gap_category_count
      << " | gap_repair_cards="
      << source_location_stats.source_gap_repair_card_count
      << " | source_exactness_percent="
      << source_location_category_exactness_percent(source_location_stats)
      << "\n";
  out << "warehouse_used: "
      << (request.source.warehouse_used ? "true" : "false") << "\n";
  out << "fixture_used: "
      << (request.source.fixture_used ? "true" : "false") << "\n";
  out << "direct_scan_used: "
      << (request.source.direct_scan_used ? "true" : "false") << "\n";
  out << "direct_scan_source_read_issues: "
      << request.source.scan_source_read_issue_count << "\n";
  out << "matched_records: " << artifacts.matched_records.size() << "\n";
  out << "blocked_or_partial_records: " << artifacts.blocker_record_count
      << "\n";
  out << "root_cause_repair_cards: " << release.root_cause_repair_card_count
      << " | projection_source=warehouse_query_snapshot\n";
  out << "repair_path_source_gaps: " << release.source_gap_count
      << " | source_read_issues=" << release.source_read_issue_count
      << " | source_locations="
      << release.repair_path_source_location_count
      << " | source_location_gaps="
      << release.repair_path_source_location_gap_count
      << " | root_cause_source_location_cards="
      << release.effective_source_location_card_count
      << " | current_repo_cards=" << release.current_repo_repair_card_count
      << " | target_product_replay_cards="
      << release.target_product_replay_repair_card_count
      << " | target_gpu_execute_cards="
      << release.target_gpu_execute_repair_card_count
      << " | target_machine_cards="
      << release.target_machine_repair_card_count
      << " | repair_environment_split=" << release.repair_counts.split_status
      << " | repair_environment_split_schema_gap="
      << (release.repair_counts.split_schema_gap ? "true" : "false")
      << " | repair_environment_legacy_fallback="
      << (release.repair_counts.legacy_fallback_used ? "true" : "false")
      << " | flow_check_cards=" << release.flow_check_repair_card_count
      << "\n";
  out << "repair_card_groups: groups=" << guidance.group_count
      << " | cards=" << guidance.repair_card_count
      << " | current_repo=" << guidance.current_repo_group_count
      << " | current_server=" << guidance.current_server_group_count
      << " | target_machine=" << guidance.target_machine_group_count
      << " | target_product=" << guidance.target_product_group_count
      << " | source_location_gaps=" << guidance.source_location_gap_count
      << " | exact_locations=" << guidance.exact_wiring_location_count
      << " | affected_claims=" << guidance.affected_claim_count
      << " | source_worklist_items="
      << source_location_worklist_item_count(groups)
      << " | source_worklist_current_repo="
      << source_location_worklist_current_repo_item_count(groups)
      << " | source_worklist_target_or_validation="
      << source_location_worklist_target_or_validation_item_count(groups)
      << " | source_worklist_gaps=" << guidance.source_location_gap_count
      << " | path_ranks=" << guidance.group_count
      << " | path_rank_current_repo="
      << repair_card_path_current_repo_rank_count(groups)
      << " | path_rank_target_or_validation="
      << repair_card_path_target_or_validation_rank_count(groups)
      << " | path_rank_location_gaps=" << guidance.source_location_gap_count
      << " | readiness_links=" << guidance.group_count
      << " | readiness_current_repo_links="
      << repair_card_path_current_repo_rank_count(groups)
      << " | readiness_target_or_validation_links="
      << repair_card_path_target_or_validation_rank_count(groups)
      << " | readiness_report_links="
      << repair_card_rank_readiness_report_link_count(groups)
      << " | readiness_replay_links="
      << repair_card_rank_readiness_replay_link_count(groups)
      << " | readiness_execute_links="
      << repair_card_rank_readiness_execute_link_count(groups)
      << " | source_exactness_cards="
      << source_exactness.repair_card_count
      << " | source_exact_cards=" << source_exactness.source_exact_card_count
      << " | source_gap_cards=" << source_exactness.source_gap_card_count
      << " | source_exactness_percent="
      << repair_card_source_exactness_percent(source_exactness)
      << " | primary_env="
      << (primary_group == nullptr ? "" : primary_group->repair_environment)
      << " | primary_category="
      << (primary_group == nullptr ? "" : primary_group->first_category)
      << " | navigation_steps=" << groups.size()
      << " | navigation_current_repo="
      << repair_group_navigation_current_repo_step_count(groups)
      << " | navigation_target_or_validation="
      << repair_group_navigation_target_or_validation_step_count(groups)
      << " | primary_action_present="
      << (primary_group != nullptr && !primary_group->first_action.empty()
              ? "true"
              : "false")
      << "\n";
  return out.str();
}

}  // namespace inferlab_evidence_query::detail
