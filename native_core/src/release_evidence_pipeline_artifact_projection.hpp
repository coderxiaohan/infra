#pragma once

#include "high_value_artifact_catalog.hpp"
#include "release_evidence_pipeline_request.hpp"
#include "release_evidence_pipeline_snapshot_diff_link_projection.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct RepairPathSummaryMetrics {
  std::size_t path_count = 0;
  std::size_t source_location_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t evidence_counter_count = 0;
  std::size_t group_plan_step = 0;
  bool current_repo_action_required = false;
  bool target_or_validation_action_required = false;
};

struct ReleasePipelineRepairSummaryProjection {
  explicit ReleasePipelineRepairSummaryProjection(
      const ReleasePipelineRepairProjection& repair,
      const ReleasePipelineSnapshotDiffReleaseLinkProjection& snapshot_link);

  RepairPathSummaryMetrics total;
  std::size_t current_repo_path_count = 0;
  std::size_t target_or_validation_path_count = 0;
  RepairPathSummaryMetrics catalog_flow;
  RepairPathSummaryMetrics catalog_field_coverage;
  RepairPathSummaryMetrics catalog_fallback_shortest_path;
  RepairPathSummaryMetrics catalog_fallback_parity_gate;
  RepairPathSummaryMetrics evidence_query_snapshot_diff;
  RepairPathSummaryMetrics native_core_audit;
  RepairPathSummaryMetrics native_core_audit_scope;
  RepairPathSummaryMetrics native_core_audit_backlog;
};

struct ReleasePipelineArtifactProjection {
  explicit ReleasePipelineArtifactProjection(
      const ReleaseEvidencePipelineRequest& request_value);

  ReleasePipelineArtifactProjection(
      const ReleasePipelineArtifactProjection&) = delete;
  ReleasePipelineArtifactProjection& operator=(
      const ReleasePipelineArtifactProjection&) = delete;
  ReleasePipelineArtifactProjection(
      ReleasePipelineArtifactProjection&&) = delete;
  ReleasePipelineArtifactProjection& operator=(
      ReleasePipelineArtifactProjection&&) = delete;

  const ReleaseEvidencePipelineRequest& request;
  const inferlab_high_value::ManagedArtifact& artifact;
  ReleasePipelineRepairSummaryProjection repair_summary;
  std::vector<std::string> recommended_next_commands;
};

const char* release_pipeline_claim_scope() noexcept;

}  // namespace inferlab_release_pipeline
