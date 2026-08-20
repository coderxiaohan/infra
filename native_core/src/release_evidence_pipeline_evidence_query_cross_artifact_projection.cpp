#include "release_evidence_pipeline_evidence_query_cross_artifact_projection.hpp"

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

using inferlab_evidence_query::CrossArtifactBlockerFamily;

void add_contract_issue(std::size_t& count, const char* label) {
  count = inferlab::internal::checked_add_size(count, 1U, label);
}

bool required_unsigned(const std::string& source, const char* key,
                       std::size_t& value, std::size_t& issues,
                       const char* label) {
  const auto field = inferlab_artifact::project_unsigned_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issues, label);
    return false;
  }
  value = field.value;
  return true;
}

bool required_bool(const std::string& source, const char* key, bool& value,
                   std::size_t& issues, const char* label) {
  const auto field = inferlab_artifact::project_bool_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issues, label);
    return false;
  }
  value = field.value;
  return true;
}

bool required_string(const std::string& source, const char* key,
                     std::string& value, std::size_t& issues,
                     const char* label) {
  const auto field = inferlab_artifact::project_string_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issues, label);
    return false;
  }
  value = field.value;
  return true;
}

bool required_string_array(const std::string& source, const char* key,
                           std::vector<std::string>& values,
                           std::size_t& issues, const char* label) {
  if (!inferlab_artifact::contains_top_level_key_fallback(source, key)) {
    add_contract_issue(issues, label);
    return false;
  }
  const std::string section =
      inferlab_artifact::json_array_section_or_empty(source, key);
  if (section.empty() ||
      !inferlab_artifact::parse_string_array_items_fallback(section, values)) {
    add_contract_issue(issues, label);
    return false;
  }
  return true;
}

void expect_unsigned(const std::string& source, const char* key,
                     const std::size_t expected, std::size_t& issues,
                     const char* label) {
  std::size_t actual = 0U;
  if (required_unsigned(source, key, actual, issues, label) &&
      actual != expected) {
    add_contract_issue(issues, label);
  }
}

void expect_bool(const std::string& source, const char* key,
                 const bool expected, std::size_t& issues,
                 const char* label) {
  bool actual = false;
  if (required_bool(source, key, actual, issues, label) &&
      actual != expected) {
    add_contract_issue(issues, label);
  }
}

void expect_string(const std::string& source, const char* key,
                   const std::string& expected, std::size_t& issues,
                   const char* label) {
  std::string actual;
  if (required_string(source, key, actual, issues, label) &&
      actual != expected) {
    add_contract_issue(issues, label);
  }
}

bool parse_cross_artifact_family(const std::string& object,
                                 CrossArtifactBlockerFamily& family,
                                 std::size_t& issues) {
  const std::size_t before = issues;
  required_string(object, "family", family.family, issues,
                  "Evidence Query cross-artifact family identity issue");
  required_string(object, "source_artifact", family.source_artifact, issues,
                  "Evidence Query cross-artifact source issue");
  required_string_array(
      object, "source_artifacts", family.source_artifacts, issues,
      "Evidence Query cross-artifact source-array issue");
  required_string(object, "blocker_reason", family.blocker_reason, issues,
                  "Evidence Query cross-artifact blocker-reason issue");
  required_string(object, "primary_environment", family.primary_environment,
                  issues,
                  "Evidence Query cross-artifact repair-environment issue");
  required_string(object, "next_action", family.next_action, issues,
                  "Evidence Query cross-artifact next-action issue");
  required_unsigned(object, "blocker_count", family.blocker_count, issues,
                    "Evidence Query cross-artifact blocker-count issue");
  required_unsigned(
      object, "semantic_linkage_blocker_count",
      family.semantic_linkage_blocker_count, issues,
      "Evidence Query cross-artifact semantic-linkage issue");
  required_unsigned(
      object, "replay_or_execute_missing_count",
      family.replay_or_execute_missing_count, issues,
      "Evidence Query cross-artifact replay-or-execute issue");
  required_unsigned(
      object, "current_repo_action_count", family.current_repo_action_count,
      issues, "Evidence Query cross-artifact current-repo issue");
  required_unsigned(object, "target_or_validation_action_count",
                    family.target_or_validation_action_count, issues,
                    "Evidence Query cross-artifact target-action issue");
  required_unsigned(object, "target_product_replay_action_count",
                    family.target_product_replay_action_count, issues,
                    "Evidence Query cross-artifact product-action issue");
  required_unsigned(object, "target_gpu_execute_action_count",
                    family.target_gpu_execute_action_count, issues,
                    "Evidence Query cross-artifact execute-action issue");
  required_unsigned(object, "source_location_gap_count",
                    family.source_location_gap_count, issues,
                    "Evidence Query cross-artifact source-location issue");
  required_bool(object, "present", family.present, issues,
                "Evidence Query cross-artifact presence issue");
  required_bool(object, "readable", family.readable, issues,
                "Evidence Query cross-artifact readability issue");

  if (issues != before) return false;
  expect_unsigned(
      object, "source_artifact_count",
      inferlab_evidence_query::cross_artifact_source_artifacts(family).size(),
      issues, "Evidence Query cross-artifact source-count drift");
  expect_string(object, "status",
                inferlab_evidence_query::cross_artifact_family_status(family),
                issues, "Evidence Query cross-artifact status drift");
  return issues == before;
}

