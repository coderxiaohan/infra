#pragma once

#include "artifact_text_source.hpp"
#include "evidence_domain_types.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence {

inline const char* release_decision_name(const ReleaseDecision decision) {
  switch (decision) {
    case ReleaseDecision::kBlocked:
      return "blocked";
    case ReleaseDecision::kReplayReady:
      return "replay_ready";
    case ReleaseDecision::kExecuteReady:
      return "execute_ready";
    case ReleaseDecision::kReportReady:
      return "report_ready";
  }
  return "blocked";
}

inline bool try_parse_release_decision(const std::string& value,
                                       ReleaseDecision& decision) {
  decision = ReleaseDecision::kBlocked;
  if (value == "blocked") {
    return true;
  }
  if (value == "report_ready") {
    decision = ReleaseDecision::kReportReady;
    return true;
  }
  if (value == "replay_ready") {
    decision = ReleaseDecision::kReplayReady;
    return true;
  }
  if (value == "execute_ready") {
    decision = ReleaseDecision::kExecuteReady;
    return true;
  }
  return false;
}

inline bool is_readiness_target(const std::string& value) {
  ReleaseDecision decision = ReleaseDecision::kBlocked;
  return try_parse_release_decision(value, decision) &&
         decision != ReleaseDecision::kBlocked;
}

inline std::vector<ReleaseDecision> readiness_targets() {
  return {ReleaseDecision::kReportReady, ReleaseDecision::kReplayReady,
          ReleaseDecision::kExecuteReady};
}

inline std::vector<std::string> readiness_target_names() {
  std::vector<std::string> names;
  for (const auto target : readiness_targets()) {
    names.push_back(release_decision_name(target));
  }
  return names;
}

inline std::vector<std::string> readiness_path_steps(
    const ReleaseDecision target) {
  switch (target) {
    case ReleaseDecision::kReportReady:
      return {"schema present", "summary present", "claim_scope present",
              "report/final-report wiring present", "freshness tracked"};
    case ReleaseDecision::kReplayReady:
      return {"report_ready", "replay artifact imported",
              "semantic correlation refreshed", "release gate refreshed"};
    case ReleaseDecision::kExecuteReady:
      return {"report_ready", "target-machine build",
              "native/CUDA/product execute artifact",
              "summary/Prometheus/final report refreshed"};
    case ReleaseDecision::kBlocked:
      return {};
  }
  return {};
}

