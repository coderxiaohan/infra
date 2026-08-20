#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_query_consumer_filter_signals.hpp"
#include "evidence_query_cross_artifact.hpp"
#include "evidence_query_record_policy.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {

using inferlab_evidence::catalog_flow_fallback_removal_parity_gate_active;
using inferlab_evidence::catalog_flow_fallback_removal_shortest_path_active;

inline std::size_t cross_artifact_blocker_category_count(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const std::string& category) {
  std::size_t count = 0;
  for (const auto& record : records) {
    if (blocker_category(record) == category) {
      count = inferlab::internal::checked_add_size(
          count, 1U, "cross-artifact blocker category count");
    }
  }
  return count;
}

inline std::vector<CrossArtifactBlockerFamily> cross_artifact_blocker_families(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const std::size_t root_cause_repair_card_count,
    const std::size_t current_repo_repair_card_count,
    const std::size_t target_product_replay_repair_card_count,
    const std::size_t target_gpu_execute_repair_card_count,
    const std::size_t repair_path_source_location_gap_count,
    const CatalogFlowFilterSignals& catalog_flow_signals,
    const NativeCoreAuditFilterSignals& native_core_audit_signals,
    const ProductReplaySemanticLinkageSignals& product_replay_signals) {
  std::vector<CrossArtifactBlockerFamily> families;
  families.reserve(5U);

  CrossArtifactBlockerFamily metadata;
  metadata.family = "warehouse_metadata";
  metadata.source_artifact = "native_evidence_warehouse_index.json";
  metadata.present = true;
  metadata.readable = true;
  metadata.blocker_count = inferlab::internal::checked_sum_size(
      {cross_artifact_blocker_category_count(records, "artifact_missing"),
       cross_artifact_blocker_category_count(records, "schema_missing"),
       cross_artifact_blocker_category_count(records, "claim_scope_missing"),
       cross_artifact_blocker_category_count(records, "report_wiring_missing"),
       cross_artifact_blocker_category_count(records, "freshness_stale"),
       cross_artifact_blocker_category_count(records, "artifact_blocked")},
      "cross-artifact warehouse metadata blocker count");
  metadata.current_repo_action_count =
      inferlab::internal::checked_sum_size(
          {cross_artifact_blocker_category_count(records, "schema_missing"),
           cross_artifact_blocker_category_count(records,
                                                 "claim_scope_missing"),
           cross_artifact_blocker_category_count(records,
                                                 "report_wiring_missing"),
           cross_artifact_blocker_category_count(records,
                                                 "artifact_blocked")},
          "cross-artifact warehouse current-repo action count");
  metadata.target_or_validation_action_count =
      inferlab::internal::checked_sum_size(
          {cross_artifact_blocker_category_count(records,
                                                 "artifact_missing"),
           cross_artifact_blocker_category_count(records,
                                                 "freshness_stale")},
          "cross-artifact warehouse target action count");
  metadata.blocker_reason =
      metadata.blocker_count == 0 ? "none" : "artifact_metadata_or_wiring";
  metadata.primary_environment =
      metadata.current_repo_action_count > 0
          ? "current_repo_fixable"
          : (metadata.target_or_validation_action_count > 0
                 ? "current_server_when_validation_enabled"
                 : "none");
  metadata.next_action =
      metadata.current_repo_action_count > 0
          ? "repair schema, claim-scope, report wiring, or artifact blockers before release review"
          : (metadata.target_or_validation_action_count > 0
                 ? "refresh missing or stale artifacts when validation is enabled"
                 : "no warehouse metadata blocker");
  families.push_back(std::move(metadata));

  CrossArtifactBlockerFamily root_cause;
  root_cause.family = "root_cause_repair_cards";
  root_cause.source_artifact = "native_evidence_query_result.json";
  root_cause.present = true;
  root_cause.readable = true;
  root_cause.blocker_count = root_cause_repair_card_count;
  root_cause.current_repo_action_count = current_repo_repair_card_count;
  root_cause.target_product_replay_action_count =
      target_product_replay_repair_card_count;
  root_cause.target_gpu_execute_action_count =
      target_gpu_execute_repair_card_count;
  root_cause.target_or_validation_action_count =
      inferlab::internal::checked_sum_size(
          {target_product_replay_repair_card_count,
           target_gpu_execute_repair_card_count},
          "cross-artifact root-cause target action count");
  root_cause.source_location_gap_count = repair_path_source_location_gap_count;
  root_cause.blocker_reason =
      root_cause_repair_card_count == 0 ? "none" : "root_cause_repair_cards";
  root_cause.primary_environment =
      current_repo_repair_card_count > 0
          ? "current_repo_fixable"
          : (target_product_replay_repair_card_count > 0
                 ? "target_product_or_replay_evidence"
                 : (target_gpu_execute_repair_card_count > 0
                        ? "target_gpu_or_execute_machine"
                        : "none"));
  root_cause.next_action =
      root_cause.source_location_gap_count > 0
          ? "add exact source locations to root-cause repair cards before handoff"
          : (current_repo_repair_card_count > 0
                 ? "close current-repo root-cause repair cards"
                 : (target_product_replay_repair_card_count > 0
                        ? "import or refresh the required product replay evidence"
                        : (target_gpu_execute_repair_card_count > 0
                               ? "run the required GPU or execute validation on the target machine"
                               : "no root-cause repair-card blocker")));
  families.push_back(std::move(root_cause));

  CrossArtifactBlockerFamily catalog_flow;
  catalog_flow.family = "catalog_flow";
  catalog_flow.source_artifact = "native_artifact_catalog_single_source.json";
  catalog_flow.present = catalog_flow_signals.catalog_artifact_present;
  catalog_flow.readable = catalog_flow_signals.catalog_artifact_readable;
  catalog_flow.blocker_count = inferlab::internal::checked_sum_size(
      {catalog_flow_signals.discrepancy_count,
       catalog_flow_signals.input_gap_count,
       catalog_flow_signals.missing_surface_count,
       catalog_flow_signals.surface_consumer_fallback_removal_blocked_count,
       catalog_flow_fallback_removal_parity_gate_active(catalog_flow_signals)
           ? 1U
           : 0U,
       catalog_flow_fallback_removal_shortest_path_active(catalog_flow_signals)
           ? 1U
           : 0U,
       catalog_flow_signals.consumer_field_coverage_field_gap_count},
      "cross-artifact catalog-flow blocker count");
  catalog_flow.current_repo_action_count =
      inferlab::internal::checked_sum_size(
          {catalog_flow_signals.surface_current_repo_action_count,
           catalog_flow_signals.surface_consumer_current_repo_action_count,
           catalog_flow_signals
               .fallback_removal_queue_current_repo_action_count,
           catalog_flow_signals
               .fallback_removal_parity_gate_current_repo_action_count,
           catalog_flow_signals
               .fallback_removal_shortest_path_current_repo_step_count,
           catalog_flow_signals
               .consumer_field_coverage_current_repo_action_count,
           catalog_flow_signals
               .consumer_field_coverage_current_repo_repair_card_count},
          "cross-artifact catalog-flow current-repo action count");
  catalog_flow.target_or_validation_action_count =
      inferlab::internal::checked_sum_size(
          {catalog_flow_signals.surface_target_machine_action_count,
           catalog_flow_signals.surface_consumer_target_machine_action_count,
           catalog_flow_signals
               .fallback_removal_queue_target_machine_action_count,
           catalog_flow_signals
               .fallback_removal_parity_gate_target_machine_action_count,
           catalog_flow_signals
               .fallback_removal_shortest_path_target_machine_step_count,
           catalog_flow_signals
               .consumer_field_coverage_target_machine_action_count,
           catalog_flow_signals
               .consumer_field_coverage_target_machine_repair_card_count,
           catalog_flow_signals
               .surface_consumer_parity_refresh_required_count,
           catalog_flow_signals
               .fallback_removal_parity_gate_parity_refresh_required_count,
           catalog_flow_signals
               .consumer_field_coverage_parity_refresh_required_count},
          "cross-artifact catalog-flow target action count");
  catalog_flow.source_location_gap_count =
      inferlab::internal::checked_sum_size(
          {catalog_flow_signals.consumer_field_coverage_field_gap_count,
           catalog_flow_signals
               .fallback_removal_parity_gate_source_location_gap_count,
           catalog_flow_signals
               .fallback_removal_shortest_path_source_location_gap_count},
          "cross-artifact catalog-flow source-location gap count");
  catalog_flow.blocker_reason =
      catalog_flow.blocker_count == 0 ? "none" : "catalog_flow_gap";
  catalog_flow.primary_environment =
      catalog_flow.current_repo_action_count > 0
          ? "current_repo_fixable"
          : (catalog_flow.target_or_validation_action_count > 0
                 ? "current_server_when_validation_enabled"
                 : "none");
  if (catalog_flow_fallback_removal_parity_gate_active(catalog_flow_signals)) {
    catalog_flow.next_action =
        catalog_flow_fallback_removal_parity_gate_next_action(
            catalog_flow_signals);
  } else if (catalog_flow_fallback_removal_shortest_path_active(
          catalog_flow_signals)) {
    catalog_flow.next_action =
        catalog_flow_fallback_removal_shortest_path_next_action(
            catalog_flow_signals);
  } else if (catalog_flow.current_repo_action_count > 0) {
    catalog_flow.next_action =
        "repair catalog registry fields or fallback-removal queue entries in the current repo";
  } else if (catalog_flow.target_or_validation_action_count > 0) {
    catalog_flow.next_action =
        "refresh catalog consumer parity on a validation-enabled machine";
  } else {
    catalog_flow.next_action = "no catalog-flow blocker";
  }
  families.push_back(std::move(catalog_flow));

  CrossArtifactBlockerFamily native_audit;
  native_audit.family = "native_core_audit";
  native_audit.source_artifact = "native_core_audit_register.json";
  native_audit.present = native_core_audit_signals.audit_register_present;
  native_audit.readable = native_core_audit_signals.read_issue_count == 0;
  native_audit.blocker_count =
      native_core_audit_blocker_count(native_core_audit_signals);
  native_audit.current_repo_action_count =
      native_core_audit_signals.audit_review_coverage_current_repo_action_count;
  native_audit.target_or_validation_action_count =
      native_core_audit_signals.audit_review_coverage_target_machine_action_count;
  native_audit.source_location_gap_count =
      native_core_audit_signals.audit_review_coverage_source_location_gap_count;
  native_audit.blocker_reason =
      native_audit.blocker_count == 0
          ? "none"
          : (native_core_audit_signals
                     .audit_review_coverage_primary_blocker != "unknown"
                 ? native_core_audit_signals
                       .audit_review_coverage_primary_blocker
                 : "native_core_audit_backlog");
  native_audit.primary_environment =
      native_core_audit_signals.audit_review_coverage_primary_environment !=
              "unknown"
          ? native_core_audit_signals
                .audit_review_coverage_primary_environment
          : (native_audit.current_repo_action_count > 0
                 ? "current_repo_fixable"
                 : (native_audit.target_or_validation_action_count > 0
                        ? "target_gpu_or_execute_machine"
                        : "none"));
  native_audit.next_action =
      native_core_audit_signals.audit_review_coverage_next_action != "unknown"
          ? native_core_audit_signals.audit_review_coverage_next_action
          : (native_audit.current_repo_action_count > 0
                 ? "close current-repo native-core audit worklist items"
                 : (native_audit.target_or_validation_action_count > 0
                        ? "run target-machine validation after static audit closure"
                        : "no native-core audit blocker"));
  families.push_back(std::move(native_audit));

  CrossArtifactBlockerFamily product_replay;
  product_replay.family = "product_replay_or_execute";
  product_replay.source_artifact = "product_replay_semantic_events.json";
  product_replay.source_artifacts = {
      "product_replay_semantic_events.json",
      "native_product_replay_alignment.json"};
  product_replay.present = product_replay_signals.semantic_events_present ||
                           product_replay_signals.alignment_present;
  product_replay.readable = product_replay_signals.read_issue_count == 0;
  const std::size_t replay_missing_count =
      cross_artifact_blocker_category_count(records, "replay_missing");
  const std::size_t execute_missing_count =
      cross_artifact_blocker_category_count(records, "execute_missing");
  product_replay.replay_or_execute_missing_count =
      inferlab::internal::checked_sum_size(
          {replay_missing_count, execute_missing_count},
          "cross-artifact replay-or-execute missing count");
  product_replay.semantic_linkage_blocker_count =
      product_replay_signals.blocker_count;
  product_replay.blocker_count = inferlab::internal::checked_sum_size(
      {product_replay.replay_or_execute_missing_count,
       product_replay_signals.blocker_count},
      "cross-artifact product replay blocker count");
  product_replay.current_repo_action_count = 0;
  product_replay.target_product_replay_action_count =
      inferlab::internal::checked_sum_size(
          {replay_missing_count,
           product_replay_signals.target_or_validation_action_count},
          "cross-artifact product replay action count");
  product_replay.target_gpu_execute_action_count = execute_missing_count;
  product_replay.target_or_validation_action_count =
      inferlab::internal::checked_sum_size(
          {product_replay.target_product_replay_action_count,
           product_replay.target_gpu_execute_action_count},
          "cross-artifact product target action count");
  product_replay.blocker_reason =
      product_replay.blocker_count == 0
          ? "none"
          : (product_replay_signals.blocker_count > 0
                 ? "product_replay_semantic_linkage_incomplete"
                 : "product_replay_or_execute_missing");
  product_replay.primary_environment =
      product_replay.target_product_replay_action_count > 0
          ? "target_product_or_replay_evidence"
          : (product_replay.target_gpu_execute_action_count > 0
                 ? "target_gpu_or_execute_machine"
                 : "none");
  product_replay.next_action =
      product_replay_signals.blocker_count > 0
          ? product_replay_signals.next_action
          : (product_replay.target_product_replay_action_count > 0
                 ? "import product replay evidence"
                 : (product_replay.target_gpu_execute_action_count > 0
                        ? "run execute validation on the target GPU machine"
                        : "no product replay or execute blocker"));
  families.push_back(std::move(product_replay));

  return families;
}

}  // namespace inferlab_evidence_query
