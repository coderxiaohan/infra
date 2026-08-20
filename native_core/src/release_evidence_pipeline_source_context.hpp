#pragma once

#include "evidence_domain_utils.hpp"
#include "native_core_audit_merge.hpp"
#include "high_value_artifact_catalog.hpp"
#include "product_replay_semantic_linkage_projection.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct ReleasePipelineSourceContext {
  ReleasePipelineSourceContext(
      const std::filesystem::path& results,
      inferlab_evidence::EvidenceSnapshotRepository& snapshots);

  ReleasePipelineSourceContext(const ReleasePipelineSourceContext&) = delete;
  ReleasePipelineSourceContext& operator=(
      const ReleasePipelineSourceContext&) = delete;
  ReleasePipelineSourceContext(ReleasePipelineSourceContext&&) = delete;
  ReleasePipelineSourceContext& operator=(
      ReleasePipelineSourceContext&&) = delete;

  const inferlab_evidence::EvidenceSnapshot* evidence_query = nullptr;
  const inferlab_evidence::EvidenceSnapshot* native_audit = nullptr;

  const std::string& evidence_query_document;
  inferlab_artifact::StrictJsonObjectSectionOrRootResult
      evidence_query_summary_projection;
  const std::string& evidence_query_summary;
  const std::string& native_audit_document;
  std::string native_audit_summary;

  inferlab_evidence::EvidenceQuerySnapshotDiffSignals snapshot_diff;
  inferlab_evidence::EvidenceQueryNavigationSignals navigation;
  inferlab_evidence::ProductReplayReadinessSignals product_replay_readiness;
  inferlab_evidence::NativeCoreAuditMergedProjection native_audit_projection;

  std::size_t present_source_count = 0;
  std::size_t read_issue_count = 0;
};

}  // namespace inferlab_release_pipeline
