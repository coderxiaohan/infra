#pragma once

#include "evidence_query_repair_groups.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct SourceLocationWorklistFocusProjection {
  std::size_t item_count = 0;
  std::size_t current_repo_item_count = 0;
  std::size_t target_or_validation_item_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t source_location_gap_card_count = 0;
  const RepairCardGroup* first_item_group = nullptr;
  const RepairCardGroup* first_gap_group = nullptr;
  std::string first_item_source_location;
  std::string first_item_selector;
  std::string first_gap_selector;
  std::string next_action;
  bool first_item_current_repo = false;
  bool gap_has_priority = false;
};

inline SourceLocationWorklistFocusProjection
project_source_location_worklist_focus(
    const std::vector<RepairCardGroup>& groups) {
  SourceLocationWorklistFocusProjection projection;
  projection.item_count = source_location_worklist_item_count(groups);
  projection.current_repo_item_count =
      source_location_worklist_current_repo_item_count(groups);
  projection.target_or_validation_item_count =
      source_location_worklist_target_or_validation_item_count(groups);
  projection.source_location_gap_count =
      repair_card_group_source_location_gap_count(groups);
  projection.source_location_gap_card_count =
      repair_card_group_source_location_gap_card_count(groups);

  const auto ranked = ranked_repair_card_groups(groups);
  for (const RepairCardGroup* group : ranked) {
    if (group == nullptr) continue;
    if (projection.first_item_group == nullptr &&
        !group->exact_wiring_locations.empty()) {
      projection.first_item_group = group;
      projection.first_item_source_location =
          group->exact_wiring_locations.front();
    }
    if (projection.first_gap_group == nullptr &&
        group->source_location_gap_card_count > 0U) {
      projection.first_gap_group = group;
    }
    if (projection.first_item_group != nullptr &&
        projection.first_gap_group != nullptr) {
      break;
    }
  }

  if (projection.first_item_group != nullptr) {
    projection.first_item_current_repo =
        repair_environment_priority(
            projection.first_item_group->repair_environment) == 0;
    projection.first_item_selector =
        !projection.first_item_group->first_artifact.empty()
            ? projection.first_item_group->first_artifact
            : projection.first_item_group->first_category;
  }
  if (projection.first_gap_group != nullptr) {
    projection.first_gap_selector =
        !projection.first_gap_group->first_source_gap_artifact.empty()
            ? projection.first_gap_group->first_source_gap_artifact
            : projection.first_gap_group->first_source_gap_category;
  }
  projection.gap_has_priority = source_location_worklist_prioritizes_gap(
      projection.first_item_group, projection.first_gap_group);
  projection.next_action = source_location_worklist_next_action(
      projection.first_item_group, projection.first_gap_group);
  return projection;
}

}  // namespace inferlab_evidence_query
