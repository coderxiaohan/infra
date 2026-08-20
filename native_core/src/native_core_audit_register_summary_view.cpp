#include "native_core_audit_register_summary_view.hpp"

#include "native_core_audit_register_view_utils.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_native_audit {

std::string native_core_audit_summary_json(
    const NativeCoreAuditRegisterRequest& request, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      checked_view_indent(indent, 2, "native audit summary indent"));
  const auto& summary = request.summary();

  std::ostringstream out;
  out << "{\n";
  out << nested << "\"source_root_count\": " << summary.source_root_count
      << ",\n";
  out << nested << "\"source_root_issue_count\": "
      << summary.source_root_issue_count << ",\n";
  out << nested << "\"source_root_error_count\": "
      << summary.source_root_error_count << ",\n";
  out << nested << "\"scope_blocker_count\": "
      << summary.scope_blocker_count << ",\n";
  out << nested << "\"empty_scan\": "
      << inferlab_plan::json_bool(summary.empty_scan) << ",\n";
  out << nested << "\"scan_status\": "
      << inferlab_plan::json_string(summary.scan_status) << ",\n";
  out << nested << "\"review_manifest_configured\": "
      << inferlab_plan::json_bool(summary.review_manifest_configured)
      << ",\n";
  out << nested << "\"review_manifest_present\": "
      << inferlab_plan::json_bool(summary.review_manifest_present) << ",\n";
  out << nested << "\"review_manifest_parse_ok\": "
      << inferlab_plan::json_bool(summary.review_manifest_parse_ok) << ",\n";
  out << nested << "\"review_manifest_override_count\": "
      << summary.review_manifest_override_count << ",\n";
  out << nested << "\"review_manifest_applied_override_count\": "
      << summary.review_manifest_applied_override_count << ",\n";
  out << nested << "\"review_manifest_invalid_override_count\": "
      << summary.review_manifest_invalid_override_count << ",\n";
  out << nested << "\"review_manifest_unmatched_override_count\": "
      << summary.review_manifest_unmatched_override_count << ",\n";
  out << nested << "\"review_manifest_closed_file_count\": "
      << summary.review_manifest_closed_file_count << ",\n";
  out << nested << "\"in_scope_file_count\": "
      << summary.in_scope_file_count << ",\n";
  out << nested << "\"subsystem_count\": " << summary.subsystem_count
      << ",\n";
  out << nested << "\"reviewed_file_count\": "
      << summary.reviewed_file_count << ",\n";
  out << nested << "\"reviewed_with_fix_file_count\": "
      << summary.reviewed_with_fix_file_count << ",\n";
  out << nested << "\"deferred_with_reason_file_count\": "
      << summary.deferred_with_reason_file_count << ",\n";
  out << nested << "\"issue_count\": " << summary.issue_count << ",\n";
  out << nested << "\"source_unreadable_count\": "
      << summary.source_unreadable_count << ",\n";
  out << nested << "\"file_read_status_ok_count\": "
      << summary.file_read_status_ok_count << ",\n";
  out << nested << "\"file_read_status_empty_count\": "
      << summary.file_read_status_empty_count << ",\n";
  out << nested << "\"file_read_status_non_ok_count\": "
      << summary.file_read_status_non_ok_count << ",\n";
  out << nested << "\"file_read_error_count\": "
      << summary.file_read_error_count << ",\n";
  out << nested << "\"path_redaction_review_count\": "
      << summary.path_redaction_review_count << ",\n";
  out << nested << "\"checked_arithmetic_review_count\": "
      << summary.checked_arithmetic_review_count << ",\n";
  out << nested << "\"artifact_contract_review_count\": "
      << summary.artifact_contract_review_count << ",\n";
  out << nested << "\"closed_review_file_count\": "
      << summary.closed_review_file_count << ",\n";
  out << nested << "\"open_review_item_count\": "
      << summary.open_review_item_count << ",\n";
  out << nested << "\"high_priority_review_item_count\": "
      << summary.high_priority_review_item_count << ",\n";
  out << nested << "\"audit_completion_percent\": "
      << summary.audit_completion_percent << ",\n";
  out << nested << "\"audit_progress_state\": "
      << inferlab_plan::json_string(summary.audit_progress_state) << ",\n";
  out << nested << "\"audit_review_coverage_state\": "
      << inferlab_plan::json_string(summary.audit_review_coverage_state)
      << ",\n";
  out << nested << "\"audit_review_coverage_open_file_count\": "
      << summary.audit_review_coverage_open_file_count << ",\n";
  out << nested
      << "\"audit_review_coverage_high_priority_open_file_count\": "
      << summary.audit_review_coverage_high_priority_open_file_count
      << ",\n";
  out << nested
      << "\"audit_review_coverage_current_repo_action_count\": "
      << summary.audit_review_coverage_current_repo_action_count << ",\n";
  out << nested
      << "\"audit_review_coverage_target_machine_action_count\": "
      << summary.audit_review_coverage_target_machine_action_count << ",\n";
  out << nested
      << "\"audit_review_coverage_source_location_gap_count\": "
      << summary.audit_review_coverage_source_location_gap_count << ",\n";
  out << nested << "\"audit_review_coverage_primary_blocker\": "
      << inferlab_plan::json_string(
             summary.audit_review_coverage_primary_blocker)
      << ",\n";
  out << nested << "\"audit_review_coverage_primary_environment\": "
      << inferlab_plan::json_string(
             summary.audit_review_coverage_primary_environment)
      << ",\n";
  out << nested << "\"audit_review_coverage_first_open_file\": "
      << inferlab_plan::json_string(
             summary.audit_review_coverage_first_open_file)
      << ",\n";
  out << nested << "\"audit_review_coverage_next_action\": "
      << inferlab_plan::json_string(summary.audit_review_coverage_next_action)
      << ",\n";
  out << nested
      << "\"audit_review_coverage_blocks_full_audit_closure\": "
      << inferlab_plan::json_bool(
             summary.audit_review_coverage_blocks_full_audit_closure)
      << ",\n";
  out << nested << "\"top_review_priority\": "
      << inferlab_plan::json_string(summary.top_review_priority) << ",\n";
  out << nested << "\"audit_decision\": "
      << inferlab_plan::json_string(summary.audit_decision) << ",\n";
  out << nested << "\"evidence_confidence\": "
      << inferlab_plan::json_string(summary.evidence_confidence) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_native_audit
