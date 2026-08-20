#include "evidence_query_selector_export_artifact_view.hpp"

#include "evidence_query_selection.hpp"
#include "evidence_query_selector_view.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_evidence_query {

std::string render_selector_repair_card_export_artifact(
    const EvidenceQueryRequest& request) {
  const bool fixture_used = request.source.fixture_used;
  const auto& query = request.query;
  const auto& artifact_filter = request.artifact_filter;
  const auto& blocker_filter = request.blocker_filter;
  const auto& selector_repair_card_output =
      request.selector_repair_card_output;
  const auto& query_operation = request.query_operation;
  const auto& resolution = request.selector_resolution;
  const auto& write_status =
      request.selector_repair_card_export_write_status;

  const std::size_t current_repo_card_count =
      selector_repair_card_environment_count(resolution,
                                             "current_repo_fixable");
  const std::size_t current_server_card_count =
      selector_repair_card_environment_count(
          resolution, "current_server_when_validation_enabled");
  const std::size_t target_machine_card_count =
      selector_repair_card_environment_count(
          resolution, "target_gpu_or_execute_machine");
  const std::size_t target_product_card_count =
      selector_repair_card_environment_count(
          resolution, "target_product_or_replay_evidence");
  const std::size_t source_location_gap_card_count =
      selector_repair_card_source_location_gap_card_count(resolution);
  const std::string export_review_status =
      selector_repair_card_export_review_status(
          resolution, selector_repair_card_output.string(), write_status);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(
             "native_evidence_query_selector_repair_card_export")
      << ",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(
             "selector repair-card export only; does not execute repairs or create benchmark/replay/execute evidence")
      << ",\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(export_review_status) << ",\n";
  out << "  \"query\": {\n";
  out << "    \"name\": " << inferlab_plan::json_string(query) << ",\n";
  out << "    \"normalized_query\": "
      << inferlab_plan::json_string(query_operation.normalized_query)
      << ",\n";
  out << "    \"operation\": "
      << inferlab_plan::json_string(query_operation.operation) << ",\n";
  out << "    \"selector\": "
      << inferlab_plan::json_string(query_operation.selector) << ",\n";
  out << "    \"target\": "
      << inferlab_plan::json_string(query_operation.target) << ",\n";
  out << "    \"artifact_filter\": "
      << inferlab_plan::json_string(artifact_filter) << ",\n";
  out << "    \"blocker_filter\": "
      << inferlab_plan::json_string(blocker_filter) << ",\n";
  out << inferlab_plan::redacted_path_json_fields(
      "selector_repair_card_output", selector_repair_card_output.string(), 4);
  out << "    \"fixture_used\": " << (fixture_used ? "true" : "false")
      << ",\n";
  out << "    \"warehouse_used\": "
      << (request.source.warehouse_used ? "true" : "false") << ",\n";
  out << "    \"direct_scan_used\": "
      << (request.source.direct_scan_used ? "true" : "false") << "\n";
  out << "  },\n";
  out << "  \"selector_resolution_summary\": {\n";
  out << "    \"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << "    \"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << "    \"resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << "    \"total_match_count\": " << resolution.total_match_count
      << ",\n";
  out << "    \"ambiguous\": "
      << (resolution.ambiguous ? "true" : "false") << ",\n";
  out << "    \"first_artifact\": "
      << inferlab_plan::json_string(resolution.first_artifact) << ",\n";
  out << "    \"first_category\": "
      << inferlab_plan::json_string(resolution.first_category) << ",\n";
  out << "    \"first_action\": "
      << inferlab_plan::json_string(resolution.first_action) << ",\n";
  out << "    \"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << "\n";
  out << "  },\n";
  out << "  \"export_contract\": {\n";
  out << "    \"export_status\": \"full_external_export\",\n";
  out << "    \"generated_by_cli_option\": "
      << inferlab_plan::json_string("--selector-repair-card-output")
      << ",\n";
  out << "    \"total_card_count\": " << resolution.repair_card_count
      << ",\n";
  out << "    \"full_card_count\": " << resolution.all_repair_cards.size()
      << ",\n";
  out << "    \"embedded_card_limit\": " << kMaxSelectorRepairCards
      << ",\n";
  out << "    \"full_export_available_in_current_artifact\": true,\n";
  out << "    \"external_export_written\": true,\n";
  out << "    \"write_status\": "
      << inferlab_plan::json_string(write_status) << ",\n";
  out << "    \"claim_boundary\": "
      << inferlab_plan::json_string(
             "full selector repair-card export is still read-only metadata; it does not execute repair commands, mutate repository files, refresh evidence, or create benchmark/replay/execute evidence")
      << "\n";
  out << "  },\n";
  out << "  \"export_review_focus\":\n"
      << selector_repair_card_export_review_focus_json(
             resolution, 2, selector_repair_card_output.string(),
             write_status)
      << ",\n";
  out << "  \"repair_cards\": "
      << selector_repair_cards_json(resolution.all_repair_cards, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"selector_repair_card_full_export_count\": "
      << resolution.all_repair_cards.size() << ",\n";
  out << "    \"selector_repair_card_current_repo_count\": "
      << current_repo_card_count << ",\n";
  out << "    \"selector_repair_card_current_server_count\": "
      << current_server_card_count << ",\n";
  out << "    \"selector_repair_card_target_machine_count\": "
      << target_machine_card_count << ",\n";
  out << "    \"selector_repair_card_target_product_count\": "
      << target_product_card_count << ",\n";
  out << "    \"selector_repair_card_source_location_gap_count\": "
      << source_location_gap_card_count << ",\n";
  out << "    \"selector_repair_card_export_review_status\": "
      << inferlab_plan::json_string(export_review_status)
      << ",\n";
  out << "    \"selector_repair_card_export_blocker_reason\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_blocker_reason(
                 resolution, selector_repair_card_output.string(),
                 write_status))
      << ",\n";
  out << "    \"selector_repair_card_external_file_required\": "
      << (selector_repair_card_external_file_required(
              resolution, write_status)
              ? "true"
              : "false")
      << "\n";
  out << "  },\n";
  out << "  \"claim_boundary\": "
      << inferlab_plan::json_string(
             "selector repair-card export is a read-only operator aid; generated cards are not repair execution, target-machine validation, product replay, product execute, CUDA timing, or benchmark evidence")
      << "\n";
  out << "}\n";
  return out.str();
}


}  // namespace inferlab_evidence_query
