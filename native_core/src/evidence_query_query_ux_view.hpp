#pragma once

#include "evidence_query_artifact_view.hpp"
#include "evidence_query_blocker_drilldown_view.hpp"
#include "evidence_query_operation_guidance_view.hpp"
#include "evidence_query_operation_catalog.hpp"
#include "evidence_query_readiness_path_view.hpp"
#include "evidence_query_repair_card_view.hpp"
#include "evidence_query_repair_collection_view.hpp"
#include "evidence_query_repair_group_view.hpp"
#include "evidence_query_repair_path_view.hpp"
#include "evidence_query_repair_source_view.hpp"
#include "evidence_query_selector_view.hpp"
#include "evidence_query_snapshot_diff_view.hpp"
#include "evidence_query_source_location_view.hpp"
#include "evidence_query_source_location_worklist_view.hpp"
#include "evidence_query_view_context.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

inline std::string query_ux_json(const EvidenceQueryViewContext& context,
                                 const int indent) {
  const auto& operation_view = context.operation_view;
  const auto& artifact_projection = operation_view.artifact_projection;
  const auto& operation = operation_view.operation;
  const auto& shortest_paths = operation_view.shortest_paths;
  const auto& repair_groups = operation_view.repair_groups;
  const auto& guidance = operation_view.guidance;
  const auto& resolution = operation_view.resolution;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"human_label\": "
      << inferlab_plan::json_string(operation.human_label) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(operation.selector) << ",\n";
  out << nested << "\"target\": "
      << inferlab_plan::json_string(operation.target) << ",\n";
  out << nested << "\"supported_operations\": "
      << inferlab_plan::json_string_array(
             evidence_query_operation_ux_forms(), indent + 2)
      << ",\n";
  out << nested << "\"list_artifacts\": "
      << artifact_list_json(artifact_projection, operation_view.query,
                            indent + 2)
      << ",\n";
  out << nested << "\"show_artifact\": "
      << show_artifact_json(artifact_projection.show_artifact, indent + 2)
      << ",\n";
  out << nested << "\"explain_tree\": "
      << explain_tree_json(operation_view.blocker_drilldown, operation,
                           indent + 2)
      << ",\n";
  out << nested << "\"blocker_drilldown\": "
      << blocker_drilldown_json(operation_view.blocker_drilldown, operation,
                                resolution, indent + 2)
      << ",\n";
  out << nested << "\"repair_cards\": "
      << repair_cards_json(context.repair_cards, indent + 2) << ",\n";
  out << nested << "\"selector_action_plan\": "
      << selector_action_plan_json(guidance, resolution, operation, indent + 2)
      << ",\n";
  out << nested << "\"repair_card_group_plan\": "
      << repair_card_group_plan_json(repair_groups, indent + 2) << ",\n";
  out << nested << "\"repair_narrative\": "
      << repair_narrative_json(guidance, resolution, operation, indent + 2)
      << ",\n";
  out << nested << "\"repair_group_summary\": "
      << repair_group_summary_json(repair_groups, indent + 2, &resolution)
      << ",\n";
  out << nested << "\"repair_group_navigation\": "
      << repair_group_navigation_json(repair_groups, indent + 2) << ",\n";
  out << nested << "\"source_location_repair_worklist\": "
      << source_location_repair_worklist_json(
             repair_groups, context.source_location_focus, indent + 2)
      << ",\n";
  out << nested << "\"repair_card_path_ranking\": "
      << repair_card_path_ranking_json(repair_groups, indent + 2) << ",\n";
  out << nested << "\"repair_card_rank_readiness_linkage\": "
      << repair_card_rank_readiness_linkage_json(
             repair_groups, shortest_paths, indent + 2)
      << ",\n";
  out << nested << "\"repair_card_source_exactness\": "
      << repair_card_source_exactness_json(context.source_exactness,
                                           indent + 2)
      << ",\n";
  out << nested << "\"source_location_category_coverage\": "
      << source_location_category_coverage_json(
             context.source_location_coverage, indent + 2)
      << ",\n";
  out << nested << "\"affected_claims\": "
      << affected_claims_json(context.repair_collection, indent + 2)
      << ",\n";
  out << nested << "\"exact_wiring_locations\": "
      << exact_wiring_locations_json(context.repair_collection, indent + 2)
      << ",\n";
  out << nested << "\"current_repo_actions\": "
      << action_array_for_environment_json(
             context.repair_collection, "current_repo_fixable", indent + 2)
      << ",\n";
  out << nested << "\"current_server_actions\": "
      << action_array_for_environment_json(
             context.repair_collection,
             "current_server_when_validation_enabled", indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << action_array_for_environment_json(
             context.repair_collection, "target_gpu_or_execute_machine",
             indent + 2)
      << ",\n";
  out << nested << "\"target_product_actions\": "
      << action_array_for_environment_json(
             context.repair_collection, "target_product_or_replay_evidence",
             indent + 2)
      << ",\n";
  out << nested << "\"shortest_path_to_report_ready\": "
      << shortest_path_json(shortest_paths, "report_ready", indent + 2)
      << ",\n";
  out << nested << "\"shortest_path_to_replay_ready\": "
      << shortest_path_json(shortest_paths, "replay_ready", indent + 2)
      << ",\n";
  out << nested << "\"shortest_path_to_execute_ready\": "
      << shortest_path_json(shortest_paths, "execute_ready", indent + 2)
      << ",\n";
  out << nested << "\"readiness_path_drilldown\": "
      << readiness_path_drilldown_json(guidance, operation, resolution,
                                       indent + 2)
      << ",\n";
  out << nested << "\"snapshot_diff\": "
      << snapshot_diff_json(operation_view.snapshot_diff, indent + 2)
      << ",\n";
  out << nested << "\"confidence_level\": \"read_only_evidence_query\",\n";
  out << nested
      << "\"claim_boundary\": \"query UX explains existing evidence and repair paths only; it does not execute commands, mutate files, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
