#include "evidence_query_request.hpp"

#include "checked_index.hpp"
#include "evidence_query_cross_artifact_adapter.hpp"
#include "evidence_query_repair_groups.hpp"
#include "product_replay_semantic_linkage_projection.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

std::string path_state(const std::size_t blocker_count) {
  return blocker_count == 0U ? "ready" : "blocked";
}

std::vector<RepairPathSummary> project_repair_paths(
    const EvidenceQueryReleaseSourceProjection& release_sources,
    const CatalogFlowFilterSignals& catalog_flow,
    const NativeCoreAuditFilterSignals& native_core_audit) {
  const std::size_t report_ready_blockers =
      inferlab::internal::checked_sum_size(
          {release_sources.source_gap_count,
           release_sources.repair_path_source_location_gap_count,
           release_sources.current_repo_repair_card_count,
           release_sources.flow_check_repair_card_count,
           catalog_flow.discrepancy_count,
           catalog_flow.consumer_field_coverage_repair_card_count,
           native_core_audit_blocker_count(native_core_audit),
           release_sources.repair_counts.split_schema_gap ? 1U : 0U},
          "Evidence Query report-ready blocker count");
  const std::size_t replay_ready_blockers =
      inferlab::internal::checked_add_size(
          report_ready_blockers,
          release_sources.target_product_replay_repair_card_count,
          "Evidence Query replay-ready blocker count");
  const std::size_t execute_ready_blockers =
      inferlab::internal::checked_add_size(
          report_ready_blockers,
          release_sources.target_gpu_execute_repair_card_count,
          "Evidence Query execute-ready blocker count");

  return {
      {"report_ready",
       path_state(report_ready_blockers),
       report_ready_blockers,
       catalog_flow.discrepancy_count,
       catalog_flow.surface_consumer_fallback_removal_blocked_count,
       catalog_flow.surface_consumer_parity_refresh_required_count,
       catalog_flow.surface_consumer_current_repo_action_count,
       catalog_flow.surface_consumer_target_machine_action_count,
       catalog_flow.consumer_field_coverage_repair_card_count,
       catalog_flow.consumer_field_coverage_field_gap_count,
       catalog_flow.consumer_field_coverage_parity_refresh_required_count,
       {"repair current-repo schema/claim/report/freshness cards",
        "repair catalog-flow discrepancies reported by the authoritative catalog",
        "repair catalog consumer field-coverage gaps before treating catalog consumption as complete",
        "repair root-cause cards and source-location gaps before querying release paths",
        "refresh native_core_audit_register and close deferred native review items",
        "refresh the catalog single-source artifact when validation is enabled",
        "refresh Review Pack only after Evidence Query",
        "refresh evidence warehouse, summary, Prometheus, and final report"}},
      {"replay_ready",
       path_state(replay_ready_blockers),
       replay_ready_blockers,
       catalog_flow.discrepancy_count,
       catalog_flow.surface_consumer_fallback_removal_blocked_count,
       catalog_flow.surface_consumer_parity_refresh_required_count,
       catalog_flow.surface_consumer_current_repo_action_count,
       catalog_flow.surface_consumer_target_machine_action_count,
       catalog_flow.consumer_field_coverage_repair_card_count,
       catalog_flow.consumer_field_coverage_field_gap_count,
       catalog_flow.consumer_field_coverage_parity_refresh_required_count,
       {"complete report_ready path",
        "import product replay logs or normalized replay JSON",
        "refresh semantic correlation, Evidence Query, and release gate"}},
      {"execute_ready",
       path_state(execute_ready_blockers),
       execute_ready_blockers,
       catalog_flow.discrepancy_count,
       catalog_flow.surface_consumer_fallback_removal_blocked_count,
       catalog_flow.surface_consumer_parity_refresh_required_count,
       catalog_flow.surface_consumer_current_repo_action_count,
       catalog_flow.surface_consumer_target_machine_action_count,
       catalog_flow.consumer_field_coverage_repair_card_count,
       catalog_flow.consumer_field_coverage_field_gap_count,
       catalog_flow.consumer_field_coverage_parity_refresh_required_count,
       {"complete report_ready path",
        "run target-machine native/CUDA/product execute workflows",
        "refresh summary, Prometheus, final report, and release gate"}},
  };
}

ShowArtifactSelection project_show_artifact_selection(
    const QueryArtifactProjection& artifacts,
    const QueryOperation& operation) {
  ShowArtifactSelection selection = artifacts.show_artifact.selection;
  if (operation.operation != "show_artifact") {
    selection = {};
    selection.selection_strategy = "not_show_artifact_operation";
  }
  return selection;
}

