#include "release_evidence_pipeline_non_gpu_review_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_evidence_query_closure_view.hpp"
#include "release_evidence_pipeline_native_audit_closure_view.hpp"
#include "release_evidence_pipeline_repair_group_view.hpp"
#include "release_evidence_pipeline_repair_link_view.hpp"
#include "release_evidence_pipeline_repair_path_view.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string non_gpu_review_closure_json(
    const ReleasePipelineArtifactProjection& projection, const int indent) {
  const auto& request = projection.request;
  const auto& repair_paths = request.repair.paths;
  const auto& repair_groups = request.repair.groups;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline non-GPU review section indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"evidence_query_catalog_flow_filter\": "
      << evidence_query_catalog_flow_filter_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_explain_tree_review\": "
      << evidence_query_explain_tree_review_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_blocker_drilldown_review\": "
      << evidence_query_blocker_drilldown_review_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_readiness_path_drilldown_review\": "
      << evidence_query_readiness_path_review_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_cross_artifact_blocker_explanation\": "
      << evidence_query_cross_artifact_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_product_replay_semantic_linkage\": "
      << evidence_query_product_replay_closure_json(
             request.evidence_query_closure, repair_paths, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_source_location_category_coverage\": "
      << evidence_query_source_location_coverage_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_repair_group_plan\": "
      << evidence_query_repair_group_plan_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_operation_catalog\": "
      << evidence_query_operation_catalog_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_selected_operation_result\": "
      << evidence_query_selected_operation_result_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_selector_resolution\": "
      << evidence_query_selector_resolution_closure_json(
             request.evidence_query_closure, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_snapshot_diff_review\": "
      << evidence_query_snapshot_diff_review_closure_json(
             request.snapshot_diff_release_link, nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_snapshot_to_release_link\": "
      << snapshot_diff_release_link_json(
             request.snapshot_diff_release_link, nested_indent)
      << ",\n";
  out << nested << "\"blocker_repair_card_link\": "
      << blocker_repair_card_link_json(request.blocker_repair_link,
                                       nested_indent)
      << ",\n";
  out << nested << "\"evidence_query_native_core_audit_filter\": "
      << native_audit_closure_json(request.native_audit_closure,
                                   nested_indent)
      << ",\n";
  out << nested << "\"repair_path_count\": " << repair_paths.size()
      << ",\n";
  out << nested << "\"repair_paths\": "
      << non_gpu_review_repair_paths_json(repair_paths, nested_indent)
      << ",\n";
  out << nested << "\"repair_path_group_count\": " << repair_groups.size()
      << ",\n";
  out << nested << "\"repair_path_current_repo_action_group_count\": "
      << request.repair.current_repo_group_count << ",\n";
  out << nested
      << "\"repair_path_target_or_validation_action_group_count\": "
      << request.repair.target_or_validation_group_count << ",\n";
  out << nested << "\"repair_path_groups\": "
      << repair_path_groups_json(repair_paths, repair_groups, nested_indent)
      << ",\n";
  out << nested << "\"repair_path_group_plan_step_count\": "
      << repair_groups.size() << ",\n";
  out << nested << "\"repair_path_group_plan_current_repo_step_count\": "
      << request.repair.current_repo_group_count << ",\n";
  out << nested
      << "\"repair_path_group_plan_target_or_validation_step_count\": "
      << request.repair.target_or_validation_group_count << ",\n";
  out << nested << "\"repair_path_group_plan\": "
      << repair_path_group_plan_json(repair_paths, repair_groups,
                                     nested_indent)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"non-GPU review closure only summarizes query and audit evidence; it does not create execute, replay, CUDA timing, or benchmark evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
