#pragma once

#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::dependency_governance {

enum class DependencyRisk {
  low,
  medium,
  high,
};

enum class DependencyKind {
  build_or_tooling,
  product_runtime,
};

enum class LicenseReviewStatus {
  recorded_permissive,
  manual_review_required,
  metadata_missing,
};

enum class GovernanceDecision {
  documented,
  blocked,
};

const char* dependency_risk_name(DependencyRisk risk);
const char* dependency_kind_name(DependencyKind kind);
const char* license_review_status_name(LicenseReviewStatus status);
const char* governance_decision_name(GovernanceDecision decision);

struct DependencyRecord {
  inferlab_high_value::DependencySpec dependency;
  DependencyRisk risk = DependencyRisk::low;
  DependencyKind kind = DependencyKind::build_or_tooling;
  LicenseReviewStatus license_review_status =
      LicenseReviewStatus::metadata_missing;
  std::vector<std::string> metadata_gaps;
  bool metadata_complete = false;
  bool fallback_ready = false;
  bool fallback_blocks_core_claim = false;
  bool dry_run_claim_allowed = false;
};

struct LicenseGroup {
  std::string license;
  std::size_t component_count = 0;
};

struct Summary {
  std::size_t dependency_count = 0;
  std::size_t required_dependency_count = 0;
  std::size_t optional_dependency_count = 0;
  std::size_t product_runtime_dependency_count = 0;
  std::size_t high_risk_dependency_count = 0;
  std::size_t fallback_ready_count = 0;
  std::size_t metadata_complete_dependency_count = 0;
  std::size_t permissive_license_count = 0;
  std::size_t manual_license_review_count = 0;
  std::size_t blocker_count = 0;
  GovernanceDecision decision = GovernanceDecision::blocked;
};

struct Plan {
  Summary summary;
  std::vector<DependencyRecord> records;
  std::vector<LicenseGroup> license_groups;
  std::vector<std::size_t> blocker_record_indices;
  std::vector<std::string> dependency_names;
  std::vector<std::string> product_runtime_names;
  std::vector<std::string> target_machine_validation_commands;
};

Plan build_plan(
    const std::vector<inferlab_high_value::DependencySpec>& dependencies,
    const inferlab_high_value::ManagedArtifact& policy_artifact,
    const inferlab_high_value::ManagedArtifact& sbom_artifact);

}  // namespace inferlab::dependency_governance
