#pragma once

#include "evidence_domain_utils.hpp"
#include "release_evidence_pipeline_decision_projection.hpp"
#include "release_evidence_pipeline_alignment_projection.hpp"
#include "release_evidence_pipeline_evidence_query_closure_projection.hpp"
#include "release_evidence_pipeline_native_audit_closure_projection.hpp"
#include "release_evidence_pipeline_repair_projection.hpp"
#include "release_evidence_pipeline_root_cause_projection.hpp"
#include "release_evidence_pipeline_source_context.hpp"
#include "release_evidence_pipeline_snapshot_diff_link_projection.hpp"
#include "release_evidence_pipeline_stage.hpp"
#include "repository_evidence_dag.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct ReleaseEvidencePipelineRequest {
  ReleaseEvidencePipelineRequest(const std::filesystem::path& repo_root_value,
                                 const std::filesystem::path& results_value,
                                 const std::filesystem::path& output_value,
                                 const std::string& format_value);

  ReleaseEvidencePipelineRequest(const ReleaseEvidencePipelineRequest&) =
      delete;
  ReleaseEvidencePipelineRequest& operator=(
      const ReleaseEvidencePipelineRequest&) = delete;
  ReleaseEvidencePipelineRequest(ReleaseEvidencePipelineRequest&&) = delete;
  ReleaseEvidencePipelineRequest& operator=(
      ReleaseEvidencePipelineRequest&&) = delete;

  std::filesystem::path repo_root;
  std::filesystem::path results;
  std::filesystem::path output;
  std::string format;
  std::vector<PipelineStage> stages;
  inferlab_evidence::MetadataBlockerPolicy metadata_policy;
  inferlab_evidence::EvidenceSnapshotRepository snapshots;
  EvidenceSnapshotRefs stage_snapshots;
  ReleasePipelineSourceContext sources;
  ReleasePipelineAlignmentProjection alignment;
  inferlab_repository_dag::RepositoryEvidenceDag repository_dependency_graph;
  ReleasePipelineNativeAuditClosureProjection native_audit_closure;
  ReleasePipelineEvidenceQueryClosureProjection evidence_query_closure;
  ReleasePipelineRootCauseSourceProjection root_cause_source;
  ReleasePipelineRepairProjection repair;
  ReleasePipelineSnapshotDiffReleaseLinkProjection snapshot_diff_release_link;
  ReleasePipelineBlockerRepairCardLinkProjection blocker_repair_link;
  ReleasePipelineDecisionProjection decision;
};

}  // namespace inferlab_release_pipeline
