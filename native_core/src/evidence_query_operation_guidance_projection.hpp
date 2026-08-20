#pragma once

#include "evidence_query_operation_policy.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct OperationGuidanceProjection {
  std::string readiness_target;
  const RepairPathSummary* readiness_path = nullptr;
  ReadinessPathActionabilityProjection readiness;
  std::string decision;
  std::size_t target_or_validation_step_count = 0;
  std::vector<std::string> action_plan_steps;

  const RepairCardGroup* primary_group = nullptr;
  std::size_t group_count = 0;
  std::size_t repair_card_count = 0;
  std::size_t current_repo_group_count = 0;
  std::size_t current_server_group_count = 0;
  std::size_t target_machine_group_count = 0;
  std::size_t target_product_group_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t exact_wiring_location_count = 0;
  std::size_t affected_claim_count = 0;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::vector<std::string> narrative_steps;
};

namespace detail {

inline std::vector<std::string> build_selector_action_plan_steps(
    const SelectorResolution& resolution,
    const RepairPathSummary* path) {
  std::vector<std::string> steps;
  inferlab_plan::append_unique_nonempty_string(steps,
                                                resolution.repair_hint);
  if (resolution.first_action != resolution.repair_hint) {
    inferlab_plan::append_unique_nonempty_string(steps,
                                                  resolution.first_action);
  }
  for (const auto& action : resolution.current_repo_actions) {
    inferlab_plan::append_unique_nonempty_string(steps,
                                                  "current repo: " + action);
  }
  for (const auto& action : resolution.current_server_actions) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "validation-enabled current server: " + action);
  }
  for (const auto& action : resolution.target_machine_actions) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "target GPU/execute machine: " + action);
  }
  for (const auto& action : resolution.target_product_actions) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "target product/replay evidence: " + action);
  }
  if (resolution.source_location_gap_count > 0U) {
    inferlab_plan::append_unique_nonempty_string(
        steps,
        "add exact wiring locations for selector repair cards before claiming source-trace closure");
  }
  if (resolution.repair_card_truncated) {
    inferlab_plan::append_unique_nonempty_string(
        steps,
        "narrow selector or export full selector repair cards before reviewing every matched blocker");
  }
  if (path != nullptr && path->state == "blocked" && !path->steps.empty()) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "readiness path: " + path->steps.front());
  }
  if (steps.empty()) {
    steps.push_back(
        "no selector-specific repair action is currently available");
  }
  return steps;
}

inline std::vector<std::string> build_repair_narrative_steps(
    const OperationGuidanceProjection& projection) {
  std::vector<std::string> steps;
  if (projection.group_count == 0U) {
    inferlab_plan::append_unique_nonempty_string(
        steps,
        "no repair-card group matched the current selector; broaden the selector or inspect warehouse blockers");
  } else if (projection.primary_group != nullptr) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "start with " + projection.primary_group->repair_environment +
                   " repair group " +
                   projection.primary_group->first_category + " on " +
                   projection.primary_group->first_artifact);
    inferlab_plan::append_unique_nonempty_string(
        steps, projection.primary_group->first_action);
  }
  for (const auto& step : projection.action_plan_steps) {
    inferlab_plan::append_unique_nonempty_string(steps, step);
  }
  if (projection.source_location_gap_count > 0U) {
    inferlab_plan::append_unique_nonempty_string(
        steps,
        "close source-location gaps before claiming exact repair handoff readiness");
  }
  if (projection.readiness_path != nullptr) {
    inferlab_plan::append_unique_nonempty_string(
        steps, "readiness path " + projection.readiness_target + " is " +
                   projection.readiness_path->state + " with " +
                   std::to_string(
                       projection.readiness_path->blocking_repair_card_count) +
                   " blocking repair cards");
  } else {
    inferlab_plan::append_unique_nonempty_string(
        steps, "readiness path " + projection.readiness_target +
                   " is not present in current evidence");
  }
  return steps;
}

}  // namespace detail

inline OperationGuidanceProjection operation_guidance_projection(
    const std::vector<RepairCardGroup>& groups,
    const SelectorResolution& resolution,
    const QueryOperation& operation,
    const std::vector<RepairPathSummary>& shortest_paths) {
  OperationGuidanceProjection projection;
  projection.readiness_target = selected_action_plan_target(operation);
  projection.readiness_path =
      selected_action_plan_path(shortest_paths, operation);
  projection.readiness = readiness_path_actionability_projection(
      projection.readiness_path, resolution, projection.readiness_target);
  projection.decision = selector_action_plan_decision(resolution);
  projection.target_or_validation_step_count =
      projection.readiness.target_or_validation_step_count;
  projection.action_plan_steps =
      detail::build_selector_action_plan_steps(resolution,
                                               projection.readiness_path);

  // Borrowed pointers remain valid while the source vectors are not modified.
  projection.primary_group = primary_repair_card_group(groups);
  projection.group_count = groups.size();
  projection.repair_card_count = repair_card_group_card_count(groups);
  projection.current_repo_group_count = repair_card_group_environment_count(
      groups, "current_repo_fixable");
  projection.current_server_group_count = repair_card_group_environment_count(
      groups, "current_server_when_validation_enabled");
  projection.target_machine_group_count = repair_card_group_environment_count(
      groups, "target_gpu_or_execute_machine");
  projection.target_product_group_count = repair_card_group_environment_count(
      groups, "target_product_or_replay_evidence");
  projection.source_location_gap_count =
      repair_card_group_source_location_gap_count(groups);
  projection.exact_wiring_location_count =
      repair_card_group_exact_wiring_location_count(groups);
  projection.affected_claim_count =
      repair_card_group_affected_claim_count(groups);

  if (projection.primary_group != nullptr) {
    projection.affected_claims = projection.primary_group->affected_claims;
    projection.exact_wiring_locations =
        projection.primary_group->exact_wiring_locations;
  }
  if (projection.affected_claims.empty()) {
    projection.affected_claims = resolution.affected_claims;
  }
  if (projection.exact_wiring_locations.empty()) {
    projection.exact_wiring_locations = resolution.exact_wiring_locations;
  }
  projection.narrative_steps =
      detail::build_repair_narrative_steps(projection);
  return projection;
}

}  // namespace inferlab_evidence_query