bool duplicate_family(
    const std::vector<CrossArtifactBlockerFamily>& families,
    const std::string& family) {
  for (const auto& existing : families) {
    if (existing.family == family) return true;
  }
  return false;
}

void derive_cross_artifact_projection(
    EvidenceQueryCrossArtifactClosureProjection& projection) {
  using namespace inferlab_evidence_query;
  auto& derived = projection.derived;
  const auto& families = projection.families;
  derived.active_family_count = cross_artifact_active_family_count(families);
  derived.total_blocker_count = cross_artifact_blocker_total(families);
  derived.current_repo_action_count =
      cross_artifact_current_repo_action_total(families);
  derived.target_or_validation_action_count =
      cross_artifact_target_or_validation_action_total(families);
  derived.source_location_gap_count =
      cross_artifact_source_location_gap_total(families);
  const auto* primary = primary_cross_artifact_family(families);
  if (primary != nullptr) {
    derived.primary_family = primary->family;
    derived.primary_blocker_reason = primary->blocker_reason;
    derived.primary_environment = primary->primary_environment;
    derived.primary_action = primary->next_action;
  }
  derived.affected_claims = cross_artifact_affected_claims(families);
  derived.repair_paths = ranked_cross_artifact_repair_paths(families);
  derived.current_repo_actionable =
      derived.current_repo_action_count > 0U;
  derived.target_or_validation_required =
      derived.target_or_validation_action_count > 0U;
  derived.source_location_metadata_required =
      derived.source_location_gap_count > 0U;
  const auto* product =
      find_cross_artifact_family(families, "product_replay_or_execute");
  if (product != nullptr) {
    derived.product_replay_family_present = true;
    derived.product_replay_family_status =
        cross_artifact_family_status(*product);
    derived.product_replay_family = *product;
    derived.product_replay_source_artifacts =
        cross_artifact_source_artifacts(*product);
  }
  projection.status =
      cross_artifact_blocker_explanation_status(families);
  projection.release_focus_decision =
      cross_artifact_release_focus_decision(families);
  projection.release_focus_repair_lane =
      cross_artifact_release_focus_repair_lane(families);
}

