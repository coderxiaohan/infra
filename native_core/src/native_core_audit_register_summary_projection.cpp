#include "native_core_audit_register_summary_projection.hpp"

#include "checked_index.hpp"
#include "native_core_audit_register_count_utils.hpp"
#include "native_core_audit_register_file_policy.hpp"
#include "native_core_audit_register_source.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_native_audit {
namespace {

void count_review_status(const FileAudit& audit,
                         NativeAuditSummaryProjection& projection) {
  if (audit.review_status == "reviewed") {
    checked_increment_count(projection.reviewed_file_count,
                            "native audit reviewed file count");
  } else if (audit.review_status == "reviewed_with_fix") {
    checked_increment_count(projection.reviewed_with_fix_file_count,
                            "native audit reviewed-with-fix file count");
  } else if (audit.review_status == "deferred_with_reason") {
    checked_increment_count(projection.deferred_with_reason_file_count,
                            "native audit deferred file count");
  } else {
    throw std::logic_error("unknown native audit review status: " +
                           audit.review_status);
  }
}

void count_read_status(const FileAudit& audit,
                       NativeAuditSummaryProjection& projection) {
  if (audit.read_status == "ok") {
    checked_increment_count(projection.file_read_status_ok_count,
                            "native audit readable file count");
  } else {
    checked_increment_count(projection.file_read_status_non_ok_count,
                            "native audit non-ok read-status count");
  }
  if (audit.read_status == "empty") {
    checked_increment_count(projection.file_read_status_empty_count,
                            "native audit empty-file count");
  }
  if (!audit.read_error.empty()) {
    checked_increment_count(projection.file_read_error_count,
                            "native audit file-read error count");
  }
}

void count_static_findings(const FileAudit& audit,
                           NativeAuditSummaryProjection& projection) {
  projection.issue_count = inferlab::internal::checked_add_size(
      projection.issue_count, audit.issues_found.size(),
      "native audit issue count");
  if (audit_has_issue(audit, "source_unreadable")) {
    checked_increment_count(projection.source_unreadable_count,
                            "native audit source-unreadable count");
  }
  if (audit_has_issue(audit, "path_output_needs_redaction_review")) {
    checked_increment_count(projection.path_redaction_review_count,
                            "native audit path-redaction review count");
  }
  if (audit_has_issue(
          audit, "numeric_hot_path_needs_checked_arithmetic_review")) {
    checked_increment_count(projection.checked_arithmetic_review_count,
                            "native audit checked-arithmetic review count");
  }
  if (audit_has_issue(audit,
                      "artifact_main_needs_cli_or_contract_review")) {
    checked_increment_count(projection.artifact_contract_review_count,
                            "native audit artifact-contract review count");
  }
}

void remember_subsystem(const FileAudit& audit,
                        std::vector<std::string>& subsystems) {
  if (std::find(subsystems.begin(), subsystems.end(), audit.subsystem) ==
      subsystems.end()) {
    subsystems.push_back(audit.subsystem);
  }
}

std::string progress_state(const NativeAuditSummaryProjection& projection) {
  if (projection.scope_blocker_count > 0U) return "scope_blocked";
  if (projection.in_scope_file_count == 0U) return "empty_scan";
  if (projection.open_review_item_count == 0U) return "static_review_closed";
  if (projection.high_priority_review_item_count > 0U) {
    return "high_priority_backlog";
  }
  return "human_review_backlog";
}

std::string coverage_state(const NativeAuditSummaryProjection& projection) {
  if (projection.scope_blocker_count > 0U) return "scope_blocked";
  if (projection.in_scope_file_count == 0U) return "empty_scan";
  if (projection.high_priority_review_item_count > 0U) {
    return "high_priority_review_blocked";
  }
  if (projection.open_review_item_count > 0U) {
    return "human_review_backlog";
  }
  return "static_review_coverage_closed";
}

}  // namespace

