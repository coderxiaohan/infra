#pragma once

#include "evidence_query_repair_collection_projection.hpp"
#include "plan_json_utils.hpp"

#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string affected_claims_json(
    const RepairCollectionProjection& projection,
    const int indent) {
  if (!projection.affected_claims.empty()) {
    return inferlab_plan::json_string_array(projection.affected_claims, indent);
  }
  return inferlab_plan::json_string_array(
      {"no blocking claim impact detected for current filters"}, indent);
}

inline std::string exact_wiring_locations_json(
    const RepairCollectionProjection& projection,
    const int indent) {
  if (!projection.exact_wiring_locations.empty()) {
    return inferlab_plan::json_string_array(
        projection.exact_wiring_locations, indent);
  }
  return inferlab_plan::json_string_array(
      {"no blocker-specific source-exact wiring gap"}, indent);
}

inline std::string repair_collection_empty_action(
    const std::string& environment) {
  if (environment == "current_repo_fixable") {
    return "no current-repo repair required for current filters";
  }
  if (environment == "current_server_when_validation_enabled") {
    return "no current-server refresh required for current filters";
  }
  if (environment == "target_gpu_or_execute_machine") {
    return "no target GPU/execute action required for current filters";
  }
  return "no target product replay action required for current filters";
}

inline std::string action_array_for_environment_json(
    const RepairCollectionProjection& projection,
    const std::string& environment,
    const int indent) {
  const auto& actions = repair_collection_actions(projection, environment);
  if (!actions.empty()) {
    return inferlab_plan::json_string_array(actions, indent);
  }
  return inferlab_plan::json_string_array(
      {repair_collection_empty_action(environment)}, indent);
}

}  // namespace inferlab_evidence_query
