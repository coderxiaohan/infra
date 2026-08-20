#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_groups.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

using ExplainTreeCategoryCounts = std::map<std::string, std::size_t>;

struct ExplainTreeSourceStats {
  std::size_t node_count = 0;
  std::size_t source_exact_node_count = 0;
  std::size_t source_gap_node_count = 0;
  std::size_t current_repo_source_gap_count = 0;
  std::size_t target_or_validation_source_gap_count = 0;
};

struct SourceLocationCategoryCoverageStats {
  std::size_t category_count = 0;
  std::size_t active_category_count = 0;
  std::size_t source_exact_category_count = 0;
  std::size_t source_gap_category_count = 0;
  std::size_t current_repo_source_gap_category_count = 0;
  std::size_t target_or_validation_source_gap_category_count = 0;
  std::size_t source_gap_repair_card_count = 0;
};

struct SourceLocationCategoryCoverage {
  ExplainTreeCategoryCounts repair_card_counts;
  std::vector<std::string> categories;
  SourceLocationCategoryCoverageStats stats;
};

struct BlockerDrilldown {
  ExplainTreeCategoryCounts category_counts;
  ExplainTreeSourceStats source_stats;
  std::size_t blocker_record_count = 0;
  std::size_t source_location_gap_count = 0;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::vector<std::string> current_repo_actions;
  std::vector<std::string> current_server_actions;
  std::vector<std::string> target_machine_actions;
  std::vector<std::string> target_product_actions;
  std::string first_artifact;
  std::string first_category;
  std::string first_action;
  std::string first_location;
};

inline ExplainTreeSourceStats explain_tree_source_stats(
    const ExplainTreeCategoryCounts& category_counts) {
  ExplainTreeSourceStats stats;
  stats.node_count = category_counts.size();
  for (const auto& entry : category_counts) {
    const auto locations = wiring_locations_for_category(entry.first);
    const bool source_exact =
        category_has_exact_wiring_locations(entry.first) && !locations.empty();
    if (source_exact) {
      stats.source_exact_node_count = inferlab::internal::checked_add_size(
          stats.source_exact_node_count, 1U,
          "explain-tree source-exact node count");
      continue;
    }
    stats.source_gap_node_count = inferlab::internal::checked_add_size(
        stats.source_gap_node_count, 1U,
        "explain-tree source-gap node count");
    if (inferlab_evidence::repair_environment_kind(entry.first) ==
        inferlab_evidence::RepairEnvironment::kCurrentRepoFixable) {
      stats.current_repo_source_gap_count =
          inferlab::internal::checked_add_size(
              stats.current_repo_source_gap_count, 1U,
              "explain-tree current-repo source-gap count");
    } else {
      stats.target_or_validation_source_gap_count =
          inferlab::internal::checked_add_size(
              stats.target_or_validation_source_gap_count, 1U,
              "explain-tree target-or-validation source-gap count");
    }
  }
  return stats;
}

inline int explain_tree_source_exactness_percent(
    const ExplainTreeSourceStats& stats) {
  return inferlab::internal::checked_floor_percentage(
      stats.source_exact_node_count, stats.node_count,
      "explain-tree source exactness");
}

inline std::vector<std::string> source_location_category_coverage_keys(
    const ExplainTreeCategoryCounts& counts) {
  std::vector<std::string> categories = known_blocker_categories();
  for (const auto& entry : counts) {
    inferlab_plan::append_unique_nonempty_string(categories, entry.first);
  }
  return categories;
}

