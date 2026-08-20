#pragma once

#include "checked_index.hpp"
#include "evidence_query_selection.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct RepairPathSummary {
  std::string target;
  std::string state;
  std::size_t blocking_repair_card_count = 0;
  std::size_t catalog_flow_discrepancy_count = 0;
  std::size_t catalog_flow_surface_consumer_fallback_removal_blocked_count = 0;
  std::size_t catalog_flow_surface_consumer_parity_refresh_required_count = 0;
  std::size_t catalog_flow_surface_consumer_current_repo_action_count = 0;
  std::size_t catalog_flow_surface_consumer_target_machine_action_count = 0;
  std::size_t catalog_flow_consumer_field_coverage_repair_card_count = 0;
  std::size_t catalog_flow_consumer_field_coverage_field_gap_count = 0;
  std::size_t catalog_flow_consumer_field_coverage_parity_refresh_required_count = 0;
  std::vector<std::string> steps;
};

struct RepairCardGroup {
  std::string repair_environment;
  std::size_t repair_card_count = 0;
  std::size_t source_exact_card_count = 0;
  std::size_t source_location_gap_card_count = 0;
  std::vector<std::string> artifacts;
  std::vector<std::string> categories;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::string first_artifact;
  std::string first_category;
  std::string first_action;
  std::string first_source_gap_artifact;
  std::string first_source_gap_category;
  std::string first_source_gap_action;
};

inline bool repair_card_record_matches(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  const bool selector_is_category =
      is_known_blocker_category(operation.selector);
  if (!selector_is_category && !operation.selector.empty() &&
      record.artifact != operation.selector &&
      !inferlab_plan::contains(record.artifact, operation.selector)) {
    return false;
  }
  if (!artifact_filter.empty() && record.artifact != artifact_filter &&
      !inferlab_plan::contains(record.artifact, artifact_filter)) {
    return false;
  }
  const std::string category = blocker_category(record);
  if (category == "none") return false;
  if (selector_is_category && category != operation.selector) return false;
  if (!blocker_filter.empty() && category != blocker_filter) return false;
  return true;
}

namespace detail {

inline RepairCardGroup& find_or_append_repair_card_group(
    std::vector<RepairCardGroup>& groups,
    const std::string& environment) {
  for (auto& group : groups) {
    if (group.repair_environment == environment) return group;
  }
  groups.push_back(RepairCardGroup{});
  groups.back().repair_environment = environment;
  return groups.back();
}

inline void increment_count(std::size_t& value, const char* name) {
  value = inferlab::internal::checked_add_size(value, 1U, name);
}

inline inferlab_evidence::RepairEnvironment repair_group_environment_kind(
    const RepairCardGroup& group) {
  return inferlab_evidence::parse_repair_environment(
      group.repair_environment);
}

}  // namespace detail

inline std::vector<RepairCardGroup> repair_card_groups(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  std::vector<RepairCardGroup> groups;
  for (const auto& record : records) {
    if (!repair_card_record_matches(record, operation, artifact_filter,
                                    blocker_filter)) {
      continue;
    }
    const std::string category = blocker_category(record);
    RepairCardGroup& group = detail::find_or_append_repair_card_group(
        groups, repair_environment(record));
    detail::increment_count(group.repair_card_count,
                            "repair-card group card count");
    if (group.first_artifact.empty()) group.first_artifact = record.artifact;
    if (group.first_category.empty()) group.first_category = category;
    if (group.first_action.empty()) group.first_action = repair_action(category);
    inferlab_plan::append_unique_nonempty_string(group.artifacts,
                                                  record.artifact);
    inferlab_plan::append_unique_nonempty_string(group.categories, category);
    append_affected_claims(group.affected_claims, record);

    const auto locations = wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    if (source_exact) {
      detail::increment_count(group.source_exact_card_count,
                              "repair-card group source-exact card count");
      for (const auto& location : locations) {
        inferlab_plan::append_unique_nonempty_string(
            group.exact_wiring_locations, location);
      }
    } else {
      detail::increment_count(
          group.source_location_gap_card_count,
          "repair-card group source-location gap card count");
      if (group.first_source_gap_artifact.empty()) {
        group.first_source_gap_artifact = record.artifact;
      }
      if (group.first_source_gap_category.empty()) {
        group.first_source_gap_category = category;
      }
      if (group.first_source_gap_action.empty()) {
        group.first_source_gap_action = repair_action(category);
      }
    }
  }
  return groups;
}

inline std::size_t repair_card_group_card_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    count = inferlab::internal::checked_add_size(
        count, group.repair_card_count, "repair-card group card count");
  }
  return count;
}

