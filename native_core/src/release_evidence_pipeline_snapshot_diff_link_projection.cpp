#include "release_evidence_pipeline_snapshot_diff_link_projection.hpp"

#include "release_evidence_pipeline_repair_group_projection.hpp"

#include <stdexcept>
#include <string>

namespace inferlab_release_pipeline {
namespace {

constexpr const char* kSnapshotDiffRepairGroupId =
    "category_evidence_query_snapshot_diff_review";

std::string legacy_snapshot_diff_release_reason(
    const inferlab_evidence::EvidenceQuerySnapshotDiffSignals& snapshot) {
  if (!snapshot.query_present) return "snapshot_diff_query_missing";
  if (snapshot.deep_semantic_decision ==
      "snapshot_diff_deep_semantic_parse_error") {
    return "snapshot_diff_parse_error";
  }
  if (snapshot.deep_semantic_truncated) return "snapshot_diff_truncated";
  if (snapshot.added_deep_path_count > 0U ||
      snapshot.removed_deep_path_count > 0U ||
      snapshot.changed_deep_value_count > 0U) {
    return "snapshot_diff_deep_path_changed";
  }
  return "snapshot_diff_no_release_review_needed";
}

}  // namespace

ReleasePipelineSnapshotDiffReleaseLinkProjection::
    ReleasePipelineSnapshotDiffReleaseLinkProjection(
        const ReleasePipelineEvidenceQueryClosureProjection& evidence_query,
        const ReleasePipelineRepairProjection& repair)
    : artifact(evidence_query.artifact),
      snapshot_diff(evidence_query.snapshot_diff),
      group_spec(require_repair_path_group_spec(kSnapshotDiffRepairGroupId)),
      direct_artifact_present(evidence_query.source_present) {
  if (group_spec.grouping != "category") {
    throw std::logic_error(
        "release pipeline snapshot-diff repair group contract drift");
  }

  const bool canonical_release_focus_reason_used =
      snapshot_diff.release_focus_present &&
      !snapshot_diff.release_focus_blocker_reason.empty();
  release_blocker_reason =
      canonical_release_focus_reason_used
          ? snapshot_diff.release_focus_blocker_reason
          : legacy_snapshot_diff_release_reason(snapshot_diff);

  const auto* group =
      active_repair_path_group_for_id(repair.groups, group_spec.id);
  if (group == nullptr) {
    link_status = snapshot_diff.query_present
                      ? "snapshot_diff_observed_without_release_blocker"
                      : "snapshot_diff_query_missing";
    recommended_action =
        "no snapshot-diff release repair path is active";
    return;
  }
  if (group->spec.grouping != group_spec.grouping ||
      group->spec.key != group_spec.key ||
      group->source_location_gap_count > group->path_count) {
    throw std::logic_error(
        "release pipeline active snapshot-diff repair group is invalid");
  }

  repair_path_count = group->path_count;
  source_location_gap_count = group->source_location_gap_count;
  evidence_counter_count = group->evidence_counter_count;
  group_plan_step = group->step;
  current_repo_action_required = group->current_repo_action_required;
  target_or_validation_action_required =
      group->target_or_validation_action_required;
  link_status = "linked_to_release_repair_path";

  const auto* first_path = first_repair_path_for_group(repair.paths, *group);
  if (first_path == nullptr || first_path->category != group_spec.key) {
    throw std::logic_error(
        "release pipeline snapshot-diff group has no valid first path");
  }
  first_repair_path_present = true;
  first_repair_path_id = first_path->id;
  if (!first_path->source_locations.empty()) {
    first_source_location = first_path->source_locations.front();
  }
  recommended_action = first_path->recommended_action;
  command = first_path->command;
}

}  // namespace inferlab_release_pipeline
