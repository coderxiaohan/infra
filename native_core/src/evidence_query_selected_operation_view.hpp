#pragma once

#include "evidence_query_artifact_view.hpp"
#include "evidence_query_blocker_drilldown_view.hpp"
#include "evidence_query_operation_guidance_view.hpp"
#include "evidence_query_operation_policy.hpp"
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

struct SelectedOperationJsonContext {
  const EvidenceQueryViewContext& view;
  const SelectedOperationProjection& selected_operation;
};

inline std::string selected_operation_result_json(
    const SelectedOperationJsonContext& context,
    const int indent) {
  const auto& view = context.view;
  const auto& operation_view = view.operation_view;
  const auto& artifact_projection = operation_view.artifact_projection;
  const auto& operation = operation_view.operation;
  const auto& shortest_paths = operation_view.shortest_paths;
  const auto& repair_groups = operation_view.repair_groups;
  const auto& guidance = operation_view.guidance;
  const auto& resolution = operation_view.resolution;
  const auto& snapshot_diff = operation_view.snapshot_diff;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string& readiness_target = guidance.readiness_target;
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
  out << nested << "\"result_kind\": "
      << inferlab_plan::json_string(context.selected_operation.result_kind)
      << ",\n";
  out << nested << "\"selector_recommended\": "
      << inferlab_plan::json_bool(operation_selector_recommended(operation))
      << ",\n";
  out << nested << "\"selector_present\": "
      << inferlab_plan::json_bool(!operation.selector.empty()) << ",\n";
  out << nested << "\"target_required\": "
      << inferlab_plan::json_bool(operation_target_required(operation))
      << ",\n";
  out << nested << "\"target_present\": "
      << inferlab_plan::json_bool(!operation.target.empty()) << ",\n";
  out << nested << "\"snapshot_inputs_required\": "
      << inferlab_plan::json_bool(
             operation_snapshot_inputs_required(operation))
      << ",\n";
  out << nested << "\"snapshot_inputs_configured\": "
      << inferlab_plan::json_bool(snapshot_diff.before_configured &&
                                  snapshot_diff.after_configured)
      << ",\n";
  out << nested << "\"input_ready\": "
      << inferlab_plan::json_bool(context.selected_operation.input_ready)
      << ",\n";
  out << nested << "\"payload_count\": "
      << context.selected_operation.payload_count << ",\n";
  out << nested << "\"read_only\": true,\n";
  out << nested << "\"executes_commands\": false,\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(operation_current_repo_action(operation))
      << ",\n";
  out << nested << "\"target_machine_action\": "
      << inferlab_plan::json_string(operation_target_machine_action(operation))
      << ",\n";
  out << nested << "\"selector_resolution\": "
      << selector_resolution_json(resolution, indent + 2) << ",\n";
  out << nested << "\"selector_action_plan\": "
      << selector_action_plan_json(guidance, resolution, operation, indent + 2)
      << ",\n";

  if (operation.operation == "list_artifacts") {
    out << nested << "\"artifact_list\": "
        << artifact_list_json(artifact_projection, operation_view.query,
                              indent + 2)
        << ",\n";
  } else if (operation.operation == "show_artifact") {
    out << nested << "\"artifact_detail\": "
        << show_artifact_json(artifact_projection.show_artifact, indent + 2)
        << ",\n";
  } else if (operation.operation == "explain_blocker") {
    out << nested << "\"explain_tree\": "
        << explain_tree_json(operation_view.blocker_drilldown, operation,
                             indent + 2)
        << ",\n";
    out << nested << "\"blocker_drilldown\": "
        << blocker_drilldown_json(operation_view.blocker_drilldown, operation,
                                  resolution, indent + 2)
        << ",\n";
    out << nested << "\"affected_claims\": "
        << affected_claims_json(view.repair_collection, indent + 2)
        << ",\n";
    out << nested << "\"exact_wiring_locations\": "
        << exact_wiring_locations_json(view.repair_collection, indent + 2)
        << ",\n";
    out << nested << "\"source_location_category_coverage\": "
        << source_location_category_coverage_json(
               view.source_location_coverage, indent + 2)
        << ",\n";
  } else if (operation.operation == "suggest_repair") {
    out << nested << "\"repair_cards\": "
        << repair_cards_json(view.repair_cards, indent + 2) << ",\n";
    out << nested << "\"repair_card_group_plan\": "
        << repair_card_group_plan_json(repair_groups, indent + 2) << ",\n";
    out << nested << "\"repair_narrative\": "
        << repair_narrative_json(guidance, resolution, operation, indent + 2)
        << ",\n";
    out << nested << "\"repair_card_path_ranking\": "
        << repair_card_path_ranking_json(repair_groups, indent + 2) << ",\n";
    out << nested << "\"repair_card_rank_readiness_linkage\": "
        << repair_card_rank_readiness_linkage_json(
               repair_groups, shortest_paths, indent + 2)
        << ",\n";
    out << nested << "\"repair_card_source_exactness\": "
        << repair_card_source_exactness_json(view.source_exactness,
                                             indent + 2)
        << ",\n";
    out << nested << "\"source_location_category_coverage\": "
        << source_location_category_coverage_json(
               view.source_location_coverage, indent + 2)
        << ",\n";
    out << nested << "\"current_repo_actions\": "
        << action_array_for_environment_json(
               view.repair_collection, "current_repo_fixable", indent + 2)
        << ",\n";
    out << nested << "\"current_server_actions\": "
        << action_array_for_environment_json(
               view.repair_collection,
               "current_server_when_validation_enabled", indent + 2)
        << ",\n";
    out << nested << "\"target_machine_actions\": "
        << action_array_for_environment_json(
               view.repair_collection, "target_gpu_or_execute_machine",
               indent + 2)
        << ",\n";
    out << nested << "\"target_product_actions\": "
        << action_array_for_environment_json(
               view.repair_collection,
               "target_product_or_replay_evidence", indent + 2)
        << ",\n";
  } else if (operation.operation == "repair_group_summary") {
    out << nested << "\"repair_group_summary\": "
        << repair_group_summary_json(repair_groups, indent + 2, &resolution)
        << ",\n";
    out << nested << "\"repair_group_navigation\": "
        << repair_group_navigation_json(repair_groups, indent + 2) << ",\n";
    out << nested << "\"source_location_repair_worklist\": "
        << source_location_repair_worklist_json(
               repair_groups, view.source_location_focus, indent + 2)
        << ",\n";
    out << nested << "\"repair_card_path_ranking\": "
        << repair_card_path_ranking_json(repair_groups, indent + 2) << ",\n";
    out << nested << "\"repair_card_rank_readiness_linkage\": "
        << repair_card_rank_readiness_linkage_json(
               repair_groups, shortest_paths, indent + 2)
        << ",\n";
    out << nested << "\"repair_card_source_exactness\": "
        << repair_card_source_exactness_json(view.source_exactness,
                                             indent + 2)
        << ",\n";
    out << nested << "\"source_location_category_coverage\": "
        << source_location_category_coverage_json(
               view.source_location_coverage, indent + 2)
        << ",\n";
    out << nested << "\"repair_card_group_plan\": "
        << repair_card_group_plan_json(repair_groups, indent + 2) << ",\n";
    out << nested << "\"affected_claims\": "
        << affected_claims_json(view.repair_collection, indent + 2)
        << ",\n";
    out << nested << "\"exact_wiring_locations\": "
        << exact_wiring_locations_json(view.repair_collection, indent + 2)
        << ",\n";
  } else if (operation.operation == "diff_snapshots") {
    out << nested << "\"snapshot_diff\": "
        << snapshot_diff_json(snapshot_diff, indent + 2) << ",\n";
  } else if (operation.operation == "path") {
    out << nested << "\"readiness_path\": "
        << shortest_path_json(shortest_paths, readiness_target, indent + 2)
        << ",\n";
    out << nested << "\"readiness_path_drilldown\": "
        << readiness_path_drilldown_json(guidance, operation, resolution,
                                         indent + 2)
        << ",\n";
  } else {
    out << nested << "\"warehouse_query_results\": "
        << results_json(artifact_projection, operation_view.query, indent + 2)
        << ",\n";
  }

  out << nested
      << "\"claim_boundary\": \"selected operation result is a read-only projection of existing Evidence Query data for the requested operation; it does not execute commands, refresh artifacts, mutate files, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