NativeAuditSummaryProjection project_native_audit_summary(
    const std::vector<FileAudit>& audits,
    const std::vector<SourceRootStatus>& roots,
    const ReviewManifest& manifest, const std::size_t scope_blockers) {
  if (scope_blockers > 1U) {
    throw std::invalid_argument(
        "native audit summary scope blocker count must be binary");
  }

  NativeAuditSummaryProjection projection;
  projection.source_root_count = roots.size();
  projection.source_root_issue_count = source_root_issue_count(roots);
  projection.source_root_error_count = source_root_error_count(roots);
  projection.scope_blocker_count = scope_blockers;
  projection.empty_scan = audits.empty();
  if ((projection.empty_scan || projection.source_root_issue_count > 0U) !=
      (scope_blockers > 0U)) {
    throw std::logic_error(
        "native audit summary scope state disagrees with source scan");
  }

  projection.review_manifest_configured = manifest.configured;
  projection.review_manifest_present = manifest.present;
  projection.review_manifest_parse_ok = manifest.parse_ok;
  projection.review_manifest_override_count = manifest.overrides.size();
  projection.review_manifest_applied_override_count =
      manifest.applied_override_count;
  projection.review_manifest_invalid_override_count =
      manifest.invalid_override_count;
  projection.review_manifest_unmatched_override_count =
      manifest.unmatched_override_count;
  projection.review_manifest_closed_file_count = manifest.closed_file_count;
  projection.in_scope_file_count = audits.size();

  std::vector<std::string> subsystems;
  subsystems.reserve(audits.size());
  std::vector<const FileAudit*> open_audits;
  open_audits.reserve(audits.size());
  int best_priority = 4;
  for (const auto& audit : audits) {
    count_review_status(audit, projection);
    count_read_status(audit, projection);
    count_static_findings(audit, projection);
    remember_subsystem(audit, subsystems);
    if (!is_open_review_item(audit)) continue;
    open_audits.push_back(&audit);
    const int priority = review_priority_rank(audit);
    best_priority = std::min(best_priority, priority);
    if (priority <= 1) {
      checked_increment_count(projection.high_priority_review_item_count,
                              "native audit high-priority open-file count");
    }
  }
  projection.subsystem_count = subsystems.size();

  const std::size_t classified_file_count =
      inferlab::internal::checked_sum_size(
          {projection.reviewed_file_count,
           projection.reviewed_with_fix_file_count,
           projection.deferred_with_reason_file_count},
          "native audit classified file count");
  if (classified_file_count != audits.size()) {
    throw std::logic_error("native audit review status accounting drift");
  }
  const std::size_t read_status_count =
      inferlab::internal::checked_add_size(
          projection.file_read_status_ok_count,
          projection.file_read_status_non_ok_count,
          "native audit read-status classified count");
  if (read_status_count != audits.size() ||
      projection.file_read_status_empty_count >
          projection.file_read_status_non_ok_count) {
    throw std::logic_error("native audit read-status accounting drift");
  }

  std::sort(open_audits.begin(), open_audits.end(),
            open_review_item_pointer_less);
  projection.open_review_item_count = open_audits.size();
  projection.closed_review_file_count =
      projection.in_scope_file_count - projection.open_review_item_count;
  projection.audit_completion_percent =
      scope_blockers > 0U || projection.in_scope_file_count == 0U
          ? 0U
          : static_cast<std::size_t>(
                inferlab::internal::checked_floor_percentage(
                    projection.closed_review_file_count,
                    projection.in_scope_file_count,
                    "native-core audit review coverage"));
  projection.audit_progress_state = progress_state(projection);
  projection.top_review_priority = review_priority_label(best_priority);

  projection.audit_review_coverage_state = coverage_state(projection);
  projection.audit_review_coverage_open_file_count =
      scope_blockers > 0U ? 0U : projection.open_review_item_count;
  projection.audit_review_coverage_high_priority_open_file_count =
      scope_blockers > 0U ? 0U : projection.high_priority_review_item_count;
  projection.audit_review_coverage_current_repo_action_count =
      scope_blockers > 0U || projection.in_scope_file_count == 0U
          ? 1U
          : projection.open_review_item_count;
  projection.audit_review_coverage_target_machine_action_count =
      scope_blockers == 0U && projection.in_scope_file_count > 0U &&
              projection.open_review_item_count == 0U
          ? 1U
          : 0U;
  projection.audit_review_coverage_source_location_gap_count =
      scope_blockers > 0U && projection.in_scope_file_count == 0U ? 1U : 0U;
  const FileAudit* first_open =
      open_audits.empty() ? nullptr : open_audits.front();
  projection.audit_review_coverage_primary_blocker =
      scope_blockers > 0U
          ? "native_core_audit_scope_blocked"
          : (projection.in_scope_file_count == 0U
                 ? "native_core_audit_empty_scan"
                 : (first_open == nullptr
                        ? "none"
                        : file_completion_blocker_category_for(*first_open)));
  projection.audit_review_coverage_primary_environment =
      scope_blockers > 0U || first_open != nullptr ||
              projection.in_scope_file_count == 0U
          ? "current_repo_fixable"
          : "target_gpu_or_execute_machine";
  projection.audit_review_coverage_first_open_file =
      scope_blockers > 0U
          ? "native_core/include,native_core/src"
          : (first_open == nullptr ? "none" : first_open->file);
  projection.audit_review_coverage_next_action =
      scope_blockers > 0U
          ? "repair native_core source-root scope before trusting audit coverage"
          : (projection.in_scope_file_count == 0U
                 ? "repair repo-root or native_core source discovery, then refresh native_core_audit_register"
                 : (first_open == nullptr
                        ? "static audit coverage is closed; refresh audit/query/release artifacts and run target-machine validation when enabled"
                        : current_repo_action_for_file(*first_open)));
  projection.audit_review_coverage_blocks_full_audit_closure =
      scope_blockers > 0U || projection.open_review_item_count > 0U ||
      projection.audit_review_coverage_source_location_gap_count > 0U;

  projection.artifact_status =
      scope_blockers > 0U ? "native_core_audit_scope_blocked"
                          : "native_core_audit_register_ready";
  projection.scan_status =
      projection.empty_scan
          ? "empty_scan"
          : (projection.source_root_issue_count > 0U ? "root_issues"
                                                     : "ready");
  projection.audit_decision =
      scope_blockers > 0U
          ? "audit_scope_input_repair_required"
          : "static_register_ready_human_review_not_complete";
  projection.evidence_confidence =
      scope_blockers > 0U ? "stale_or_blocked" : "schema_only";
  return projection;
}

}  // namespace inferlab_native_audit
