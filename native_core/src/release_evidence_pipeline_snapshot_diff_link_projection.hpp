#pragma once

#include "evidence_query_projection.hpp"
#include "high_value_artifact_catalog.hpp"
#include "release_evidence_pipeline_evidence_query_closure_projection.hpp"
#include "release_evidence_pipeline_repair_projection.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

struct ReleasePipelineSnapshotDiffReleaseLinkProjection {
  ReleasePipelineSnapshotDiffReleaseLinkProjection(
      const ReleasePipelineEvidenceQueryClosureProjection& evidence_query,
      const ReleasePipelineRepairProjection& repair);

  const inferlab_high_value::ManagedArtifact& artifact;
  const inferlab_evidence::EvidenceQuerySnapshotDiffSignals& snapshot_diff;
  const RepairPathGroupSpec& group_spec;
  bool direct_artifact_present = false;
  std::size_t repair_path_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t evidence_counter_count = 0;
  std::size_t group_plan_step = 0;
  bool first_repair_path_present = false;
  std::string first_repair_path_id;
  std::string first_source_location;
  std::string recommended_action;
  std::string command;
  bool current_repo_action_required = false;
  bool target_or_validation_action_required = false;
  std::string link_status = "snapshot_diff_query_missing";
  std::string release_blocker_reason = "snapshot_diff_query_missing";
};

}  // namespace inferlab_release_pipeline
