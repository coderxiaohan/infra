#include "release_evidence_pipeline_repair_producer.hpp"

#include "checked_index.hpp"
#include "release_evidence_pipeline_repair_metrics.hpp"

#include <string>

namespace inferlab_release_pipeline {

using inferlab_evidence::RepairEnvironment;

void append_native_audit_repair_paths(
    const ReleasePipelineSourceContext& sources,
    RepairPathCollector& paths) {
  const auto& native_audit_meta = sources.native_audit->metadata;
  const auto& native_audit_projection = sources.native_audit_projection;
  const auto& native_audit_signals = native_audit_projection.signals;
  const int query_native_audit_blocker_count =
      inferlab::internal::checked_int_from_size(
          native_audit_projection.blocker_count,
          "release pipeline native audit blocker count");
  const int query_native_audit_deferred_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.deferred_with_reason_file_count,
          "release pipeline native audit deferred count");
  const int query_native_audit_issue_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.issue_count,
          "release pipeline native audit issue count");
  const int query_native_audit_open_review_item_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.open_review_item_count,
          "release pipeline native audit open review-item count");
  const int query_native_audit_source_root_issue_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.source_root_issue_count,
          "release pipeline native audit source-root issue count");
  const int query_native_audit_scope_blocker_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.scope_blocker_count,
          "release pipeline native audit scope blocker count");
  const bool query_native_audit_empty_scan = native_audit_signals.empty_scan;
  const bool native_audit_filter_present =
      native_audit_signals.filter_query_present;
  const int query_native_audit_review_coverage_open_file_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals.audit_review_coverage_open_file_count,
          "release pipeline native audit coverage open-file count");
  const int query_native_audit_review_coverage_high_priority_open_file_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals
              .audit_review_coverage_high_priority_open_file_count,
          "release pipeline native audit coverage high-priority count");
  const int query_native_audit_review_coverage_source_location_gap_count =
      inferlab::internal::checked_int_from_size(
          native_audit_signals
              .audit_review_coverage_source_location_gap_count,
          "release pipeline native audit coverage source gap count");
  const std::string query_native_audit_review_coverage_state =
      native_audit_signals.audit_review_coverage_state;
  const std::string query_native_audit_review_coverage_first_open_file =
      native_audit_signals.audit_review_coverage_first_open_file;
  const std::string query_native_audit_review_coverage_next_action =
      native_audit_signals.audit_review_coverage_next_action;
  const bool query_native_audit_review_coverage_blocks_full_audit_closure =
      native_audit_signals
          .audit_review_coverage_blocks_full_audit_closure ||
      query_native_audit_review_coverage_open_file_count > 0 ||
      query_native_audit_review_coverage_high_priority_open_file_count > 0 ||
      query_native_audit_review_coverage_source_location_gap_count > 0;
  const int query_native_audit_scope_issue_count =
      inferlab::internal::checked_add_int(
          inferlab::internal::checked_add_int(
              query_native_audit_source_root_issue_count,
              query_native_audit_scope_blocker_count,
              "release pipeline query native audit scope issue count"),
          query_native_audit_empty_scan ? 1 : 0,
          "release pipeline query native audit empty-scan count");
  const int native_audit_scope_issue_count =
      query_native_audit_scope_issue_count;
  const int query_native_audit_backlog_blocker_count =
      query_native_audit_blocker_count > query_native_audit_scope_issue_count
          ? query_native_audit_blocker_count -
                query_native_audit_scope_issue_count
          : 0;
  if (native_audit_scope_issue_count > 0) {
    paths.add("repair_native_core_audit_scope_blocked",
        "native_core_audit_register.json,native_evidence_query_result.json",
        "native_core_audit_scope_blocked",
        RepairEnvironment::kCurrentRepoThenValidationMachine, "high",
        "native-core audit scope is blocked by missing/unreadable source roots or an empty scan",
        "repair native_core source-root availability and refresh audit register, Evidence Query, release pipeline, summary, Prometheus, and final report",
        "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json");
    paths.back().evidence_counters = {
        {"native_core_audit_scope_issue_count",
         native_audit_scope_issue_count},
        {"query_native_core_audit_source_root_issue_count",
         query_native_audit_source_root_issue_count},
        {"query_native_core_audit_scope_blocker_count",
         query_native_audit_scope_blocker_count},
        {"query_native_core_audit_empty_scan",
         query_native_audit_empty_scan ? 1 : 0},
    };
  }
  if (query_native_audit_review_coverage_blocks_full_audit_closure) {
    paths.add("repair_native_core_audit_review_coverage_gate",
        "native_core_audit_register.json,native_evidence_query_result.json",
        "native_core_audit_review_coverage_gate",
        RepairEnvironment::kCurrentRepoThenValidationMachine, "high",
        "native-core audit review coverage gate reports open files or source-location gaps",
        query_native_audit_review_coverage_next_action.empty()
            ? "close or explicitly defer native_core audit files, then refresh audit register, Evidence Query, release pipeline, summary, Prometheus, and final report"
            : query_native_audit_review_coverage_next_action,
        "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json");
    paths.back().evidence_counters = {
        {"query_native_core_audit_review_coverage_open_file_count",
         query_native_audit_review_coverage_open_file_count},
        {"query_native_core_audit_review_coverage_high_priority_open_file_count",
         query_native_audit_review_coverage_high_priority_open_file_count},
        {"query_native_core_audit_review_coverage_source_location_gap_count",
         query_native_audit_review_coverage_source_location_gap_count},
        {"query_native_core_audit_review_coverage_blocks_full_audit_closure",
         query_native_audit_review_coverage_blocks_full_audit_closure ? 1 : 0},
    };
    if (!query_native_audit_review_coverage_state.empty()) {
      paths.back().evidence_counters.push_back(
          {"query_native_core_audit_review_coverage_state_present", 1});
    }
    if (!query_native_audit_review_coverage_first_open_file.empty()) {
      paths.back().evidence_counters.push_back(
          {"query_native_core_audit_review_coverage_first_open_file_present",
           1});
    }
  }
  if (!native_audit_meta.present || !native_audit_filter_present ||
      query_native_audit_backlog_blocker_count > 0 ||
      query_native_audit_deferred_count > 0 ||
      query_native_audit_issue_count > 0 ||
      query_native_audit_open_review_item_count > 0) {
    paths.add("repair_native_core_audit_backlog",
        native_audit_meta.present ? "native_evidence_query_result.json"
                                  : "native_core_audit_register.json",
        "native_core_audit_backlog",
        RepairEnvironment::kCurrentRepoThenValidationMachine, "high",
        native_audit_meta.present
            ? "native-core audit filter reports blockers, open or deferred files, or missing filter wiring"
            : "native_core_audit_register artifact is missing",
        "close deferred native-core audit items or record explicit deferral reasons, then refresh audit register, Evidence Query, release pipeline, summary, Prometheus, and final report",
        native_audit_meta.present && !native_audit_filter_present
            ? "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query native_core_audit --output results/raw/native_evidence_query_result.json --format json"
            : "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json");
    paths.back().evidence_counters = {
        {"native_core_audit_register_missing",
         native_audit_meta.present ? 0 : 1},
        {"native_core_audit_filter_missing",
         native_audit_filter_present ? 0 : 1},
        {"query_native_core_audit_backlog_blocker_count",
         query_native_audit_backlog_blocker_count},
        {"query_native_core_audit_deferred_with_reason_file_count",
         query_native_audit_deferred_count},
        {"query_native_core_audit_issue_count", query_native_audit_issue_count},
        {"query_native_core_audit_open_review_item_count",
         query_native_audit_open_review_item_count},
    };
  }

}

}  // namespace inferlab_release_pipeline
