#include "catalog_required_metrics_internal.hpp"

namespace inferlab_catalog::required_metrics_internal {

void append_artifact_catalog_single_source_metrics(
    std::vector<std::string>& metrics) {
  append_metric_names(
      metrics,
      {"registered_artifact_count",
       "registry_consumer_surface_count",
       "registry_consumed_surface_count",
       "hardcoded_duplicate_location_count",
       "duplicate_registration_count",
       "missing_wiring_policy_count",
       "downstream_wiring_matrix_entry_count",
       "downstream_wiring_surface_count",
       "catalog_drift_missing_surface_count",
       "catalog_fallback_retained_surface_count",
       "catalog_surface_migration_ready_count",
       "catalog_surface_migration_fallback_count",
       "catalog_surface_parity_refresh_required_count",
       "catalog_surface_current_repo_action_count",
       "catalog_surface_target_machine_action_count",
       "surface_consumer_migration_plan_count",
       "surface_consumer_fallback_removal_blocked_count",
       "surface_consumer_parity_refresh_required_count",
       "surface_consumer_current_repo_action_count",
       "surface_consumer_target_machine_action_count",
       "fallback_removal_queue_item_count",
       "fallback_removal_queue_blocked_count",
       "fallback_removal_queue_parity_refresh_required_count",
       "fallback_removal_queue_current_repo_action_count",
       "fallback_removal_queue_target_machine_action_count",
       "fallback_removal_queue_state",
       "fallback_removal_shortest_path_present",
       "fallback_removal_shortest_path_state",
       "fallback_removal_shortest_path_step_count",
       "fallback_removal_shortest_path_current_repo_step_count",
       "fallback_removal_shortest_path_target_machine_step_count",
       "fallback_removal_shortest_path_source_location_gap_count",
       "fallback_removal_shortest_path_primary_environment",
       "fallback_removal_shortest_path_first_surface",
       "fallback_removal_parity_gate_present",
       "fallback_removal_parity_gate_decision",
       "fallback_removal_parity_gate_removal_allowed",
       "fallback_removal_parity_gate_blocker_count",
       "fallback_removal_parity_gate_blocking_surface_count",
       "fallback_removal_parity_gate_fallback_surface_count",
       "fallback_removal_parity_gate_parity_refresh_required_count",
       "fallback_removal_parity_gate_current_repo_action_count",
       "fallback_removal_parity_gate_target_machine_action_count",
       "fallback_removal_parity_gate_source_location_gap_count",
       "fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
       "fallback_removal_parity_gate_first_surface",
       "consumer_field_coverage_surface_count",
       "consumer_field_coverage_catalog_managed_surface_count",
       "consumer_field_coverage_fallback_surface_count",
       "consumer_field_coverage_required_field_count",
       "consumer_field_coverage_covered_field_count",
       "consumer_field_coverage_field_gap_count",
       "consumer_field_coverage_parity_refresh_required_count",
       "consumer_field_coverage_current_repo_action_count",
       "consumer_field_coverage_target_machine_action_count",
       "single_source_repair_card_count",
       "single_source_fallback_repair_card_count",
       "single_source_parity_refresh_required_count",
       "registry_explicit_policy_complete_artifact_count",
       "registry_derived_policy_artifact_count",
       "registry_derived_policy_field_count",
       "registry_metadata_policy_card_count"});
}

void append_query_family_metrics(std::vector<std::string>& metrics) {
  append_metric_names(metrics, {"query_decision"});
}

void append_artifact_schema_registry_metrics(
    std::vector<std::string>& metrics) {
  append_metric_names(metrics,
                      {"registered_artifact_count",
                       "registered_schema_file_count",
                       "selector_repair_card_export_schema_registered",
                       "schema_registry_decision"});
}

void append_native_core_audit_register_metrics(
    std::vector<std::string>& metrics) {
  append_native_core_audit_catalog_metrics(metrics, "");
}

}  // namespace inferlab_catalog::required_metrics_internal
