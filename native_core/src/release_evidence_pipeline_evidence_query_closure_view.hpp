#pragma once

#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_evidence_query_closure_projection.hpp"
#include "release_evidence_pipeline_repair_model.hpp"
#include "release_evidence_pipeline_snapshot_diff_link_projection.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

namespace evidence_query_closure_view_detail {

inline void write_json_field(std::ostringstream& out, const std::string& pad,
                             const std::string& key, const std::string& json,
                             const bool comma = true) {
  out << pad << inferlab_plan::json_string(key) << ": " << json
      << (comma ? ",\n" : "\n");
}

inline void write_count_field(std::ostringstream& out, const std::string& pad,
                              const std::string& key, const std::size_t value,
                              const bool comma = true) {
  write_json_field(out, pad, key, std::to_string(value), comma);
}

inline void write_int_field(std::ostringstream& out, const std::string& pad,
                            const std::string& key, const int value,
                            const bool comma = true) {
  write_json_field(out, pad, key, std::to_string(value), comma);
}

inline void write_bool_field(std::ostringstream& out, const std::string& pad,
                             const std::string& key, const bool value,
                             const bool comma = true) {
  write_json_field(out, pad, key, inferlab_plan::json_bool(value), comma);
}

}  // namespace evidence_query_closure_view_detail

std::string evidence_query_operation_catalog_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_selected_operation_result_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_catalog_flow_filter_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_cross_artifact_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_product_replay_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const std::vector<NonGpuReviewRepairPath>& repair_paths, int indent);

std::string evidence_query_explain_tree_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_blocker_drilldown_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_readiness_path_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_source_location_coverage_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_repair_group_plan_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_selector_resolution_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_snapshot_diff_review_closure_json(
    const ReleasePipelineSnapshotDiffReleaseLinkProjection& projection,
    int indent);

std::string evidence_query_operation_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_catalog_flow_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_cross_artifact_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_product_replay_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_query_ux_summary_head_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_query_ux_repair_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

std::string evidence_query_query_ux_navigation_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    int indent);

}  // namespace inferlab_release_pipeline
