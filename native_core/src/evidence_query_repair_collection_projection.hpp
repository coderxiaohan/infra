#pragma once

#include "checked_index.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_repair_policy.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct RepairCollectionProjection {
  std::size_t matched_record_count = 0;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::vector<std::string> matching_categories;
  std::vector<std::string> current_repo_actions;
  std::vector<std::string> current_server_actions;
  std::vector<std::string> target_machine_actions;
  std::vector<std::string> target_product_actions;
};

inline bool repair_collection_record_matches(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  if (!artifact_filter.empty() && record.artifact != artifact_filter &&
      !inferlab_plan::contains(record.artifact, artifact_filter)) {
    return false;
  }
  const std::string& category = blocker_category(record);
  if (category == "none") return false;
  return blocker_filter.empty() || category == blocker_filter;
}

inline RepairCollectionProjection repair_collection_projection(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  RepairCollectionProjection projection;
  for (const auto& record : records) {
    if (!repair_collection_record_matches(record, artifact_filter,
                                          blocker_filter)) {
      continue;
    }
    projection.matched_record_count = inferlab::internal::checked_add_size(
        projection.matched_record_count, 1U,
        "repair collection matched record count");
    const std::string& category = blocker_category(record);
    inferlab_plan::append_unique_nonempty_string(
        projection.matching_categories, category);
    append_affected_claims(projection.affected_claims, record);
    if (category_has_exact_wiring_locations(category)) {
      for (const auto& location : wiring_locations_for_category(category)) {
        inferlab_plan::append_unique_nonempty_string(
            projection.exact_wiring_locations, location);
      }
    }

    std::vector<std::string>* actions = nullptr;
    const std::string& environment = repair_environment(record);
    if (environment == "current_repo_fixable") {
      actions = &projection.current_repo_actions;
    } else if (environment == "current_server_when_validation_enabled") {
      actions = &projection.current_server_actions;
    } else if (environment == "target_gpu_or_execute_machine") {
      actions = &projection.target_machine_actions;
    } else if (environment == "target_product_or_replay_evidence") {
      actions = &projection.target_product_actions;
    }
    if (actions != nullptr) {
      inferlab_plan::append_unique_nonempty_string(*actions,
                                                   repair_action(category));
    }
  }
  return projection;
}

inline const std::vector<std::string>& repair_collection_actions(
    const RepairCollectionProjection& projection,
    const std::string& environment) {
  if (environment == "current_repo_fixable") {
    return projection.current_repo_actions;
  }
  if (environment == "current_server_when_validation_enabled") {
    return projection.current_server_actions;
  }
  if (environment == "target_gpu_or_execute_machine") {
    return projection.target_machine_actions;
  }
  if (environment == "target_product_or_replay_evidence") {
    return projection.target_product_actions;
  }
  static const std::vector<std::string> empty;
  return empty;
}

}  // namespace inferlab_evidence_query
