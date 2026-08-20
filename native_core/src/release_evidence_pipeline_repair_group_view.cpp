#include "release_evidence_pipeline_repair_group_view.hpp"

#include "evidence_json_writer.hpp"
#include "release_evidence_pipeline_repair_group_projection.hpp"

#include <string>
#include <utility>
#include <vector>

namespace inferlab_release_pipeline {

std::string repair_path_groups_json(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::vector<ActiveRepairPathGroup>& groups,
    const int indent) {
  std::vector<inferlab_evidence::EvidenceJsonObject> objects;
  objects.reserve(groups.size());
  for (const auto& group : groups) {
    const auto* first_path = first_repair_path_for_group(paths, group);
    inferlab_evidence::EvidenceJsonObject object;
    auto& fields = object.fields;
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "id", group.spec.id));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "grouping", group.spec.grouping));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "key", group.spec.key));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "label", group.spec.label));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "priority", group.spec.priority));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "path_count", group.path_count));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "source_location_gap_count", group.source_location_gap_count));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "evidence_counter_count", group.evidence_counter_count));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field("first_category")
            : inferlab_evidence::evidence_json_string_field(
                  "first_category", first_path->category));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field("first_source")
            : inferlab_evidence::evidence_json_string_field(
                  "first_source", first_path->source));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field(
                  "first_repair_environment")
            : inferlab_evidence::evidence_json_string_field(
                  "first_repair_environment",
                  inferlab_evidence::repair_environment_name(
                      first_path->repair_environment)));
    fields.push_back(
        first_path == nullptr || first_path->source_locations.empty()
            ? inferlab_evidence::evidence_json_null_field(
                  "first_source_location")
            : inferlab_evidence::evidence_json_string_field(
                  "first_source_location",
                  first_path->source_locations.front()));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field(
                  "first_recommended_action")
            : inferlab_evidence::evidence_json_string_field(
                  "first_recommended_action",
                  first_path->recommended_action));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field("first_command")
            : inferlab_evidence::evidence_json_string_field(
                  "first_command", first_path->command));
    fields.push_back(inferlab_evidence::evidence_json_boolean_field(
        "current_repo_action_required",
        group.current_repo_action_required));
    fields.push_back(inferlab_evidence::evidence_json_boolean_field(
        "target_or_validation_action_required",
        group.target_or_validation_action_required));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "claim_boundary", group.spec.claim_boundary));
    objects.push_back(std::move(object));
  }
  return inferlab_evidence::render_evidence_json_objects(
      objects, indent, "repair-path group");
}

std::string repair_path_group_plan_json(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::vector<ActiveRepairPathGroup>& groups,
    const int indent) {
  std::vector<inferlab_evidence::EvidenceJsonObject> objects;
  objects.reserve(groups.size());
  for (const auto& group : groups) {
    const auto* first_path = first_repair_path_for_group(paths, group);
    inferlab_evidence::EvidenceJsonObject object;
    auto& fields = object.fields;
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "step", group.step));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "group_id", group.spec.id));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "priority", group.spec.priority));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "action_owner", group.action_owner));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "path_count", group.path_count));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "source_location_gap_count", group.source_location_gap_count));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "evidence_counter_count", group.evidence_counter_count));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field(
                  "recommended_action")
            : inferlab_evidence::evidence_json_string_field(
                  "recommended_action", first_path->recommended_action));
    fields.push_back(
        first_path == nullptr
            ? inferlab_evidence::evidence_json_null_field("command")
            : inferlab_evidence::evidence_json_string_field(
                  "command", first_path->command));
    fields.push_back(inferlab_evidence::evidence_json_boolean_field(
        "current_repo_action_required",
        group.current_repo_action_required));
    fields.push_back(inferlab_evidence::evidence_json_boolean_field(
        "target_or_validation_action_required",
        group.target_or_validation_action_required));
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "claim_boundary",
        "repair path group plan orders existing review metadata only; it does not execute commands, refresh artifacts, or prove readiness"));
    objects.push_back(std::move(object));
  }
  return inferlab_evidence::render_evidence_json_objects(
      objects, indent, "repair-path group plan");
}

}  // namespace inferlab_release_pipeline