inline std::size_t repair_card_group_environment_count(
    const std::vector<RepairCardGroup>& groups,
    const std::string& environment) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (group.repair_environment == environment) {
      detail::increment_count(count, "repair-card environment group count");
    }
  }
  return count;
}

inline std::size_t repair_card_group_source_location_gap_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (group.source_location_gap_card_count > 0U) {
      detail::increment_count(count,
                              "repair-card source-location gap group count");
    }
  }
  return count;
}

inline std::size_t repair_card_group_source_location_gap_card_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    count = inferlab::internal::checked_add_size(
        count, group.source_location_gap_card_count,
        "repair-card source-location gap card count");
  }
  return count;
}

inline std::size_t repair_card_group_exact_wiring_location_count(
    const std::vector<RepairCardGroup>& groups) {
  std::vector<std::string> locations;
  for (const auto& group : groups) {
    for (const auto& location : group.exact_wiring_locations) {
      inferlab_plan::append_unique_nonempty_string(locations, location);
    }
  }
  return locations.size();
}

inline std::size_t repair_card_group_affected_claim_count(
    const std::vector<RepairCardGroup>& groups) {
  std::vector<std::string> claims;
  for (const auto& group : groups) {
    for (const auto& claim : group.affected_claims) {
      inferlab_plan::append_unique_nonempty_string(claims, claim);
    }
  }
  return claims.size();
}

inline int repair_environment_priority(const std::string& environment) {
  return inferlab_evidence::repair_environment_priority(
      inferlab_evidence::parse_repair_environment(environment));
}

namespace detail {

inline bool repair_card_group_precedes(const RepairCardGroup& lhs,
                                       const RepairCardGroup& rhs) {
  const int lhs_priority =
      repair_environment_priority(lhs.repair_environment);
  const int rhs_priority =
      repair_environment_priority(rhs.repair_environment);
  if (lhs_priority != rhs_priority) return lhs_priority < rhs_priority;
  const bool lhs_source_complete =
      lhs.source_location_gap_card_count == 0U &&
      !lhs.exact_wiring_locations.empty();
  const bool rhs_source_complete =
      rhs.source_location_gap_card_count == 0U &&
      !rhs.exact_wiring_locations.empty();
  if (lhs_source_complete != rhs_source_complete) {
    return lhs_source_complete;
  }
  const bool lhs_has_location = !lhs.exact_wiring_locations.empty();
  const bool rhs_has_location = !rhs.exact_wiring_locations.empty();
  if (lhs_has_location != rhs_has_location) return lhs_has_location;
  if (lhs.repair_card_count != rhs.repair_card_count) {
    return lhs.repair_card_count > rhs.repair_card_count;
  }
  if (lhs.first_category != rhs.first_category) {
    return lhs.first_category < rhs.first_category;
  }
  if (lhs.first_artifact != rhs.first_artifact) {
    return lhs.first_artifact < rhs.first_artifact;
  }
  return lhs.repair_environment < rhs.repair_environment;
}

}  // namespace detail

inline const RepairCardGroup* primary_repair_card_group(
    const std::vector<RepairCardGroup>& groups) {
  const RepairCardGroup* selected = nullptr;
  for (const auto& group : groups) {
    if (selected == nullptr) {
      selected = &group;
      continue;
    }
    if (detail::repair_card_group_precedes(group, *selected)) {
      selected = &group;
    }
  }
  return selected;
}

inline std::size_t repair_group_navigation_current_repo_step_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_environment_priority(group.repair_environment) == 0) {
      detail::increment_count(count, "current-repo repair-group step count");
    }
  }
  return count;
}

inline std::size_t repair_group_navigation_target_or_validation_step_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_environment_priority(group.repair_environment) > 0) {
      detail::increment_count(
          count, "repair-group target-or-validation step count");
    }
  }
  return count;
}

inline std::size_t source_location_worklist_item_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    count = inferlab::internal::checked_add_size(
        count, group.exact_wiring_locations.size(),
        "source-location worklist item count");
  }
  return count;
}

inline std::size_t source_location_worklist_current_repo_item_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_environment_priority(group.repair_environment) == 0) {
      count = inferlab::internal::checked_add_size(
          count, group.exact_wiring_locations.size(),
          "current-repo source-location worklist item count");
    }
  }
  return count;
}

inline std::size_t source_location_worklist_target_or_validation_item_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_environment_priority(group.repair_environment) > 0) {
      count = inferlab::internal::checked_add_size(
          count, group.exact_wiring_locations.size(),
          "target source-location worklist item count");
    }
  }
  return count;
}

inline int repair_card_path_priority_score(const RepairCardGroup& group) {
  const int environment_score =
      (5 - repair_environment_priority(group.repair_environment)) * 1000;
  const bool source_location_complete =
      group.source_location_gap_card_count == 0U &&
      !group.exact_wiring_locations.empty();
  const int source_location_score = source_location_complete ? 100 : 0;
  const int card_score =
      group.repair_card_count > 99U
          ? 99
          : static_cast<int>(group.repair_card_count);
  return environment_score + source_location_score + card_score;
}

