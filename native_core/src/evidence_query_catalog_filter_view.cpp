#include "evidence_query_filter_view.hpp"

#include "evidence_catalog_fallback_removal_projection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

std::string catalog_flow_field_coverage_repair_cards_json(
    const CatalogFlowFilterSignals& signals,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string inner = inferlab_plan::json_indent(indent + 4);
  std::ostringstream out;
  bool first = true;
  auto emit_card = [&](const std::string& id, const std::string& category,
                       const std::string& environment,
                       const std::string& trigger,
                       const std::string& action,
                       const std::vector<std::string>& locations,
                       const std::vector<std::pair<std::string, std::size_t>>&
                           counters) {
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "{\n";
    out << nested << "  \"id\": " << inferlab_plan::json_string(id)
        << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(environment) << ",\n";
    out << nested << "  \"severity\": \"high\",\n";
    out << nested << "  \"trigger\": " << inferlab_plan::json_string(trigger)
        << ",\n";
    out << nested << "  \"repair_action\": "
        << inferlab_plan::json_string(action) << ",\n";
    out << nested << "  \"affected_claims\": "
        << inferlab_plan::json_string_array(
               {"catalog_single_source_consumption",
                "release_report_readiness",
                "non_gpu_review_closure"},
               indent + 2)
        << ",\n";
    out << nested << "  \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(locations, indent + 2) << ",\n";
    out << nested << "  \"source_location_count\": " << locations.size()
        << ",\n";
    out << nested << "  \"source_location_gap_count\": 0,\n";
    out << nested << "  \"evidence_counters\": [";
    for (std::size_t i = 0; i < counters.size(); ++i) {
      if (i > 0) out << ",";
      out << "\n" << inner << "{\"name\": "
          << inferlab_plan::json_string(counters[i].first)
          << ", \"value\": " << counters[i].second << "}";
    }
    if (!counters.empty()) out << "\n" << nested << "  ";
    out << "],\n";
    out << nested
        << "  \"claim_boundary\": \"catalog field-coverage repair cards are read-only migration guidance; they do not refresh artifacts, remove fallback lists, run validation, replay products, or prove execute readiness\"\n";
    out << nested << "}";
  };

  out << "[";
  if (signals.consumer_field_coverage_current_repo_repair_card_count > 0) {
    emit_card(
        "catalog_flow_field_coverage_required_field_gap",
        "catalog_flow_field_coverage_gap", "current_repo_fixable",
        "catalog consumer field coverage reports missing required registry fields",
        "complete required catalog metadata and derived-view field coverage, then refresh catalog, Evidence Query, release pipeline, summary, Prometheus, and final report when validation is enabled",
        {"native_core/src/high_value_artifact_catalog.hpp",
         "native_core/src/catalog_derived_views.hpp",
         "native_core/src/artifact_catalog_single_source_main.cpp",
         "native_core/src/evidence_query_main.cpp"},
        {{"catalog_flow_consumer_field_coverage_required_field_count",
          signals.consumer_field_coverage_required_field_count},
         {"catalog_flow_consumer_field_coverage_covered_field_count",
          signals.consumer_field_coverage_covered_field_count},
         {"catalog_flow_consumer_field_coverage_field_gap_count",
          signals.consumer_field_coverage_field_gap_count},
         {"catalog_flow_consumer_field_coverage_current_repo_action_count",
          signals.consumer_field_coverage_current_repo_action_count}});
  }
  if (signals.consumer_field_coverage_target_machine_repair_card_count > 0) {
    emit_card(
        "catalog_flow_field_coverage_parity_refresh_required",
        "catalog_flow_field_coverage_parity_refresh",
        "current_server_when_validation_enabled",
        "catalog consumer field coverage still has fallback surfaces or parity refresh requirements",
        "after current-repo catalog field fixes, refresh downstream artifacts and report/export surfaces before removing legacy fallback lists",
        {"native_core/src/deep_dive_contract_check_main.cpp",
         "scripts/summarize_benchmarks.py",
         "scripts/export_prometheus_metrics.py",
         "results/raw/native_release_evidence_pipeline.json",
         "results/raw/native_evidence_query_result.json"},
        {{"catalog_flow_consumer_field_coverage_surface_count",
          signals.consumer_field_coverage_surface_count},
         {"catalog_flow_consumer_field_coverage_fallback_surface_count",
          signals.consumer_field_coverage_fallback_surface_count},
         {"catalog_flow_consumer_field_coverage_parity_refresh_required_count",
          signals.consumer_field_coverage_parity_refresh_required_count},
         {"catalog_flow_consumer_field_coverage_target_machine_action_count",
          signals.consumer_field_coverage_target_machine_action_count}});
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}


}  // namespace

using inferlab_evidence::catalog_flow_fallback_removal_parity_gate_active;
using inferlab_evidence::catalog_flow_fallback_removal_queue_active;
using inferlab_evidence::catalog_flow_fallback_removal_queue_environment;
using inferlab_evidence::catalog_flow_fallback_removal_queue_next_action;
using inferlab_evidence::catalog_flow_fallback_removal_queue_state;
using inferlab_evidence::catalog_flow_fallback_removal_shortest_path_active;

