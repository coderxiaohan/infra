#include "evidence_query_result_artifact_sections.hpp"

#include "evidence_catalog_fallback_removal_projection.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

using inferlab_evidence::catalog_flow_fallback_removal_parity_gate_active;
using inferlab_evidence::catalog_flow_fallback_removal_shortest_path_active;

void append_result_artifact_summary_catalog_review(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& request = projection.request;
  const auto& release_sources = request.release_sources;
  const auto& catalog_flow_signals = request.catalog_flow;
  const std::size_t filter_source_read_issue_count =
      request.filter_source_read_issue_count;
  out << "    \"catalog_flow_filter_query_present\": true,\n";
  out << "    \"filter_source_read_issue_count\": "
      << filter_source_read_issue_count << ",\n";
  out << "    \"catalog_flow_catalog_artifact_readable\": "
      << (catalog_flow_signals.catalog_artifact_readable ? "true" : "false")
      << ",\n";
  out << "    \"catalog_flow_catalog_artifact_read_status\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.catalog_artifact_read_status)
      << ",\n";
  out << "    \"catalog_flow_read_issue_count\": "
      << catalog_flow_signals.read_issue_count << ",\n";
  out << "    \"catalog_flow_input_gap_count\": "
      << catalog_flow_signals.input_gap_count << ",\n";
  out << "    \"catalog_flow_missing_surface_count\": "
      << catalog_flow_signals.missing_surface_count << ",\n";
  out << "    \"catalog_flow_fallback_retained_surface_count\": "
      << catalog_flow_signals.fallback_retained_surface_count << ",\n";
  out << "    \"catalog_flow_single_source_repair_card_count\": "
      << catalog_flow_signals.single_source_repair_card_count << ",\n";
  out << "    \"catalog_flow_single_source_parity_refresh_required_count\": "
      << catalog_flow_signals.single_source_parity_refresh_required_count
      << ",\n";
  out << "    \"catalog_flow_surface_migration_ready_count\": "
      << catalog_flow_signals.surface_migration_ready_count << ",\n";
  out << "    \"catalog_flow_surface_migration_fallback_count\": "
      << catalog_flow_signals.surface_migration_fallback_count << ",\n";
  out << "    \"catalog_flow_surface_parity_refresh_required_count\": "
      << catalog_flow_signals.surface_parity_refresh_required_count << ",\n";
  out << "    \"catalog_flow_surface_current_repo_action_count\": "
      << catalog_flow_signals.surface_current_repo_action_count << ",\n";
  out << "    \"catalog_flow_surface_target_machine_action_count\": "
      << catalog_flow_signals.surface_target_machine_action_count << ",\n";
  out << "    \"catalog_flow_surface_consumer_migration_plan_count\": "
      << catalog_flow_signals.surface_consumer_migration_plan_count << ",\n";
  out << "    \"catalog_flow_surface_consumer_fallback_removal_blocked_count\": "
      << catalog_flow_signals.surface_consumer_fallback_removal_blocked_count
      << ",\n";
  out << "    \"catalog_flow_surface_consumer_parity_refresh_required_count\": "
      << catalog_flow_signals.surface_consumer_parity_refresh_required_count
      << ",\n";
  out << "    \"catalog_flow_surface_consumer_current_repo_action_count\": "
      << catalog_flow_signals.surface_consumer_current_repo_action_count
      << ",\n";
  out << "    \"catalog_flow_surface_consumer_target_machine_action_count\": "
      << catalog_flow_signals.surface_consumer_target_machine_action_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_queue_item_count\": "
      << catalog_flow_signals.fallback_removal_queue_item_count << ",\n";
  out << "    \"catalog_flow_fallback_removal_queue_blocked_count\": "
      << catalog_flow_signals.fallback_removal_queue_blocked_count << ",\n";
  out << "    \"catalog_flow_fallback_removal_queue_parity_refresh_required_count\": "
      << catalog_flow_signals.fallback_removal_queue_parity_refresh_required_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_queue_current_repo_action_count\": "
      << catalog_flow_signals.fallback_removal_queue_current_repo_action_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_queue_target_machine_action_count\": "
      << catalog_flow_signals.fallback_removal_queue_target_machine_action_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_present\": "
      << (catalog_flow_signals.fallback_removal_shortest_path_present ? "true"
                                                                      : "false")
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_state\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.fallback_removal_shortest_path_state)
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_step_count\": "
      << catalog_flow_signals.fallback_removal_shortest_path_step_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_current_repo_step_count\": "
      << catalog_flow_signals
             .fallback_removal_shortest_path_current_repo_step_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_target_machine_step_count\": "
      << catalog_flow_signals
             .fallback_removal_shortest_path_target_machine_step_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_source_location_gap_count\": "
      << catalog_flow_signals
             .fallback_removal_shortest_path_source_location_gap_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_primary_environment\": "
      << inferlab_plan::json_string(
             catalog_flow_signals
                 .fallback_removal_shortest_path_primary_environment)
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_first_surface\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.fallback_removal_shortest_path_first_surface)
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_shortest_path_blocks_removal\": "
      << (catalog_flow_fallback_removal_shortest_path_active(
              catalog_flow_signals)
              ? "true"
              : "false")
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_present\": "
      << (catalog_flow_signals.fallback_removal_parity_gate_present ? "true"
                                                                    : "false")
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_decision\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.fallback_removal_parity_gate_decision)
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_removal_allowed\": "
      << (catalog_flow_signals.fallback_removal_parity_gate_removal_allowed
              ? "true"
              : "false")
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_blocker_count\": "
      << catalog_flow_signals.fallback_removal_parity_gate_blocker_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_blocking_surface_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_blocking_surface_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_fallback_surface_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_fallback_surface_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_parity_refresh_required_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_current_repo_action_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_current_repo_action_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_target_machine_action_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_target_machine_action_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_source_location_gap_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_source_location_gap_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count\": "
      << catalog_flow_signals
             .fallback_removal_parity_gate_blocking_surface_source_location_gap_count
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_first_surface\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.fallback_removal_parity_gate_first_surface)
      << ",\n";
  out << "    \"catalog_flow_fallback_removal_parity_gate_blocks_removal\": "
      << (catalog_flow_fallback_removal_parity_gate_active(
              catalog_flow_signals)
              ? "true"
              : "false")
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_surface_count\": "
      << catalog_flow_signals.consumer_field_coverage_surface_count << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_catalog_managed_surface_count\": "
      << catalog_flow_signals
             .consumer_field_coverage_catalog_managed_surface_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_fallback_surface_count\": "
      << catalog_flow_signals.consumer_field_coverage_fallback_surface_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_required_field_count\": "
      << catalog_flow_signals.consumer_field_coverage_required_field_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_covered_field_count\": "
      << catalog_flow_signals.consumer_field_coverage_covered_field_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_field_gap_count\": "
      << catalog_flow_signals.consumer_field_coverage_field_gap_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_parity_refresh_required_count\": "
      << catalog_flow_signals
             .consumer_field_coverage_parity_refresh_required_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_current_repo_action_count\": "
      << catalog_flow_signals.consumer_field_coverage_current_repo_action_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_target_machine_action_count\": "
      << catalog_flow_signals
             .consumer_field_coverage_target_machine_action_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_repair_card_count\": "
      << catalog_flow_signals.consumer_field_coverage_repair_card_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_current_repo_repair_card_count\": "
      << catalog_flow_signals
             .consumer_field_coverage_current_repo_repair_card_count
      << ",\n";
  out << "    \"catalog_flow_consumer_field_coverage_target_machine_repair_card_count\": "
      << catalog_flow_signals
             .consumer_field_coverage_target_machine_repair_card_count
      << ",\n";
  out << "    \"catalog_flow_metadata_policy_card_count\": "
      << catalog_flow_signals.metadata_policy_card_count << ",\n";
  out << "    \"catalog_flow_metadata_derived_policy_artifact_count\": "
      << catalog_flow_signals.metadata_derived_policy_artifact_count << ",\n";
  out << "    \"catalog_flow_metadata_derived_policy_field_count\": "
      << catalog_flow_signals.metadata_derived_policy_field_count << ",\n";
  out << "    \"catalog_flow_missing_step_count\": "
      << catalog_flow_signals.missing_step_count << ",\n";
  out << "    \"catalog_flow_repair_card_count\": "
      << catalog_flow_signals.repair_card_count << ",\n";
  out << "    \"catalog_flow_discrepancy_count\": "
      << catalog_flow_signals.discrepancy_count << ",\n";
  out << "    \"catalog_flow_filter_decision\": "
      << inferlab_plan::json_string(catalog_flow_signals.filter_decision)
      << ",\n";
}

}  // namespace inferlab_evidence_query::detail
