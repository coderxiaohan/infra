#include "evidence_query_text_view.hpp"

#include "evidence_query_consumer_filter_signals.hpp"

#include <sstream>

namespace inferlab_evidence_query::detail {

std::string evidence_query_text_native_audit_and_snapshot(
    const EvidenceQueryRequest& request) {
  const auto& audit = request.native_core_audit;
  const auto& snapshot = request.snapshot_diff;

  std::ostringstream out;
  out << "native_core_audit_filter: decision=" << audit.filter_decision
      << " | present=" << (audit.audit_register_present ? "true" : "false")
      << " | summary_present="
      << (audit.audit_summary_present ? "true" : "false")
      << " | read_status=" << audit.audit_register_read_status
      << " | read_issues=" << audit.read_issue_count
      << " | source_root_issues=" << audit.source_root_issue_count
      << " | scope_blockers=" << audit.scope_blocker_count
      << " | in_scope_files=" << audit.in_scope_file_count
      << " | reviewed=" << audit.reviewed_file_count
      << " | reviewed_with_fix=" << audit.reviewed_with_fix_file_count
      << " | deferred=" << audit.deferred_with_reason_file_count
      << " | issues=" << audit.issue_count
      << " | open_items=" << audit.open_review_item_count
      << " | high_priority=" << audit.high_priority_review_item_count
      << " | completion_percent=" << audit.audit_completion_percent
      << " | manifest_invalid="
      << audit.review_manifest_invalid_override_count
      << " | manifest_unmatched="
      << audit.review_manifest_unmatched_override_count
      << " | coverage_state=" << audit.audit_review_coverage_state
      << " | coverage_primary_blocker="
      << audit.audit_review_coverage_primary_blocker
      << " | next_action=" << audit.audit_review_coverage_next_action
      << " | blockers=" << native_core_audit_blocker_count(audit) << "\n";
  out << "snapshot_diff: decision=" << snapshot.diff_decision
      << " | configured=" << (snapshot.configured ? "true" : "false")
      << " | path_issues=" << snapshot.path_issue_count
      << " | read_issues=" << snapshot.read_issue_count
      << " | content_compared="
      << (snapshot.content_compared ? "true" : "false")
      << " | content_changed="
      << (snapshot.content_changed ? "true" : "false")
      << " | semantic_compared="
      << (snapshot.semantic_compared ? "true" : "false")
      << " | deep_compared="
      << (snapshot.deep_semantic_compared ? "true" : "false")
      << " | deep_truncated="
      << (snapshot.deep_semantic_truncated ? "true" : "false")
      << " | size_delta=" << snapshot.size_delta_bytes
      << " | semantic_decision=" << snapshot.semantic_diff_decision
      << " | deep_semantic_decision="
      << snapshot.deep_semantic_diff_decision << " | read_only=true\n";
  return out.str();
}

}  // namespace inferlab_evidence_query::detail
