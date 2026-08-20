#pragma once

#include "artifact_evidence_contract.hpp"
#include "evidence_release_policy.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_release_traffic {

struct ArtifactSpec {
  std::string artifact;
  std::string artifact_type;
  std::string source_role;
};

struct ArtifactAlignmentProjection {
  std::string artifact;
  std::string expected_artifact_type;
  std::string observed_artifact_type = "unknown";
  std::string profile_id = "missing";
  std::string source_role = "missing";
  std::string matched_source_identity;
  double request_qps = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double gpu_hour_cost = 0.0;
  std::size_t match_count = 0U;
  bool profile_section_present = false;
  bool matched = false;
  bool ready = false;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  std::vector<std::string> issues;
};

struct AlignmentProjection {
  std::string profile_id = "missing";
  std::vector<ArtifactAlignmentProjection> artifacts;
  std::size_t required_check_count = 0U;
  std::size_t satisfied_check_count = 0U;
  double coverage = 0.0;
  double gap = 1.0;
  bool cross_artifact_reconciled = false;
  bool ready = false;
  std::string status = "blocked";
  std::string blocker_category = "artifact_missing";
  std::string repair_environment =
      "current_server_when_validation_enabled";
  std::vector<std::string> issues;
};

AlignmentProjection evaluate_alignment(
    const std::filesystem::path& repo_root,
    inferlab_evidence::EvidenceSnapshotRepository& repository);
const std::vector<ArtifactSpec>& alignment_artifact_registry();
void validate_alignment_artifact_registry();

}  // namespace inferlab_release_traffic