void validate_cross_artifact_summary(
    const std::string& source,
    EvidenceQueryCrossArtifactClosureProjection& projection) {
  const auto& derived = projection.derived;
  const auto& paths = derived.repair_paths;
  const auto* first = paths.empty() ? nullptr : &paths.front();
  auto& issues = projection.contract_issue_count;

  expect_bool(source, "present", true, issues,
              "Evidence Query cross-artifact object presence drift");
  expect_string(source, "status", projection.status, issues,
                "Evidence Query cross-artifact object status drift");
  expect_unsigned(source, "family_count", projection.families.size(), issues,
                  "Evidence Query cross-artifact family-count drift");
  expect_unsigned(source, "active_family_count", derived.active_family_count,
                  issues,
                  "Evidence Query cross-artifact active-count drift");
  expect_unsigned(source, "total_blocker_count", derived.total_blocker_count,
                  issues,
                  "Evidence Query cross-artifact blocker-total drift");
  expect_unsigned(source, "current_repo_action_count",
                  derived.current_repo_action_count, issues,
                  "Evidence Query cross-artifact repo-total drift");
  expect_unsigned(source, "target_or_validation_action_count",
                  derived.target_or_validation_action_count, issues,
                  "Evidence Query cross-artifact target-total drift");
  expect_unsigned(source, "source_location_gap_count",
                  derived.source_location_gap_count, issues,
                  "Evidence Query cross-artifact source-gap drift");
  expect_string(source, "primary_family", derived.primary_family, issues,
                "Evidence Query cross-artifact primary-family drift");
  expect_string(source, "primary_blocker_reason",
                derived.primary_blocker_reason, issues,
                "Evidence Query cross-artifact primary-blocker drift");
  expect_string(source, "primary_environment", derived.primary_environment,
                issues,
                "Evidence Query cross-artifact primary-environment drift");
  expect_string(source, "primary_next_action", derived.primary_action, issues,
                "Evidence Query cross-artifact primary-action drift");

  const std::string focus =
      inferlab_artifact::json_object_section_or_empty(source, "release_focus");
  projection.release_focus_present = !focus.empty();
  if (!projection.release_focus_present) {
    add_contract_issue(issues,
                       "Evidence Query cross-artifact release-focus issue");
    return;
  }
  expect_bool(focus, "present", true, issues,
              "Evidence Query cross-artifact focus-presence drift");
  expect_string(focus, "decision", projection.release_focus_decision, issues,
                "Evidence Query cross-artifact focus-decision drift");
  expect_string(focus, "repair_lane", projection.release_focus_repair_lane,
                issues, "Evidence Query cross-artifact focus-lane drift");
  expect_unsigned(focus, "affected_claim_count",
                  derived.affected_claims.size(), issues,
                  "Evidence Query cross-artifact claim-count drift");
  expect_unsigned(focus, "repair_path_count", paths.size(), issues,
                  "Evidence Query cross-artifact path-count drift");
  expect_string(focus, "first_repair_family",
                first == nullptr ? "" : first->family, issues,
                "Evidence Query cross-artifact first-family drift");
  expect_string(focus, "first_repair_environment",
                first == nullptr ? "" : first->primary_environment, issues,
                "Evidence Query cross-artifact first-environment drift");
  expect_string(focus, "first_repair_action",
                first == nullptr ? "" : first->next_action, issues,
                "Evidence Query cross-artifact first-action drift");
  expect_bool(focus, "current_repo_actionable",
              derived.current_repo_actionable, issues,
              "Evidence Query cross-artifact repo-action drift");
  expect_bool(focus, "target_or_validation_required",
              derived.target_or_validation_required, issues,
              "Evidence Query cross-artifact target-action drift");
  expect_bool(focus, "source_location_metadata_required",
              derived.source_location_metadata_required, issues,
              "Evidence Query cross-artifact source-metadata drift");
}

}  // namespace

EvidenceQueryCrossArtifactClosureProjection
project_evidence_query_cross_artifact_closure(
    const std::string& evidence_query_document, const bool summary_usable,
    const bool source_present) {
  EvidenceQueryCrossArtifactClosureProjection projection;
  if (!source_present) return projection;
  if (!summary_usable) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  const std::string source = inferlab_artifact::json_object_section_or_empty(
      evidence_query_document, "cross_artifact_blocker_explanation");
  projection.object_present = !source.empty();
  if (!projection.object_present) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  const std::string family_section =
      inferlab_artifact::json_array_section_or_empty(source, "families");
  const auto values = inferlab_artifact::top_level_json_values(family_section);
  const auto objects = inferlab_artifact::top_level_json_objects(family_section);
  if (family_section.empty() || values.empty() ||
      values.size() != objects.size()) {
    add_contract_issue(
        projection.contract_issue_count,
        "Evidence Query cross-artifact family-array contract issue");
  } else {
    projection.families.reserve(objects.size());
    for (const auto& object : objects) {
      CrossArtifactBlockerFamily family;
      if (!parse_cross_artifact_family(
              object, family, projection.contract_issue_count)) {
        continue;
      }
      if (family.family.empty() ||
          duplicate_family(projection.families, family.family)) {
        add_contract_issue(
            projection.contract_issue_count,
            "Evidence Query cross-artifact duplicate family issue");
        continue;
      }
      projection.families.push_back(std::move(family));
    }
  }
  derive_cross_artifact_projection(projection);
  validate_cross_artifact_summary(source, projection);
  if (projection.contract_issue_count > 0U) {
    projection.status = "contract_invalid";
    projection.release_focus_decision = "contract_invalid";
    projection.release_focus_repair_lane = "manual_review_required";
  }
  return projection;
}

