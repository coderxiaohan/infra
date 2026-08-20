#include "artifact_evidence_contract.hpp"

#include "artifact_metadata_reader.hpp"
#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace inferlab_evidence {
namespace {

bool valid_schema_contract(const std::string& document) {
  std::size_t schema_version = 0;
  const bool version_present =
      inferlab_artifact::extract_unsigned_key_fallback(
          document, "schema_version", schema_version) &&
      schema_version > 0U;
  const bool schema_object_present =
      !inferlab_artifact::json_object_section_or_empty(document, "schema")
           .empty();
  return version_present || schema_object_present;
}

bool status_is_ok(const std::string& document) {
  std::string status;
  if (!inferlab_artifact::extract_top_level_string_key_fallback(
          document, "status", status)) {
    return false;
  }
  if (status == "ok" || status == "verified" ||
      status == "external_export_written") {
    return true;
  }
  constexpr const char* kReadySuffix = "_ready";
  constexpr std::size_t kReadySuffixSize = 6U;
  return status.size() > kReadySuffixSize &&
         status.compare(status.size() - kReadySuffixSize,
                        kReadySuffixSize, kReadySuffix) == 0;
}

bool summary_is_object(const std::string& document) {
  return inferlab_artifact::contains_top_level_key_fallback(document,
                                                             "summary") &&
         !inferlab_artifact::json_object_section_or_empty(document,
                                                           "summary")
              .empty();
}

bool mode_is_supported(const std::string& mode) {
  const auto& modes = inferlab_catalog::registry_allowed_modes();
  return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

bool mode_flags_are_consistent(
    const inferlab_artifact::Metadata& metadata) {
  if (!metadata.executed_present || !metadata.replayed_present) return false;
  if (inferlab_artifact::json_mode_matches(metadata.mode, "execute")) {
    return metadata.executed && !metadata.replayed;
  }
  if (inferlab_artifact::json_mode_matches(metadata.mode, "replay")) {
    return metadata.replayed && !metadata.executed;
  }
  if (inferlab_artifact::json_mode_matches(metadata.mode, "dry_run")) {
    return !metadata.executed && !metadata.replayed;
  }
  if (metadata.mode == "plan") {
    return !metadata.executed && !metadata.replayed;
  }
  return false;
}

bool blockers_contract_is_clear(
    const std::string& document,
    const inferlab_artifact::Metadata& metadata) {
  if (!inferlab_artifact::contains_top_level_key_fallback(document,
                                                           "blockers")) {
    return metadata.blocker_count == 0U;
  }
  const std::string blockers =
      inferlab_artifact::json_array_section_or_empty(document, "blockers");
  return !blockers.empty() && metadata.blocker_count == 0U;
}

void add_check(ArtifactContractProjection& projection,
               const bool enabled,
               const bool satisfied,
               const std::string& issue) {
  if (!enabled) return;
  projection.required_check_count = inferlab::internal::checked_add_size(
      projection.required_check_count, 1U,
      "artifact evidence contract required check count");
  if (satisfied) {
    projection.satisfied_check_count = inferlab::internal::checked_add_size(
        projection.satisfied_check_count, 1U,
        "artifact evidence contract satisfied check count");
    return;
  }
  inferlab_plan::append_unique_nonempty_string(projection.issues, issue);
}

}  // namespace

ArtifactContractProjection evaluate_artifact_contract(
    const EvidenceSnapshot& snapshot,
    const ArtifactContractPolicy& policy) {
  ArtifactContractProjection projection;
  const auto& read_result = snapshot.read_result;
  const auto& metadata = snapshot.metadata;
  const std::string& document = read_result.text;

  projection.present = metadata.present;
  projection.readable = read_result.readable;
  projection.parse_ok = metadata.parse_ok;
  projection.schema_present =
      metadata.parse_ok && valid_schema_contract(document);
  projection.artifact_type_present =
      metadata.parse_ok && !metadata.artifact_type.empty() &&
      metadata.artifact_type != "unknown";
  projection.status_ok = metadata.parse_ok && status_is_ok(document);
  projection.summary_present =
      metadata.parse_ok && summary_is_object(document);
  projection.claim_scope_present =
      metadata.claim_scope_present && !metadata.claim_scope.empty();
  projection.supported_mode = mode_is_supported(metadata.mode);
  projection.executed_present = metadata.executed_present;
  projection.replayed_present = metadata.replayed_present;
  projection.mode_flags_consistent = mode_flags_are_consistent(metadata);
  projection.blockers_clear =
      metadata.parse_ok && blockers_contract_is_clear(document, metadata);
  projection.executed = metadata.executed;
  projection.replayed = metadata.replayed;
  projection.upstream_blocker_count = metadata.blocker_count;
  projection.read_status = read_result.status;
  projection.artifact_type = metadata.artifact_type;
  projection.mode = metadata.mode;
  projection.evidence_level = evidence_level_name(evidence_level(metadata));

  add_check(projection, true, read_result.readable && metadata.parse_ok,
            "artifact_missing_unreadable_empty_or_invalid_json");
  add_check(projection, policy.require_schema, projection.schema_present,
            "schema_contract_missing_or_invalid");
  add_check(projection, policy.require_artifact_type,
            projection.artifact_type_present,
            "artifact_type_missing_empty_or_wrong_type");
  add_check(projection, policy.require_status_ok, projection.status_ok,
            "artifact_status_missing_or_not_ok");
  add_check(projection, policy.require_summary_object,
            projection.summary_present,
            "summary_object_missing_or_wrong_type");
  add_check(projection, policy.require_claim_scope,
            projection.claim_scope_present,
            "claim_scope_missing_empty_or_wrong_type");
  add_check(projection, policy.require_supported_mode,
            projection.supported_mode, "mode_missing_or_unsupported");
  add_check(projection, policy.require_executed_flag,
            projection.executed_present,
            "executed_boolean_missing_or_wrong_type");
  add_check(projection, policy.require_replayed_flag,
            projection.replayed_present,
            "replayed_boolean_missing_or_wrong_type");
  add_check(projection, policy.require_mode_flag_consistency,
            projection.mode_flags_consistent,
            "mode_executed_replayed_contract_inconsistent");
  add_check(projection, policy.require_clear_blockers,
            projection.blockers_clear,
            "upstream_blocker_or_invalid_blockers_array");

  if (projection.required_check_count == 0U ||
      projection.satisfied_check_count > projection.required_check_count) {
    throw std::logic_error("artifact evidence contract accounting is invalid");
  }
  projection.coverage =
      static_cast<double>(projection.satisfied_check_count) /
      static_cast<double>(projection.required_check_count);
  projection.gap = 1.0 - projection.coverage;
  if (!std::isfinite(projection.coverage) ||
      !std::isfinite(projection.gap) || projection.coverage < 0.0 ||
      projection.coverage > 1.0 || projection.gap < 0.0 ||
      projection.gap > 1.0) {
    throw std::logic_error(
        "artifact evidence contract coverage is not finite or bounded");
  }
  projection.ready = projection.satisfied_check_count ==
                     projection.required_check_count;

  auto blocker = metadata_blocker_category(metadata);
  if (!projection.ready && blocker == BlockerCategory::kNone) {
    blocker = BlockerCategory::kArtifactBlocked;
  }
  projection.blocker_category = blocker_category_name(blocker);
  projection.repair_environment = repair_environment_name(
      repair_environment_kind(projection.blocker_category));
  return projection;
}

}  // namespace inferlab_evidence
