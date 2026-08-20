#include "evidence_query_result_artifact_sections.hpp"

#include "evidence_domain_utils.hpp"
#include "evidence_query_artifact_support_view.hpp"
#include "evidence_query_cross_artifact_view.hpp"
#include "deep_dive_trace_token_join_view.hpp"
#include "evidence_query_operation_contract_view.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

void append_result_artifact_header_and_summary_core(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& request = projection.request;
  const auto& warehouse_path = request.warehouse_path;
  const auto& repo_root = request.repo_root;
  const auto& results_path = request.results_path;
  const auto& fixture_dir = request.fixture_dir;
  const auto& query = request.query;
  const auto& artifact_filter = request.artifact_filter;
  const auto& blocker_filter = request.blocker_filter;
  const auto& selector_repair_card_output =
      request.selector_repair_card_output;
  const auto& selector_repair_card_export_write_status =
      request.selector_repair_card_export_write_status;
  const bool fixture_used = request.source.fixture_used;
  const auto& scan_sources = request.source.scan_sources;
  const std::size_t scan_source_read_issue_count =
      request.source.scan_source_read_issue_count;
  const int record_count = request.source.record_count;
  const int fixture_record_count = request.source.fixture_record_count;
  const auto& query_operation = request.query_operation;
  const auto& query_artifacts = request.query_artifacts;
  const std::size_t matched_count = query_artifacts.matched_records.size();
  const std::size_t blocked_count = query_artifacts.blocked_result_count;
  const std::size_t execute_verified_count =
      query_artifacts.execute_verified_count;
  const std::size_t replay_supported_count =
      query_artifacts.replay_supported_count;
  const std::size_t dry_run_ready_count =
      query_artifacts.dry_run_ready_count;
  const std::size_t schema_only_count = query_artifacts.schema_only_count;
  const auto& release_sources = request.release_sources;
  const std::size_t root_cause_repair_card_count =
      release_sources.root_cause_repair_card_count;
  const auto& root_cause_repair_count_projection =
      release_sources.repair_counts;
  const std::size_t current_repo_repair_card_count =
      release_sources.current_repo_repair_card_count;
  const std::size_t target_product_replay_repair_card_count =
      release_sources.target_product_replay_repair_card_count;
  const std::size_t target_gpu_execute_repair_card_count =
      release_sources.target_gpu_execute_repair_card_count;
  const std::size_t target_machine_repair_card_count =
      release_sources.target_machine_repair_card_count;
  const std::size_t flow_check_repair_card_count =
      release_sources.flow_check_repair_card_count;
  const std::size_t root_cause_source_location_card_count =
      release_sources.effective_source_location_card_count;
  const std::size_t repair_path_source_location_count =
      release_sources.repair_path_source_location_count;
  const std::size_t repair_path_source_location_gap_count =
      release_sources.repair_path_source_location_gap_count;
  const std::size_t repair_path_source_gap_count =
      release_sources.source_gap_count;
  const std::size_t repair_path_source_read_issue_count =
      release_sources.source_read_issue_count;
  const auto& snapshot_diff_signals = request.snapshot_diff;
  const auto& shortest_paths = request.shortest_paths;
  const auto& query_selector_resolution = request.selector_resolution;
  const auto& query_operation_guidance = request.operation_guidance;
  const auto& query_selected_operation = request.selected_operation;
  const auto& product_replay_linkage_signals =
      request.product_replay_linkage;
  const auto& native_trace_correlation = request.native_trace_correlation;
  const auto& cross_artifact_families = request.cross_artifact_families;
  const auto& query_show_artifact_selection =
      request.show_artifact_selection;
  const auto& operation_catalog_summary =
      projection.operation_catalog_summary;
  const bool query_ready = request.query_ready;
  const bool payload_selector_focus_present =
      projection.payload_selector_focus_present;
  out << "{\n";
  out << "  \"schema_version\": 3,\n";
  out << "  \"artifact_type\": \"native_evidence_query_result\",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, "native_evidence_query_result",
      "evidence warehouse query only; does not create new performance claims",
      {"nlohmann/json", "SQLite3"}, "inferlab_evidence_query",
      "results/raw/native_evidence_query_result.json",
      "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query confidence_summary --output results/raw/native_evidence_query_result.json --format json");
  out << "  \"status\": " << inferlab_plan::json_string(query_ready ? "query_ready" : "query_with_blockers") << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"evidence warehouse query only; does not create new performance claims\",\n";
  out << "  \"query\": {\n";
  out << "    \"name\": " << inferlab_plan::json_string(query) << ",\n";
  out << "    \"normalized_query\": "
      << inferlab_plan::json_string(query_operation.normalized_query)
      << ",\n";
  out << "    \"operation\": "
      << inferlab_plan::json_string(query_operation.operation) << ",\n";
  out << "    \"operation_label\": "
      << inferlab_plan::json_string(query_operation.human_label) << ",\n";
  out << "    \"operation_selector\": "
      << inferlab_plan::json_string(query_operation.selector) << ",\n";
  out << "    \"operation_target\": "
      << inferlab_plan::json_string(query_operation.target) << ",\n";
  out << "    \"artifact_filter\": " << inferlab_plan::json_string(artifact_filter) << ",\n";
  out << "    \"blocker_filter\": " << inferlab_plan::json_string(blocker_filter) << ",\n";
  out << inferlab_plan::redacted_path_json_fields(
      "warehouse", warehouse_path.string(), 4);
  out << inferlab_plan::redacted_path_json_fields(
      "repo_root", repo_root.string(), 4);
  out << inferlab_plan::redacted_path_json_fields(
      "results", results_path.string(), 4);
  out << inferlab_plan::redacted_path_json_fields(
      "fixture_dir", fixture_dir.string(), 4);
  out << "    \"warehouse_used\": "
      << (request.source.warehouse_used ? "true" : "false") << ",\n";
  out << "    \"fixture_used\": " << (fixture_used ? "true" : "false") << ",\n";
  out << "    \"direct_scan_used\": "
      << (request.source.direct_scan_used ? "true" : "false") << "\n";
  out << "  },\n";
  out << "  \"operation_descriptor\": "
      << operation_descriptor_json(query_operation, snapshot_diff_signals, 2)
      << ",\n";
  out << "  \"operation_catalog\": " << operation_catalog_json(2)
      << ",\n";
  out << "  \"selector_resolution\": "
      << selector_resolution_json(query_selector_resolution, 2) << ",\n";
  out << "  \"selector_action_plan\": "
      << selector_action_plan_json(query_operation_guidance,
                                   query_selector_resolution,
                                   query_operation, 2)
      << ",\n";
  out << "  \"selector_repair_card_export\": "
      << selector_repair_card_export_json(
             query_selector_resolution, 2, selector_repair_card_output.string(),
             selector_repair_card_export_write_status)
      << ",\n";
  out << "  \"cross_artifact_blocker_explanation\": "
      << cross_artifact_blocker_explanation_json(cross_artifact_families, 2)
      << ",\n";
  out << "  \"product_replay_semantic_linkage\": "
      << product_replay_semantic_linkage_json(product_replay_linkage_signals, 2)
      << ",\n";
  out << "  \"native_trace_correlation\": "
      << inferlab_deep_dive_trace::trace_token_join_json(
             native_trace_correlation, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"query_result_count\": " << matched_count << ",\n";
  out << "    \"record_count\": " << record_count << ",\n";
  out << "    \"blocked_result_count\": " << blocked_count << ",\n";
  out << "    \"queried_evidence_has_blockers\": "
      << (blocked_count > 0U ? "true" : "false") << ",\n";
  out << "    \"queried_evidence_blocks_query_execution\": false,\n";
  out << "    \"query_source_contract_ready\": "
      << (query_ready ? "true" : "false") << ",\n";
  out << "    \"query_readiness_scope\": \"direct_source_contract_and_native_audit_only\",\n";
  out << "    \"execute_verified_count\": " << execute_verified_count << ",\n";
  out << "    \"replay_supported_count\": " << replay_supported_count << ",\n";
  out << "    \"dry_run_ready_count\": " << dry_run_ready_count << ",\n";
  out << "    \"schema_only_count\": " << schema_only_count << ",\n";
  out << "    \"fixture_mode\": " << (fixture_used ? "true" : "false") << ",\n";
  out << "    \"fixture_record_count\": " << fixture_record_count << ",\n";
  out << "    \"direct_scan_source_file_count\": "
      << scan_sources.size() << ",\n";
  out << "    \"direct_scan_source_read_issue_count\": "
      << scan_source_read_issue_count << ",\n";
  out << "    \"repair_path_query_present\": true,\n";
  out << "    \"root_cause_repair_card_count\": "
      << root_cause_repair_card_count << ",\n";
  out << "    \"current_repo_repair_card_count\": "
      << current_repo_repair_card_count << ",\n";
  out << "    \"target_product_replay_repair_card_count\": "
      << target_product_replay_repair_card_count << ",\n";
  out << "    \"target_gpu_execute_repair_card_count\": "
      << target_gpu_execute_repair_card_count << ",\n";
  out << "    \"target_machine_repair_card_count\": "
      << target_machine_repair_card_count << ",\n";
  out << "    \"root_cause_current_repo_repair_card_count\": "
      << current_repo_repair_card_count << ",\n";
  out << "    \"root_cause_target_product_replay_repair_card_count\": "
      << target_product_replay_repair_card_count << ",\n";
  out << "    \"root_cause_target_gpu_execute_repair_card_count\": "
      << target_gpu_execute_repair_card_count << ",\n";
  out << "    \"root_cause_target_machine_repair_card_count\": "
      << target_machine_repair_card_count << ",\n";
  out << "    \"root_cause_repair_environment_split_status\": "
      << inferlab_plan::json_string(
             root_cause_repair_count_projection.split_status)
      << ",\n";
  out << "    \"root_cause_repair_environment_split_schema_gap\": "
      << (root_cause_repair_count_projection.split_schema_gap ? "true"
                                                              : "false")
      << ",\n";
  out << "    \"root_cause_repair_environment_legacy_fallback_used\": "
      << (root_cause_repair_count_projection.legacy_fallback_used ? "true"
                                                                  : "false")
      << ",\n";
  out << "    \"root_cause_flow_check_repair_card_count\": "
      << flow_check_repair_card_count << ",\n";
  out << "    \"root_cause_source_location_card_count\": "
      << root_cause_source_location_card_count << ",\n";
  out << "    \"repair_path_source_gap_count\": "
      << repair_path_source_gap_count << ",\n";
  out << "    \"repair_path_source_read_issue_count\": "
      << repair_path_source_read_issue_count << ",\n";
  out << "    \"repair_path_source_location_count\": "
      << repair_path_source_location_count << ",\n";
  out << "    \"repair_path_source_location_gap_count\": "
      << repair_path_source_location_gap_count << ",\n";
  out << "    \"repair_path_count\": " << shortest_paths.size() << ",\n";
  out << "    \"repair_path_first_action_present\": "
      << (!shortest_paths.empty() && !shortest_paths.front().steps.empty()
              ? "true"
              : "false")
      << ",\n";
  out << cross_artifact_summary_json_fields(cross_artifact_families, 4);
  out << "    \"product_replay_semantic_linkage_present\": true,\n";
  out << "    \"product_replay_semantic_linkage_status\": "
      << inferlab_plan::json_string(product_replay_linkage_signals.status)
      << ",\n";
  out << "    \"product_replay_semantic_events_present\": "
      << (product_replay_linkage_signals.semantic_events_present ? "true"
                                                                 : "false")
      << ",\n";
  out << "    \"product_replay_alignment_present\": "
      << (product_replay_linkage_signals.alignment_present ? "true" : "false")
      << ",\n";
  out << "    \"product_replay_semantic_linkage_read_issue_count\": "
      << product_replay_linkage_signals.read_issue_count << ",\n";
  out << "    \"product_replay_semantic_linkage_available_event_count\": "
      << product_replay_linkage_signals.available_event_count << ",\n";
  out << "    \"product_replay_semantic_linkage_pending_event_count\": "
      << product_replay_linkage_signals.pending_event_count << ",\n";
  out << "    \"product_replay_semantic_linkage_sample_event_count\": "
      << product_replay_linkage_signals.sample_event_count << ",\n";
  out << "    \"product_replay_semantic_linkage_sample_replay_present\": "
      << (product_replay_linkage_signals.sample_replay_present ? "true"
                                                               : "false")
      << ",\n";
  out << "    \"product_replay_semantic_linkage_replay_supported_event_count\": "
      << product_replay_linkage_signals.replay_supported_event_count << ",\n";
  out << "    \"product_replay_semantic_linkage_alignment_rule_count\": "
      << product_replay_linkage_signals.alignment_rule_count << ",\n";
  out << "    \"product_replay_semantic_linkage_aligned_rule_count\": "
      << product_replay_linkage_signals.aligned_rule_count << ",\n";
  out << "    \"product_replay_semantic_linkage_pending_rule_count\": "
      << product_replay_linkage_signals.pending_rule_count << ",\n";
  out << "    \"product_replay_semantic_linkage_correlation_count\": "
      << product_replay_linkage_signals.correlation_count << ",\n";
  out << "    \"product_replay_semantic_linkage_ready_correlation_count\": "
      << product_replay_linkage_signals.ready_correlation_count << ",\n";
  out << "    \"product_replay_semantic_linkage_blocked_correlation_count\": "
      << product_replay_linkage_signals.blocked_correlation_count << ",\n";
  out << "    \"product_replay_semantic_linkage_blocker_count\": "
      << product_replay_linkage_signals.blocker_count << ",\n";
  out << "    \"product_replay_semantic_linkage_target_or_validation_action_count\": "
      << product_replay_linkage_signals.target_or_validation_action_count
      << ",\n";
  out << "    \"product_replay_semantic_linkage_first_blocker\": "
      << inferlab_plan::json_string(product_replay_linkage_signals.first_blocker)
      << ",\n";
  out << "    \"product_replay_semantic_linkage_next_action\": "
      << inferlab_plan::json_string(product_replay_linkage_signals.next_action)
      << ",\n";
  out << "    \"native_trace_correlation_present\": true,\n";
  out << "    \"native_trace_source_count\": "
      << native_trace_correlation.summary.source_count << ",\n";
  out << "    \"native_trace_ready_source_count\": "
      << native_trace_correlation.summary.ready_source_count << ",\n";
  out << "    \"native_trace_product_source_count\": "
      << native_trace_correlation.summary.product_source_count << ",\n";
  out << "    \"native_trace_native_source_count\": "
      << native_trace_correlation.summary.native_source_count << ",\n";
  out << "    \"native_trace_matched_group_count\": "
      << native_trace_correlation.summary.matched_trace_group_count << ",\n";
  out << "    \"native_trace_blocker_count\": "
      << native_trace_correlation.summary.blocker_count << ",\n";
  out << "    \"native_trace_decision\": "
      << inferlab_plan::json_string(
             inferlab_deep_dive_trace::trace_token_join_decision_name(
                 native_trace_correlation.summary.decision))
      << ",\n";
  out << "    \"native_trace_time_window_decision\": "
      << inferlab_plan::json_string(
             inferlab_deep_dive_trace::trace_time_window_decision_name(
                 native_trace_correlation.summary.time_window_decision))
      << ",\n";
  out << "    \"native_trace_span_lineage_decision\": "
      << inferlab_plan::json_string(
             inferlab_deep_dive_trace::trace_span_lineage_decision_name(
                 native_trace_correlation.summary.span_lineage_decision))
      << ",\n";
  out << "    \"native_trace_span_lifecycle_decision\": "
      << inferlab_plan::json_string(
             inferlab_deep_dive_trace::trace_span_lifecycle_decision_name(
                 native_trace_correlation.summary.span_lifecycle_decision))
      << ",\n";
  out << "    \"query_operation\": "
      << inferlab_plan::json_string(query_operation.operation) << ",\n";
  out << "    \"query_operation_selector_present\": "
      << (!query_operation.selector.empty() ? "true" : "false") << ",\n";
  out << "    \"query_operation_target_present\": "
      << (!query_operation.target.empty() ? "true" : "false") << ",\n";
  out << "    \"query_operation_snapshot_inputs_required\": "
      << (operation_snapshot_inputs_required(query_operation) ? "true"
                                                              : "false")
      << ",\n";
  out << "    \"query_operation_snapshot_inputs_configured\": "
      << (snapshot_diff_signals.before_configured &&
                  snapshot_diff_signals.after_configured
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_operation_selected_result_present\": true,\n";
  out << "    \"query_operation_selected_result_input_ready\": "
      << inferlab_plan::json_bool(query_selected_operation.input_ready)
      << ",\n";
  out << "    \"query_operation_selected_result_payload_count\": "
      << query_selected_operation.payload_count
      << ",\n";
  out << "    \"query_operation_selected_result_kind\": "
      << inferlab_plan::json_string(query_selected_operation.result_kind)
      << ",\n";
  out << "    \"query_operation_show_artifact_candidate_count\": "
      << query_show_artifact_selection.candidate_count << ",\n";
  out << "    \"query_operation_show_artifact_candidate_truncated\": "
      << (query_show_artifact_selection.candidate_list_truncated ? "true"
                                                                 : "false")
      << ",\n";
  out << "    \"query_operation_show_artifact_exact_match\": "
      << (query_show_artifact_selection.exact_match ? "true" : "false")
      << ",\n";
  out << "    \"query_operation_show_artifact_selector_ambiguous\": "
      << (query_show_artifact_selection.selector_ambiguous ? "true" : "false")
      << ",\n";
  out << "    \"query_operation_show_artifact_selection_strategy\": "
      << inferlab_plan::json_string(
             query_show_artifact_selection.selection_strategy)
      << ",\n";
  out << "    \"query_operation_payload_selector_focus_present\": "
      << (payload_selector_focus_present ? "true" : "false") << ",\n";
  out << "    \"query_operation_payload_selector_focus_status\": "
      << inferlab_plan::json_string(
             payload_selector_focus_present
                 ? query_selector_resolution.resolution_status
                 : "not_applicable")
      << ",\n";
  out << "    \"query_operation_payload_selector_focus_match_count\": "
      << (payload_selector_focus_present
              ? query_selector_resolution.total_match_count
              : 0U)
      << ",\n";
  out << "    \"query_operation_payload_selector_focus_ambiguous\": "
      << (payload_selector_focus_present && query_selector_resolution.ambiguous
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_operation_payload_selector_focus_source_location_gap_count\": "
      << (payload_selector_focus_present
              ? query_selector_resolution.source_location_gap_count
              : 0U)
      << ",\n";
  out << "    \"query_operation_selector_resolution_status\": "
      << inferlab_plan::json_string(
             query_selector_resolution.resolution_status)
      << ",\n";
  out << "    \"query_operation_selector_kind\": "
      << inferlab_plan::json_string(query_selector_resolution.selector_kind)
      << ",\n";
  out << "    \"query_operation_selector_match_count\": "
      << query_selector_resolution.total_match_count << ",\n";
  out << "    \"query_operation_selector_ambiguous\": "
      << (query_selector_resolution.ambiguous ? "true" : "false") << ",\n";
  out << "    \"query_operation_selector_repair_card_count\": "
      << query_selector_resolution.repair_card_count << ",\n";
  out << "    \"query_operation_selector_repair_card_emitted_count\": "
      << query_selector_resolution.repair_card_emitted_count << ",\n";
  out << "    \"query_operation_selector_repair_card_truncated\": "
      << (query_selector_resolution.repair_card_truncated ? "true" : "false")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_export_status\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_status(query_selector_resolution))
      << ",\n";
  out << "    \"query_operation_selector_repair_card_embedded_limit\": "
      << kMaxSelectorRepairCards << ",\n";
  out << "    \"query_operation_selector_repair_card_full_export_count\": "
      << query_selector_resolution.all_repair_cards.size() << ",\n";
  out << "    \"query_operation_selector_repair_card_external_export_configured\": "
      << (!selector_repair_card_output.empty() ? "true" : "false") << ",\n";
  out << "    \"query_operation_selector_repair_card_external_export_written\": "
      << (selector_repair_card_external_export_written(
              selector_repair_card_export_write_status)
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_external_export_status\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_write_status)
      << ",\n";
  out << "    \"query_operation_selector_repair_card_current_repo_count\": "
      << selector_repair_card_environment_count(
             query_selector_resolution, "current_repo_fixable")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_current_server_count\": "
      << selector_repair_card_environment_count(
             query_selector_resolution,
             "current_server_when_validation_enabled")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_target_machine_count\": "
      << selector_repair_card_environment_count(
             query_selector_resolution, "target_gpu_or_execute_machine")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_target_product_count\": "
      << selector_repair_card_environment_count(
             query_selector_resolution,
             "target_product_or_replay_evidence")
      << ",\n";
  out << "    \"query_operation_selector_repair_card_source_location_gap_card_count\": "
      << selector_repair_card_source_location_gap_card_count(
             query_selector_resolution)
      << ",\n";
  out << "    \"query_operation_selector_repair_card_export_review_status\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_review_status(
                 query_selector_resolution,
                 selector_repair_card_output.string(),
                 selector_repair_card_export_write_status))
      << ",\n";
  out << "    \"query_operation_selector_repair_card_export_blocker_reason\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_blocker_reason(
                 query_selector_resolution,
                 selector_repair_card_output.string(),
                 selector_repair_card_export_write_status))
      << ",\n";
  out << "    \"query_operation_selector_repair_card_external_file_required\": "
      << (selector_repair_card_external_file_required(
              query_selector_resolution,
              selector_repair_card_export_write_status)
              ? "true"
              : "false")
      << ",\n";
  out << "    \"query_operation_selector_source_location_count\": "
      << query_selector_resolution.exact_wiring_location_count << ",\n";
  out << "    \"query_operation_selector_source_location_gap_count\": "
      << query_selector_resolution.source_location_gap_count << ",\n";
  out << "    \"query_operation_selector_current_repo_action_count\": "
      << query_selector_resolution.current_repo_actions.size() << ",\n";
  out << "    \"query_operation_selector_current_server_action_count\": "
      << query_selector_resolution.current_server_actions.size() << ",\n";
  out << "    \"query_operation_selector_target_machine_action_count\": "
      << query_selector_resolution.target_machine_actions.size() << ",\n";
  out << "    \"query_operation_selector_target_product_action_count\": "
      << query_selector_resolution.target_product_actions.size() << ",\n";
  out << "    \"query_operation_selector_action_plan_present\": true,\n";
  out << "    \"query_operation_selector_action_plan_decision\": "
      << inferlab_plan::json_string(query_operation_guidance.decision)
      << ",\n";
  out << "    \"query_operation_selector_action_plan_step_count\": "
      << query_operation_guidance.action_plan_steps.size() << ",\n";
  out << "    \"query_operation_selector_action_plan_current_repo_step_count\": "
      << query_selector_resolution.current_repo_actions.size() << ",\n";
  out << "    \"query_operation_selector_action_plan_target_or_validation_step_count\": "
      << query_operation_guidance.target_or_validation_step_count << ",\n";
  out << "    \"query_operation_selector_action_plan_source_location_gap_count\": "
      << query_selector_resolution.source_location_gap_count << ",\n";
  out << "    \"query_operation_selector_action_plan_readiness_blocker_count\": "
      << query_operation_guidance.readiness.blocking_repair_card_count
      << ",\n";
  out << "    \"query_operation_selector_action_plan_readiness_state\": "
      << inferlab_plan::json_string(query_operation_guidance.readiness.state)
      << ",\n";
  out << "    \"operation_catalog_supported_count\": "
      << operation_catalog_summary.supported_operation_count << ",\n";
  out << "    \"operation_catalog_read_only_count\": "
      << operation_catalog_summary.read_only_operation_count << ",\n";
  out << "    \"operation_catalog_snapshot_required_count\": "
      << operation_catalog_summary.snapshot_required_operation_count
      << ",\n";
  out << "    \"operation_catalog_selector_recommended_count\": "
      << operation_catalog_summary.selector_recommended_operation_count
      << ",\n";
  out << "    \"operation_catalog_target_required_count\": "
      << operation_catalog_summary.target_required_operation_count << ",\n";

}

}  // namespace inferlab_evidence_query::detail
