#include "evidence_query_result_artifact_sections.hpp"

#include "evidence_query_snapshot_diff.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

void append_result_artifact_summary_snapshot_and_close(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& snapshot = projection.request.snapshot_diff;
  out << "    \"snapshot_diff_query_present\": true,\n";
  out << "    \"snapshot_diff_configured\": "
      << inferlab_plan::json_bool(snapshot.configured) << ",\n";
  out << "    \"snapshot_diff_before_configured\": "
      << inferlab_plan::json_bool(snapshot.before_configured) << ",\n";
  out << "    \"snapshot_diff_after_configured\": "
      << inferlab_plan::json_bool(snapshot.after_configured) << ",\n";
  out << "    \"snapshot_diff_before_present\": "
      << inferlab_plan::json_bool(snapshot.before_present) << ",\n";
  out << "    \"snapshot_diff_after_present\": "
      << inferlab_plan::json_bool(snapshot.after_present) << ",\n";
  out << "    \"snapshot_diff_path_issue_count\": "
      << snapshot.path_issue_count << ",\n";
  out << "    \"snapshot_diff_read_issue_count\": "
      << snapshot.read_issue_count << ",\n";
  out << "    \"snapshot_diff_content_compared\": "
      << inferlab_plan::json_bool(snapshot.content_compared) << ",\n";
  out << "    \"snapshot_diff_content_changed\": "
      << inferlab_plan::json_bool(snapshot.content_changed) << ",\n";
  out << "    \"snapshot_diff_semantic_compared\": "
      << inferlab_plan::json_bool(snapshot.semantic_compared) << ",\n";
  out << "    \"snapshot_diff_deep_semantic_compared\": "
      << inferlab_plan::json_bool(snapshot.deep_semantic_compared) << ",\n";
  out << "    \"snapshot_diff_deep_semantic_truncated\": "
      << inferlab_plan::json_bool(snapshot.deep_semantic_truncated) << ",\n";
  out << "    \"snapshot_diff_added_top_level_key_count\": "
      << snapshot.added_top_level_key_count << ",\n";
  out << "    \"snapshot_diff_removed_top_level_key_count\": "
      << snapshot.removed_top_level_key_count << ",\n";
  out << "    \"snapshot_diff_changed_top_level_value_count\": "
      << snapshot.changed_top_level_value_count << ",\n";
  out << "    \"snapshot_diff_added_deep_path_count\": "
      << snapshot.added_deep_path_count << ",\n";
  out << "    \"snapshot_diff_removed_deep_path_count\": "
      << snapshot.removed_deep_path_count << ",\n";
  out << "    \"snapshot_diff_changed_deep_value_count\": "
      << snapshot.changed_deep_value_count << ",\n";
  out << "    \"snapshot_diff_before_size_bytes\": "
      << snapshot.before_size_bytes << ",\n";
  out << "    \"snapshot_diff_after_size_bytes\": "
      << snapshot.after_size_bytes << ",\n";
  out << "    \"snapshot_diff_size_delta_bytes\": "
      << snapshot.size_delta_bytes << ",\n";
  out << "    \"snapshot_diff_semantic_decision\": "
      << inferlab_plan::json_string(snapshot.semantic_diff_decision) << ",\n";
  out << "    \"snapshot_diff_deep_semantic_decision\": "
      << inferlab_plan::json_string(snapshot.deep_semantic_diff_decision)
      << ",\n";
  out << "    \"snapshot_diff_decision\": "
      << inferlab_plan::json_string(snapshot.diff_decision) << ",\n";
  out << "    \"snapshot_to_release_focus_present\": true,\n";
  out << "    \"snapshot_to_release_focus_link_status\": "
      << inferlab_plan::json_string(snapshot_release_link_status(snapshot))
      << ",\n";
  out << "    \"snapshot_to_release_focus_blocker_reason\": "
      << inferlab_plan::json_string(snapshot_release_blocker_reason(snapshot))
      << ",\n";
  out << "    \"snapshot_to_release_focus_review_required\": "
      << inferlab_plan::json_bool(snapshot_release_review_required(snapshot))
      << ",\n";
  out << "    \"snapshot_to_release_focus_current_repo_action_count\": "
      << (snapshot_release_review_required(snapshot) ? 1 : 0) << ",\n";
  out << "    \"snapshot_to_release_focus_changed_deep_path_count\": "
      << snapshot_deep_change_count(snapshot) << ",\n";
  out << "    \"query_decision\": "
      << inferlab_plan::json_string(projection.request.query_ready
                                        ? "query_ready"
                                        : "blocked_or_partial")
      << "\n";
  out << "  },\n";
}

}  // namespace inferlab_evidence_query::detail
