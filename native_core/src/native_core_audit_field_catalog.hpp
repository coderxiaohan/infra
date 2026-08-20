#pragma once

#include "native_core_audit_projection.hpp"

#include <cstddef>
#include <string>

namespace inferlab_evidence {

using NativeCoreAuditSignals = NativeCoreAuditProjectionSignals;

struct NativeCoreAuditSizeFieldSpec {
  const char* direct_key;
  std::size_t NativeCoreAuditSignals::* member;
};

struct NativeCoreAuditBoolFieldSpec {
  const char* direct_key;
  bool NativeCoreAuditSignals::* member;
};

struct NativeCoreAuditStringFieldSpec {
  const char* direct_key;
  std::string NativeCoreAuditSignals::* member;
};

inline constexpr NativeCoreAuditSizeFieldSpec kNativeCoreAuditSizeFields[] = {
    {"source_root_count", &NativeCoreAuditSignals::source_root_count},
    {"source_root_issue_count",
     &NativeCoreAuditSignals::source_root_issue_count},
    {"source_root_error_count",
     &NativeCoreAuditSignals::source_root_error_count},
    {"scope_blocker_count", &NativeCoreAuditSignals::scope_blocker_count},
    {"review_manifest_override_count",
     &NativeCoreAuditSignals::review_manifest_override_count},
    {"review_manifest_applied_override_count",
     &NativeCoreAuditSignals::review_manifest_applied_override_count},
    {"review_manifest_invalid_override_count",
     &NativeCoreAuditSignals::review_manifest_invalid_override_count},
    {"review_manifest_unmatched_override_count",
     &NativeCoreAuditSignals::review_manifest_unmatched_override_count},
    {"review_manifest_closed_file_count",
     &NativeCoreAuditSignals::review_manifest_closed_file_count},
    {"in_scope_file_count", &NativeCoreAuditSignals::in_scope_file_count},
    {"subsystem_count", &NativeCoreAuditSignals::subsystem_count},
    {"reviewed_file_count", &NativeCoreAuditSignals::reviewed_file_count},
    {"reviewed_with_fix_file_count",
     &NativeCoreAuditSignals::reviewed_with_fix_file_count},
    {"deferred_with_reason_file_count",
     &NativeCoreAuditSignals::deferred_with_reason_file_count},
    {"issue_count", &NativeCoreAuditSignals::issue_count},
    {"source_unreadable_count",
     &NativeCoreAuditSignals::source_unreadable_count},
    {"file_read_status_ok_count",
     &NativeCoreAuditSignals::file_read_status_ok_count},
    {"file_read_status_empty_count",
     &NativeCoreAuditSignals::file_read_status_empty_count},
    {"file_read_status_non_ok_count",
     &NativeCoreAuditSignals::file_read_status_non_ok_count},
    {"file_read_error_count", &NativeCoreAuditSignals::file_read_error_count},
    {"path_redaction_review_count",
     &NativeCoreAuditSignals::path_redaction_review_count},
    {"checked_arithmetic_review_count",
     &NativeCoreAuditSignals::checked_arithmetic_review_count},
    {"artifact_contract_review_count",
     &NativeCoreAuditSignals::artifact_contract_review_count},
    {"closed_review_file_count",
     &NativeCoreAuditSignals::closed_review_file_count},
    {"open_review_item_count",
     &NativeCoreAuditSignals::open_review_item_count},
    {"high_priority_review_item_count",
     &NativeCoreAuditSignals::high_priority_review_item_count},
    {"audit_completion_percent",
     &NativeCoreAuditSignals::audit_completion_percent},
    {"audit_review_coverage_open_file_count",
     &NativeCoreAuditSignals::audit_review_coverage_open_file_count},
    {"audit_review_coverage_high_priority_open_file_count",
     &NativeCoreAuditSignals::
         audit_review_coverage_high_priority_open_file_count},
    {"audit_review_coverage_current_repo_action_count",
     &NativeCoreAuditSignals::audit_review_coverage_current_repo_action_count},
    {"audit_review_coverage_target_machine_action_count",
     &NativeCoreAuditSignals::audit_review_coverage_target_machine_action_count},
    {"audit_review_coverage_source_location_gap_count",
     &NativeCoreAuditSignals::audit_review_coverage_source_location_gap_count},
};

inline constexpr NativeCoreAuditBoolFieldSpec kNativeCoreAuditBoolFields[] = {
    {"empty_scan", &NativeCoreAuditSignals::empty_scan},
    {"review_manifest_configured",
     &NativeCoreAuditSignals::review_manifest_configured},
    {"review_manifest_present", &NativeCoreAuditSignals::review_manifest_present},
    {"review_manifest_parse_ok",
     &NativeCoreAuditSignals::review_manifest_parse_ok},
    {"audit_review_coverage_blocks_full_audit_closure",
     &NativeCoreAuditSignals::
         audit_review_coverage_blocks_full_audit_closure},
};

inline constexpr NativeCoreAuditStringFieldSpec
    kNativeCoreAuditStringFields[] = {
        {"scan_status", &NativeCoreAuditSignals::scan_status},
        {"audit_progress_state", &NativeCoreAuditSignals::audit_progress_state},
        {"audit_review_coverage_state",
         &NativeCoreAuditSignals::audit_review_coverage_state},
        {"audit_review_coverage_primary_blocker",
         &NativeCoreAuditSignals::audit_review_coverage_primary_blocker},
        {"audit_review_coverage_primary_environment",
         &NativeCoreAuditSignals::audit_review_coverage_primary_environment},
        {"audit_review_coverage_first_open_file",
         &NativeCoreAuditSignals::audit_review_coverage_first_open_file},
        {"audit_review_coverage_next_action",
         &NativeCoreAuditSignals::audit_review_coverage_next_action},
        {"top_review_priority", &NativeCoreAuditSignals::top_review_priority},
        {"audit_decision", &NativeCoreAuditSignals::audit_decision},
        {"evidence_confidence", &NativeCoreAuditSignals::evidence_confidence},
};

inline std::string native_core_audit_query_key(const char* direct_key) {
  return "native_core_audit_" + std::string(direct_key);
}

inline bool native_core_audit_has_meaningful_string(
    const std::string& value) {
  return !value.empty() && value != "unknown";
}

inline void normalize_native_core_audit_unknown_strings(
    NativeCoreAuditSignals& signals) {
  for (const auto& field : kNativeCoreAuditStringFields) {
    if ((signals.*(field.member)).empty()) {
      signals.*(field.member) = "unknown";
    }
  }
}

}  // namespace inferlab_evidence