inline std::string readiness_paths_json(const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  const auto targets = readiness_targets();
  out << "[\n";
  for (std::size_t i = 0; i < targets.size(); ++i) {
    out << nested << "{\n";
    out << nested << "  \"target\": "
        << inferlab_plan::json_string(release_decision_name(targets[i]))
        << ",\n";
    out << nested << "  \"steps\": "
        << inferlab_plan::json_string_array(
               readiness_path_steps(targets[i]), indent + 2)
        << "\n";
    out << nested << "}" << (i + 1 == targets.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

inline EvidenceLevel release_decision_evidence_level(
    const ReleaseDecision decision) {
  switch (decision) {
    case ReleaseDecision::kExecuteReady:
      return EvidenceLevel::kExecuteVerified;
    case ReleaseDecision::kReplayReady:
      return EvidenceLevel::kReplaySupported;
    case ReleaseDecision::kReportReady:
      return EvidenceLevel::kDryRunReady;
    case ReleaseDecision::kBlocked:
      return EvidenceLevel::kStaleOrBlocked;
  }
  return EvidenceLevel::kStaleOrBlocked;
}

using inferlab_plan::contains;

inline bool has_read_issue(
    const inferlab_artifact::TextReadResult& read_result) {
  return inferlab_artifact::text_read_has_issue(read_result);
}

inline std::string read_text_if_readable(const std::filesystem::path& path) {
  const auto read_result = inferlab_artifact::read_text_with_status(path);
  return read_result.readable ? read_result.text : "";
}

inline EvidenceLevel evidence_level(
    const inferlab_artifact::Metadata& metadata) {
  if (!metadata.present || !metadata.parse_ok || metadata.blocker_count > 0) {
    return EvidenceLevel::kStaleOrBlocked;
  }
  if (metadata.executed &&
      inferlab_artifact::json_mode_matches(metadata.mode, "execute")) {
    return EvidenceLevel::kExecuteVerified;
  }
  if (metadata.replayed) return EvidenceLevel::kReplaySupported;
  if (metadata.schema_present && metadata.summary_present &&
      metadata.claim_scope_present) {
    return EvidenceLevel::kDryRunReady;
  }
  if (metadata.schema_present) return EvidenceLevel::kSchemaOnly;
  return EvidenceLevel::kStaleOrBlocked;
}

inline std::string evidence_confidence(
    const inferlab_artifact::Metadata& metadata,
    const FixtureConfidencePolicy fixture_policy =
        FixtureConfidencePolicy::kIgnoreModeOverride) {
  constexpr const char* kFixturePrefix = "fixture_confidence:";
  if (fixture_policy == FixtureConfidencePolicy::kHonorModeOverride &&
      metadata.mode.rfind(kFixturePrefix, 0) == 0) {
    return metadata.mode.substr(std::string(kFixturePrefix).size());
  }
  return evidence_level_name(evidence_level(metadata));
}

inline BlockerCategory metadata_blocker_category(
    const inferlab_artifact::Metadata& metadata,
    const MetadataBlockerPolicy& policy = MetadataBlockerPolicy{}) {
  if (!metadata.present) return BlockerCategory::kArtifactMissing;
  if (!metadata.parse_ok || !metadata.schema_present) {
    return BlockerCategory::kSchemaMissing;
  }
  const bool missing_claim_scope =
      policy.require_claim_scope && !metadata.claim_scope_present;
  const bool missing_summary =
      policy.require_summary && !metadata.summary_present;
  if (policy.required_field_priority ==
      RequiredFieldPriority::kSummaryBeforeClaimScope) {
    if (missing_summary) return BlockerCategory::kSummaryMissing;
    if (missing_claim_scope) return BlockerCategory::kClaimScopeMissing;
  } else {
    if (missing_claim_scope) return BlockerCategory::kClaimScopeMissing;
    if (missing_summary) return BlockerCategory::kSummaryMissing;
  }
  if (metadata.blocker_count > 0) return policy.blocked_category;
  return BlockerCategory::kNone;
}

inline bool metadata_ready(
    const inferlab_artifact::Metadata& metadata,
    const MetadataBlockerPolicy& policy = MetadataBlockerPolicy{}) {
  return metadata_blocker_category(metadata, policy) ==
         BlockerCategory::kNone;
}

inline RepairEnvironment repair_environment_kind(
    const std::string& category) {
  if (category.empty() || category == "none") {
    return RepairEnvironment::kNone;
  }
  if (category == "execute_missing" ||
      category == "execute_evidence_missing" ||
      category == "microbenchmark_not_run" ||
      category == "dependency_unavailable" ||
      category == "product_environment_missing") {
    return RepairEnvironment::kTargetGpuOrExecuteMachine;
  }
  if (category == "replay_missing" ||
      category == "replay_evidence_missing" ||
      category == "product_replay_semantic_linkage" ||
      category == "product_replay_family_gate") {
    return RepairEnvironment::kTargetProductOrReplayEvidence;
  }
  if (category == "artifact_missing" ||
      category == "artifact_contract_blocked" ||
      category == "freshness_stale" || category == "upstream_blocker") {
    return RepairEnvironment::kCurrentServerWhenValidationEnabled;
  }
  return RepairEnvironment::kCurrentRepoFixable;
}

inline std::string repair_action_for_blocker(const std::string& category) {
  if (category == "artifact_missing") {
    return "refresh artifact producer from benchmark matrix when validation is enabled";
  }
  if (category == "schema_missing") {
    return "update producer schema fields or refresh artifact with schema_version/artifact_type";
  }
  if (category == "claim_scope_missing") {
    return "add claim_scope or claim_scope_detail before allowing report claims";
  }
  if (category == "summary_missing") {
    return "refresh or repair summary projection before allowing report claims";
  }
  if (category == "report_wiring_missing") {
    return "wire artifact into structured summary, Prometheus, and JSON evidence indexes";
  }
  if (category == "freshness_stale") {
    return "refresh runtime evidence freshness gate after artifact refresh";
  }
  if (category == "replay_missing") {
    return "import product replay evidence or run sample semantic extraction";
  }
  if (category == "execute_missing") {
    return "run target-machine execute/correctness/timing workflow";
  }
  if (category == "artifact_blocked") {
    return "inspect artifact blockers and regenerate upstream evidence";
  }
  if (category == "upstream_blocker") {
    return "repair upstream evidence and refresh dependent release/query artifacts";
  }
  return "no repair required";
}

inline int repair_environment_priority(
    const RepairEnvironment environment) {
  switch (environment) {
    case RepairEnvironment::kUnknown:
    case RepairEnvironment::kNone:
    case RepairEnvironment::kCurrentRepoFixable:
    case RepairEnvironment::kCurrentRepoThenValidationMachine:
      return 0;
    case RepairEnvironment::kCurrentServerWhenValidationEnabled:
    case RepairEnvironment::kTargetOrValidationEnabledMachine:
      return 1;
    case RepairEnvironment::kTargetProductOrReplayEvidence:
      return 2;
    case RepairEnvironment::kTargetGpuOrExecuteMachine:
      return 3;
  }
  return 0;
}

inline bool repair_environment_blocks_report_ready(
    const RepairEnvironment environment) {
  return environment != RepairEnvironment::kTargetProductOrReplayEvidence &&
         environment != RepairEnvironment::kTargetGpuOrExecuteMachine;
}

inline bool repair_environment_blocks_replay_ready(
    const RepairEnvironment environment) {
  return repair_environment_blocks_report_ready(environment) ||
         environment == RepairEnvironment::kTargetProductOrReplayEvidence;
}

inline bool repair_environment_blocks_execute_ready(
    const RepairEnvironment environment) {
  return repair_environment_blocks_report_ready(environment) ||
         environment == RepairEnvironment::kTargetGpuOrExecuteMachine;
}

inline ReleaseDecision primary_readiness_target_for_repair_environment(
    const RepairEnvironment environment) {
  if (environment == RepairEnvironment::kTargetProductOrReplayEvidence) {
    return ReleaseDecision::kReplayReady;
  }
  if (environment == RepairEnvironment::kTargetGpuOrExecuteMachine) {
    return ReleaseDecision::kExecuteReady;
  }
  return ReleaseDecision::kReportReady;
}

inline EvidenceAssessment assess_evidence(
    const inferlab_artifact::Metadata& metadata,
    const MetadataBlockerPolicy& policy = MetadataBlockerPolicy{}) {
  EvidenceAssessment assessment;
  assessment.evidence_level = evidence_level(metadata);
  assessment.blocker_category = metadata_blocker_category(metadata, policy);
  assessment.repair_environment = repair_environment_kind(
      blocker_category_name(assessment.blocker_category));
  return assessment;
}

inline std::vector<std::string> evidence_affected_claims(
    const inferlab_artifact::Metadata& metadata,
    const std::string& confidence) {
  std::vector<std::string> claims;
  if (!confidence.empty()) claims.push_back(confidence);
  if (!metadata.claim_scope.empty() && metadata.claim_scope != confidence) {
    claims.push_back(metadata.claim_scope);
  }
  return claims;
}

inline void append_evidence_affected_claims(
    std::vector<std::string>& claims,
    const inferlab_artifact::Metadata& metadata,
    const std::string& confidence) {
  for (const auto& candidate :
       evidence_affected_claims(metadata, confidence)) {
    bool already_present = false;
    for (const auto& existing : claims) {
      if (existing == candidate) {
        already_present = true;
        break;
      }
    }
    if (!already_present) claims.push_back(candidate);
  }
}

inline EvidenceArtifact describe_evidence_artifact(
    const std::filesystem::path& path,
    const inferlab_artifact::TextReadResult& read_result,
    const inferlab_artifact::Metadata& metadata) {
  EvidenceArtifact descriptor;
  descriptor.id = metadata.source.empty() ? path.filename().string()
                                          : metadata.source;
  descriptor.source_label =
      inferlab_plan::redacted_path_label(path.generic_string());
  descriptor.content_size_bytes = read_result.readable
                                      ? read_result.text.size()
                                      : inferlab_artifact::file_size_or_zero(path);

  std::ostringstream state;
  state << "present=" << (metadata.present ? "1" : "0");
  state << ";size=" << descriptor.content_size_bytes;
  state << ";schema=" << (metadata.schema_present ? "1" : "0");
  state << ";summary=" << (metadata.summary_present ? "1" : "0");
  state << ";claim=" << (metadata.claim_scope_present ? "1" : "0");
  state << ";executed=" << (metadata.executed ? "1" : "0");
  state << ";replayed=" << (metadata.replayed ? "1" : "0");
  state << ";blockers=" << metadata.blocker_count;
  state << ";type=" << metadata.artifact_type;
  state << ";mode=" << metadata.mode;
  descriptor.state_fingerprint = state.str();

  if (read_result.readable) {
    descriptor.content_fingerprint =
        descriptor.content_fingerprint_algorithm + ":" +
        inferlab_plan::hex_u64(inferlab_plan::fnv1a64(read_result.text));
    descriptor.content_fingerprint_available = true;
  }
  return descriptor;
}

inline EvidenceSnapshot capture_evidence_snapshot(
    const std::filesystem::path& path,
    const MetadataBlockerPolicy& blocker_policy = MetadataBlockerPolicy{},
    const FixtureConfidencePolicy fixture_policy =
        FixtureConfidencePolicy::kIgnoreModeOverride) {
  EvidenceSnapshot snapshot;
  snapshot.read_result = inferlab_artifact::read_text_with_status(path);
  snapshot.metadata =
      inferlab_artifact::read_metadata(path, snapshot.read_result);
  snapshot.descriptor =
      describe_evidence_artifact(path, snapshot.read_result, snapshot.metadata);
  snapshot.artifact = snapshot.descriptor.id;
  snapshot.assessment = assess_evidence(snapshot.metadata, blocker_policy);
  if (fixture_policy == FixtureConfidencePolicy::kHonorModeOverride) {
    snapshot.confidence = evidence_confidence(snapshot.metadata, fixture_policy);
  } else {
    snapshot.confidence =
        evidence_level_name(snapshot.assessment.evidence_level);
  }
  snapshot.affected_claims =
      evidence_affected_claims(snapshot.metadata, snapshot.confidence);
  return snapshot;
}

inline bool evidence_snapshot_ready(const EvidenceSnapshot& snapshot) {
  return snapshot.assessment.blocker_category == BlockerCategory::kNone;
}

class EvidenceSnapshotRepository {
 public:
  explicit EvidenceSnapshotRepository(
      MetadataBlockerPolicy blocker_policy = MetadataBlockerPolicy{},
      FixtureConfidencePolicy fixture_policy =
          FixtureConfidencePolicy::kIgnoreModeOverride)
      : blocker_policy_(blocker_policy), fixture_policy_(fixture_policy) {}

  const EvidenceSnapshot& get(const std::filesystem::path& path) {
    const std::string key = path.lexically_normal().generic_string();
    const auto found = snapshots_.find(key);
    if (found != snapshots_.end()) return found->second;
    const auto inserted = snapshots_.emplace(
        key, capture_evidence_snapshot(path, blocker_policy_, fixture_policy_));
    return inserted.first->second;
  }

  std::size_t size() const { return snapshots_.size(); }

 private:
  MetadataBlockerPolicy blocker_policy_;
  FixtureConfidencePolicy fixture_policy_;
  std::map<std::string, EvidenceSnapshot> snapshots_;
};

inline std::string repair_environment(const std::string& category) {
  return repair_environment_name(repair_environment_kind(category));
}

}  // namespace inferlab_evidence
