#include "evidence_query_text_view.hpp"

#include "evidence_catalog_fallback_removal_projection.hpp"
#include "evidence_query_consumer_filter_signals.hpp"

#include <sstream>

namespace inferlab_evidence_query::detail {

std::string evidence_query_text_filter_status(
    const EvidenceQueryRequest& request) {
  const auto& signals = request.catalog_flow;
  std::ostringstream out;
  out << "catalog_flow_filter: decision=" << signals.filter_decision
      << " | catalog_present="
      << (signals.catalog_artifact_present ? "true" : "false")
      << " | read_status=" << signals.catalog_artifact_read_status
      << " | read_issues=" << signals.read_issue_count
      << " | discrepancies=" << signals.discrepancy_count
      << " | input_gaps=" << signals.input_gap_count
      << " | missing_surfaces=" << signals.missing_surface_count
      << " | fallback_retained_surfaces="
      << signals.fallback_retained_surface_count
      << " | single_source_cards="
      << signals.single_source_repair_card_count
      << " | parity_refresh_required="
      << signals.single_source_parity_refresh_required_count
      << " | surface_ready=" << signals.surface_migration_ready_count
      << " | surface_fallback=" << signals.surface_migration_fallback_count
      << " | surface_parity_refresh="
      << signals.surface_parity_refresh_required_count
      << " | surface_consumer_plans="
      << signals.surface_consumer_migration_plan_count
      << " | surface_consumer_fallback_blocked="
      << signals.surface_consumer_fallback_removal_blocked_count
      << " | surface_consumer_parity_refresh="
      << signals.surface_consumer_parity_refresh_required_count
      << " | surface_consumer_current_repo_actions="
      << signals.surface_consumer_current_repo_action_count
      << " | surface_consumer_target_machine_actions="
      << signals.surface_consumer_target_machine_action_count
      << " | field_coverage_surfaces="
      << signals.consumer_field_coverage_surface_count
      << " | field_coverage_fallback_surfaces="
      << signals.consumer_field_coverage_fallback_surface_count
      << " | field_coverage_required_fields="
      << signals.consumer_field_coverage_required_field_count
      << " | field_coverage_field_gaps="
      << signals.consumer_field_coverage_field_gap_count
      << " | field_coverage_parity_refresh="
      << signals.consumer_field_coverage_parity_refresh_required_count
      << " | field_coverage_repair_cards="
      << signals.consumer_field_coverage_repair_card_count
      << " | field_coverage_current_repo_cards="
      << signals.consumer_field_coverage_current_repo_repair_card_count
      << " | field_coverage_target_machine_cards="
      << signals.consumer_field_coverage_target_machine_repair_card_count
      << " | metadata_policy_cards=" << signals.metadata_policy_card_count
      << " | metadata_policy_artifacts="
      << signals.metadata_derived_policy_artifact_count
      << " | metadata_policy_fields="
      << signals.metadata_derived_policy_field_count
      << " | missing_steps=" << signals.missing_step_count
      << " | repair_cards=" << signals.repair_card_count << "\n";
  out << "catalog_flow_fallback_removal_queue: state="
      << inferlab_evidence::catalog_flow_fallback_removal_queue_state(signals)
      << " | items=" << signals.fallback_removal_queue_item_count
      << " | blocked=" << signals.fallback_removal_queue_blocked_count
      << " | parity_refresh_required="
      << signals.fallback_removal_queue_parity_refresh_required_count
      << " | current_repo_actions="
      << signals.fallback_removal_queue_current_repo_action_count
      << " | target_machine_actions="
      << signals.fallback_removal_queue_target_machine_action_count
      << " | blocks_fallback_removal="
      << (inferlab_evidence::catalog_flow_fallback_removal_queue_active(signals)
              ? "true"
              : "false")
      << " | repair_environment="
      << inferlab_evidence::catalog_flow_fallback_removal_queue_environment(
             signals)
      << " | next_action="
      << inferlab_evidence::catalog_flow_fallback_removal_queue_next_action(
             signals)
      << "\n";
  out << "catalog_flow_fallback_removal_shortest_path: present="
      << (signals.fallback_removal_shortest_path_present ? "true" : "false")
      << " | state=" << signals.fallback_removal_shortest_path_state
      << " | steps=" << signals.fallback_removal_shortest_path_step_count
      << " | current_repo_steps="
      << signals.fallback_removal_shortest_path_current_repo_step_count
      << " | target_machine_steps="
      << signals.fallback_removal_shortest_path_target_machine_step_count
      << " | source_location_gaps="
      << signals.fallback_removal_shortest_path_source_location_gap_count
      << " | primary_environment="
      << signals.fallback_removal_shortest_path_primary_environment
      << " | first_surface="
      << (signals.fallback_removal_shortest_path_first_surface.empty()
              ? "<none>"
              : signals.fallback_removal_shortest_path_first_surface)
      << " | blocks_fallback_removal="
      << (inferlab_evidence::
                  catalog_flow_fallback_removal_shortest_path_active(signals)
              ? "true"
              : "false")
      << " | next_action="
      << catalog_flow_fallback_removal_shortest_path_next_action(signals)
      << "\n";
  out << "catalog_flow_fallback_removal_parity_gate: present="
      << (signals.fallback_removal_parity_gate_present ? "true" : "false")
      << " | decision=" << signals.fallback_removal_parity_gate_decision
      << " | removal_allowed="
      << (signals.fallback_removal_parity_gate_removal_allowed ? "true"
                                                               : "false")
      << " | blockers=" << signals.fallback_removal_parity_gate_blocker_count
      << " | fallback_surfaces="
      << signals.fallback_removal_parity_gate_fallback_surface_count
      << " | parity_refresh_required="
      << signals.fallback_removal_parity_gate_parity_refresh_required_count
      << " | current_repo_actions="
      << signals.fallback_removal_parity_gate_current_repo_action_count
      << " | target_machine_actions="
      << signals.fallback_removal_parity_gate_target_machine_action_count
      << " | source_location_gaps="
      << signals.fallback_removal_parity_gate_source_location_gap_count
      << " | first_surface="
      << (signals.fallback_removal_parity_gate_first_surface.empty()
              ? "<none>"
              : signals.fallback_removal_parity_gate_first_surface)
      << " | blocks_fallback_removal="
      << (inferlab_evidence::catalog_flow_fallback_removal_parity_gate_active(
              signals)
              ? "true"
              : "false")
      << " | next_action="
      << catalog_flow_fallback_removal_parity_gate_next_action(signals)
      << "\n";
  return out.str();
}

}  // namespace inferlab_evidence_query::detail