inline SourceLocationCategoryCoverageStats
source_location_category_coverage_stats(
    const ExplainTreeCategoryCounts& counts,
    const std::vector<std::string>& categories) {
  SourceLocationCategoryCoverageStats stats;
  stats.category_count = categories.size();
  for (const auto& category : categories) {
    const auto count_it = counts.find(category);
    const std::size_t repair_card_count =
        count_it == counts.end() ? 0U : count_it->second;
    if (repair_card_count == 0U) continue;
    stats.active_category_count = inferlab::internal::checked_add_size(
        stats.active_category_count, 1U,
        "source-location active category count");
    const auto locations = wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    if (source_exact) {
      stats.source_exact_category_count = inferlab::internal::checked_add_size(
          stats.source_exact_category_count, 1U,
          "source-location exact category count");
      continue;
    }
    stats.source_gap_category_count = inferlab::internal::checked_add_size(
        stats.source_gap_category_count, 1U,
        "source-location gap category count");
    stats.source_gap_repair_card_count = inferlab::internal::checked_add_size(
        stats.source_gap_repair_card_count, repair_card_count,
        "source-location gap repair-card count");
    if (inferlab_evidence::repair_environment_kind(category) ==
        inferlab_evidence::RepairEnvironment::kCurrentRepoFixable) {
      stats.current_repo_source_gap_category_count =
          inferlab::internal::checked_add_size(
              stats.current_repo_source_gap_category_count, 1U,
              "source-location current-repo gap category count");
    } else {
      stats.target_or_validation_source_gap_category_count =
          inferlab::internal::checked_add_size(
              stats.target_or_validation_source_gap_category_count, 1U,
              "source-location target-or-validation gap category count");
    }
  }
  return stats;
}

inline SourceLocationCategoryCoverage build_source_location_category_coverage(
    const ExplainTreeCategoryCounts& counts) {
  SourceLocationCategoryCoverage coverage;
  coverage.repair_card_counts = counts;
  coverage.categories = source_location_category_coverage_keys(counts);
  coverage.stats = source_location_category_coverage_stats(
      coverage.repair_card_counts, coverage.categories);
  return coverage;
}

inline int source_location_category_exactness_percent(
    const SourceLocationCategoryCoverageStats& stats) {
  return inferlab::internal::checked_floor_percentage(
      stats.source_exact_category_count, stats.active_category_count,
      "source-location category exactness");
}

inline BlockerDrilldown build_blocker_drilldown(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  BlockerDrilldown drilldown;
  for (const auto& record : records) {
    if (!repair_card_record_matches(record, operation, artifact_filter,
                                    blocker_filter)) {
      continue;
    }
    drilldown.blocker_record_count = inferlab::internal::checked_add_size(
        drilldown.blocker_record_count, 1U,
        "blocker-drilldown record count");
    const std::string category = blocker_category(record);
    auto& category_count = drilldown.category_counts[category];
    category_count = inferlab::internal::checked_add_size(
        category_count, 1U, "blocker-drilldown category record count");
    const std::string action = repair_action(category);
    const inferlab_evidence::RepairEnvironment environment =
        inferlab_evidence::repair_environment_kind(category);
    const auto locations = wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    if (drilldown.first_artifact.empty()) {
      drilldown.first_artifact = record.artifact;
    }
    if (drilldown.first_category.empty()) {
      drilldown.first_category = category;
    }
    if (drilldown.first_action.empty()) drilldown.first_action = action;
    if (drilldown.first_location.empty() && !locations.empty()) {
      drilldown.first_location = locations.front();
    }
    append_affected_claims(drilldown.affected_claims, record);
    if (source_exact) {
      for (const auto& location : locations) {
        inferlab_plan::append_unique_nonempty_string(
            drilldown.exact_wiring_locations, location);
      }
    } else {
      drilldown.source_location_gap_count =
          inferlab::internal::checked_add_size(
              drilldown.source_location_gap_count, 1U,
              "blocker-drilldown source-location gap count");
    }
    std::vector<std::string>* actions = &drilldown.target_product_actions;
    if (environment ==
        inferlab_evidence::RepairEnvironment::kCurrentRepoFixable) {
      actions = &drilldown.current_repo_actions;
    } else if (
        environment == inferlab_evidence::RepairEnvironment::
                           kCurrentServerWhenValidationEnabled) {
      actions = &drilldown.current_server_actions;
    } else if (environment ==
               inferlab_evidence::RepairEnvironment::
                   kTargetGpuOrExecuteMachine) {
      actions = &drilldown.target_machine_actions;
    }
    inferlab_plan::append_unique_nonempty_string(*actions, action);
  }

  drilldown.source_stats =
      explain_tree_source_stats(drilldown.category_counts);
  if (drilldown.first_location.empty()) drilldown.first_location = "<none>";
  if (drilldown.first_action.empty()) {
    drilldown.first_action = "no blocker-specific action";
  }
  return drilldown;
}

}  // namespace inferlab_evidence_query
