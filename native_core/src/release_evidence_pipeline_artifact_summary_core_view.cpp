#include "release_evidence_pipeline_artifact_sections.hpp"

#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_cli_view.hpp"
#include "release_evidence_pipeline_decision_view.hpp"

#include <ostream>

namespace inferlab_release_pipeline::detail {

void append_release_pipeline_header_and_summary_core(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection) {
  const auto& request = projection.request;
  const auto& repair = request.repair;
  const auto& repair_summary = projection.repair_summary;

  out << "{\n";
  out << "  \"schema_version\": 3,\n";
  out << "  \"artifact_type\": \"native_release_evidence_pipeline\",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, "native_release_evidence_pipeline", release_pipeline_claim_scope(),
      {"vLLM", "SGLang", "TensorRT-LLM", "FlashInfer", "LMCache",
       "Mooncake", "Dynamo"},
      projection.artifact.target,
      "results/raw/" + projection.artifact.artifact,
      projection.artifact.command);
  out << "  \"status\": "
      << inferlab_plan::json_string(request.decision.decision_name)
      << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(release_pipeline_claim_scope())
      << ",\n";
  out << "  \"cli_context\": " << pipeline_cli_context_json(request, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << release_pipeline_decision_summary_fields_json(request.decision, 4);
  append_count_field(out, "non_gpu_review_signal_count",
                     request.sources.present_source_count);
  append_count_field(out, "non_gpu_review_read_issue_count",
                     request.sources.read_issue_count);
  append_string_field(out, "evidence_query_read_status",
                      request.sources.evidence_query->read_result.status);
  append_string_field(out, "native_core_audit_register_read_status",
                      request.sources.native_audit->read_result.status);

  append_count_field(out, "non_gpu_repair_path_count",
                     repair_summary.total.path_count);
  append_count_field(out, "non_gpu_current_repo_repair_count",
                     repair_summary.current_repo_path_count);
  append_count_field(out, "non_gpu_target_machine_repair_count",
                     repair_summary.target_or_validation_path_count);
  append_count_field(out, "non_gpu_repair_path_source_location_count",
                     repair_summary.total.source_location_count);
  append_count_field(out, "non_gpu_repair_path_source_location_gap_count",
                     repair_summary.total.source_location_gap_count);
  append_count_field(out, "non_gpu_repair_path_evidence_counter_count",
                     repair_summary.total.evidence_counter_count);
  append_count_field(out, "non_gpu_repair_path_group_count",
                     repair.groups.size());
  append_count_field(out, "non_gpu_repair_path_environment_group_count",
                     repair.environment_group_count);
  append_count_field(out, "non_gpu_repair_path_category_group_count",
                     repair.category_group_count);
  append_count_field(out,
                     "non_gpu_repair_path_current_repo_action_group_count",
                     repair.current_repo_group_count);
  append_count_field(
      out, "non_gpu_repair_path_target_or_validation_action_group_count",
      repair.target_or_validation_group_count);
  append_count_field(out, "non_gpu_repair_path_group_plan_step_count",
                     repair.groups.size());
  append_count_field(
      out, "non_gpu_repair_path_group_plan_current_repo_step_count",
      repair.current_repo_group_count);
  append_count_field(
      out,
      "non_gpu_repair_path_group_plan_target_or_validation_step_count",
      repair.target_or_validation_group_count);

  append_repair_metric_fields(out, "catalog_flow",
                              repair_summary.catalog_flow);
  append_repair_metric_fields(out, "catalog_flow_field_coverage",
                              repair_summary.catalog_field_coverage);
  append_repair_metric_fields(
      out, "catalog_flow_fallback_removal_shortest_path",
      repair_summary.catalog_fallback_shortest_path);
  append_repair_metric_fields(out, "evidence_query_snapshot_diff",
                              repair_summary.evidence_query_snapshot_diff);

  const auto& snapshot = request.evidence_query_closure.snapshot_diff;
  append_count_field(out, "evidence_query_snapshot_diff_added_deep_path_count",
                     snapshot.added_deep_path_count);
  append_count_field(
      out, "evidence_query_snapshot_diff_removed_deep_path_count",
      snapshot.removed_deep_path_count);
  append_count_field(
      out, "evidence_query_snapshot_diff_changed_deep_value_count",
      snapshot.changed_deep_value_count);
  append_bool_field(out, "evidence_query_snapshot_diff_deep_semantic_truncated",
                    snapshot.deep_semantic_truncated);
  out << "    \"evidence_query_snapshot_diff_deep_semantic_decision\": "
      << (snapshot.deep_semantic_decision.empty()
              ? "null"
              : inferlab_plan::json_string(snapshot.deep_semantic_decision))
      << ",\n";
  append_count_field(out, "evidence_query_snapshot_to_release_link_count",
                     snapshot.query_present ? 1U : 0U);
  append_count_field(
      out, "evidence_query_snapshot_to_release_blocker_count",
      repair_summary.evidence_query_snapshot_diff.path_count > 0U ? 1U : 0U);
  append_count_field(
      out, "evidence_query_snapshot_to_release_group_plan_step",
      repair_summary.evidence_query_snapshot_diff.group_plan_step);
  append_count_field(
      out, "evidence_query_snapshot_to_release_current_repo_action_count",
      repair_summary.evidence_query_snapshot_diff.current_repo_action_required
          ? 1U
          : 0U);
}

}  // namespace inferlab_release_pipeline::detail
