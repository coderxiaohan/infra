#pragma once

#include "evidence_domain_types.hpp"
#include "high_value_artifact_catalog.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::artifact_schema_registry {

struct ConfidenceSpec {
  inferlab_evidence::EvidenceLevel level =
      inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
  std::string meaning;
};

struct ClaimBoundaryRule {
  std::string rule;
  std::string decision;
};

struct ReleaseDecisionRule {
  inferlab_evidence::ReleaseDecision decision =
      inferlab_evidence::ReleaseDecision::kBlocked;
  std::string requires;
};

struct RegistryPlan {
  std::vector<const inferlab_high_value::ManagedArtifact*> artifacts;
  std::vector<std::string> registry_files;
  std::vector<std::string> optional_fields;
  std::vector<ConfidenceSpec> confidence_levels;
  std::vector<ClaimBoundaryRule> claim_boundary_rules;
  std::vector<ReleaseDecisionRule> release_decision_rules;
  std::vector<std::string> recommended_next_commands;
  bool selector_export_schema_registered = false;
};

RegistryPlan build_plan(const std::filesystem::path& repo_root);

void validate_plan(const RegistryPlan& plan);

}  // namespace inferlab::artifact_schema_registry
