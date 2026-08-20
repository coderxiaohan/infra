#pragma once

#include "artifact_metadata_reader.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

namespace detail {

inline bool artifact_contains_any(
    const std::string& artifact,
    const std::initializer_list<const char*> fragments) {
  for (const char* fragment : fragments) {
    if (inferlab_plan::contains(artifact, fragment)) return true;
  }
  return false;
}

}  // namespace detail

inline std::string evidence_confidence(
    const inferlab_artifact::Metadata& metadata) {
  return inferlab_evidence::evidence_confidence(
      metadata,
      inferlab_evidence::FixtureConfidencePolicy::kHonorModeOverride);
}

namespace detail {

inline std::string compute_blocker_category(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  inferlab_evidence::MetadataBlockerPolicy policy;
  policy.require_summary = false;
  const auto assessment =
      inferlab_evidence::assess_evidence(record.metadata, policy);
  if (assessment.blocker_category !=
      inferlab_evidence::BlockerCategory::kNone) {
    return inferlab_evidence::blocker_category_name(
        assessment.blocker_category);
  }
  if (!record.summary_present || !record.final_report_present) {
    return "report_wiring_missing";
  }
  if (!record.freshness_present) return "freshness_stale";

  const auto evidence_level =
      inferlab_evidence::evidence_level(record.metadata);
  const bool execute_verified =
      evidence_level == inferlab_evidence::EvidenceLevel::kExecuteVerified;
  const bool replay_supported =
      evidence_level == inferlab_evidence::EvidenceLevel::kReplaySupported;
  const bool execute_required = artifact_contains_any(
      record.artifact, {"microbench", "cuda", "kernel"});
  if (execute_required && !execute_verified) return "execute_missing";

  const bool replay_or_execute_required = artifact_contains_any(
      record.artifact, {"product", "replay"});
  if (replay_or_execute_required && !replay_supported &&
      !execute_verified) {
    return "replay_missing";
  }
  return "none";
}

inline void resolve_evidence_projection(
    inferlab_artifact::WarehouseArtifactRecord& record) {
  auto& projection = record.evidence_projection;
  if (projection.resolved) return;
  projection.blocker_category = compute_blocker_category(record);
  projection.repair_environment =
      inferlab_evidence::repair_environment(projection.blocker_category);
  projection.confidence = evidence_confidence(record.metadata);
  projection.affected_claims = inferlab_evidence::evidence_affected_claims(
      record.metadata, projection.confidence);
  projection.resolved = true;
}

inline const inferlab_artifact::WarehouseEvidenceProjection&
evidence_projection(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  if (!record.evidence_projection.resolved) {
    throw std::logic_error(
        "evidence query record projection must be resolved before use");
  }
  return record.evidence_projection;
}

}  // namespace detail

inline void resolve_evidence_projections(
    std::vector<inferlab_artifact::WarehouseArtifactRecord>& records) {
  for (auto& record : records) detail::resolve_evidence_projection(record);
}

inline const std::string& blocker_category(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  return detail::evidence_projection(record).blocker_category;
}

inline const std::string& evidence_confidence(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  return detail::evidence_projection(record).confidence;
}

inline const std::string& repair_environment(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  return detail::evidence_projection(record).repair_environment;
}

inline const std::vector<std::string>& affected_claims(
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  return detail::evidence_projection(record).affected_claims;
}

inline std::string result_status(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const std::string& query) {
  if (!record.present) return "artifact_missing";
  if (!record.metadata.parse_ok) return "artifact_unreadable";
  if (query == "report_coverage") {
    return record.summary_present && record.prometheus_present &&
                   record.report_present && record.final_report_present
               ? "report_covered"
               : "report_wiring_gap";
  }
  if (query == "freshness") {
    return record.freshness_present ? "freshness_tracked"
                                    : "freshness_tracking_gap";
  }
  if (query == "claim_scope") {
    return record.metadata.claim_scope_present ? "claim_scope_present"
                                               : "claim_scope_missing";
  }
  if (query == "product_replay_linkage") {
    return record.metadata.replayed ? "replay_supported"
                                    : "replay_or_execute_required";
  }
  if (record.metadata.blocker_count > 0U) return "blockers_present";
  return evidence_confidence(record);
}

inline std::string repair_action(const std::string& category) {
  return inferlab_evidence::repair_action_for_blocker(category);
}

inline bool include_record(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const std::string& query,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  if (!artifact_filter.empty() && record.artifact != artifact_filter &&
      !inferlab_plan::contains(record.artifact, artifact_filter)) {
    return false;
  }
  if (!blocker_filter.empty() && blocker_filter != blocker_category(record)) {
    return false;
  }
  if (query == "confidence_summary" || query == "artifact_lineage" ||
      query == "all") {
    return true;
  }
  if (query == "release_blockers") {
    return blocker_category(record) != "none";
  }
  if (query == "claim_scope") {
    return record.metadata.claim_scope_present || !record.present;
  }
  if (query == "report_coverage" || query == "freshness") return true;
  if (query == "product_replay_linkage") {
    return detail::artifact_contains_any(
        record.artifact, {"product", "replay", "release"});
  }
  if (query == "catalog_flow_discrepancy") {
    return detail::artifact_contains_any(
        record.artifact, {"artifact_catalog", "evidence_query"});
  }
  if (query == "native_core_audit") {
    return detail::artifact_contains_any(
        record.artifact,
        {"native_core_audit", "artifact_catalog",
         "release_evidence_pipeline"});
  }
  return true;
}

}  // namespace inferlab_evidence_query