EvidenceQueryProductReplayClosureProjection
project_evidence_query_product_replay_closure(
    const std::string& evidence_query_document, const bool summary_usable,
    const bool source_present) {
  EvidenceQueryProductReplayClosureProjection projection;
  if (!source_present) return projection;
  if (!summary_usable) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  const std::string source = inferlab_artifact::json_object_section_or_empty(
      evidence_query_document, "product_replay_semantic_linkage");
  projection.object_present = !source.empty();
  if (!projection.object_present) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  auto& signals = projection.signals;
  auto& issues = projection.contract_issue_count;
  bool declared_present = false;
  if (required_bool(source, "present", declared_present, issues,
                    "Evidence Query Product Replay object presence issue") &&
      !declared_present) {
    add_contract_issue(
        issues, "Evidence Query Product Replay object presence drift");
  }
  required_string(source, "status", signals.status, issues,
                  "Evidence Query Product Replay status issue");
  required_bool(source, "semantic_events_present",
                signals.semantic_events_present, issues,
                "Evidence Query Product Replay semantic presence issue");
  required_bool(source, "semantic_events_readable",
                signals.semantic_events_readable, issues,
                "Evidence Query Product Replay semantic readability issue");
  required_string(source, "semantic_events_read_status",
                  signals.semantic_events_read_status, issues,
                  "Evidence Query Product Replay semantic status issue");
  required_bool(source, "alignment_present", signals.alignment_present,
                issues, "Evidence Query Product Replay alignment presence issue");
  required_bool(source, "alignment_readable", signals.alignment_readable,
                issues,
                "Evidence Query Product Replay alignment readability issue");
  required_string(source, "alignment_read_status",
                  signals.alignment_read_status, issues,
                  "Evidence Query Product Replay alignment status issue");
  required_unsigned(source, "read_issue_count", signals.read_issue_count,
                    issues, "Evidence Query Product Replay read issue");
  required_unsigned(source, "available_event_count",
                    signals.available_event_count, issues,
                    "Evidence Query Product Replay available-event issue");
  required_unsigned(source, "pending_event_count", signals.pending_event_count,
                    issues,
                    "Evidence Query Product Replay pending-event issue");
  required_unsigned(source, "sample_event_count", signals.sample_event_count,
                    issues, "Evidence Query Product Replay sample-event issue");
  required_bool(source, "sample_replay_present", signals.sample_replay_present,
                issues, "Evidence Query Product Replay sample presence issue");
  required_unsigned(source, "replay_supported_event_count",
                    signals.replay_supported_event_count, issues,
                    "Evidence Query Product Replay supported-event issue");
  required_unsigned(source, "alignment_rule_count",
                    signals.alignment_rule_count, issues,
                    "Evidence Query Product Replay rule-count issue");
  required_unsigned(source, "aligned_rule_count", signals.aligned_rule_count,
                    issues, "Evidence Query Product Replay aligned-rule issue");
  required_unsigned(source, "pending_rule_count", signals.pending_rule_count,
                    issues, "Evidence Query Product Replay pending-rule issue");
  required_unsigned(source, "correlation_count", signals.correlation_count,
                    issues, "Evidence Query Product Replay correlation issue");
  required_unsigned(source, "ready_correlation_count",
                    signals.ready_correlation_count, issues,
                    "Evidence Query Product Replay ready-correlation issue");
  required_unsigned(source, "blocked_correlation_count",
                    signals.blocked_correlation_count, issues,
                    "Evidence Query Product Replay blocked-correlation issue");
  required_unsigned(source, "blocker_count", signals.blocker_count, issues,
                    "Evidence Query Product Replay blocker-count issue");
  required_unsigned(source, "target_or_validation_action_count",
                    signals.target_or_validation_action_count, issues,
                    "Evidence Query Product Replay target-action issue");
  required_string(source, "first_blocker", signals.first_blocker, issues,
                  "Evidence Query Product Replay first-blocker issue");
  required_string(source, "next_action", signals.next_action, issues,
                  "Evidence Query Product Replay next-action issue");
  if (issues > 0U) {
    signals.status = "contract_invalid";
    signals.first_blocker =
        "Evidence Query product replay semantic linkage contract is invalid";
    signals.next_action =
        "refresh or repair the Evidence Query Product Replay linkage object";
  }
  return projection;
}

}  // namespace inferlab_release_pipeline
