#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_source_location_worklist_projection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string source_location_worklist_focus_json(
    const SourceLocationWorklistFocusProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const RepairCardGroup* first_item_group = projection.first_item_group;
  const RepairCardGroup* first_gap_group = projection.first_gap_group;
  const bool first_item_present = first_item_group != nullptr;
  const bool first_gap_present = first_gap_group != nullptr;

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"item_count\": " << projection.item_count << ",\n";
  out << nested << "\"current_repo_item_count\": "
      << projection.current_repo_item_count << ",\n";
  out << nested << "\"target_or_validation_item_count\": "
      << projection.target_or_validation_item_count
      << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << projection.source_location_gap_count << ",\n";
  out << nested << "\"source_location_gap_card_count\": "
      << projection.source_location_gap_card_count << ",\n";
  out << nested << "\"first_item_present\": "
      << (first_item_present ? "true" : "false") << ",\n";
  out << nested << "\"first_item_repair_environment\": "
      << inferlab_plan::json_string(
             first_item_present ? first_item_group->repair_environment : "")
      << ",\n";
  out << nested << "\"first_item_artifact\": "
      << inferlab_plan::json_string(
             first_item_present ? first_item_group->first_artifact : "")
      << ",\n";
  out << nested << "\"first_item_category\": "
      << inferlab_plan::json_string(
             first_item_present ? first_item_group->first_category : "")
      << ",\n";
  out << nested << "\"first_item_action\": "
      << inferlab_plan::json_string(
             first_item_present ? first_item_group->first_action : "")
      << ",\n";
  out << nested << "\"first_item_source_location\": "
      << inferlab_plan::json_string(projection.first_item_source_location)
      << ",\n";
  out << nested << "\"first_item_current_repo_action_required\": "
      << (projection.first_item_current_repo ? "true" : "false") << ",\n";
  out << nested << "\"first_item_target_or_validation_action_required\": "
      << (first_item_present && !projection.first_item_current_repo ? "true"
                                                                    : "false")
      << ",\n";
  out << nested << "\"first_item_suggest_repair_command\": "
      << inferlab_plan::json_string(
             projection.first_item_selector.empty()
                 ? "inferlab_evidence_query --operation suggest_repair"
                 : "inferlab_evidence_query --operation suggest_repair --selector " +
                       projection.first_item_selector)
      << ",\n";
  out << nested << "\"first_gap_present\": "
      << (first_gap_present ? "true" : "false") << ",\n";
  out << nested << "\"first_gap_repair_environment\": "
      << inferlab_plan::json_string(
             first_gap_present ? first_gap_group->repair_environment : "")
      << ",\n";
  out << nested << "\"first_gap_artifact\": "
      << inferlab_plan::json_string(
             first_gap_present
                 ? first_gap_group->first_source_gap_artifact
                 : "")
      << ",\n";
  out << nested << "\"first_gap_category\": "
      << inferlab_plan::json_string(
             first_gap_present
                 ? first_gap_group->first_source_gap_category
                 : "")
      << ",\n";
  out << nested << "\"first_gap_action\": "
      << inferlab_plan::json_string(
             first_gap_present ? first_gap_group->first_source_gap_action : "")
      << ",\n";
  out << nested << "\"first_gap_explain_blocker_command\": "
      << inferlab_plan::json_string(
             projection.first_gap_selector.empty()
                 ? "inferlab_evidence_query --operation explain_blocker"
                 : "inferlab_evidence_query --operation explain_blocker --selector " +
                       projection.first_gap_selector)
      << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(projection.next_action)
      << ",\n";
  out << nested << "\"release_gate_key\": "
      << inferlab_plan::json_string(
             projection.gap_has_priority
                 ? "source_location_metadata_gap"
                 : (projection.first_item_current_repo
                        ? "current_repo_source_location_repair"
                        : (first_item_present
                               ? "target_or_validation_source_location_repair"
                               : "source_location_metadata_gap")))
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"source-location worklist focus is read-only triage metadata for release-gate explain/repair UX; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string source_location_repair_worklist_json(
    const std::vector<RepairCardGroup>& groups,
    const SourceLocationWorklistFocusProjection& focus,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  bool first_item = true;
  bool first_gap = true;
  std::size_t item_id = 1U;
  std::size_t gap_id = 1U;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"item_count\": " << focus.item_count << ",\n";
  out << nested << "\"current_repo_item_count\": "
      << focus.current_repo_item_count << ",\n";
  out << nested << "\"target_or_validation_item_count\": "
      << focus.target_or_validation_item_count
      << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << focus.source_location_gap_count << ",\n";
  out << nested << "\"source_location_worklist_focus\":\n"
      << source_location_worklist_focus_json(focus, indent + 2) << ",\n";
  out << nested << "\"items\": [";
  for (const auto& group : groups) {
    const std::string selector =
        !group.first_artifact.empty() ? group.first_artifact
                                      : group.first_category;
    for (const auto& location : group.exact_wiring_locations) {
      if (!first_item) out << ",";
      first_item = false;
      out << "\n" << nested << "  {\n";
      out << nested << "    \"id\": "
          << inferlab_plan::json_string(
                 "source_location_repair_" + std::to_string(item_id))
          << ",\n";
      item_id = inferlab::internal::checked_add_size(
          item_id, 1U, "source-location worklist item id");
      out << nested << "    \"repair_environment\": "
          << inferlab_plan::json_string(group.repair_environment) << ",\n";
      out << nested << "    \"artifact\": "
          << inferlab_plan::json_string(group.first_artifact) << ",\n";
      out << nested << "    \"category\": "
          << inferlab_plan::json_string(group.first_category) << ",\n";
      out << nested << "    \"action\": "
          << inferlab_plan::json_string(group.first_action) << ",\n";
      out << nested << "    \"source_location\": "
          << inferlab_plan::json_string(location) << ",\n";
      out << nested << "    \"affected_claims\": "
          << inferlab_plan::json_string_array(group.affected_claims,
                                              indent + 2)
          << ",\n";
      const bool current_repo =
          repair_environment_priority(group.repair_environment) == 0;
      out << nested << "    \"current_repo_action_required\": "
          << (current_repo ? "true" : "false") << ",\n";
      out << nested << "    \"target_or_validation_action_required\": "
          << (current_repo ? "false" : "true") << ",\n";
      out << nested << "    \"show_artifact_command\": "
          << inferlab_plan::json_string(
                 selector.empty()
                     ? "inferlab_evidence_query --operation list_artifacts"
                     : "inferlab_evidence_query --operation show_artifact --selector " +
                           selector)
          << ",\n";
      out << nested << "    \"suggest_repair_command\": "
          << inferlab_plan::json_string(
                 selector.empty()
                     ? "inferlab_evidence_query --operation suggest_repair"
                     : "inferlab_evidence_query --operation suggest_repair --selector " +
                           selector)
          << ",\n";
      out << nested
          << "    \"claim_boundary\": \"source-location worklist item is read-only operator guidance; it identifies where to inspect wiring or schema gaps and does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
      out << nested << "  }";
    }
  }
  if (!first_item) out << "\n" << nested;
  out << "],\n";
  out << nested << "\"gaps\": [";
  for (const auto& group : groups) {
    if (group.source_location_gap_card_count == 0U) continue;
    const std::string selector =
        !group.first_source_gap_artifact.empty()
            ? group.first_source_gap_artifact
            : group.first_source_gap_category;
    if (!first_gap) out << ",";
    first_gap = false;
    out << "\n" << nested << "  {\n";
    out << nested << "    \"id\": "
        << inferlab_plan::json_string(
               "source_location_gap_" + std::to_string(gap_id))
        << ",\n";
    gap_id = inferlab::internal::checked_add_size(
        gap_id, 1U, "source-location worklist gap id");
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(group.repair_environment) << ",\n";
    out << nested << "    \"artifact\": "
        << inferlab_plan::json_string(group.first_source_gap_artifact)
        << ",\n";
    out << nested << "    \"category\": "
        << inferlab_plan::json_string(group.first_source_gap_category)
        << ",\n";
    out << nested << "    \"action\": "
        << inferlab_plan::json_string(group.first_source_gap_action)
        << ",\n";
    out << nested << "    \"source_location_gap\": true,\n";
    out << nested << "    \"source_location_gap_card_count\": "
        << group.source_location_gap_card_count << ",\n";
    out << nested << "    \"next_action\": "
        << inferlab_plan::json_string(
               selector.empty()
                   ? "add a category-to-wiring-location mapping before claiming this repair is current-repo actionable"
                   : "inspect blocker with inferlab_evidence_query --operation explain_blocker --selector " +
                         selector)
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"source-location gap is a metadata gap only; it does not prove a code defect or benchmark behavior\"\n";
    out << nested << "  }";
  }
  if (!first_gap) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"source-location repair worklist is read-only triage metadata derived from repair-card groups; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