std::string catalog_flow_filter_query_json(
    const CatalogFlowFilterSignals& signals,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"query_filter\": \"catalog_flow_discrepancy\",\n";
  out << nested << "\"source_artifact\": \"native_artifact_catalog_single_source.json\",\n";
  out << nested << "\"catalog_artifact_present\": "
      << (signals.catalog_artifact_present ? "true" : "false") << ",\n";
  out << nested << "\"catalog_artifact_readable\": "
      << (signals.catalog_artifact_readable ? "true" : "false") << ",\n";
  out << nested << "\"catalog_artifact_read_status\": "
      << inferlab_plan::json_string(signals.catalog_artifact_read_status)
      << ",\n";
  out << nested << "\"read_issue_count\": " << signals.read_issue_count
      << ",\n";
  out << nested << "\"filter_decision\": "
      << inferlab_plan::json_string(signals.filter_decision) << ",\n";
  out << nested << "\"catalog_flow_input_gap_count\": "
      << signals.input_gap_count << ",\n";
  out << nested << "\"catalog_flow_missing_surface_count\": "
      << signals.missing_surface_count << ",\n";
  out << nested << "\"catalog_flow_fallback_retained_surface_count\": "
      << signals.fallback_retained_surface_count << ",\n";
  out << nested << "\"catalog_flow_single_source_repair_card_count\": "
      << signals.single_source_repair_card_count << ",\n";
  out << nested << "\"catalog_flow_single_source_parity_refresh_required_count\": "
      << signals.single_source_parity_refresh_required_count << ",\n";
  out << nested << "\"catalog_flow_surface_migration_ready_count\": "
      << signals.surface_migration_ready_count << ",\n";
  out << nested << "\"catalog_flow_surface_migration_fallback_count\": "
      << signals.surface_migration_fallback_count << ",\n";
  out << nested << "\"catalog_flow_surface_parity_refresh_required_count\": "
      << signals.surface_parity_refresh_required_count << ",\n";
  out << nested << "\"catalog_flow_surface_current_repo_action_count\": "
      << signals.surface_current_repo_action_count << ",\n";
  out << nested << "\"catalog_flow_surface_target_machine_action_count\": "
      << signals.surface_target_machine_action_count << ",\n";
  out << nested << "\"catalog_flow_surface_consumer_migration_plan_count\": "
      << signals.surface_consumer_migration_plan_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_fallback_removal_blocked_count\": "
      << signals.surface_consumer_fallback_removal_blocked_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_parity_refresh_required_count\": "
      << signals.surface_consumer_parity_refresh_required_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_current_repo_action_count\": "
      << signals.surface_consumer_current_repo_action_count << ",\n";
  out << nested
      << "\"catalog_flow_surface_consumer_target_machine_action_count\": "
      << signals.surface_consumer_target_machine_action_count << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_item_count\": "
      << signals.fallback_removal_queue_item_count << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_blocked_count\": "
      << signals.fallback_removal_queue_blocked_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_queue_parity_refresh_required_count\": "
      << signals.fallback_removal_queue_parity_refresh_required_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_queue_current_repo_action_count\": "
      << signals.fallback_removal_queue_current_repo_action_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_queue_target_machine_action_count\": "
      << signals.fallback_removal_queue_target_machine_action_count << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_state\": "
      << inferlab_plan::json_string(
             catalog_flow_fallback_removal_queue_state(signals))
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_environment\": "
      << inferlab_plan::json_string(
             catalog_flow_fallback_removal_queue_environment(signals))
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_next_action\": "
      << inferlab_plan::json_string(
             catalog_flow_fallback_removal_queue_next_action(signals))
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_queue_blocks_removal\": "
      << (catalog_flow_fallback_removal_queue_active(signals) ? "true"
                                                              : "false")
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_shortest_path_present\": "
      << (signals.fallback_removal_shortest_path_present ? "true" : "false")
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_shortest_path_state\": "
      << inferlab_plan::json_string(
             signals.fallback_removal_shortest_path_state)
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_shortest_path_step_count\": "
      << signals.fallback_removal_shortest_path_step_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_current_repo_step_count\": "
      << signals.fallback_removal_shortest_path_current_repo_step_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_target_machine_step_count\": "
      << signals.fallback_removal_shortest_path_target_machine_step_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_source_location_gap_count\": "
      << signals.fallback_removal_shortest_path_source_location_gap_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_primary_environment\": "
      << inferlab_plan::json_string(
             signals.fallback_removal_shortest_path_primary_environment)
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_first_surface\": "
      << inferlab_plan::json_string(
             signals.fallback_removal_shortest_path_first_surface)
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_blocks_removal\": "
      << (catalog_flow_fallback_removal_shortest_path_active(signals) ? "true"
                                                                      : "false")
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_shortest_path_next_action\": "
      << inferlab_plan::json_string(
             catalog_flow_fallback_removal_shortest_path_next_action(signals))
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_parity_gate_present\": "
      << (signals.fallback_removal_parity_gate_present ? "true" : "false")
      << ",\n";
  out << nested << "\"catalog_flow_fallback_removal_parity_gate_decision\": "
      << inferlab_plan::json_string(
             signals.fallback_removal_parity_gate_decision)
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_removal_allowed\": "
      << (signals.fallback_removal_parity_gate_removal_allowed ? "true"
                                                               : "false")
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_blocker_count\": "
      << signals.fallback_removal_parity_gate_blocker_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_blocking_surface_count\": "
      << signals.fallback_removal_parity_gate_blocking_surface_count << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_fallback_surface_count\": "
      << signals.fallback_removal_parity_gate_fallback_surface_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count\": "
      << signals.fallback_removal_parity_gate_parity_refresh_required_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_current_repo_action_count\": "
      << signals.fallback_removal_parity_gate_current_repo_action_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_target_machine_action_count\": "
      << signals.fallback_removal_parity_gate_target_machine_action_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_source_location_gap_count\": "
      << signals.fallback_removal_parity_gate_source_location_gap_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count\": "
      << signals.fallback_removal_parity_gate_blocking_surface_source_location_gap_count
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_first_surface\": "
      << inferlab_plan::json_string(
             signals.fallback_removal_parity_gate_first_surface)
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_blocks_removal\": "
      << (catalog_flow_fallback_removal_parity_gate_active(signals) ? "true"
                                                                    : "false")
      << ",\n";
  out << nested
      << "\"catalog_flow_fallback_removal_parity_gate_next_action\": "
      << inferlab_plan::json_string(
             catalog_flow_fallback_removal_parity_gate_next_action(signals))
      << ",\n";
  out << nested << "\"catalog_flow_consumer_field_coverage_surface_count\": "
      << signals.consumer_field_coverage_surface_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_catalog_managed_surface_count\": "
      << signals.consumer_field_coverage_catalog_managed_surface_count
      << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_fallback_surface_count\": "
      << signals.consumer_field_coverage_fallback_surface_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_required_field_count\": "
      << signals.consumer_field_coverage_required_field_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_covered_field_count\": "
      << signals.consumer_field_coverage_covered_field_count << ",\n";
  out << nested << "\"catalog_flow_consumer_field_coverage_field_gap_count\": "
      << signals.consumer_field_coverage_field_gap_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_parity_refresh_required_count\": "
      << signals.consumer_field_coverage_parity_refresh_required_count
      << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_current_repo_action_count\": "
      << signals.consumer_field_coverage_current_repo_action_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_target_machine_action_count\": "
      << signals.consumer_field_coverage_target_machine_action_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_repair_card_count\": "
      << signals.consumer_field_coverage_repair_card_count << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_current_repo_repair_card_count\": "
      << signals.consumer_field_coverage_current_repo_repair_card_count
      << ",\n";
  out << nested
      << "\"catalog_flow_consumer_field_coverage_target_machine_repair_card_count\": "
      << signals.consumer_field_coverage_target_machine_repair_card_count
      << ",\n";
  out << nested << "\"catalog_flow_consumer_field_coverage_repair_cards\": "
      << catalog_flow_field_coverage_repair_cards_json(signals, indent + 2)
      << ",\n";
  out << nested << "\"catalog_flow_metadata_policy_card_count\": "
      << signals.metadata_policy_card_count << ",\n";
  out << nested << "\"catalog_flow_metadata_derived_policy_artifact_count\": "
      << signals.metadata_derived_policy_artifact_count << ",\n";
  out << nested << "\"catalog_flow_metadata_derived_policy_field_count\": "
      << signals.metadata_derived_policy_field_count << ",\n";
  out << nested << "\"catalog_flow_missing_step_count\": "
      << signals.missing_step_count << ",\n";
  out << nested << "\"catalog_flow_repair_card_count\": "
      << signals.repair_card_count << ",\n";
  out << nested << "\"catalog_flow_discrepancy_count\": "
      << signals.discrepancy_count << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(
	             {"inspect native_artifact_catalog_single_source.consumer_field_coverage for field-level catalog consumer gaps",
	              "inspect native_artifact_catalog_single_source.metadata_completeness_review and metadata_policy_cards",
	              "repair catalog registry input gaps and missing consumer surfaces",
	              "promote important catalog entries from derived defaults to explicit metadata fields when metadata-policy warnings are present",
	              "refresh catalog, evidence query, release pipeline, summary, Prometheus, and final report when validation is enabled"},
             indent + 2)
      << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(
             {"after current-repo catalog-flow repair, rerun target-machine replay/execute only for artifacts whose claim boundary requires it"},
             indent + 2)
      << ",\n";
  out << nested << "\"claim_boundary\": \"catalog-flow filter query is a read-only projection of the canonical catalog artifact; it does not execute repairs, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}


}  // namespace inferlab_evidence_query
