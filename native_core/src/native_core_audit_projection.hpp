#pragma once

#include "checked_index.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_evidence {

// Consumer projection of the Native Audit version-2 summary contract.
// Detailed file facts remain in the producer's `files` array and are queried
// on demand instead of being mirrored through Query and Release artifacts.
struct NativeCoreAuditProjectionSignals {
  bool evidence_query_present = false;
  bool evidence_query_readable = false;
  std::string evidence_query_read_status = "missing";
  bool filter_query_present = false;
  bool audit_register_present = false;
  bool audit_register_readable = false;
  std::string audit_register_read_status = "missing";
  std::size_t read_issue_count = 0U;
  bool audit_summary_present = false;

  std::size_t source_root_count = 0U;
  std::size_t source_root_issue_count = 0U;
  std::size_t source_root_error_count = 0U;
  std::size_t scope_blocker_count = 0U;
  bool empty_scan = false;
  std::string scan_status = "unknown";

  bool review_manifest_configured = false;
  bool review_manifest_present = false;
  bool review_manifest_parse_ok = false;
  std::size_t review_manifest_override_count = 0U;
  std::size_t review_manifest_applied_override_count = 0U;
  std::size_t review_manifest_invalid_override_count = 0U;
  std::size_t review_manifest_unmatched_override_count = 0U;
  std::size_t review_manifest_closed_file_count = 0U;

  std::size_t in_scope_file_count = 0U;
  std::size_t subsystem_count = 0U;
  std::size_t reviewed_file_count = 0U;
  std::size_t reviewed_with_fix_file_count = 0U;
  std::size_t deferred_with_reason_file_count = 0U;
  std::size_t issue_count = 0U;
  std::size_t source_unreadable_count = 0U;
  std::size_t file_read_status_ok_count = 0U;
  std::size_t file_read_status_empty_count = 0U;
  std::size_t file_read_status_non_ok_count = 0U;
  std::size_t file_read_error_count = 0U;
  std::size_t path_redaction_review_count = 0U;
  std::size_t checked_arithmetic_review_count = 0U;
  std::size_t artifact_contract_review_count = 0U;
  std::size_t closed_review_file_count = 0U;
  std::size_t open_review_item_count = 0U;
  std::size_t high_priority_review_item_count = 0U;
  std::size_t audit_completion_percent = 0U;
  std::string audit_progress_state = "unknown";

  std::string audit_review_coverage_state = "unknown";
  std::size_t audit_review_coverage_open_file_count = 0U;
  std::size_t audit_review_coverage_high_priority_open_file_count = 0U;
  std::size_t audit_review_coverage_current_repo_action_count = 0U;
  std::size_t audit_review_coverage_target_machine_action_count = 0U;
  std::size_t audit_review_coverage_source_location_gap_count = 0U;
  std::string audit_review_coverage_primary_blocker = "unknown";
  std::string audit_review_coverage_primary_environment = "unknown";
  std::string audit_review_coverage_first_open_file = "unknown";
  std::string audit_review_coverage_next_action = "unknown";
  bool audit_review_coverage_blocks_full_audit_closure = false;

  std::string top_review_priority = "unknown";
  std::string audit_decision = "unknown";
  std::string evidence_confidence = "unknown";
};

inline std::size_t native_core_audit_scope_blocker_count(
    const NativeCoreAuditProjectionSignals& signals) {
  return inferlab::internal::checked_sum_size(
      {signals.scope_blocker_count, signals.source_root_issue_count,
       signals.source_root_error_count, signals.empty_scan ? 1U : 0U},
      "native-core audit scope blocker count");
}

inline std::size_t native_core_audit_core_blocker_count(
    const NativeCoreAuditProjectionSignals& signals) {
  const std::size_t source_contract_blocker =
      signals.read_issue_count > 0U || !signals.audit_register_present ||
              !signals.audit_summary_present
          ? 1U
          : 0U;
  const std::size_t manifest_blockers =
      inferlab::internal::checked_sum_size(
          {signals.review_manifest_configured &&
                   !signals.review_manifest_parse_ok
               ? 1U
               : 0U,
           signals.review_manifest_invalid_override_count,
           signals.review_manifest_unmatched_override_count},
          "native-core audit manifest blocker count");
  const std::size_t review_backlog = inferlab::internal::checked_sum_size(
      {signals.deferred_with_reason_file_count,
       std::max(signals.issue_count, signals.source_unreadable_count),
       std::max(signals.open_review_item_count,
                signals.high_priority_review_item_count)},
      "native-core audit review backlog count");
  const std::size_t coverage_blocker =
      signals.audit_review_coverage_blocks_full_audit_closure ||
              signals.audit_review_coverage_open_file_count > 0U ||
              signals.audit_review_coverage_high_priority_open_file_count >
                  0U ||
              signals.audit_review_coverage_source_location_gap_count > 0U
          ? 1U
          : 0U;
  return inferlab::internal::checked_sum_size(
      {source_contract_blocker, native_core_audit_scope_blocker_count(signals),
       manifest_blockers, review_backlog, coverage_blocker},
      "native-core audit core blocker count");
}

}  // namespace inferlab_evidence