inline std::vector<const RepairCardGroup*> ranked_repair_card_groups(
    const std::vector<RepairCardGroup>& groups) {
  std::vector<const RepairCardGroup*> ranked;
  ranked.reserve(groups.size());
  for (const auto& group : groups) ranked.push_back(&group);
  std::sort(ranked.begin(), ranked.end(),
            [](const RepairCardGroup* lhs, const RepairCardGroup* rhs) {
              if (lhs == nullptr || rhs == nullptr) return lhs != nullptr;
              return detail::repair_card_group_precedes(*lhs, *rhs);
            });
  return ranked;
}

inline bool source_location_worklist_prioritizes_gap(
    const RepairCardGroup* first_item_group,
    const RepairCardGroup* first_gap_group) {
  if (first_gap_group == nullptr) return false;
  if (first_item_group == nullptr) return true;
  return repair_environment_priority(first_gap_group->repair_environment) <=
         repair_environment_priority(first_item_group->repair_environment);
}

inline std::string source_location_worklist_next_action(
    const RepairCardGroup* first_item_group,
    const RepairCardGroup* first_gap_group) {
  if (source_location_worklist_prioritizes_gap(first_item_group,
                                                first_gap_group)) {
    return "add exact wiring metadata for the first source-location gap before claiming this repair is current-repo actionable";
  }
  if (first_item_group != nullptr) {
    return repair_environment_priority(first_item_group->repair_environment) == 0
               ? "inspect the first source-location worklist item in the current repo and close the repair card with explicit evidence"
               : "collect the target-machine or replay evidence for the first source-location worklist item before claiming readiness";
  }
  return "no source-location worklist item or gap is available; inspect repair_group_summary for broader blocker context";
}

inline std::size_t repair_card_path_current_repo_rank_count(
    const std::vector<RepairCardGroup>& groups) {
  return repair_group_navigation_current_repo_step_count(groups);
}

inline std::size_t repair_card_path_target_or_validation_rank_count(
    const std::vector<RepairCardGroup>& groups) {
  return repair_group_navigation_target_or_validation_step_count(groups);
}

inline const RepairPathSummary* find_repair_path(
    const std::vector<RepairPathSummary>& paths,
    const std::string& target) {
  for (const auto& path : paths) {
    if (path.target == target) return &path;
  }
  return nullptr;
}

inline std::string readiness_path_state(
    const std::vector<RepairPathSummary>& paths,
    const std::string& target) {
  const RepairPathSummary* path = find_repair_path(paths, target);
  return path == nullptr ? "unknown" : path->state;
}

inline std::size_t readiness_path_blocker_count(
    const std::vector<RepairPathSummary>& paths,
    const std::string& target) {
  const RepairPathSummary* path = find_repair_path(paths, target);
  return path == nullptr ? 0U : path->blocking_repair_card_count;
}

inline bool repair_group_blocks_report_ready(const RepairCardGroup& group) {
  return inferlab_evidence::repair_environment_blocks_report_ready(
      detail::repair_group_environment_kind(group));
}

inline bool repair_group_blocks_replay_ready(const RepairCardGroup& group) {
  return inferlab_evidence::repair_environment_blocks_replay_ready(
      detail::repair_group_environment_kind(group));
}

inline bool repair_group_blocks_execute_ready(const RepairCardGroup& group) {
  return inferlab_evidence::repair_environment_blocks_execute_ready(
      detail::repair_group_environment_kind(group));
}

inline std::string primary_readiness_target_for_group(
    const RepairCardGroup& group) {
  return inferlab_evidence::release_decision_name(
      inferlab_evidence::primary_readiness_target_for_repair_environment(
          detail::repair_group_environment_kind(group)));
}

inline std::size_t repair_card_rank_readiness_report_link_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_group_blocks_report_ready(group)) {
      detail::increment_count(count, "report-readiness repair-group count");
    }
  }
  return count;
}

inline std::size_t repair_card_rank_readiness_replay_link_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_group_blocks_replay_ready(group)) {
      detail::increment_count(count, "replay-readiness repair-group count");
    }
  }
  return count;
}

inline std::size_t repair_card_rank_readiness_execute_link_count(
    const std::vector<RepairCardGroup>& groups) {
  std::size_t count = 0;
  for (const auto& group : groups) {
    if (repair_group_blocks_execute_ready(group)) {
      detail::increment_count(count, "execute-readiness repair-group count");
    }
  }
  return count;
}

}  // namespace inferlab_evidence_query
