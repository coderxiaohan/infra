#include "evidence_query_result_artifact_sections.hpp"

#include "artifact_metadata_reader.hpp"
#include "evidence_query_artifact_support_view.hpp"
#include "evidence_query_artifact_view.hpp"
#include "evidence_query_blocker_drilldown_view.hpp"
#include "evidence_query_blocker_list_view.hpp"
#include "evidence_query_cli_contract.hpp"
#include "evidence_query_filter_view.hpp"
#include "evidence_query_query_ux_view.hpp"
#include "evidence_query_repair_card_view.hpp"
#include "evidence_query_repair_collection_view.hpp"
#include "evidence_query_repair_group_view.hpp"
#include "evidence_query_repair_path_query_view.hpp"
#include "evidence_query_repair_path_view.hpp"
#include "evidence_query_repair_source_view.hpp"
#include "evidence_query_selected_operation_view.hpp"
#include "evidence_query_snapshot_diff_view.hpp"
#include "evidence_query_source_location_view.hpp"
#include "evidence_query_source_location_worklist_view.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

void append_result_artifact_body(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& request = projection.request;
  const auto& warehouse = request.source.warehouse;
  const auto& scan_sources = request.source.scan_sources;
  const auto& query_artifacts = request.query_artifacts;
  const auto& query = request.query;
  const auto& blocker_filter = request.blocker_filter;
  const auto& blocker_list = request.blocker_list;
  const auto& query_view_context = projection.query_view;
  const auto& selected_operation_json_context =
      projection.selected_operation_view;
  const auto& snapshot_diff_signals = request.snapshot_diff;
  const auto& query_ux_blocker_drilldown = request.blocker_drilldown;
  const auto& query_operation = request.query_operation;
  const auto& query_ux_repair_cards = request.repair_cards;
  const auto& query_ux_repair_groups = request.repair_groups;
  const auto& query_ux_source_exactness_projection =
      request.source_exactness;
  const auto& query_ux_source_location_category_coverage =
      request.source_location_coverage;
  const auto& query_ux_repair_collection = request.repair_collection;
  const auto& shortest_paths = request.shortest_paths;
  const auto& catalog_flow_signals = request.catalog_flow;
  const auto& release_sources = request.release_sources;
  const auto& native_core_audit_signals = request.native_core_audit;
  const std::size_t matched_count = query_artifacts.matched_records.size();
  const std::size_t blocked_count = query_artifacts.blocked_result_count;
  out << "  \"query_plan\": "
      << query_plan_json(request, 2)
      << ",\n";
  out << "  \"warehouse_query\": "
      << inferlab_artifact::warehouse_index_json(warehouse, 2) << ",\n";
  out << "  \"direct_scan_source_files\": "
      << source_files_json(scan_sources, 2) << ",\n";
  out << "  \"results\": "
      << results_json(query_artifacts, query, 2)
      << ",\n";
  out << "  \"blockers\": " << blockers_json(blocker_list, 2) << ",\n";
  out << "  \"top_blockers\": " << top_blockers_json(blocker_list, 2)
      << ",\n";
  out << "  \"repair_plan\": " << repair_plan_json(blocker_list, 2)
      << ",\n";
  out << "  \"query_ux\": "
      << query_ux_json(query_view_context, 2)
      << ",\n";
  out << "  \"selected_operation_result\": "
      << selected_operation_result_json(selected_operation_json_context, 2)
      << ",\n";
  out << "  \"snapshot_diff\": "
      << snapshot_diff_json(snapshot_diff_signals, 2)
      << ",\n";
  out << "  \"explain_tree\": "
      << explain_tree_json(query_ux_blocker_drilldown, query_operation, 2)
      << ",\n";
  out << "  \"repair_cards\": "
      << repair_cards_json(query_ux_repair_cards, 2)
      << ",\n";
  out << "  \"root_cause_repair_cards\": "
      << root_cause_repair_cards_json(release_sources, 2)
      << ",\n";
  out << "  \"repair_card_group_plan\": "
      << repair_card_group_plan_json(query_ux_repair_groups, 2)
      << ",\n";
  out << "  \"repair_group_summary\": "
      << repair_group_summary_json(query_ux_repair_groups, 2)
      << ",\n";
  out << "  \"repair_group_navigation\": "
      << repair_group_navigation_json(query_ux_repair_groups, 2)
      << ",\n";
  out << "  \"source_location_repair_worklist\": "
      << source_location_repair_worklist_json(
             query_ux_repair_groups, projection.source_location_focus, 2)
      << ",\n";
  out << "  \"repair_card_path_ranking\": "
      << repair_card_path_ranking_json(query_ux_repair_groups, 2)
      << ",\n";
  out << "  \"repair_card_rank_readiness_linkage\": "
      << repair_card_rank_readiness_linkage_json(query_ux_repair_groups,
                                                 shortest_paths, 2)
      << ",\n";
  out << "  \"repair_card_source_exactness\": "
      << repair_card_source_exactness_json(
             query_ux_source_exactness_projection, 2)
      << ",\n";
  out << "  \"source_location_category_coverage\": "
      << source_location_category_coverage_json(
             query_ux_source_location_category_coverage, 2)
      << ",\n";
  out << "  \"affected_claims\": "
      << affected_claims_json(query_ux_repair_collection, 2)
      << ",\n";
  out << "  \"exact_wiring_locations\": "
      << exact_wiring_locations_json(query_ux_repair_collection, 2)
      << ",\n";
  out << "  \"current_repo_actions\": "
      << action_array_for_environment_json(query_ux_repair_collection,
                                           "current_repo_fixable", 2)
      << ",\n";
  out << "  \"current_server_actions\": "
      << action_array_for_environment_json(
             query_ux_repair_collection,
             "current_server_when_validation_enabled", 2)
      << ",\n";
  out << "  \"target_machine_actions\": "
      << action_array_for_environment_json(
             query_ux_repair_collection,
             "target_gpu_or_execute_machine", 2)
      << ",\n";
  out << "  \"target_product_actions\": "
      << action_array_for_environment_json(
             query_ux_repair_collection,
             "target_product_or_replay_evidence", 2)
      << ",\n";
  out << "  \"shortest_path_to_report_ready\": "
      << shortest_path_json(shortest_paths, "report_ready", 2) << ",\n";
  out << "  \"shortest_path_to_replay_ready\": "
      << shortest_path_json(shortest_paths, "replay_ready", 2) << ",\n";
  out << "  \"shortest_path_to_execute_ready\": "
      << shortest_path_json(shortest_paths, "execute_ready", 2) << ",\n";
  out << "  \"confidence_level\": \"read_only_evidence_query\",\n";
  out << "  \"repair_path_query\": "
      << repair_path_query_json(request, 2)
      << ",\n";
  out << "  \"catalog_flow_filter_query\": "
      << catalog_flow_filter_query_json(catalog_flow_signals, 2) << ",\n";
  out << "  \"native_core_audit_filter_query\": "
      << native_core_audit_filter_query_json(native_core_audit_signals, 2)
      << ",\n";
  out << "  \"environment_split\": "
      << environment_split_json(blocker_list, 2) << ",\n";
  out << "  \"explain_repair_linkage\": {\n";
  out << "    \"artifact\": \"native_evidence_query_result.json\",\n";
  out << "    \"target\": \"inferlab_evidence_query\",\n";
  out << "    \"fields\": [\"explain_tree\", \"repair_cards\", \"exact_wiring_locations\", \"shortest_path_to_report_ready\", \"shortest_path_to_replay_ready\", \"shortest_path_to_execute_ready\"],\n";
  out << "    \"claim_boundary\": \"Evidence Query explains existing evidence and repair paths only; it does not execute repairs or create benchmark evidence\"\n";
  out << "  },\n";
  out << "  \"human_readable_summary\": "
      << human_summary_json(matched_count, blocked_count, query, blocker_filter, 2)
      << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             {"./build/inferlab_evidence_warehouse_index --repo-root . --results results/raw --output results/raw/native_evidence_warehouse_index.json --format json",
              "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json",
              "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query catalog_flow_discrepancy --output results/raw/native_evidence_query_result.json --format json",
              "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query native_core_audit --output results/raw/native_evidence_query_result.json --format json",
              "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query release_blockers --output results/raw/native_evidence_query_result.json --format json",
              inferlab_evidence_query_cli::selector_repair_card_export_command(),
              "./build/inferlab_evidence_query --repo-root . --fixture-dir configs/evidence_fixtures --query release_blockers --output results/raw/native_evidence_query_result.json --format json",
              "./build/inferlab_release_evidence_pipeline --repo-root . --results results/raw --output results/raw/native_release_evidence_pipeline.json --format json"},
             2)
      << "\n";
  out << "}\n";

}

}  // namespace inferlab_evidence_query::detail
