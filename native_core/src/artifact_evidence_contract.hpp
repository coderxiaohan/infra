#pragma once

#include "evidence_domain_types.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence {

struct ArtifactContractPolicy {
  bool require_schema = true;
  bool require_artifact_type = true;
  bool require_status_ok = true;
  bool require_summary_object = true;
  bool require_claim_scope = true;
  bool require_supported_mode = true;
  bool require_executed_flag = true;
  bool require_replayed_flag = true;
  bool require_mode_flag_consistency = true;
  bool require_clear_blockers = true;
};

struct ArtifactContractProjection {
  bool present = false;
  bool readable = false;
  bool parse_ok = false;
  bool schema_present = false;
  bool artifact_type_present = false;
  bool status_ok = false;
  bool summary_present = false;
  bool claim_scope_present = false;
  bool supported_mode = false;
  bool executed_present = false;
  bool replayed_present = false;
  bool mode_flags_consistent = false;
  bool blockers_clear = false;
  bool ready = false;
  bool executed = false;
  bool replayed = false;
  std::size_t upstream_blocker_count = 0;
  std::size_t required_check_count = 0;
  std::size_t satisfied_check_count = 0;
  double coverage = 0.0;
  double gap = 1.0;
  std::string read_status = "missing";
  std::string artifact_type = "unknown";
  std::string mode = "missing";
  std::string evidence_level = "stale_or_blocked";
  std::string blocker_category = "artifact_missing";
  std::string repair_environment = "current_server_when_validation_enabled";
  std::vector<std::string> issues;
};

ArtifactContractProjection evaluate_artifact_contract(
    const EvidenceSnapshot& snapshot,
    const ArtifactContractPolicy& policy = ArtifactContractPolicy{});

}  // namespace inferlab_evidence
