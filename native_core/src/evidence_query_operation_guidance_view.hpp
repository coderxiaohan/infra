#pragma once

#include "evidence_query_operation_guidance_projection.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

inline std::string selector_action_plan_json(
    const OperationGuidanceProjection& projection,
    const SelectorResolution& resolution,
    const QueryOperation& operation,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << nested << "\"selector_resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"selector_focus\": "
      << selector_focus_json(resolution, indent + 2) << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(projection.decision) << ",\n";
  out << nested << "\"recommended_readiness_target\": "
      << inferlab_plan::json_string(projection.readiness_target) << ",\n";
  out << nested << "\"readiness_path_present\": "
      << inferlab_plan::json_bool(projection.readiness_path != nullptr)
      << ",\n";
  out << nested << "\"readiness_path_state\": "
      << inferlab_plan::json_string(projection.readiness.state) << ",\n";
  out << nested << "\"readiness_path_blocker_count\": "
      << projection.readiness.blocking_repair_card_count << ",\n";
  out << nested << "\"first_artifact\": "
      << inferlab_plan::json_string(resolution.first_artifact) << ",\n";
  out << nested << "\"first_category\": "
      << inferlab_plan::json_string(resolution.first_category) << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(resolution.first_action) << ",\n";
  out << nested << "\"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << ",\n";
  out << nested << "\"repair_card_count\": " << resolution.repair_card_count
      << ",\n";
  out << nested << "\"current_repo_step_count\": "
      << resolution.current_repo_actions.size() << ",\n";
  out << nested << "\"target_or_validation_step_count\": "
      << projection.target_or_validation_step_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << resolution.source_location_gap_count << ",\n";
  out << nested << "\"affected_claim_count\": "
      << resolution.affected_claims.size() << ",\n";
  out << nested << "\"exact_wiring_location_count\": "
      << resolution.exact_wiring_location_count << ",\n";
  out << nested << "\"step_count\": " << projection.action_plan_steps.size()
      << ",\n";
  out << nested << "\"steps\": "
      << inferlab_plan::json_string_array(projection.action_plan_steps,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(resolution.current_repo_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_server_actions\": "
      << inferlab_plan::json_string_array(resolution.current_server_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(resolution.target_machine_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_product_actions\": "
      << inferlab_plan::json_string_array(resolution.target_product_actions,
                                          indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"selector action plan is read-only triage guidance derived from existing selector resolution and repair paths; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string selector_action_plan_text(
    const OperationGuidanceProjection& projection,
    const SelectorResolution& resolution) {
  std::ostringstream out;
  out << "selector_action_plan: decision=" << projection.decision
      << " | selector_status=" << resolution.resolution_status
      << " | owner=" << resolution.repair_owner_hint
      << " | repair_cards=" << resolution.repair_card_count
      << " | steps=" << projection.action_plan_steps.size()
      << " | current_repo_steps=" << resolution.current_repo_actions.size()
      << " | target_or_validation_steps="
      << projection.target_or_validation_step_count
      << " | source_location_gaps="
      << resolution.source_location_gap_count
      << " | readiness_target=" << projection.readiness_target
      << " | readiness_state=" << projection.readiness.state
      << " | first_step="
      << (projection.action_plan_steps.empty()
              ? "no action"
              : projection.action_plan_steps.front())
      << "\n";
  return out.str();
}

inline std::string repair_narrative_json(
    const OperationGuidanceProjection& projection,
    const SelectorResolution& resolution,
    const QueryOperation& operation,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const RepairCardGroup* primary = projection.primary_group;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(projection.decision) << ",\n";
  out << nested << "\"readiness_target\": "
      << inferlab_plan::json_string(projection.readiness_target) << ",\n";
  out << nested << "\"readiness_state\": "
      << inferlab_plan::json_string(projection.readiness.state) << ",\n";
  out << nested << "\"readiness_blocker_count\": "
      << projection.readiness.blocking_repair_card_count << ",\n";
  out << nested << "\"group_count\": " << projection.group_count << ",\n";
  out << nested << "\"repair_card_count\": " << projection.repair_card_count
      << ",\n";
  out << nested << "\"current_repo_group_count\": "
      << projection.current_repo_group_count << ",\n";
  out << nested << "\"current_server_group_count\": "
      << projection.current_server_group_count << ",\n";
  out << nested << "\"target_machine_group_count\": "
      << projection.target_machine_group_count << ",\n";
  out << nested << "\"target_product_group_count\": "
      << projection.target_product_group_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << projection.source_location_gap_count << ",\n";
  out << nested << "\"exact_wiring_location_count\": "
      << projection.exact_wiring_location_count << ",\n";
  out << nested << "\"affected_claim_count\": "
      << projection.affected_claim_count << ",\n";
  out << nested << "\"primary_repair_environment\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->repair_environment)
      << ",\n";
  out << nested << "\"primary_category\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->first_category)
      << ",\n";
  out << nested << "\"primary_artifact\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->first_artifact)
      << ",\n";
  out << nested << "\"primary_action\": "
      << inferlab_plan::json_string(primary == nullptr ? ""
                                                       : primary->first_action)
      << ",\n";
  out << nested << "\"primary_source_location_present\": "
      << inferlab_plan::json_bool(
             primary != nullptr && !primary->exact_wiring_locations.empty())
      << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(resolution.current_repo_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_server_actions\": "
      << inferlab_plan::json_string_array(resolution.current_server_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(resolution.target_machine_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_product_actions\": "
      << inferlab_plan::json_string_array(resolution.target_product_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"affected_claims\": "
      << inferlab_plan::json_string_array(projection.affected_claims,
                                          indent + 2)
      << ",\n";
  out << nested << "\"exact_wiring_locations\": "
      << inferlab_plan::json_string_array(projection.exact_wiring_locations,
                                          indent + 2)
      << ",\n";
  out << nested << "\"narrative_step_count\": "
      << projection.narrative_steps.size() << ",\n";
  out << nested << "\"narrative_steps\": "
      << inferlab_plan::json_string_array(projection.narrative_steps,
                                          indent + 2)
      << ",\n";
  out << nested << "\"suggest_repair_command\": "
      << inferlab_plan::json_string(
             operation.selector.empty()
                 ? "inferlab_evidence_query --operation suggest_repair"
                 : "inferlab_evidence_query --operation suggest_repair "
                   "--selector " +
                       operation.selector)
      << ",\n";
  out << nested << "\"repair_group_summary_command\": "
      << inferlab_plan::json_string(
             operation.selector.empty()
                 ? "inferlab_evidence_query --operation repair_group_summary"
                 : "inferlab_evidence_query --operation repair_group_summary "
                   "--selector " +
                       operation.selector)
      << ",\n";
  out << nested << "\"readiness_path_command\": "
      << inferlab_plan::json_string(
             "inferlab_evidence_query --operation path --target " +
             projection.readiness_target)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"repair narrative is read-only operator guidance derived from existing repair cards, selector resolution, and readiness paths; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
