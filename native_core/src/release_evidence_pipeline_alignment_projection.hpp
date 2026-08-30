#pragma once

#include "artifact_evidence_contract.hpp"
#include "evidence_release_policy.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct ReleasePipelineAlignmentDimension {
  std::string id;
  std::string profile_id;
  std::string status = "blocked";
  std::string blocker_category = "artifact_missing";
  std::string repair_environment =
      "current_server_when_validation_enabled";
  bool release_blocking = false;
  bool ready = false;
  bool cross_artifact_reconciled = false;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  std::vector<std::string> issues;
};

struct ReleasePipelineAlignmentProjection {
  ReleasePipelineAlignmentProjection(
      const std::filesystem::path& repo_root,
      inferlab_evidence::EvidenceSnapshotRepository& snapshots);

  std::vector<ReleasePipelineAlignmentDimension> dimensions;
  std::size_t ready_dimension_count = 0U;
  std::size_t blocking_dimension_count = 0U;
  std::size_t advisory_dimension_count = 0U;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  std::size_t issue_count = 0U;
};

}  // namespace inferlab_release_pipeline
