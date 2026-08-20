#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_groups.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string repair_card_path_ranking_json(
    const std::vector<RepairCardGroup>& groups,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<const RepairCardGroup*> ranked =
      ranked_repair_card_groups(groups);
  const RepairCardGroup* top = ranked.empty() ? nullptr : ranked.front();
  std::size_t rank = 1U;
  bool first = true;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"rank_count\": " << ranked.size() << ",\n";
  out << nested << "\"current_repo_rank_count\": "
      << repair_card_path_current_repo_rank_count(groups) << ",\n";
  out << nested << "\"target_or_validation_rank_count\": "
      << repair_card_path_target_or_validation_rank_count(groups) << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << repair_card_group_source_location_gap_count(groups) << ",\n";
  out << nested << "\"source_location_gap_card_count\": "
      << repair_card_group_source_location_gap_card_count(groups) << ",\n";
  out << nested << "\"top_rank_present\": "
      << (top == nullptr ? "false" : "true") << ",\n";
  out << nested << "\"top_rank_environment\": "
      << inferlab_plan::json_string(top == nullptr ? ""
                                                   : top->repair_environment)
      << ",\n";
  out << nested << "\"top_rank_category\": "
      << inferlab_plan::json_string(top == nullptr ? ""
                                                   : top->first_category)
      << ",\n";
  out << nested << "\"top_rank_artifact\": "
      << inferlab_plan::json_string(top == nullptr ? ""
                                                   : top->first_artifact)
      << ",\n";
  out << nested << "\"top_rank_source_location_present\": "
      << (top != nullptr && !top->exact_wiring_locations.empty() ? "true"
                                                                 : "false")
      << ",\n";
  out << nested << "\"top_rank_source_location_complete\": "
      << (top != nullptr && top->source_location_gap_card_count == 0U &&
                  !top->exact_wiring_locations.empty()
              ? "true"
              : "false")
      << ",\n";
  out << nested << "\"ranks\": [";
  for (const RepairCardGroup* group : ranked) {
    if (group == nullptr) continue;
    if (!first) out << ",";
    first = false;
    const bool current_repo =
        repair_environment_priority(group->repair_environment) == 0;
    const bool source_location_present =
        !group->exact_wiring_locations.empty();
    const std::string selector =
        !group->first_artifact.empty() ? group->first_artifact
                                       : group->first_category;
    out << "\n" << nested << "  {\n";
    out << nested << "    \"rank\": " << rank << ",\n";
    out << nested << "    \"id\": "
        << inferlab_plan::json_string("repair_card_path_rank_" +
                                      std::to_string(rank))
        << ",\n";
    rank = inferlab::internal::checked_add_size(
        rank, 1U, "repair-card path rank display id");
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(group->repair_environment) << ",\n";
    out << nested << "    \"first_artifact\": "
        << inferlab_plan::json_string(group->first_artifact) << ",\n";
    out << nested << "    \"first_category\": "
        << inferlab_plan::json_string(group->first_category) << ",\n";
    out << nested << "    \"first_action\": "
        << inferlab_plan::json_string(group->first_action) << ",\n";
    out << nested << "    \"repair_card_count\": "
        << group->repair_card_count << ",\n";
    out << nested << "    \"category_count\": "
        << group->categories.size() << ",\n";
    out << nested << "    \"artifact_count\": "
        << group->artifacts.size() << ",\n";
    out << nested << "    \"source_location_count\": "
        << group->exact_wiring_locations.size() << ",\n";
    out << nested << "    \"source_location_gap\": "
        << (group->source_location_gap_card_count > 0U ? "true" : "false")
        << ",\n";
    out << nested << "    \"source_location_gap_card_count\": "
        << group->source_location_gap_card_count << ",\n";
    out << nested << "    \"source_location_exact_card_count\": "
        << group->source_exact_card_count << ",\n";
    out << nested << "    \"source_location_complete\": "
        << (group->source_location_gap_card_count == 0U &&
                    source_location_present
                ? "true"
                : "false")
        << ",\n";
    out << nested << "    \"affected_claim_count\": "
        << group->affected_claims.size() << ",\n";
    out << nested << "    \"priority_score\": "
        << repair_card_path_priority_score(*group) << ",\n";
    out << nested << "    \"current_repo_action_required\": "
        << (current_repo ? "true" : "false") << ",\n";
    out << nested << "    \"target_or_validation_action_required\": "
        << (current_repo ? "false" : "true") << ",\n";
    out << nested << "    \"ranking_reason\": "
        << inferlab_plan::json_string(
               current_repo
                   ? "current-repo repair groups rank before validation-only groups"
                   : "target or validation evidence is required before this group can close")
        << ",\n";
    out << nested << "    \"source_locations\": "
        << inferlab_plan::json_string_array(group->exact_wiring_locations,
                                            indent + 2)
        << ",\n";
    out << nested << "    \"affected_claims\": "
        << inferlab_plan::json_string_array(group->affected_claims, indent + 2)
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
        << "    \"claim_boundary\": \"repair-card path rank is read-only ordering guidance derived from repair-card groups; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
    out << nested << "  }";
  }
  if (!first) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"repair-card path ranking is read-only ordering guidance; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string repair_card_rank_readiness_linkage_json(
    const std::vector<RepairCardGroup>& groups,
    const std::vector<RepairPathSummary>& shortest_paths,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<const RepairCardGroup*> ranked =
      ranked_repair_card_groups(groups);
  const RepairCardGroup* top = ranked.empty() ? nullptr : ranked.front();
  const std::string report_ready_path_state =
      readiness_path_state(shortest_paths, "report_ready");
  const std::string replay_ready_path_state =
      readiness_path_state(shortest_paths, "replay_ready");
  const std::string execute_ready_path_state =
      readiness_path_state(shortest_paths, "execute_ready");
  const std::size_t report_ready_blocker_count =
      readiness_path_blocker_count(shortest_paths, "report_ready");
  const std::size_t replay_ready_blocker_count =
      readiness_path_blocker_count(shortest_paths, "replay_ready");
  const std::size_t execute_ready_blocker_count =
      readiness_path_blocker_count(shortest_paths, "execute_ready");
  bool first = true;
  std::size_t rank = 1U;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"link_count\": " << ranked.size() << ",\n";
  out << nested << "\"current_repo_link_count\": "
      << repair_card_path_current_repo_rank_count(groups) << ",\n";
  out << nested << "\"target_or_validation_link_count\": "
      << repair_card_path_target_or_validation_rank_count(groups) << ",\n";
  out << nested << "\"report_ready_link_count\": "
      << repair_card_rank_readiness_report_link_count(groups) << ",\n";
  out << nested << "\"replay_ready_link_count\": "
      << repair_card_rank_readiness_replay_link_count(groups) << ",\n";
  out << nested << "\"execute_ready_link_count\": "
      << repair_card_rank_readiness_execute_link_count(groups) << ",\n";
  out << nested << "\"top_link_present\": "
      << (top == nullptr ? "false" : "true") << ",\n";
  out << nested << "\"top_link_primary_readiness_target\": "
      << inferlab_plan::json_string(
             top == nullptr ? "" : primary_readiness_target_for_group(*top))
      << ",\n";
  out << nested << "\"links\": [";
  for (const RepairCardGroup* group : ranked) {
    if (group == nullptr) continue;
    if (!first) out << ",";
    first = false;
    const bool report_blocked = repair_group_blocks_report_ready(*group);
    const bool replay_blocked = repair_group_blocks_replay_ready(*group);
    const bool execute_blocked = repair_group_blocks_execute_ready(*group);
    const std::string primary_target =
        primary_readiness_target_for_group(*group);
    const std::string selector =
        !group->first_artifact.empty() ? group->first_artifact
                                       : group->first_category;
    out << "\n" << nested << "  {\n";
    out << nested << "    \"rank\": " << rank << ",\n";
    out << nested << "    \"id\": "
        << inferlab_plan::json_string("repair_card_rank_readiness_link_" +
                                      std::to_string(rank))
        << ",\n";
    rank = inferlab::internal::checked_add_size(
        rank, 1U, "repair-card rank-to-readiness display id");
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(group->repair_environment) << ",\n";
    out << nested << "    \"first_artifact\": "
        << inferlab_plan::json_string(group->first_artifact) << ",\n";
    out << nested << "    \"first_category\": "
        << inferlab_plan::json_string(group->first_category) << ",\n";
    out << nested << "    \"primary_readiness_target\": "
        << inferlab_plan::json_string(primary_target) << ",\n";
    out << nested << "    \"report_ready_blocked\": "
        << (report_blocked ? "true" : "false") << ",\n";
    out << nested << "    \"replay_ready_blocked\": "
        << (replay_blocked ? "true" : "false") << ",\n";
    out << nested << "    \"execute_ready_blocked\": "
        << (execute_blocked ? "true" : "false") << ",\n";
    out << nested << "    \"report_ready_path_state\": "
        << inferlab_plan::json_string(report_ready_path_state) << ",\n";
    out << nested << "    \"replay_ready_path_state\": "
        << inferlab_plan::json_string(replay_ready_path_state) << ",\n";
    out << nested << "    \"execute_ready_path_state\": "
        << inferlab_plan::json_string(execute_ready_path_state) << ",\n";
    out << nested << "    \"report_ready_blocking_repair_card_count\": "
        << report_ready_blocker_count << ",\n";
    out << nested << "    \"replay_ready_blocking_repair_card_count\": "
        << replay_ready_blocker_count << ",\n";
    out << nested << "    \"execute_ready_blocking_repair_card_count\": "
        << execute_ready_blocker_count << ",\n";
    out << nested << "    \"source_location_count\": "
        << group->exact_wiring_locations.size() << ",\n";
    out << nested << "    \"source_location_gap\": "
        << (group->source_location_gap_card_count > 0U ? "true" : "false")
        << ",\n";
    out << nested << "    \"source_location_gap_card_count\": "
        << group->source_location_gap_card_count << ",\n";
    out << nested << "    \"suggest_repair_command\": "
        << inferlab_plan::json_string(
               selector.empty()
                   ? "inferlab_evidence_query --operation suggest_repair"
                   : "inferlab_evidence_query --operation suggest_repair --selector " +
                         selector)
        << ",\n";
    out << nested << "    \"path_command\": "
        << inferlab_plan::json_string(
               "inferlab_evidence_query --operation path --target " +
               primary_target)
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"rank-to-readiness linkage is a read-only inference from repair environment to readiness path; it does not execute repairs, mutate files, refresh artifacts, or prove readiness\"\n";
    out << nested << "  }";
  }
  if (!first) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"repair-card rank readiness linkage is read-only release triage metadata; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string shortest_path_json(
    const std::vector<RepairPathSummary>& paths,
    const std::string& target,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const RepairPathSummary* selected = find_repair_path(paths, target);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"target\": " << inferlab_plan::json_string(target)
      << ",\n";
  out << nested << "\"present\": " << (selected != nullptr ? "true" : "false");
  if (selected != nullptr) {
    out << ",\n";
    out << nested << "\"state\": "
        << inferlab_plan::json_string(selected->state) << ",\n";
    out << nested << "\"blocking_repair_card_count\": "
        << selected->blocking_repair_card_count << ",\n";
    out << nested << "\"catalog_flow_discrepancy_count\": "
        << selected->catalog_flow_discrepancy_count << ",\n";
    out << nested
        << "\"catalog_flow_surface_consumer_fallback_removal_blocked_count\": "
        << selected
               ->catalog_flow_surface_consumer_fallback_removal_blocked_count
        << ",\n";
    out << nested
        << "\"catalog_flow_surface_consumer_parity_refresh_required_count\": "
        << selected
               ->catalog_flow_surface_consumer_parity_refresh_required_count
        << ",\n";
    out << nested
        << "\"catalog_flow_surface_consumer_current_repo_action_count\": "
        << selected->catalog_flow_surface_consumer_current_repo_action_count
        << ",\n";
    out << nested
        << "\"catalog_flow_surface_consumer_target_machine_action_count\": "
        << selected->catalog_flow_surface_consumer_target_machine_action_count
        << ",\n";
    out << nested
        << "\"catalog_flow_consumer_field_coverage_repair_card_count\": "
        << selected->catalog_flow_consumer_field_coverage_repair_card_count
        << ",\n";
    out << nested
        << "\"catalog_flow_consumer_field_coverage_field_gap_count\": "
        << selected->catalog_flow_consumer_field_coverage_field_gap_count
        << ",\n";
    out << nested
        << "\"catalog_flow_consumer_field_coverage_parity_refresh_required_count\": "
        << selected
               ->catalog_flow_consumer_field_coverage_parity_refresh_required_count
        << ",\n";
    out << nested << "\"first_action\": "
        << inferlab_plan::json_string(selected->steps.empty()
                                          ? ""
                                          : selected->steps.front())
        << ",\n";
    out << nested << "\"steps\": "
        << inferlab_plan::json_string_array(selected->steps, indent + 2)
        << "\n";
  } else {
    out << "\n";
  }
  out << pad << "}";
  return out.str();
}


}  // namespace inferlab_evidence_query
