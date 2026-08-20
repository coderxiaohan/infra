#pragma once

#include "artifact_metadata_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inferlab_evidence {

enum class FixtureConfidencePolicy {
  kIgnoreModeOverride,
  kHonorModeOverride,
};

enum class EvidenceLevel {
  kStaleOrBlocked,
  kSchemaOnly,
  kDryRunReady,
  kReplaySupported,
  kExecuteVerified,
};

enum class BlockerCategory {
  kNone,
  kArtifactMissing,
  kSchemaMissing,
  kClaimScopeMissing,
  kSummaryMissing,
  kArtifactBlocked,
  kUpstreamBlocker,
  kReportWiringMissing,
  kFreshnessStale,
  kReplayMissing,
  kExecuteMissing,
};

enum class RequiredFieldPriority {
  kClaimScopeBeforeSummary,
  kSummaryBeforeClaimScope,
};

enum class RepairEnvironment {
  kUnknown,
  kNone,
  kCurrentRepoFixable,
  kCurrentRepoThenValidationMachine,
  kCurrentServerWhenValidationEnabled,
  kTargetOrValidationEnabledMachine,
  kTargetGpuOrExecuteMachine,
  kTargetProductOrReplayEvidence,
};

enum class ReleaseDecision {
  kBlocked,
  kReplayReady,
  kExecuteReady,
  kReportReady,
};

struct MetadataBlockerPolicy {
  bool require_claim_scope = true;
  bool require_summary = true;
  RequiredFieldPriority required_field_priority =
      RequiredFieldPriority::kClaimScopeBeforeSummary;
  BlockerCategory blocked_category = BlockerCategory::kArtifactBlocked;
};

struct RepairCard {
  std::string id;
  std::string artifact;
  std::string category;
  RepairEnvironment repair_environment = RepairEnvironment::kNone;
  std::string severity;
  std::string issue;
  std::string repair_action;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::vector<std::string> current_repo_actions;
  std::vector<std::string> target_machine_actions;
  std::size_t exact_wiring_location_count = 0;
  std::size_t source_location_gap_count = 0;
};

struct EvidenceAssessment {
  EvidenceLevel evidence_level = EvidenceLevel::kStaleOrBlocked;
  BlockerCategory blocker_category = BlockerCategory::kNone;
  RepairEnvironment repair_environment = RepairEnvironment::kNone;
};

struct EvidenceArtifact {
  std::string id;
  std::string source_label;
  std::uintmax_t content_size_bytes = 0;
  std::string state_fingerprint;
  std::string content_fingerprint;
  std::string content_fingerprint_algorithm = "fnv1a64";
  bool content_fingerprint_available = false;
};

struct EvidenceSnapshot {
  std::string artifact;
  EvidenceArtifact descriptor;
  inferlab_artifact::TextReadResult read_result;
  inferlab_artifact::Metadata metadata;
  EvidenceAssessment assessment;
  std::string confidence = "stale_or_blocked";
  std::vector<std::string> affected_claims;
};

inline const char* evidence_level_name(const EvidenceLevel level) {
  switch (level) {
    case EvidenceLevel::kSchemaOnly:
      return "schema_only";
    case EvidenceLevel::kDryRunReady:
      return "dry_run_ready";
    case EvidenceLevel::kReplaySupported:
      return "replay_supported";
    case EvidenceLevel::kExecuteVerified:
      return "execute_verified";
    case EvidenceLevel::kStaleOrBlocked:
      return "stale_or_blocked";
  }
  return "stale_or_blocked";
}

inline bool try_parse_evidence_level(const std::string& value,
                                     EvidenceLevel& level) {
  level = EvidenceLevel::kStaleOrBlocked;
  if (value == "stale_or_blocked") return true;
  if (value == "schema_only") {
    level = EvidenceLevel::kSchemaOnly;
    return true;
  }
  if (value == "dry_run_ready") {
    level = EvidenceLevel::kDryRunReady;
    return true;
  }
  if (value == "replay_supported") {
    level = EvidenceLevel::kReplaySupported;
    return true;
  }
  if (value == "execute_verified") {
    level = EvidenceLevel::kExecuteVerified;
    return true;
  }
  return false;
}