std::size_t project_filter_source_read_issue_count(
    const CatalogFlowFilterSignals& catalog_flow,
    const EvidenceQueryReleaseSourceProjection& release_sources,
    const NativeCoreAuditFilterSignals& native_core_audit) {
  return inferlab::internal::checked_sum_size(
      {catalog_flow.read_issue_count,
       release_sources.source_read_issue_count,
       native_core_audit.read_issue_count},
      "evidence-query filter source read-issue count");
}

bool project_query_ready(
    const EvidenceQueryReleaseSourceProjection& release_sources,
    const NativeCoreAuditFilterSignals& native_core_audit) {
  return release_sources.source_gap_count == 0U &&
         release_sources.source_read_issue_count == 0U &&
         native_core_audit.filter_decision == "native_core_audit_ready";
}

}  // namespace

EvidenceQueryRequest::EvidenceQueryRequest(
    std::filesystem::path warehouse_path_value,
    std::filesystem::path repo_root_value,
    std::filesystem::path results_path_value,
    std::filesystem::path fixture_dir_value,
    std::string query_value,
    std::string artifact_filter_value,
    std::string blocker_filter_value,
    std::filesystem::path before_snapshot_value,
    std::filesystem::path after_snapshot_value,
    std::filesystem::path selector_repair_card_output_value,
    std::string selector_repair_card_export_write_status_value)
    : warehouse_path(std::move(warehouse_path_value)),
      repo_root(std::move(repo_root_value)),
      results_path(std::move(results_path_value)),
      fixture_dir(std::move(fixture_dir_value)),
      query(std::move(query_value)),
      artifact_filter(std::move(artifact_filter_value)),
      blocker_filter(std::move(blocker_filter_value)),
      before_snapshot(std::move(before_snapshot_value)),
      after_snapshot(std::move(after_snapshot_value)),
      selector_repair_card_output(
          std::move(selector_repair_card_output_value)),
      selector_repair_card_export_write_status(
          std::move(selector_repair_card_export_write_status_value)),
      source(project_evidence_query_sources(warehouse_path, repo_root,
                                            results_path, fixture_dir,
                                            snapshots)),
      query_operation(
          parse_query_operation(query, artifact_filter, blocker_filter)),
      query_artifacts(query_artifact_projection(
          source.records, query_operation, query, artifact_filter,
          blocker_filter)),
      blocker_list(blocker_list_projection(source.records, blocker_filter)),
      release_sources(project_evidence_query_release_sources(source.records)),
      catalog_flow(
          project_catalog_flow_filter_signals(results_path, snapshots)),
      native_core_audit(
          project_native_core_audit_filter_signals(results_path, snapshots)),
      snapshot_diff(
          read_snapshot_diff_signals(before_snapshot, after_snapshot)),
      shortest_paths(
          project_repair_paths(release_sources, catalog_flow,
                               native_core_audit)),
      repair_collection(repair_collection_projection(
          source.records, artifact_filter, blocker_filter)),
      repair_groups(repair_card_groups(source.records, query_operation,
                                       artifact_filter, blocker_filter)),
      repair_cards(repair_card_render_projection(
          source.records, query_operation, artifact_filter, blocker_filter)),
      source_exactness(repair_card_source_exactness_projection(
          source.records, query_operation, artifact_filter, blocker_filter,
          kMaxRepairCardSourceExactnessCards)),
      blocker_drilldown(build_blocker_drilldown(
          source.records, query_operation, artifact_filter, blocker_filter)),
      source_location_coverage(build_source_location_category_coverage(
          blocker_drilldown.category_counts)),
      selector_resolution(inferlab_evidence_query::selector_resolution(
          source.records, query_operation, query, artifact_filter,
          blocker_filter, snapshot_diff)),
      operation_guidance(operation_guidance_projection(
          repair_groups, selector_resolution, query_operation,
          shortest_paths)),
      selected_operation(selected_operation_projection(
          query_artifacts, query_operation,
          operation_guidance.repair_card_count, operation_guidance.group_count,
          snapshot_diff)),
      product_replay_linkage(
          inferlab_evidence::project_product_replay_semantic_linkage(
              results_path, snapshots)),
      native_trace_correlation(
          project_evidence_query_native_trace_correlation(results_path,
                                                          snapshots)),
      cross_artifact_families(cross_artifact_blocker_families(
          source.records, release_sources.root_cause_repair_card_count,
          release_sources.current_repo_repair_card_count,
          release_sources.target_product_replay_repair_card_count,
          release_sources.target_gpu_execute_repair_card_count,
          release_sources.repair_path_source_location_gap_count,
          catalog_flow, native_core_audit, product_replay_linkage)),
      show_artifact_selection(
          project_show_artifact_selection(query_artifacts, query_operation)),
      filter_source_read_issue_count(project_filter_source_read_issue_count(
          catalog_flow, release_sources, native_core_audit)),
      query_ready(project_query_ready(release_sources, native_core_audit)) {}

}  // namespace inferlab_evidence_query
