#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_collection_projection.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct BlockerListRecordProjection {
  std::string artifact;
  std::string category;
  std::string status;
  std::string repair_environment;
  std::string repair_action;
  std::string evidence_confidence;
};

struct BlockerCategoryAggregate {
  std::size_t count = 0;
  std::string repair_environment;
  std::string repair_action;
  std::vector<std::string> artifacts;
};

struct BlockerListProjection {
  std::vector<BlockerListRecordProjection> blocker_records;
  std::map<std::string, BlockerCategoryAggregate> categories;
  std::vector<std::string> ranked_categories;
  std::vector<std::string> repair_plan_categories;
  std::vector<std::string> current_repo_fixable_artifacts;
  std::vector<std::string> current_server_validation_artifacts;
  std::vector<std::string> target_machine_required_artifacts;
};

inline const std::vector<std::string>& blocker_repair_plan_category_order() {
  static const std::vector<std::string> order = {
      "schema_missing", "claim_scope_missing", "report_wiring_missing",
      "artifact_missing", "freshness_stale", "artifact_blocked",
      "replay_missing", "execute_missing"};
  return order;
}

inline BlockerListProjection blocker_list_projection(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const std::string& blocker_filter) {
  BlockerListProjection projection;
  for (const auto& record : records) {
    if (!repair_collection_record_matches(record, "", blocker_filter)) {
      continue;
    }

    const std::string& category = blocker_category(record);
    const std::string& environment = repair_environment(record);
    const std::string action = repair_action(category);
    projection.blocker_records.push_back(
        {record.artifact,
         category,
         result_status(record, "release_blockers"),
         environment,
         action,
         evidence_confidence(record)});

    auto& aggregate = projection.categories[category];
    aggregate.count = inferlab::internal::checked_add_size(
        aggregate.count, 1U, "blocker-list category count");
    if (aggregate.repair_environment.empty()) {
      aggregate.repair_environment = environment;
    }
    if (aggregate.repair_action.empty()) aggregate.repair_action = action;
    aggregate.artifacts.push_back(record.artifact);

    if (environment == "current_repo_fixable") {
      projection.current_repo_fixable_artifacts.push_back(record.artifact);
    } else if (environment ==
               "current_server_when_validation_enabled") {
      projection.current_server_validation_artifacts.push_back(
          record.artifact);
    } else {
      projection.target_machine_required_artifacts.push_back(record.artifact);
    }
  }

  projection.ranked_categories.reserve(projection.categories.size());
  for (const auto& entry : projection.categories) {
    projection.ranked_categories.push_back(entry.first);
  }
  std::sort(
      projection.ranked_categories.begin(),
      projection.ranked_categories.end(),
      [&projection](const std::string& lhs, const std::string& rhs) {
        const auto& lhs_aggregate = projection.categories.at(lhs);
        const auto& rhs_aggregate = projection.categories.at(rhs);
        if (lhs_aggregate.count != rhs_aggregate.count) {
          return lhs_aggregate.count > rhs_aggregate.count;
        }
        return lhs < rhs;
      });

  for (const auto& category : blocker_repair_plan_category_order()) {
    if (projection.categories.find(category) != projection.categories.end()) {
      projection.repair_plan_categories.push_back(category);
    }
  }
  return projection;
}

}  // namespace inferlab_evidence_query