inline const char* blocker_category_name(const BlockerCategory category) {
  switch (category) {
    case BlockerCategory::kNone:
      return "none";
    case BlockerCategory::kArtifactMissing:
      return "artifact_missing";
    case BlockerCategory::kSchemaMissing:
      return "schema_missing";
    case BlockerCategory::kClaimScopeMissing:
      return "claim_scope_missing";
    case BlockerCategory::kSummaryMissing:
      return "summary_missing";
    case BlockerCategory::kArtifactBlocked:
      return "artifact_blocked";
    case BlockerCategory::kUpstreamBlocker:
      return "upstream_blocker";
    case BlockerCategory::kReportWiringMissing:
      return "report_wiring_missing";
    case BlockerCategory::kFreshnessStale:
      return "freshness_stale";
    case BlockerCategory::kReplayMissing:
      return "replay_missing";
    case BlockerCategory::kExecuteMissing:
      return "execute_missing";
  }
  return "none";
}

inline bool try_parse_blocker_category(const std::string& value,
                                       BlockerCategory& category) {
  category = BlockerCategory::kNone;
  if (value == "none") return true;
  if (value == "artifact_missing") {
    category = BlockerCategory::kArtifactMissing;
    return true;
  }
  if (value == "schema_missing") {
    category = BlockerCategory::kSchemaMissing;
    return true;
  }
  if (value == "claim_scope_missing") {
    category = BlockerCategory::kClaimScopeMissing;
    return true;
  }
  if (value == "summary_missing") {
    category = BlockerCategory::kSummaryMissing;
    return true;
  }
  if (value == "artifact_blocked") {
    category = BlockerCategory::kArtifactBlocked;
    return true;
  }
  if (value == "upstream_blocker") {
    category = BlockerCategory::kUpstreamBlocker;
    return true;
  }
  if (value == "report_wiring_missing") {
    category = BlockerCategory::kReportWiringMissing;
    return true;
  }
  if (value == "freshness_stale") {
    category = BlockerCategory::kFreshnessStale;
    return true;
  }
  if (value == "replay_missing") {
    category = BlockerCategory::kReplayMissing;
    return true;
  }
  if (value == "execute_missing") {
    category = BlockerCategory::kExecuteMissing;
    return true;
  }
  return false;
}

inline const char* repair_environment_name(
    const RepairEnvironment environment) {
  switch (environment) {
    case RepairEnvironment::kUnknown:
      return "unknown";
    case RepairEnvironment::kNone:
      return "none";
    case RepairEnvironment::kCurrentRepoFixable:
      return "current_repo_fixable";
    case RepairEnvironment::kCurrentRepoThenValidationMachine:
      return "current_repo_then_validation_machine";
    case RepairEnvironment::kCurrentServerWhenValidationEnabled:
      return "current_server_when_validation_enabled";
    case RepairEnvironment::kTargetOrValidationEnabledMachine:
      return "target_or_validation_enabled_machine";
    case RepairEnvironment::kTargetGpuOrExecuteMachine:
      return "target_gpu_or_execute_machine";
    case RepairEnvironment::kTargetProductOrReplayEvidence:
      return "target_product_or_replay_evidence";
  }
  return "none";
}

inline RepairEnvironment parse_repair_environment(const std::string& value) {
  if (value == "none") return RepairEnvironment::kNone;
  if (value == "current_repo_fixable") {
    return RepairEnvironment::kCurrentRepoFixable;
  }
  if (value == "current_repo_then_validation_machine") {
    return RepairEnvironment::kCurrentRepoThenValidationMachine;
  }
  if (value == "current_server_when_validation_enabled") {
    return RepairEnvironment::kCurrentServerWhenValidationEnabled;
  }
  if (value == "target_or_validation_enabled_machine") {
    return RepairEnvironment::kTargetOrValidationEnabledMachine;
  }
  if (value == "target_gpu_or_execute_machine") {
    return RepairEnvironment::kTargetGpuOrExecuteMachine;
  }
  if (value == "target_product_or_replay_evidence") {
    return RepairEnvironment::kTargetProductOrReplayEvidence;
  }
  return RepairEnvironment::kUnknown;
}

inline bool repair_environment_is_current_repo(
    const RepairEnvironment environment) {
  return environment == RepairEnvironment::kCurrentRepoFixable ||
         environment == RepairEnvironment::kCurrentRepoThenValidationMachine;
}

inline bool repair_environment_is_target_or_validation(
    const RepairEnvironment environment) {
  return environment ==
             RepairEnvironment::kCurrentServerWhenValidationEnabled ||
         environment == RepairEnvironment::kTargetOrValidationEnabledMachine ||
         environment == RepairEnvironment::kTargetGpuOrExecuteMachine ||
         environment == RepairEnvironment::kTargetProductOrReplayEvidence;
}

}  // namespace inferlab_evidence
