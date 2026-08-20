#pragma once

#include "evidence_query_cross_artifact.hpp"
#include "product_replay_semantic_linkage_projection.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct EvidenceQueryCrossArtifactDerivedProjection {
  std::size_t active_family_count = 0;
  std::size_t total_blocker_count = 0;
  std::size_t current_repo_action_count = 0;
  std::size_t target_or_validation_action_count = 0;
  std::size_t source_location_gap_count = 0;
  std::string primary_family;
  std::string primary_blocker_reason;
  std::string primary_environment;
  std::string primary_action;
  std::vector<std::string> affected_claims;
  std::vector<inferlab_evidence_query::CrossArtifactBlockerFamily>
      repair_paths;
  bool current_repo_actionable = false;
  bool target_or_validation_required = false;
  bool source_location_metadata_required = false;
  bool product_replay_family_present = false;
  std::string product_replay_family_status = "not_reported";
  inferlab_evidence_query::CrossArtifactBlockerFamily product_replay_family;
  std::vector<std::string> product_replay_source_artifacts;
};

struct EvidenceQueryCrossArtifactClosureProjection {
  bool object_present = false;
  bool release_focus_present = false;
  std::string status = "not_reported";
  std::string release_focus_decision = "not_reported";
  std::string release_focus_repair_lane = "not_reported";
  std::vector<inferlab_evidence_query::CrossArtifactBlockerFamily> families;
  EvidenceQueryCrossArtifactDerivedProjection derived;
  std::size_t contract_issue_count = 0;
};

struct EvidenceQueryProductReplayClosureProjection {
  bool object_present = false;
  inferlab_evidence::ProductReplaySemanticLinkageSignals signals;
  std::size_t contract_issue_count = 0;
};

EvidenceQueryCrossArtifactClosureProjection
project_evidence_query_cross_artifact_closure(
    const std::string& evidence_query_document, bool summary_usable,
    bool source_present);

EvidenceQueryProductReplayClosureProjection
project_evidence_query_product_replay_closure(
    const std::string& evidence_query_document, bool summary_usable,
    bool source_present);

}  // namespace inferlab_release_pipeline
