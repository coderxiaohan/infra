#pragma once

#include "checked_index.hpp"
#include "evidence_catalog_consumer_field_coverage_projection.hpp"
#include "evidence_catalog_fallback_removal_projection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct CatalogFlowMetricSignals : CatalogFallbackRemovalSignals,
                                  ConsumerFieldCoverageSignals {
  std::size_t read_issue_count = 0;
  std::size_t input_gap_count = 0;
  std::size_t missing_surface_count = 0;
  std::size_t fallback_retained_surface_count = 0;
  std::size_t single_source_repair_card_count = 0;
  std::size_t single_source_parity_refresh_required_count = 0;
  std::size_t surface_migration_ready_count = 0;
  std::size_t surface_migration_fallback_count = 0;
  std::size_t surface_parity_refresh_required_count = 0;
  std::size_t surface_current_repo_action_count = 0;
  std::size_t surface_target_machine_action_count = 0;
  std::size_t surface_consumer_migration_plan_count = 0;
  std::size_t surface_consumer_fallback_removal_blocked_count = 0;
  std::size_t surface_consumer_parity_refresh_required_count = 0;
  std::size_t surface_consumer_current_repo_action_count = 0;
  std::size_t surface_consumer_target_machine_action_count = 0;
  std::size_t metadata_policy_card_count = 0;
  std::size_t metadata_derived_policy_artifact_count = 0;
  std::size_t metadata_derived_policy_field_count = 0;
  std::size_t missing_step_count = 0;
  std::size_t repair_card_count = 0;
  std::size_t discrepancy_count = 0;
};

inline std::size_t catalog_flow_discrepancy_floor(
    const CatalogFlowMetricSignals& signals) {
  return inferlab::internal::checked_add_size(
      inferlab::internal::checked_add_size(
          signals.input_gap_count, signals.missing_surface_count,
          "catalog flow discrepancy count"),
      inferlab::internal::checked_add_size(
          signals.missing_step_count,
          signals.consumer_field_coverage_field_gap_count,
          "catalog flow discrepancy count"),
      "catalog flow discrepancy count");
}

inline CatalogFlowMetricSignals project_catalog_flow_metrics(
    const std::string& catalog_text,
    const CatalogFallbackRemovalProjectionOptions& fallback_options = {}) {
  CatalogFlowMetricSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(catalog_text, key);
  };

  signals.missing_surface_count =
      count("catalog_drift_missing_surface_count");
  signals.fallback_retained_surface_count =
      count("catalog_fallback_retained_surface_count");
  signals.single_source_repair_card_count =
      count("single_source_repair_card_count");
  signals.single_source_parity_refresh_required_count =
      count("single_source_parity_refresh_required_count");
  signals.surface_migration_ready_count =
      count("catalog_surface_migration_ready_count");
  signals.surface_migration_fallback_count =
      count("catalog_surface_migration_fallback_count");
  signals.surface_parity_refresh_required_count =
      count("catalog_surface_parity_refresh_required_count");
  signals.surface_current_repo_action_count =
      count("catalog_surface_current_repo_action_count");
  signals.surface_target_machine_action_count =
      count("catalog_surface_target_machine_action_count");
  signals.surface_consumer_migration_plan_count =
      count("surface_consumer_migration_plan_count");
  signals.surface_consumer_fallback_removal_blocked_count =
      count("surface_consumer_fallback_removal_blocked_count");
  signals.surface_consumer_parity_refresh_required_count =
      count("surface_consumer_parity_refresh_required_count");
  signals.surface_consumer_current_repo_action_count =
      count("surface_consumer_current_repo_action_count");
  signals.surface_consumer_target_machine_action_count =
      count("surface_consumer_target_machine_action_count");
  static_cast<CatalogFallbackRemovalSignals&>(signals) =
      project_catalog_fallback_removal_signals(catalog_text,
                                               fallback_options);
  static_cast<ConsumerFieldCoverageSignals&>(signals) =
      project_consumer_field_coverage_signals(catalog_text);
  signals.metadata_policy_card_count =
      count("registry_metadata_policy_card_count");
  signals.metadata_derived_policy_artifact_count =
      count("registry_derived_policy_artifact_count");
  signals.metadata_derived_policy_field_count =
      count("registry_derived_policy_field_count");
  signals.repair_card_count = std::max(
      signals.single_source_repair_card_count,
      signals.consumer_field_coverage_repair_card_count);
  signals.discrepancy_count = catalog_flow_discrepancy_floor(signals);
  return signals;
}

}  // namespace inferlab_evidence
