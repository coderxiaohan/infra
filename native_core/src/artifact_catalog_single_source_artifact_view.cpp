#include "artifact_catalog_single_source_views.hpp"

#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_catalog_single_source {

std::string render_catalog_single_source_artifact(
    const CatalogSingleSourceProjection& projection,
    const std::string& output,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& output_path,
    const std::string& format) {
  const auto* first_fallback = first_fallback_surface(projection);
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_artifact_catalog_single_source\",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, "native_artifact_catalog_single_source",
      "artifact catalog single source only; it centralizes discovery and wiring metadata without running validation",
      {"vLLM", "SGLang", "TensorRT-LLM", "FlashInfer", "LMCache",
       "Mooncake", "Dynamo", "KServe", "llm-d", "Ray Serve", "TGI",
       "KTransformers"},
      "inferlab_artifact_catalog_single_source",
      output.empty() ? "results/raw/native_artifact_catalog_single_source.json"
                     : output,
      "./build/inferlab_artifact_catalog_single_source --repo-root . --output results/raw/native_artifact_catalog_single_source.json --format json");
  out << "  \"status\": "
      << inferlab_plan::json_string(projection.artifact_status) << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"single-source artifact catalog and wiring "
         "policy only; no benchmark, replay, or execute claims are "
         "produced\",\n";
  out << "  \"cli_context\": "
      << cli_context_json(repo_root, output_path, format, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"registered_artifact_count\": " << projection.artifacts.size()
      << ",\n";
  out << "    \"registry_consumer_surface_count\": "
      << projection.surfaces.size() << ",\n";
  out << "    \"registry_consumed_surface_count\": "
      << projection.consumed_surface_count << ",\n";
  out << "    \"hardcoded_duplicate_location_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"registry_decision\": "
      << inferlab_plan::json_string(projection.registry_decision) << ",\n";
  out << "    \"duplicate_registration_count\": "
      << projection.duplicate_registration_count << ",\n";
  out << "    \"missing_wiring_policy_count\": "
      << projection.missing_wiring_policy_count << ",\n";
  out << "    \"downstream_wiring_matrix_entry_count\": "
      << projection.matrix_entry_count << ",\n";
  out << "    \"downstream_wiring_surface_count\": "
      << projection.surfaces.size() << ",\n";
  out << "    \"catalog_drift_missing_surface_count\": "
      << projection.missing_surface_count << ",\n";
  out << "    \"catalog_fallback_retained_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"catalog_surface_migration_ready_count\": "
      << projection.migration_ready_surface_count << ",\n";
  out << "    \"catalog_surface_migration_fallback_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"catalog_surface_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"catalog_surface_current_repo_action_count\": "
      << projection.surface_current_repo_action_count << ",\n";
  out << "    \"catalog_surface_target_machine_action_count\": "
      << projection.surface_target_machine_action_count << ",\n";
  out << "    \"surface_consumer_migration_plan_count\": "
      << projection.surfaces.size() << ",\n";
  out << "    \"surface_consumer_fallback_removal_blocked_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"surface_consumer_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"surface_consumer_current_repo_action_count\": "
      << projection.surface_current_repo_action_count << ",\n";
  out << "    \"surface_consumer_target_machine_action_count\": "
      << projection.surface_target_machine_action_count << ",\n";
  out << "    \"fallback_removal_queue_item_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_queue_blocked_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_queue_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_queue_current_repo_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_queue_target_machine_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_queue_state\": "
      << inferlab_plan::json_string(projection.fallback_queue_state) << ",\n";
  out << "    \"fallback_removal_shortest_path_present\": true,\n";
  out << "    \"fallback_removal_shortest_path_state\": "
      << inferlab_plan::json_string(projection.fallback_shortest_path_state)
      << ",\n";
  out << "    \"fallback_removal_shortest_path_step_count\": "
      << projection.fallback_shortest_path_step_count << ",\n";
  out << "    \"fallback_removal_shortest_path_current_repo_step_count\": "
      << projection.fallback_shortest_path_current_repo_step_count << ",\n";
  out << "    \"fallback_removal_shortest_path_target_machine_step_count\": "
      << projection.fallback_shortest_path_target_machine_step_count << ",\n";
  out << "    \"fallback_removal_shortest_path_source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << "    \"fallback_removal_shortest_path_primary_environment\": "
      << inferlab_plan::json_string(
             projection.fallback_removal_blocked
                 ? "current_repo_then_target_machine"
                 : "review_clear")
      << ",\n";
  out << "    \"fallback_removal_shortest_path_first_surface\": "
      << inferlab_plan::json_string(
             first_fallback == nullptr ? ""
                                       : first_fallback->status.surface)
      << ",\n";
  out << "    \"fallback_removal_parity_gate_present\": true,\n";
  out << "    \"fallback_removal_parity_gate_decision\": "
      << inferlab_plan::json_string(projection.fallback_parity_gate_decision)
      << ",\n";
  out << "    \"fallback_removal_parity_gate_removal_allowed\": "
      << inferlab_plan::json_bool(!projection.fallback_removal_blocked)
      << ",\n";
  out << "    \"fallback_removal_parity_gate_blocker_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_blocking_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_fallback_surface_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_current_repo_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_target_machine_action_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"fallback_removal_parity_gate_source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << "    \"fallback_removal_parity_gate_blocking_surface_source_location_gap_count\": "
      << projection.fallback_source_location_gap_count << ",\n";
  out << "    \"fallback_removal_parity_gate_first_surface\": "
      << inferlab_plan::json_string(
             first_fallback == nullptr ? ""
                                       : first_fallback->status.surface)
      << ",\n";
  out << "    \"consumer_field_coverage_surface_count\": "
      << projection.surfaces.size() << ",\n";
  out << "    \"consumer_field_coverage_catalog_managed_surface_count\": "
      << projection.field_coverage_catalog_managed_surface_count << ",\n";
  out << "    \"consumer_field_coverage_fallback_surface_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << "    \"consumer_field_coverage_required_field_count\": "
      << projection.field_coverage_required_field_count << ",\n";
  out << "    \"consumer_field_coverage_covered_field_count\": "
      << projection.field_coverage_covered_field_count << ",\n";
  out << "    \"consumer_field_coverage_field_gap_count\": "
      << projection.field_coverage_field_gap_count << ",\n";
  out << "    \"consumer_field_coverage_parity_refresh_required_count\": "
      << projection.field_coverage_fallback_surface_count << ",\n";
  out << "    \"consumer_field_coverage_current_repo_action_count\": "
      << projection.field_coverage_current_repo_action_count << ",\n";
  out << "    \"consumer_field_coverage_target_machine_action_count\": "
      << projection.field_coverage_target_machine_action_count << ",\n";
  out << "    \"single_source_repair_card_count\": "
      << projection.single_source_repair_card_count << ",\n";
  out << "    \"single_source_fallback_repair_card_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"single_source_parity_refresh_required_count\": "
      << projection.fallback_surface_count << ",\n";
  out << "    \"registry_explicit_policy_complete_artifact_count\": "
      << projection.explicit_policy_complete_artifact_count << ",\n";
  out << "    \"registry_derived_policy_artifact_count\": "
      << projection.derived_policy_artifact_count << ",\n";
  out << "    \"registry_derived_policy_field_count\": "
      << projection.derived_policy_field_count << ",\n";
  out << "    \"registry_metadata_policy_card_count\": "
      << projection.derived_policy_artifact_count << ",\n";
  out << "    \"evidence_confidence\": "
      << inferlab_plan::json_string(projection.evidence_confidence) << "\n";
  out << "  },\n";
  out << "  \"artifacts\": " << artifacts_json(projection, 2) << ",\n";
  out << "  \"artifact_entry_required_fields\": "
      << inferlab_plan::json_string_array(
             {"id", "artifact", "artifact_type", "producer_target", "report",
              "report_kind", "summary_group", "final_report_group",
              "prometheus_category", "freshness_policy",
              "benchmark_matrix_entry", "refresh_command", "required_fields",
              "optional_fields", "summary_metrics", "summary_required",
              "prometheus_source_required", "final_report_required",
              "contract_required", "freshness_required",
              "evidence_bundle_required",
              "allowed_modes", "claim_boundary", "metadata_policy",
              "external_product_boundary", "requires_execute",
              "requires_hardware", "requires_product_replay",
              "target_machine_required", "validation_order"},
             2)
      << ",\n";
  out << "  \"registry_sources\": " << registry_sources_json(2) << ",\n";
  out << "  \"derived_contract_entries\": "
      << derived_entries_json(projection, "artifact_contract", 2) << ",\n";
  out << "  \"derived_freshness_entries\": "
      << derived_entries_json(projection, "freshness_gate", 2) << ",\n";
  out << "  \"derived_summary_sources\": "
      << summary_source_views_json(projection, 2) << ",\n";
  out << "  \"derived_prometheus_sources\": "
      << prometheus_source_views_json(projection, 2) << ",\n";
  out << "  \"derived_final_report_sources\": "
      << final_report_source_views_json(projection, 2) << ",\n";
  out << "  \"derived_evidence_bundle_components\": "
      << derived_entries_json(projection, "evidence_bundle", 2) << ",\n";
  out << "  \"hardcoded_duplicate_locations\": "
      << hardcoded_duplicate_locations_json(projection, 2) << ",\n";
  out << "  \"consumer_statuses\": "
      << consumer_statuses_json(projection, 2) << ",\n";
  out << "  \"surface_migration_readiness\": "
      << surface_migration_readiness_json(projection, 2) << ",\n";
  out << "  \"surface_consumer_migration_plan\": "
      << surface_consumer_migration_plan_json(projection, 2) << ",\n";
  out << "  \"fallback_removal_queue\": "
      << fallback_removal_queue_json(projection, 2) << ",\n";
  out << "  \"fallback_removal_shortest_path\": "
      << fallback_removal_shortest_path_json(projection, 2) << ",\n";
  out << "  \"fallback_removal_parity_gate\": "
      << fallback_removal_parity_gate_json(projection, 2) << ",\n";
  out << "  \"consumer_field_coverage_summary\": "
      << consumer_field_coverage_summary_json(projection, 2) << ",\n";
  out << "  \"consumer_field_coverage\": "
      << consumer_field_coverage_json(projection, 2) << ",\n";
  out << "  \"downstream_wiring_matrix\": "
      << downstream_wiring_matrix_json(projection, 2) << ",\n";
  out << "  \"catalog_drift_review\": "
      << catalog_drift_review_json(projection, 2) << ",\n";
  out << "  \"single_source_parity_review\": "
      << single_source_parity_review_json(projection, 2) << ",\n";
  out << "  \"single_source_repair_cards\": "
      << single_source_repair_cards_json(projection, 2) << ",\n";
  out << "  \"metadata_completeness_review\": "
      << metadata_completeness_review_json(projection, 2) << ",\n";
  out << "  \"metadata_policy_cards\": "
      << metadata_policy_cards_json(projection, 2) << ",\n";
  out << "  \"migration_readiness\": "
      << migration_readiness_json(projection, 2) << ",\n";
  out << "  \"wiring_policies\": " << wiring_policies_json(2) << ",\n";
  out << "  \"claim_boundary_rules\": [\n";
  out << "    {\"rule\": \"single_registration\", \"decision\": \"a new high-value artifact should be added once to high_value_artifact_catalog.hpp before downstream wiring is generated\"},\n";
  out << "    {\"rule\": \"dry_run_not_execute\", \"decision\": \"registry presence does not imply native binary execution\"},\n";
  out << "    {\"rule\": \"external_product_boundary\", \"decision\": \"product-owned runtime capabilities cannot be rebranded as InferLab self-developed core\"}\n";
  out << "  ],\n";
  out << "  \"freshness_policies\": [\n";
  out << "    {\"artifact_class\": \"evidence_governance\", \"max_age_hours\": 168, \"requires_refresh_command\": true},\n";
  out << "    {\"artifact_class\": \"kernel_microbenchmark\", \"max_age_hours\": 168, \"requires_target_machine_execute\": true},\n";
  out << "    {\"artifact_class\": \"product_replay\", \"max_age_hours\": 168, \"requires_replay_or_execute_boundary\": true}\n";
  out << "  ],\n";
  out << "  \"report_kind_rules\": [\n";
  out << "    {\"kind\": \"evidence\", \"purpose\": \"governance, lineage, fixture, query, release, and dependency evidence\"},\n";
  out << "    {\"kind\": \"deep-dive\", \"purpose\": \"mechanism-level slices and product alignment without mature product core reimplementation\"},\n";
  out << "    {\"kind\": \"final\", \"purpose\": \"roll-up only from results/raw artifacts\"}\n";
  out << "  ],\n";
  out << "  \"generated_surfaces\": "
      << inferlab_plan::json_string_array(
             {"CMake target", "benchmark matrix refresh entry",
              "summary evidence section", "Prometheus source listing",
              "final report module/commands", "artifact contract gate",
              "freshness gate", "evidence bundle", "trace correlation",
              "report README/index"},
             2)
      << ",\n";
  out << "  \"migration_plan\": [\n";
  out << "    {\"phase\": \"read_only_registry\", \"action\": \"emit this artifact from high_value_artifact_catalog.hpp and compare downstream wiring manually\"},\n";
  out << "    {\"phase\": \"gate_consumption\", \"action\": \"contract/freshness/report gates read registry-derived metadata instead of duplicating artifact lists\"},\n";
  out << "    {\"phase\": \"dedupe_hardcoding\", \"action\": \"remove repeated per-artifact registration once generated wiring is stable\"}\n";
  out << "  ],\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(projection.blockers, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             {"./build/inferlab_artifact_catalog_single_source --repo-root . --output results/raw/native_artifact_catalog_single_source.json --format json",
              "./build/inferlab_deep_dive_contract_check --repo-root . --results results/raw --output results/raw/native_deep_dive_contract_check.json --format json",
              "./build/inferlab_runtime_evidence_freshness_gate --repo-root . --results results/raw --output results/raw/native_runtime_evidence_freshness_gate.json --format json"},
             2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_catalog_single_source
