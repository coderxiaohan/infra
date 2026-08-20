#pragma once

#include "evidence_catalog_flow_projection.hpp"
#include "evidence_query_artifact_projection.hpp"
#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_consumer_filter_signals.hpp"
#include "evidence_query_operation_guidance_projection.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_repair_policy.hpp"
#include "evidence_query_selection.hpp"
#include "evidence_query_selector_view.hpp"
#include "evidence_query_snapshot_diff.hpp"
#include "evidence_query_view_context.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_evidence_query {

inline std::string selected_operation_text_details(
    const EvidenceQueryOperationViewContext& context,
    const SelectedOperationProjection& selected_operation,
    const CatalogFlowFilterSignals& catalog_flow_signals) {
  const auto& artifact_projection = context.artifact_projection;
  const auto& operation = context.operation;
  const auto& repair_groups = context.repair_groups;
  const auto& guidance = context.guidance;
  const auto& resolution = context.resolution;
  const auto& blocker_drilldown = context.blocker_drilldown;
  const auto& snapshot_diff = context.snapshot_diff;
  const std::size_t payload_count = selected_operation.payload_count;
  std::ostringstream out;
  if (operation.operation == "show_artifact") {
    const auto& detail = artifact_projection.show_artifact;
    const auto& selection = detail.selection;
    const auto* selected = selection.selected;
    out << "selected_operation_payload: artifact_detail"
        << " | matched=" << (selected != nullptr ? "true" : "false")
        << " | candidate_count=" << selection.candidate_count
        << " | exact_match=" << (selection.exact_match ? "true" : "false")
        << " | selector_ambiguous="
        << (selection.selector_ambiguous ? "true" : "false")
        << " | selection_strategy=" << selection.selection_strategy
        << " | first_candidate="
        << (selection.candidate_artifacts.empty()
                ? "<none>"
                : selection.candidate_artifacts.front())
        << " | candidates_truncated="
        << (selection.candidate_list_truncated ? "true" : "false");
    if (selected != nullptr) {
      const auto environment_kind = detail.repair_environment_kind;
      const bool current_repo_action_required =
          environment_kind ==
          inferlab_evidence::RepairEnvironment::kCurrentRepoFixable;
      const bool target_machine_action_required =
          environment_kind ==
          inferlab_evidence::RepairEnvironment::kTargetGpuOrExecuteMachine;
      out << " | artifact=" << selected->artifact
          << " | status=" << detail.status
          << " | blocker=" << detail.category
          << " | confidence=" << detail.confidence
          << " | repair_environment=" << detail.repair_environment
          << " | source_exact="
          << (detail.source_exact ? "true" : "false")
          << " | source_locations="
          << (detail.source_exact ? detail.wiring_locations.size() : 0U)
          << " | source_location_gap="
          << detail.source_location_gap_count
          << " | affected_claims=" << detail.affected_claims.size()
          << " | current_repo_action_required="
          << (current_repo_action_required ? "true" : "false")
          << " | target_machine_action_required="
          << (target_machine_action_required ? "true" : "false")
          << " | first_location="
          << (detail.wiring_locations.empty()
                  ? "<none>"
                  : detail.wiring_locations.front())
          << " | first_action="
          << (selection.selector_ambiguous
                  ? "rerun show artifact with an exact artifact id from candidate_artifacts"
                  : repair_action(detail.category));
    }
    out << "\n";
    return out.str();
  }

  if (operation.operation == "explain_blocker") {
    out << "selected_operation_payload: blocker_explanation"
        << selector_focus_text_fragment(resolution)
        << " | categories=" << blocker_drilldown.category_counts.size()
        << " | blocker_records=" << payload_count
        << " | first_artifact="
        << (blocker_drilldown.first_artifact.empty()
                ? "<none>"
                : blocker_drilldown.first_artifact)
        << " | source_exact_nodes="
        << blocker_drilldown.source_stats.source_exact_node_count
        << " | source_gap_nodes="
        << blocker_drilldown.source_stats.source_gap_node_count
        << " | source_exactness_percent="
        << explain_tree_source_exactness_percent(
               blocker_drilldown.source_stats)
        << " | source_location_gaps="
        << blocker_drilldown.source_location_gap_count
        << " | affected_claims="
        << blocker_drilldown.affected_claims.size()
        << " | exact_locations="
        << blocker_drilldown.exact_wiring_locations.size()
        << " | current_repo_actions="
        << blocker_drilldown.current_repo_actions.size()
        << " | current_server_actions="
        << blocker_drilldown.current_server_actions.size()
        << " | target_machine_actions="
        << blocker_drilldown.target_machine_actions.size()
        << " | target_product_actions="
        << blocker_drilldown.target_product_actions.size()
        << " | first_location=" << blocker_drilldown.first_location
        << " | first_action=" << blocker_drilldown.first_action
        << "\n";
    return out.str();
  }

  if (operation.operation == "suggest_repair") {
    const auto& narrative_steps = guidance.narrative_steps;
    const RepairCardGroup* primary = guidance.primary_group;
    out << "selected_operation_payload: repair_suggestion"
        << selector_focus_text_fragment(resolution)
        << " | groups=" << guidance.group_count
        << " | cards=" << payload_count
        << " | current_repo_groups="
        << guidance.current_repo_group_count
        << " | current_server_groups="
        << guidance.current_server_group_count
        << " | target_machine_groups="
        << guidance.target_machine_group_count
        << " | target_product_groups="
        << guidance.target_product_group_count
        << " | source_location_gaps="
        << guidance.source_location_gap_count
        << " | fallback_removal_queue_state="
        << inferlab_evidence::catalog_flow_fallback_removal_queue_state(
               catalog_flow_signals)
        << " | fallback_removal_queue_next_action="
        << inferlab_evidence::catalog_flow_fallback_removal_queue_next_action(
               catalog_flow_signals)
        << " | fallback_removal_parity_gate_decision="
        << catalog_flow_signals.fallback_removal_parity_gate_decision
        << " | fallback_removal_parity_gate_next_action="
        << catalog_flow_fallback_removal_parity_gate_next_action(
               catalog_flow_signals)
        << " | narrative_steps=" << narrative_steps.size()
        << " | narrative_decision=" << guidance.decision
        << " | narrative_readiness_target=" << guidance.readiness_target
        << " | narrative_readiness_state="
        << guidance.readiness.state
        << " | narrative_primary_env="
        << (primary == nullptr ? "<none>" : primary->repair_environment)
        << " | narrative_first_step="
        << (narrative_steps.empty() ? "no action" : narrative_steps.front())
        << "\n";
    return out.str();
  }

  if (operation.operation == "repair_group_summary") {
    const RepairCardGroup* primary = guidance.primary_group;
    out << "selected_operation_payload: repair_group_summary"
        << selector_focus_text_fragment(resolution)
        << " | groups=" << guidance.group_count
        << " | cards=" << guidance.repair_card_count
        << " | exact_locations="
        << guidance.exact_wiring_location_count
        << " | affected_claims="
        << guidance.affected_claim_count
        << " | source_location_gaps="
        << guidance.source_location_gap_count
        << " | source_worklist_items="
        << source_location_worklist_item_count(repair_groups)
        << " | path_ranks=" << repair_groups.size()
        << " | path_rank_current_repo="
        << repair_card_path_current_repo_rank_count(repair_groups)
        << " | path_rank_target_or_validation="
        << repair_card_path_target_or_validation_rank_count(repair_groups)
        << " | readiness_links=" << repair_groups.size()
        << " | readiness_report_links="
        << repair_card_rank_readiness_report_link_count(repair_groups)
        << " | readiness_replay_links="
        << repair_card_rank_readiness_replay_link_count(repair_groups)
        << " | readiness_execute_links="
        << repair_card_rank_readiness_execute_link_count(repair_groups)
        << " | primary_env="
        << (primary == nullptr ? "<none>" : primary->repair_environment)
        << " | primary_category="
        << (primary == nullptr ? "<none>" : primary->first_category)
        << " | primary_artifact="
        << (primary == nullptr ? "<none>" : primary->first_artifact)
        << " | primary_action_present="
        << (primary != nullptr && !primary->first_action.empty() ? "true"
                                                                  : "false")
        << " | primary_source_location_present="
        << (primary != nullptr && !primary->exact_wiring_locations.empty()
                ? "true"
                : "false")
        << "\n";
    return out.str();
  }

  if (operation.operation == "diff_snapshots") {
    out << "selected_operation_payload: snapshot_diff"
        << " | configured=" << (snapshot_diff.configured ? "true" : "false")
        << " | before="
        << (snapshot_diff.before_configured ? snapshot_diff.before_status
                                            : "not_configured")
        << " | after="
        << (snapshot_diff.after_configured ? snapshot_diff.after_status
                                           : "not_configured")
        << " | read_issues=" << snapshot_diff.read_issue_count
        << " | content_changed="
        << (snapshot_diff.content_changed ? "true" : "false")
        << " | deep_decision="
        << snapshot_diff.deep_semantic_diff_decision
        << " | release_link_status="
        << snapshot_release_link_status(snapshot_diff)
        << " | release_blocker_reason="
        << snapshot_release_blocker_reason(snapshot_diff)
        << " | release_review_required="
        << (snapshot_release_review_required(snapshot_diff) ? "true" : "false")
        << " | release_changed_deep_paths="
        << snapshot_deep_change_count(snapshot_diff)
        << " | release_current_repo_actions="
        << (snapshot_release_review_required(snapshot_diff) ? 1 : 0)
        << " | decision=" << snapshot_diff.diff_decision << "\n";
    return out.str();
  }

  if (operation.operation == "path") {
    const std::string& target = guidance.readiness_target;
    const RepairPathSummary* selected = guidance.readiness_path;
    const ReadinessPathActionabilityProjection& projection =
        guidance.readiness;
    out << "selected_operation_payload: readiness_path"
        << selector_focus_text_fragment(resolution)
        << " | target=" << target
        << " | present=" << (projection.present ? "true" : "false")
        << " | actionability_status=" << projection.actionability_status
        << " | primary_environment=" << projection.primary_environment
        << " | blocker_reason=" << projection.blocker_reason
        << " | current_repo_actionable="
        << (projection.current_repo_actionable ? "true" : "false")
        << " | validation_or_target_required="
        << (projection.validation_or_target_required ? "true" : "false");
    if (selected != nullptr) {
      out << " | state=" << projection.state
          << " | blockers=" << projection.blocking_repair_card_count
          << " | catalog_flow="
          << projection.catalog_flow_discrepancy_count
          << " | current_repo_actions="
          << projection.catalog_flow_current_repo_action_count
          << " | target_machine_actions="
          << projection.catalog_flow_target_machine_action_count
          << " | current_repo_steps="
          << projection.current_repo_step_count
          << " | target_or_validation_steps="
          << projection.target_or_validation_step_count
          << " | source_location_gaps="
          << projection.source_location_gap_count
          << " | fallback_removal_queue_state="
          << inferlab_evidence::catalog_flow_fallback_removal_queue_state(
                 catalog_flow_signals)
          << " | fallback_removal_queue_blocks_removal="
          << (inferlab_evidence::catalog_flow_fallback_removal_queue_active(
                  catalog_flow_signals)
                  ? "true"
                  : "false")
          << " | first_action="
          << (projection.first_path_step.empty() ? "no action"
                                                  : projection.first_path_step)
          << " | next_action=" << projection.next_action;
    }
    out << "\n";
    return out.str();
  }

  const std::string first_artifact =
      artifact_projection.matched_records.empty()
          ? ""
          : artifact_projection.matched_records.front()->artifact;
  out << "selected_operation_payload: "
      << selected_operation.result_kind
      << " | records=" << payload_count
      << " | blocked_or_partial="
      << artifact_projection.blocker_record_count
      << " | first_artifact="
      << (first_artifact.empty() ? "<none>" : first_artifact) << "\n";
  return out.str();
}


}  // namespace inferlab_evidence_query
