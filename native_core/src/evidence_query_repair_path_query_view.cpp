#include "evidence_query_repair_path_query_view.hpp"

#include "evidence_query_consumer_filter_signals.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

std::string root_cause_repair_cards_json(
    const EvidenceQueryReleaseSourceProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  bool first = true;
  for (const auto& card : projection.root_cause_repair_cards.cards) {
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "{\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(card.category) << ",\n";
    out << nested << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(card.source_locations,
                                            indent + 2)
        << "\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string repair_path_query_json(const EvidenceQueryRequest& request,
                                   const int indent) {
  const auto& release_sources = request.release_sources;
  const auto& catalog_flow_signals = request.catalog_flow;
  const auto& native_core_audit_signals = request.native_core_audit;
  const auto& shortest_paths = request.shortest_paths;
  const std::size_t root_cause_repair_cards =
      release_sources.root_cause_repair_card_count;
  const auto& root_cause_repair_count_projection =
      release_sources.repair_counts;
  const std::size_t current_repo_cards =
      release_sources.current_repo_repair_card_count;
  const std::size_t target_product_replay_cards =
      release_sources.target_product_replay_repair_card_count;
  const std::size_t target_gpu_execute_cards =
      release_sources.target_gpu_execute_repair_card_count;
  const std::size_t target_machine_cards =
      release_sources.target_machine_repair_card_count;
  const std::size_t flow_check_cards =
      release_sources.flow_check_repair_card_count;
  const std::size_t root_cause_source_location_cards =
      release_sources.root_cause_source_location_card_count;
  const std::size_t repair_path_source_location_count =
      release_sources.repair_path_source_location_count;
  const std::size_t source_location_gap_count =
      release_sources.repair_path_source_location_gap_count;
  const std::size_t source_gap_count = release_sources.source_gap_count;
  const std::size_t source_read_issue_count =
      release_sources.source_read_issue_count;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"source_artifacts\": [\n";
  out << nested << "  {\"artifact\": \"native_evidence_warehouse_index.json\", \"present\": true, \"readable\": true, \"read_status\": \"projected_from_query_snapshot\"},\n";
  out << nested << "  {\"artifact\": \"native_artifact_catalog_single_source.json\", \"present\": "
      << (catalog_flow_signals.catalog_artifact_present ? "true" : "false")
      << ", \"readable\": "
      << (catalog_flow_signals.catalog_artifact_readable ? "true" : "false")
      << ", \"read_status\": "
      << inferlab_plan::json_string(
             catalog_flow_signals.catalog_artifact_read_status)
      << "},\n";
  out << nested << "  {\"artifact\": \"native_core_audit_register.json\", \"present\": "
      << (native_core_audit_signals.audit_register_present ? "true" : "false")
      << ", \"readable\": "
      << (native_core_audit_signals.audit_register_readable ? "true" : "false")
      << ", \"read_status\": "
      << inferlab_plan::json_string(
             native_core_audit_signals.audit_register_read_status)
      << "}\n";
  out << nested << "],\n";
  out << nested << "\"source_gap_count\": " << source_gap_count << ",\n";
  out << nested << "\"source_read_issue_count\": "
      << source_read_issue_count << ",\n";
  out << nested << "\"source_location_count\": "
      << repair_path_source_location_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << source_location_gap_count << ",\n";
  out << nested << "\"root_cause\": {\n";
  out << nested << "  \"present\": true,\n";
  out << nested << "  \"readable\": true,\n";
  out << nested << "  \"read_status\": \"projected_from_query_snapshot\",\n";
  out << nested << "  \"projection_owner\": \"inferlab_evidence_query\",\n";
  out << nested << "  \"release_decision\": "
      << inferlab_plan::json_string(release_sources.release_decision)
      << ",\n";
  out << nested << "  \"top_root_cause\": "
      << inferlab_plan::json_string(release_sources.top_root_cause)
      << ",\n";
  out << nested << "  \"root_cause_count\": "
      << root_cause_repair_cards
      << ",\n";
  out << nested << "  \"blocking_root_cause_count\": "
      << release_sources.blocking_root_cause_count
      << ",\n";
  out << nested << "  \"repair_card_count\": "
      << root_cause_repair_cards << ",\n";
  out << nested << "  \"current_repo_repair_card_count\": "
      << current_repo_cards << ",\n";
  out << nested << "  \"target_product_replay_repair_card_count\": "
      << target_product_replay_cards << ",\n";
  out << nested << "  \"target_gpu_execute_repair_card_count\": "
      << target_gpu_execute_cards << ",\n";
  out << nested << "  \"target_machine_repair_card_count\": "
      << target_machine_cards << ",\n";
  out << nested << "  \"repair_environment_split_status\": "
      << inferlab_plan::json_string(
             root_cause_repair_count_projection.split_status)
      << ",\n";
  out << nested << "  \"repair_environment_split_schema_gap\": "
      << (root_cause_repair_count_projection.split_schema_gap ? "true"
                                                              : "false")
      << ",\n";
  out << nested << "  \"repair_environment_legacy_fallback_used\": "
      << (root_cause_repair_count_projection.legacy_fallback_used ? "true"
                                                                  : "false")
      << ",\n";
  out << nested << "  \"flow_check_repair_card_count\": "
      << flow_check_cards << ",\n";
  out << nested << "  \"source_location_card_count\": "
      << root_cause_source_location_cards << ",\n";
  out << nested << "  \"source_location_gap_count\": "
      << (root_cause_repair_cards > root_cause_source_location_cards
              ? root_cause_repair_cards - root_cause_source_location_cards
              : 0U)
      << ",\n";
  out << nested << "  \"has_root_cause_repair_cards\": "
      << (release_sources.root_cause_repair_cards_present
              ? "true"
              : "false")
      << "\n";
  out << nested << "},\n";
  out << nested << "\"governance_inputs\": {\n";
  out << nested << "  \"catalog_flow_discrepancy_count\": "
      << catalog_flow_signals.discrepancy_count << ",\n";
  out << nested << "  \"catalog_flow_repair_card_count\": "
      << catalog_flow_signals.repair_card_count << ",\n";
  out << nested << "  \"native_core_audit_filter_present\": true,\n";
  out << nested << "  \"native_core_audit_blocker_count\": "
      << native_core_audit_blocker_count(native_core_audit_signals) << ",\n";
  out << nested << "  \"native_core_audit_filter_decision\": "
      << inferlab_plan::json_string(
             native_core_audit_signals.filter_decision)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"shortest_paths\": [\n";
  bool first_path = true;
  for (const auto& path : shortest_paths) {
    if (!first_path) out << ",\n";
    first_path = false;
    const std::string first_action =
        path.steps.empty() ? "" : path.steps.front();
    out << nested << "  {\n";
    out << nested << "    \"target\": "
        << inferlab_plan::json_string(path.target) << ",\n";
    out << nested << "    \"state\": "
        << inferlab_plan::json_string(path.state) << ",\n";
    out << nested << "    \"blocking_repair_card_count\": "
        << path.blocking_repair_card_count << ",\n";
    out << nested << "    \"source_gap_count\": "
        << source_gap_count << ",\n";
    out << nested << "    \"source_read_issue_count\": "
        << source_read_issue_count << ",\n";
    out << nested << "    \"source_location_count\": "
        << repair_path_source_location_count << ",\n";
    out << nested << "    \"source_location_gap_count\": "
        << source_location_gap_count << ",\n";
    out << nested << "    \"catalog_flow_discrepancy_count\": "
        << catalog_flow_signals.discrepancy_count << ",\n";
    out << nested << "    \"native_core_audit_blocker_count\": "
        << native_core_audit_blocker_count(native_core_audit_signals)
        << ",\n";
    out << nested << "    \"first_action\": "
        << inferlab_plan::json_string(first_action) << ",\n";
    out << nested << "    \"steps\": "
        << inferlab_plan::json_string_array(path.steps, indent + 4) << "\n";
    out << nested << "  }";
  }
  if (!shortest_paths.empty()) out << "\n";
  out << nested << "],\n";
  out << nested << "\"claim_boundary\": \"repair-path query derives root-cause repair cards from the request-owned warehouse snapshot and consumes catalog and native-audit evidence; this view does not execute repairs, run products, or create benchmark evidence\"\n";
  out << pad << "}";
  return out.str();
}


}  // namespace inferlab_evidence_query
