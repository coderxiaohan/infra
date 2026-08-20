#include "release_evidence_pipeline_repair_producer.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_domain_utils.hpp"
#include "release_evidence_pipeline_scalar_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

using inferlab_evidence::contains;
using inferlab_evidence::RepairEnvironment;

void append_evidence_query_repair_paths(
    const ReleasePipelineSourceContext& sources,
    RepairPathCollector& paths) {
  const auto& evidence_query_meta = sources.evidence_query->metadata;
  const auto& evidence_query_summary_source = sources.evidence_query_summary;
  const auto& evidence_query_text = evidence_query_summary_source;
  const auto& snapshot_diff_signals = sources.snapshot_diff;

  if (evidence_query_meta.present) {
    const int catalog_flow_field_coverage_repair_card_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_repair_card_count", 0);
    const int catalog_flow_field_coverage_current_repo_repair_card_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
            0);
    const int catalog_flow_field_coverage_target_machine_repair_card_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
            0);
    const int catalog_flow_field_coverage_required_field_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_required_field_count", 0);
    const int catalog_flow_field_coverage_covered_field_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_covered_field_count", 0);
    const int catalog_flow_field_coverage_field_gap_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_field_gap_count", 0);
    const int catalog_flow_field_coverage_parity_refresh_required_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_parity_refresh_required_count",
            0);
    const int catalog_flow_field_coverage_current_repo_action_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_current_repo_action_count", 0);
    const int catalog_flow_field_coverage_target_machine_action_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_consumer_field_coverage_target_machine_action_count",
            0);
    if (catalog_flow_field_coverage_repair_card_count > 0 ||
        catalog_flow_field_coverage_field_gap_count > 0 ||
        catalog_flow_field_coverage_parity_refresh_required_count > 0) {
      paths.add("repair_catalog_flow_field_coverage",
          "native_evidence_query_result.json",
          "catalog_flow_field_coverage_gap",
          catalog_flow_field_coverage_current_repo_repair_card_count > 0
              ? RepairEnvironment::kCurrentRepoFixable
              : RepairEnvironment::kCurrentServerWhenValidationEnabled,
          "high",
          "Evidence Query reports catalog consumer field-coverage gaps or parity refresh requirements",
          "complete catalog required-field metadata, keep legacy fallback lists until downstream parity is refreshed, then refresh Catalog, Evidence Query, release pipeline, Summary, Prometheus, and Final Report",
          "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query catalog_flow_discrepancy --output results/raw/native_evidence_query_result.json --format json");
      paths.back().evidence_counters = {
          {"catalog_flow_consumer_field_coverage_repair_card_count",
           catalog_flow_field_coverage_repair_card_count},
          {"catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
           catalog_flow_field_coverage_current_repo_repair_card_count},
          {"catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
           catalog_flow_field_coverage_target_machine_repair_card_count},
          {"catalog_flow_consumer_field_coverage_required_field_count",
           catalog_flow_field_coverage_required_field_count},
          {"catalog_flow_consumer_field_coverage_covered_field_count",
           catalog_flow_field_coverage_covered_field_count},
          {"catalog_flow_consumer_field_coverage_field_gap_count",
           catalog_flow_field_coverage_field_gap_count},
          {"catalog_flow_consumer_field_coverage_parity_refresh_required_count",
           catalog_flow_field_coverage_parity_refresh_required_count},
          {"catalog_flow_consumer_field_coverage_current_repo_action_count",
           catalog_flow_field_coverage_current_repo_action_count},
          {"catalog_flow_consumer_field_coverage_target_machine_action_count",
           catalog_flow_field_coverage_target_machine_action_count},
      };
    }
    const bool catalog_flow_parity_gate_present =
        inferlab_artifact::json_bool_value_or_false(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_present");
    const std::string catalog_flow_parity_gate_decision =
        json_string_or_empty(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_decision");
    const bool catalog_flow_parity_gate_removal_allowed =
        inferlab_artifact::json_bool_value_or_false(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_removal_allowed");
    const int catalog_flow_parity_gate_blocker_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_blocker_count", 0);
    const int catalog_flow_parity_gate_fallback_surface_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
            0);
    const int catalog_flow_parity_gate_parity_refresh_required_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
            0);
    const int catalog_flow_parity_gate_current_repo_action_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
            0);
    const int catalog_flow_parity_gate_target_machine_action_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
            0);
    const int catalog_flow_parity_gate_source_location_gap_count =
        json_number_or_default(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
            0);
    const std::string catalog_flow_parity_gate_first_surface =
        json_string_or_empty(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_first_surface");
    const bool catalog_flow_parity_gate_blocks_removal =
        inferlab_artifact::json_bool_value_or_false(
            evidence_query_text,
            "catalog_flow_fallback_removal_parity_gate_blocks_removal") ||
        (catalog_flow_parity_gate_present &&
         !catalog_flow_parity_gate_removal_allowed &&
         (catalog_flow_parity_gate_blocker_count > 0 ||
          catalog_flow_parity_gate_parity_refresh_required_count > 0 ||
          catalog_flow_parity_gate_current_repo_action_count > 0 ||
          catalog_flow_parity_gate_target_machine_action_count > 0 ||
          catalog_flow_parity_gate_source_location_gap_count > 0 ||
          contains(catalog_flow_parity_gate_decision, "blocked")));
    if (catalog_flow_parity_gate_blocks_removal) {
      RepairEnvironment repair_environment =
          RepairEnvironment::kCurrentRepoThenValidationMachine;
      if (catalog_flow_parity_gate_current_repo_action_count == 0 &&
          (catalog_flow_parity_gate_target_machine_action_count > 0 ||
           catalog_flow_parity_gate_parity_refresh_required_count > 0)) {
        repair_environment =
            RepairEnvironment::kTargetOrValidationEnabledMachine;
      }
      const std::string surface =
          catalog_flow_parity_gate_first_surface.empty()
              ? "unknown fallback surface"
              : catalog_flow_parity_gate_first_surface;
      paths.add("repair_catalog_flow_fallback_removal_parity_gate",
          "native_evidence_query_result.json",
          "catalog_flow_fallback_removal_parity_gate", repair_environment,
          catalog_flow_parity_gate_blocks_removal ? "high" : "medium",
          "Evidence Query reports catalog fallback-removal parity gate decision " +
              (catalog_flow_parity_gate_decision.empty()
                   ? std::string("unknown")
                   : catalog_flow_parity_gate_decision) +
              " for " + surface,
          "keep legacy fallback lists retained until the parity gate allows removal, refresh downstream catalog consumers on the validation or target machine, record parity evidence, then refresh release pipeline, summary, Prometheus, and final report before deleting fallback surfaces",
          "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query catalog_flow_discrepancy --output results/raw/native_evidence_query_result.json --format json");
      paths.back().evidence_counters = {
          {"catalog_flow_fallback_removal_parity_gate_present",
           catalog_flow_parity_gate_present ? 1 : 0},
          {"catalog_flow_fallback_removal_parity_gate_removal_allowed",
           catalog_flow_parity_gate_removal_allowed ? 1 : 0},
          {"catalog_flow_fallback_removal_parity_gate_blocker_count",
           catalog_flow_parity_gate_blocker_count},
          {"catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
           catalog_flow_parity_gate_fallback_surface_count},
          {"catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
           catalog_flow_parity_gate_parity_refresh_required_count},
          {"catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
           catalog_flow_parity_gate_current_repo_action_count},
          {"catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
           catalog_flow_parity_gate_target_machine_action_count},
          {"catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
           catalog_flow_parity_gate_source_location_gap_count},
          {"catalog_flow_fallback_removal_parity_gate_blocks_removal",
           catalog_flow_parity_gate_blocks_removal ? 1 : 0},
      };
    }
    const bool product_replay_linkage_present =
        inferlab_artifact::json_bool_value_or_false(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_present");
    const std::string product_replay_linkage_status = json_string_or_empty(
        evidence_query_summary_source,
        "product_replay_semantic_linkage_status");
    const int product_replay_linkage_read_issue_count =
        json_number_or_default(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_read_issue_count", 0);
    const int product_replay_linkage_pending_event_count =
        json_number_or_default(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_pending_event_count", 0);
    const int product_replay_linkage_pending_rule_count =
        json_number_or_default(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_pending_rule_count", 0);
    const int product_replay_linkage_blocked_correlation_count =
        json_number_or_default(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_blocked_correlation_count", 0);
    const int product_replay_linkage_blocker_count = json_number_or_default(
        evidence_query_summary_source,
        "product_replay_semantic_linkage_blocker_count",
        0);
    const int product_replay_linkage_target_or_validation_action_count =
        json_number_or_default(
            evidence_query_summary_source,
            "product_replay_semantic_linkage_target_or_validation_action_count",
            0);
    const std::string product_replay_linkage_first_blocker =
        json_string_or_empty(evidence_query_summary_source,
                             "product_replay_semantic_linkage_first_blocker");
    const std::string product_replay_linkage_next_action =
        json_string_or_empty(evidence_query_summary_source,
                             "product_replay_semantic_linkage_next_action");
    const bool product_replay_linkage_status_blocked =
        product_replay_linkage_present && !product_replay_linkage_status.empty() &&
        product_replay_linkage_status != "semantic_linkage_ready" &&
        product_replay_linkage_status != "ready";
    const bool product_replay_linkage_needs_repair =
        product_replay_linkage_read_issue_count > 0 ||
        product_replay_linkage_pending_event_count > 0 ||
        product_replay_linkage_pending_rule_count > 0 ||
        product_replay_linkage_blocked_correlation_count > 0 ||
        product_replay_linkage_blocker_count > 0 ||
        product_replay_linkage_target_or_validation_action_count > 0 ||
        product_replay_linkage_status_blocked;
    if (product_replay_linkage_needs_repair) {
      const std::string status =
          product_replay_linkage_status.empty() ? "unknown"
                                                : product_replay_linkage_status;
      paths.add("repair_evidence_query_product_replay_semantic_linkage",
          "native_evidence_query_result.json",
          "product_replay_semantic_linkage",
          RepairEnvironment::kTargetProductOrReplayEvidence, "high",
          "Evidence Query reports product replay semantic-linkage status " +
              status +
              (product_replay_linkage_first_blocker.empty()
                   ? std::string("")
                   : " with first blocker " +
                         product_replay_linkage_first_blocker),
          product_replay_linkage_next_action.empty()
              ? "refresh product replay semantic events, alignment, referenced native artifacts, Evidence Query, release pipeline, summary, Prometheus, and final report after replay evidence exists"
              : product_replay_linkage_next_action,
          "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query product_replay_semantic_linkage --output results/raw/native_evidence_query_result.json --format json");
      paths.back().evidence_counters = {
          {"product_replay_semantic_linkage_present",
           product_replay_linkage_present ? 1 : 0},
          {"product_replay_semantic_linkage_read_issue_count",
           product_replay_linkage_read_issue_count},
          {"product_replay_semantic_linkage_pending_event_count",
           product_replay_linkage_pending_event_count},
          {"product_replay_semantic_linkage_pending_rule_count",
           product_replay_linkage_pending_rule_count},
          {"product_replay_semantic_linkage_blocked_correlation_count",
           product_replay_linkage_blocked_correlation_count},
          {"product_replay_semantic_linkage_blocker_count",
           product_replay_linkage_blocker_count},
          {"product_replay_semantic_linkage_target_or_validation_action_count",
           product_replay_linkage_target_or_validation_action_count},
          {"product_replay_semantic_linkage_status_blocked",
           product_replay_linkage_status_blocked ? 1 : 0},
      };
    }
    const int snapshot_added_deep_path_count =
        inferlab::internal::checked_int_from_size(
            snapshot_diff_signals.added_deep_path_count,
            "snapshot diff added deep path count");
    const int snapshot_removed_deep_path_count =
        inferlab::internal::checked_int_from_size(
            snapshot_diff_signals.removed_deep_path_count,
            "snapshot diff removed deep path count");
    const int snapshot_changed_deep_value_count =
        inferlab::internal::checked_int_from_size(
            snapshot_diff_signals.changed_deep_value_count,
            "snapshot diff changed deep value count");
    const bool snapshot_needs_review =
        snapshot_diff_signals.query_present &&
        (snapshot_added_deep_path_count > 0 ||
         snapshot_removed_deep_path_count > 0 ||
         snapshot_changed_deep_value_count > 0 ||
         snapshot_diff_signals.deep_semantic_truncated ||
         snapshot_diff_signals.deep_semantic_decision ==
             "snapshot_diff_deep_semantic_parse_error");
    if (snapshot_needs_review) {
      paths.add("review_evidence_query_snapshot_diff",
          "native_evidence_query_result.json",
          "evidence_query_snapshot_diff_review",
          RepairEnvironment::kCurrentRepoFixable,
          snapshot_diff_signals.deep_semantic_decision ==
                  "snapshot_diff_deep_semantic_parse_error"
              ? "high"
              : "medium",
          snapshot_diff_signals.deep_semantic_decision.empty()
              ? "evidence query snapshot diff reports nested JSON path changes"
              : "evidence query snapshot diff decision is " +
                    snapshot_diff_signals.deep_semantic_decision,
          "inspect the bounded JSON Pointer path diff, decide whether the changed evidence is expected, then refresh evidence query, release pipeline, summary, Prometheus, and final report",
          "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation diff_snapshots --before results/raw/native_evidence_query_result.previous.json --after results/raw/native_evidence_query_result.json --output results/raw/native_evidence_query_result.json --format json");
      paths.back().evidence_counters = {
          {"snapshot_diff_added_deep_path_count",
           snapshot_added_deep_path_count},
          {"snapshot_diff_removed_deep_path_count",
           snapshot_removed_deep_path_count},
          {"snapshot_diff_changed_deep_value_count",
           snapshot_changed_deep_value_count},
          {"snapshot_diff_deep_semantic_truncated",
           snapshot_diff_signals.deep_semantic_truncated ? 1 : 0},
          {"snapshot_diff_deep_semantic_parse_error",
           snapshot_diff_signals.deep_semantic_decision ==
                   "snapshot_diff_deep_semantic_parse_error"
               ? 1
               : 0},
      };
    }
  }

}

}  // namespace inferlab_release_pipeline
