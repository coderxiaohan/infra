#pragma once

#include "native_core_audit_register_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_native_audit {

struct NativeAuditSummaryProjection {
  std::string artifact_status;
  std::string scan_status;
  std::string audit_decision;
  std::string evidence_confidence;
  std::string audit_progress_state;
  std::string top_review_priority;

  std::size_t source_root_count = 0U;
  std::size_t source_root_issue_count = 0U;
  std::size_t source_root_error_count = 0U;
  std::size_t scope_blocker_count = 0U;
  bool empty_scan = false;

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

  std::string audit_review_coverage_state;
  std::size_t audit_review_coverage_open_file_count = 0U;
  std::size_t audit_review_coverage_high_priority_open_file_count = 0U;
  std::size_t audit_review_coverage_current_repo_action_count = 0U;
  std::size_t audit_review_coverage_target_machine_action_count = 0U;
  std::size_t audit_review_coverage_source_location_gap_count = 0U;
  std::string audit_review_coverage_primary_blocker;
  std::string audit_review_coverage_primary_environment;
  std::string audit_review_coverage_first_open_file;
  std::string audit_review_coverage_next_action;
  bool audit_review_coverage_blocks_full_audit_closure = false;
};

NativeAuditSummaryProjection project_native_audit_summary(
    const std::vector<FileAudit>& audits,
    const std::vector<SourceRootStatus>& roots,
    const ReviewManifest& manifest, std::size_t scope_blockers);

}  // namespace inferlab_native_audit
