#include "catalog_required_metrics_internal.hpp"

namespace inferlab_catalog::required_metrics_internal {

void append_release_evidence_pipeline_metrics(
    std::vector<std::string>& metrics) {
  append_metric_names(metrics, {
      "stage_count",
      "current_decision_stage_count",
      "post_generation_stage_count",
      "required_stage_count",
      "ready_required_stage_count",
      "blocking_stage_count",
      "post_generation_observation_issue_count",
      "replay_evidence_status",
      "replay_evidence_ready",
      "replay_evidence_blocker_count",
      "replay_evidence_read_issue_count",
      "replay_evidence_available_event_count",
      "replay_evidence_replay_supported_event_count",
      "replay_evidence_alignment_rule_count",
      "replay_evidence_fully_aligned_rule_count",
      "replay_evidence_alignment_decision",
      "release_alignment_dimension_count",
      "release_alignment_ready_dimension_count",
      "release_alignment_blocking_dimension_count",
      "release_alignment_advisory_dimension_count",
      "release_alignment_issue_count",
      "repository_dag_acyclic",
      "repository_dag_scope_complete",
      "repository_dag_issue_count",
      "non_gpu_repair_path_count",
      "non_gpu_current_repo_repair_count",
      "non_gpu_target_machine_repair_count",
      "native_core_audit_repair_path_count",
      "native_core_audit_repair_path_source_location_count",
      "native_core_audit_repair_path_source_location_gap_count",
      "native_core_audit_backlog_repair_path_count",
      "native_core_audit_scope_repair_path_count",
      "native_core_audit_scope_repair_path_source_location_count",
      "native_core_audit_scope_repair_path_source_location_gap_count",
      "native_core_audit_filter_query_present",
      "native_core_audit_register_present",
      "native_core_audit_register_readable",
      "native_core_audit_register_read_status",
      "native_core_audit_blocker_count",
      "native_core_audit_source_contract_issue_count",
      "native_core_audit_filter_read_issue_count",
      "native_core_audit_filter_decision",
  });
  append_native_core_audit_catalog_metrics(metrics, "native_core_audit_");
  append_release_evidence_pipeline_query_metrics(metrics);
}

}  // namespace inferlab_catalog::required_metrics_internal
