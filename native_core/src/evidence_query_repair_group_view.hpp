#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string repair_group_summary_json(
    const std::vector<RepairCardGroup>& groups,
    const int indent,
    const SelectorResolution* selector_focus = nullptr) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const RepairCardGroup* primary = primary_repair_card_group(groups);
  const std::string first_location =
      primary == nullptr || primary->exact_wiring_locations.empty()
          ? "source location not derived from current blocker category"
          : primary->exact_wiring_locations.front();
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"selector_focus_present\": "
      << (selector_focus != nullptr ? "true" : "false") << ",\n";
  if (selector_focus != nullptr) {
    out << nested << "\"selector_focus\": "
        << selector_focus_json(*selector_focus, indent + 2) << ",\n";
  }
  out << nested << "\"group_count\": " << groups.size() << ",\n";
  out << nested << "\"repair_card_count\": "
      << repair_card_group_card_count(groups) << ",\n";
  out << nested << "\"current_repo_group_count\": "
      << repair_card_group_environment_count(groups, "current_repo_fixable")
      << ",\n";
  out << nested << "\"current_server_group_count\": "
      << repair_card_group_environment_count(
             groups, "current_server_when_validation_enabled")
      << ",\n";
  out << nested << "\"target_machine_group_count\": "
      << repair_card_group_environment_count(groups,
                                             "target_gpu_or_execute_machine")
      << ",\n";
  out << nested << "\"target_product_group_count\": "
      << repair_card_group_environment_count(
             groups, "target_product_or_replay_evidence")
      << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << repair_card_group_source_location_gap_count(groups) << ",\n";
  out << nested << "\"source_location_gap_card_count\": "
      << repair_card_group_source_location_gap_card_count(groups) << ",\n";
  out << nested << "\"exact_wiring_location_count\": "
      << repair_card_group_exact_wiring_location_count(groups) << ",\n";
  out << nested << "\"affected_claim_count\": "
      << repair_card_group_affected_claim_count(groups) << ",\n";
  out << nested << "\"primary_group_present\": "
      << (primary != nullptr ? "true" : "false") << ",\n";
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
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->first_action)
      << ",\n";
  out << nested << "\"primary_action_present\": "
      << (primary != nullptr && !primary->first_action.empty() ? "true"
                                                               : "false")
      << ",\n";
  out << nested << "\"primary_wiring_location\": "
      << inferlab_plan::json_string(first_location) << ",\n";
  out << nested << "\"primary_source_location_present\": "
      << (primary != nullptr && !primary->exact_wiring_locations.empty()
              ? "true"
              : "false")
      << ",\n";
  out << nested << "\"next_step\": "
      << inferlab_plan::json_string(
             primary == nullptr
                 ? "no repair-card group is active for the current selector"
                 : primary->first_action)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"repair-group summary is read-only operator guidance derived from repair-card groups and optional selector-focus metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string repair_group_navigation_json(
    const std::vector<RepairCardGroup>& groups,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  bool first = true;
  std::size_t step = 1U;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"step_count\": " << groups.size() << ",\n";
  out << nested << "\"current_repo_step_count\": "
      << repair_group_navigation_current_repo_step_count(groups) << ",\n";
  out << nested << "\"target_or_validation_step_count\": "
      << repair_group_navigation_target_or_validation_step_count(groups)
      << ",\n";
  out << nested << "\"steps\": [";
  for (const auto& group : groups) {
    if (!first) out << ",";
    first = false;
    const bool source_location_present =
        !group.exact_wiring_locations.empty();
    const std::string selector =
        !group.first_artifact.empty() ? group.first_artifact
                                      : group.first_category;
    const std::string first_location =
        source_location_present
            ? group.exact_wiring_locations.front()
            : "source location not derived from current blocker category";
    out << "\n" << nested << "  {\n";
    out << nested << "    \"step\": " << step << ",\n";
    out << nested << "    \"id\": "
        << inferlab_plan::json_string("repair_group_navigation_" +
                                      std::to_string(step))
        << ",\n";
    step = inferlab::internal::checked_add_size(
        step, 1U, "repair-group navigation step id");
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(group.repair_environment) << ",\n";
    out << nested << "    \"first_artifact\": "
        << inferlab_plan::json_string(group.first_artifact) << ",\n";
    out << nested << "    \"first_category\": "
        << inferlab_plan::json_string(group.first_category) << ",\n";
    out << nested << "    \"first_action\": "
        << inferlab_plan::json_string(group.first_action) << ",\n";
    out << nested << "    \"first_wiring_location\": "
        << inferlab_plan::json_string(first_location) << ",\n";
    out << nested << "    \"source_location_present\": "
        << (source_location_present ? "true" : "false") << ",\n";
    out << nested << "    \"source_location_gap_count\": "
        << group.source_location_gap_card_count << ",\n";
    out << nested << "    \"source_location_exact_card_count\": "
        << group.source_exact_card_count << ",\n";
    out << nested << "    \"repair_card_count\": "
        << group.repair_card_count << ",\n";
    out << nested << "    \"affected_claim_count\": "
        << group.affected_claims.size() << ",\n";
    out << nested << "    \"show_artifact_command\": "
        << inferlab_plan::json_string(
               selector.empty()
                   ? "inferlab_evidence_query --operation list_artifacts"
                   : "inferlab_evidence_query --operation show_artifact --selector " +
                         selector)
        << ",\n";
    out << nested << "    \"explain_blocker_command\": "
        << inferlab_plan::json_string(
               group.first_category.empty()
                   ? "inferlab_evidence_query --operation explain_blocker"
                   : "inferlab_evidence_query --operation explain_blocker --selector " +
                         group.first_category)
        << ",\n";
    out << nested << "    \"suggest_repair_command\": "
        << inferlab_plan::json_string(
               selector.empty()
                   ? "inferlab_evidence_query --operation suggest_repair"
                   : "inferlab_evidence_query --operation suggest_repair --selector " +
                         selector)
        << ",\n";
    out << nested << "    \"repair_group_summary_command\": "
        << inferlab_plan::json_string(
               selector.empty()
                   ? "inferlab_evidence_query --operation repair_group_summary"
                   : "inferlab_evidence_query --operation repair_group_summary --selector " +
                         selector)
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"repair-group navigation step is read-only drill-down guidance; commands are suggested inspection commands only and are not executed by this artifact\"\n";
    out << nested << "  }";
  }
  if (!first) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"repair-group navigation is read-only drill-down guidance derived from repair-card groups; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string repair_card_group_plan_json(
    const std::vector<RepairCardGroup>& groups,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  bool first = true;
  std::size_t group_id = 1U;
  std::ostringstream out;
  out << "[";
  for (const auto& group : groups) {
    if (!first) out << ",";
    first = false;
    const bool current_repo =
        repair_environment_priority(group.repair_environment) == 0;
    const bool current_server =
        group.repair_environment == "current_server_when_validation_enabled";
    const bool target_machine =
        group.repair_environment == "target_gpu_or_execute_machine";
    const bool target_product =
        group.repair_environment == "target_product_or_replay_evidence";
    out << "\n" << nested << "{\n";
    out << nested << "  \"id\": "
        << inferlab_plan::json_string("repair_group_" +
                                      std::to_string(group_id))
        << ",\n";
    group_id = inferlab::internal::checked_add_size(
        group_id, 1U, "repair-card group plan id");
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(group.repair_environment) << ",\n";
    out << nested << "  \"repair_card_count\": " << group.repair_card_count
        << ",\n";
    out << nested << "  \"source_location_exact_card_count\": "
        << group.source_exact_card_count << ",\n";
    out << nested << "  \"source_location_gap_card_count\": "
        << group.source_location_gap_card_count << ",\n";
    out << nested << "  \"category_count\": " << group.categories.size()
        << ",\n";
    out << nested << "  \"artifact_count\": " << group.artifacts.size()
        << ",\n";
    out << nested << "  \"first_artifact\": "
        << inferlab_plan::json_string(group.first_artifact) << ",\n";
    out << nested << "  \"first_category\": "
        << inferlab_plan::json_string(group.first_category) << ",\n";
    out << nested << "  \"first_action\": "
        << inferlab_plan::json_string(group.first_action) << ",\n";
    out << nested << "  \"first_wiring_location\": "
        << inferlab_plan::json_string(
               group.exact_wiring_locations.empty()
                   ? "source location not derived from current blocker category"
                   : group.exact_wiring_locations.front())
        << ",\n";
    out << nested << "  \"categories\": "
        << inferlab_plan::json_string_array(group.categories, indent + 2)
        << ",\n";
    out << nested << "  \"artifacts\": "
        << inferlab_plan::json_string_array(group.artifacts, indent + 2)
        << ",\n";
    out << nested << "  \"affected_claims\": "
        << inferlab_plan::json_string_array(group.affected_claims, indent + 2)
        << ",\n";
    out << nested << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(group.exact_wiring_locations,
                                            indent + 2)
        << ",\n";
    out << nested << "  \"current_repo_action_required\": "
        << (current_repo ? "true" : "false") << ",\n";
    out << nested << "  \"current_server_action_required\": "
        << (current_server ? "true" : "false") << ",\n";
    out << nested << "  \"target_machine_action_required\": "
        << (target_machine ? "true" : "false") << ",\n";
    out << nested << "  \"target_product_action_required\": "
        << (target_product ? "true" : "false") << ",\n";
    out << nested
        << "  \"claim_boundary\": \"repair-card group plan is read-only operator guidance; it does not execute repairs, mutate files, or create benchmark/replay/execute evidence\"\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

}  // namespace inferlab_evidence_query
