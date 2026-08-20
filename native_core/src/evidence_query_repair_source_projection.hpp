#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_groups.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {

inline constexpr std::size_t kMaxRepairCardSourceExactnessCards = 32U;

struct RepairCardSourceExactnessStats {
  std::size_t repair_card_count = 0;
  std::size_t source_exact_card_count = 0;
  std::size_t source_gap_card_count = 0;
  std::size_t current_repo_source_exact_card_count = 0;
  std::size_t current_repo_source_gap_card_count = 0;
  std::size_t target_or_validation_source_exact_card_count = 0;
  std::size_t target_or_validation_source_gap_card_count = 0;
};

struct RepairCardSourceExactnessRow {
  std::string artifact;
  std::string category;
  std::string repair_environment;
  std::vector<std::string> wiring_locations;
  bool current_repo_action_required = false;
  bool source_exact = false;
};

struct RepairCardSourceExactnessProjection {
  RepairCardSourceExactnessStats stats;
  std::vector<RepairCardSourceExactnessRow> rows;
  std::size_t row_limit = 0;
  bool truncated = false;
};

namespace detail {

inline void increment_source_exactness_count(std::size_t& value,
                                             const char* name) {
  value = inferlab::internal::checked_add_size(value, 1U, name);
}

inline bool project_repair_card_source_exactness_row(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter,
    RepairCardSourceExactnessRow& row) {
  if (!repair_card_record_matches(record, operation, artifact_filter,
                                  blocker_filter)) {
    return false;
  }
  row = RepairCardSourceExactnessRow{};
  row.artifact = record.artifact;
  row.category = blocker_category(record);
  row.repair_environment = repair_environment(record);
  row.wiring_locations = wiring_locations_for_category(row.category);
  const auto environment =
      inferlab_evidence::parse_repair_environment(row.repair_environment);
  row.current_repo_action_required =
      inferlab_evidence::repair_environment_is_current_repo(environment) ||
      environment == inferlab_evidence::RepairEnvironment::kUnknown ||
      environment == inferlab_evidence::RepairEnvironment::kNone;
  row.source_exact =
      category_has_exact_wiring_locations(row.category) &&
      !row.wiring_locations.empty();
  return true;
}

inline void accumulate_repair_card_source_exactness_stats(
    RepairCardSourceExactnessStats& stats,
    const RepairCardSourceExactnessRow& row) {
  increment_source_exactness_count(
      stats.repair_card_count, "repair-card source-exactness card count");
  if (row.source_exact) {
    increment_source_exactness_count(stats.source_exact_card_count,
                                     "repair-card source-exact count");
    if (row.current_repo_action_required) {
      increment_source_exactness_count(
          stats.current_repo_source_exact_card_count,
          "current-repo repair-card source-exact count");
    } else {
      increment_source_exactness_count(
          stats.target_or_validation_source_exact_card_count,
          "target repair-card source-exact count");
    }
    return;
  }
  increment_source_exactness_count(stats.source_gap_card_count,
                                   "repair-card source-gap count");
  if (row.current_repo_action_required) {
    increment_source_exactness_count(
        stats.current_repo_source_gap_card_count,
        "current-repo repair-card source-gap count");
  } else {
    increment_source_exactness_count(
        stats.target_or_validation_source_gap_card_count,
        "target repair-card source-gap count");
  }
}

}  // namespace detail

inline RepairCardSourceExactnessProjection
repair_card_source_exactness_projection(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter,
    const std::size_t row_limit) {
  RepairCardSourceExactnessProjection projection;
  projection.row_limit = row_limit;
  for (const auto& record : records) {
    RepairCardSourceExactnessRow row;
    if (!detail::project_repair_card_source_exactness_row(
            record, operation, artifact_filter, blocker_filter, row)) {
      continue;
    }
    detail::accumulate_repair_card_source_exactness_stats(projection.stats,
                                                          row);
    if (projection.rows.size() < row_limit) {
      projection.rows.push_back(std::move(row));
    }
  }
  projection.truncated =
      projection.stats.repair_card_count > projection.rows.size();
  return projection;
}

inline int repair_card_source_exactness_percent(
    const RepairCardSourceExactnessStats& stats) {
  return inferlab::internal::checked_floor_percentage(
      stats.source_exact_card_count, stats.repair_card_count,
      "repair-card source exactness");
}

}  // namespace inferlab_evidence_query
