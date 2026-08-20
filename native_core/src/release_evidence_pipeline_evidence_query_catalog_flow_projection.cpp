#include "release_evidence_pipeline_evidence_query_catalog_flow_projection.hpp"

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {
namespace {

void add_contract_issue(std::size_t& issue_count) {
  issue_count = inferlab::internal::checked_add_size(
      issue_count, 1U,
      "release pipeline Evidence Query catalog-flow contract issue count");
}

void project_required_unsigned(const std::string& source, const char* key,
                               std::size_t& value,
                               std::size_t& issue_count) {
  const auto field = inferlab_artifact::project_unsigned_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issue_count);
    return;
  }
  value = field.value;
}

void project_required_bool(const std::string& source, const char* key,
                           bool& value, std::size_t& issue_count) {
  const auto field = inferlab_artifact::project_bool_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issue_count);
    return;
  }
  value = field.value;
}

void project_required_string(const std::string& source, const char* key,
                             std::string& value,
                             std::size_t& issue_count) {
  const auto field = inferlab_artifact::project_string_field(source, {key});
  if (!field.present || field.invalid) {
    add_contract_issue(issue_count);
    return;
  }
  value = field.value;
}

}  // namespace

EvidenceQueryCatalogFlowClosureProjection
project_evidence_query_catalog_flow_closure(
    const std::string& evidence_query_document, const bool summary_usable,
    const bool source_present) {
  EvidenceQueryCatalogFlowClosureProjection projection;
  if (!source_present) return projection;
  if (!summary_usable) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  const std::string source =
      inferlab_artifact::json_object_section_or_empty(
          evidence_query_document, "catalog_flow_filter_query");
  projection.object_present = !source.empty();
  if (!projection.object_present) {
    projection.contract_issue_count = 1U;
    return projection;
  }

  auto& fallback = projection.fallback_removal;
  auto& coverage = projection.consumer_field_coverage;
  auto& issues = projection.contract_issue_count;

  project_required_bool(
      source, "catalog_flow_fallback_removal_shortest_path_present",
      fallback.fallback_removal_shortest_path_present, issues);
  project_required_string(
      source, "catalog_flow_fallback_removal_shortest_path_state",
      fallback.fallback_removal_shortest_path_state, issues);
  project_required_unsigned(
      source, "catalog_flow_fallback_removal_shortest_path_step_count",
      fallback.fallback_removal_shortest_path_step_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
      fallback.fallback_removal_shortest_path_current_repo_step_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
      fallback.fallback_removal_shortest_path_target_machine_step_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
      fallback.fallback_removal_shortest_path_source_location_gap_count,
      issues);
  project_required_string(
      source,
      "catalog_flow_fallback_removal_shortest_path_primary_environment",
      fallback.fallback_removal_shortest_path_primary_environment, issues);
  project_required_string(
      source, "catalog_flow_fallback_removal_shortest_path_first_surface",
      fallback.fallback_removal_shortest_path_first_surface, issues);
  project_required_bool(
      source,
      "catalog_flow_fallback_removal_shortest_path_blocks_removal",
      fallback.fallback_removal_shortest_path_blocks_removal, issues);

  project_required_bool(
      source, "catalog_flow_fallback_removal_parity_gate_present",
      fallback.fallback_removal_parity_gate_present, issues);
  project_required_string(
      source, "catalog_flow_fallback_removal_parity_gate_decision",
      fallback.fallback_removal_parity_gate_decision, issues);
  project_required_bool(
      source, "catalog_flow_fallback_removal_parity_gate_removal_allowed",
      fallback.fallback_removal_parity_gate_removal_allowed, issues);
  project_required_unsigned(
      source, "catalog_flow_fallback_removal_parity_gate_blocker_count",
      fallback.fallback_removal_parity_gate_blocker_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_blocking_surface_count",
      fallback.fallback_removal_parity_gate_blocking_surface_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
      fallback.fallback_removal_parity_gate_fallback_surface_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
      fallback.fallback_removal_parity_gate_parity_refresh_required_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
      fallback.fallback_removal_parity_gate_current_repo_action_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
      fallback.fallback_removal_parity_gate_target_machine_action_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
      fallback.fallback_removal_parity_gate_source_location_gap_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
      fallback
          .fallback_removal_parity_gate_blocking_surface_source_location_gap_count,
      issues);
  project_required_string(
      source, "catalog_flow_fallback_removal_parity_gate_first_surface",
      fallback.fallback_removal_parity_gate_first_surface, issues);
  project_required_bool(
      source, "catalog_flow_fallback_removal_parity_gate_blocks_removal",
      fallback.fallback_removal_parity_gate_blocks_removal, issues);

  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_surface_count",
      coverage.consumer_field_coverage_surface_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_catalog_managed_surface_count",
      coverage.consumer_field_coverage_catalog_managed_surface_count,
      issues);
  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_fallback_surface_count",
      coverage.consumer_field_coverage_fallback_surface_count, issues);
  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_required_field_count",
      coverage.consumer_field_coverage_required_field_count, issues);
  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_covered_field_count",
      coverage.consumer_field_coverage_covered_field_count, issues);
  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_field_gap_count",
      coverage.consumer_field_coverage_field_gap_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_parity_refresh_required_count",
      coverage.consumer_field_coverage_parity_refresh_required_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_current_repo_action_count",
      coverage.consumer_field_coverage_current_repo_action_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_target_machine_action_count",
      coverage.consumer_field_coverage_target_machine_action_count, issues);
  project_required_unsigned(
      source, "catalog_flow_consumer_field_coverage_repair_card_count",
      coverage.consumer_field_coverage_repair_card_count, issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
      coverage.consumer_field_coverage_current_repo_repair_card_count,
      issues);
  project_required_unsigned(
      source,
      "catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
      coverage.consumer_field_coverage_target_machine_repair_card_count,
      issues);

  fallback.fallback_removal_parity_gate_blocking_surface_count = std::max(
      fallback.fallback_removal_parity_gate_blocking_surface_count,
      fallback.fallback_removal_parity_gate_blocker_count);
  fallback
      .fallback_removal_parity_gate_blocking_surface_source_location_gap_count =
      std::max(
          fallback
              .fallback_removal_parity_gate_blocking_surface_source_location_gap_count,
          fallback.fallback_removal_parity_gate_source_location_gap_count);
  if (inferlab_evidence::catalog_flow_fallback_removal_shortest_path_active(
          fallback)) {
    fallback.fallback_removal_shortest_path_blocks_removal = true;
  }
  if (inferlab_evidence::catalog_flow_fallback_removal_parity_gate_active(
          fallback)) {
    fallback.fallback_removal_parity_gate_blocks_removal = true;
  }
  return projection;
}

}  // namespace inferlab_release_pipeline
