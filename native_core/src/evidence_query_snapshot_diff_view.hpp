#pragma once

#include "evidence_query_snapshot_diff.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

inline std::string snapshot_to_release_focus_json(
    const SnapshotDiffSignals& signals,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const bool review_required = snapshot_release_review_required(signals);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"link_status\": "
      << inferlab_plan::json_string(snapshot_release_link_status(signals))
      << ",\n";
  out << nested << "\"release_blocker_reason\": "
      << inferlab_plan::json_string(snapshot_release_blocker_reason(signals))
      << ",\n";
  out << nested << "\"release_review_required\": "
      << (review_required ? "true" : "false") << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << (review_required ? 1 : 0) << ",\n";
  out << nested << "\"changed_deep_path_count\": "
      << snapshot_deep_change_count(signals) << ",\n";
  out << nested << "\"added_deep_path_count\": "
      << signals.added_deep_path_count << ",\n";
  out << nested << "\"removed_deep_path_count\": "
      << signals.removed_deep_path_count << ",\n";
  out << nested << "\"changed_deep_value_count\": "
      << signals.changed_deep_value_count << ",\n";
  out << nested << "\"deep_semantic_truncated\": "
      << (signals.deep_semantic_truncated ? "true" : "false") << ",\n";
  out << nested << "\"deep_semantic_decision\": "
      << inferlab_plan::json_string(signals.deep_semantic_diff_decision)
      << ",\n";
  out << nested << "\"linked_repair_category\": "
      << inferlab_plan::json_string("evidence_query_snapshot_diff_review")
      << ",\n";
  out << nested << "\"linked_group_id\": "
      << inferlab_plan::json_string(
             "category_evidence_query_snapshot_diff_review")
      << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(snapshot_release_next_action(signals))
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"snapshot-to-release focus is read-only guidance derived from bounded snapshot diff metadata; it does not execute commands, refresh artifacts, render reports, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string snapshot_diff_json(const SnapshotDiffSignals& signals,
                                      const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"operation\": \"diff_snapshots\",\n";
  out << nested << "\"configured\": " << (signals.configured ? "true" : "false")
      << ",\n";
  out << nested << "\"before_configured\": "
      << (signals.before_configured ? "true" : "false") << ",\n";
  out << nested << "\"after_configured\": "
      << (signals.after_configured ? "true" : "false") << ",\n";
  out << nested << "\"before_snapshot_path\": "
      << inferlab_plan::redacted_optional_path_json(signals.before_path)
      << ",\n";
  out << nested << "\"before_snapshot_path_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(signals.before_path)
      << ",\n";
  out << nested << "\"before_snapshot_present\": "
      << (signals.before_present ? "true" : "false") << ",\n";
  out << nested << "\"before_snapshot_regular_file\": "
      << (signals.before_regular_file ? "true" : "false") << ",\n";
  out << nested << "\"before_snapshot_status\": "
      << inferlab_plan::json_string(signals.before_status) << ",\n";
  out << nested << "\"before_snapshot_readable\": "
      << (signals.before_readable ? "true" : "false") << ",\n";
  out << nested << "\"before_snapshot_read_status\": "
      << inferlab_plan::json_string(signals.before_read_status) << ",\n";
  out << nested << "\"before_snapshot_size_bytes\": "
      << signals.before_size_bytes << ",\n";
  out << nested << "\"before_snapshot_content_fingerprint\": "
      << (signals.before_content_fingerprint.empty()
              ? "null"
              : inferlab_plan::json_string(
                    signals.before_content_fingerprint))
      << ",\n";
  out << nested << "\"after_snapshot_path\": "
      << inferlab_plan::redacted_optional_path_json(signals.after_path)
      << ",\n";
  out << nested << "\"after_snapshot_path_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(signals.after_path)
      << ",\n";
  out << nested << "\"after_snapshot_present\": "
      << (signals.after_present ? "true" : "false") << ",\n";
  out << nested << "\"after_snapshot_regular_file\": "
      << (signals.after_regular_file ? "true" : "false") << ",\n";
  out << nested << "\"after_snapshot_status\": "
      << inferlab_plan::json_string(signals.after_status) << ",\n";
  out << nested << "\"after_snapshot_readable\": "
      << (signals.after_readable ? "true" : "false") << ",\n";
  out << nested << "\"after_snapshot_read_status\": "
      << inferlab_plan::json_string(signals.after_read_status) << ",\n";
  out << nested << "\"after_snapshot_size_bytes\": "
      << signals.after_size_bytes << ",\n";
  out << nested << "\"after_snapshot_content_fingerprint\": "
      << (signals.after_content_fingerprint.empty()
              ? "null"
              : inferlab_plan::json_string(
                    signals.after_content_fingerprint))
      << ",\n";
  out << nested << "\"path_issue_count\": " << signals.path_issue_count
      << ",\n";
  out << nested << "\"read_issue_count\": " << signals.read_issue_count
      << ",\n";
  out << nested << "\"content_compared\": "
      << (signals.content_compared ? "true" : "false") << ",\n";
  out << nested << "\"content_changed\": "
      << (signals.content_changed ? "true" : "false") << ",\n";
  out << nested << "\"semantic_compared\": "
      << (signals.semantic_compared ? "true" : "false") << ",\n";
  out << nested << "\"deep_semantic_compared\": "
      << (signals.deep_semantic_compared ? "true" : "false") << ",\n";
  out << nested << "\"deep_semantic_truncated\": "
      << (signals.deep_semantic_truncated ? "true" : "false") << ",\n";
  out << nested << "\"before_json_object\": "
      << (signals.before_json_object ? "true" : "false") << ",\n";
  out << nested << "\"after_json_object\": "
      << (signals.after_json_object ? "true" : "false") << ",\n";
  out << nested << "\"before_top_level_key_count\": "
      << signals.before_top_level_key_count << ",\n";
  out << nested << "\"after_top_level_key_count\": "
      << signals.after_top_level_key_count << ",\n";
  out << nested << "\"common_top_level_key_count\": "
      << signals.common_top_level_key_count << ",\n";
  out << nested << "\"added_top_level_key_count\": "
      << signals.added_top_level_key_count << ",\n";
  out << nested << "\"removed_top_level_key_count\": "
      << signals.removed_top_level_key_count << ",\n";
  out << nested << "\"changed_top_level_value_count\": "
      << signals.changed_top_level_value_count << ",\n";
  out << nested << "\"before_deep_path_count\": "
      << signals.before_deep_path_count << ",\n";
  out << nested << "\"after_deep_path_count\": "
      << signals.after_deep_path_count << ",\n";
  out << nested << "\"common_deep_path_count\": "
      << signals.common_deep_path_count << ",\n";
  out << nested << "\"added_deep_path_count\": "
      << signals.added_deep_path_count << ",\n";
  out << nested << "\"removed_deep_path_count\": "
      << signals.removed_deep_path_count << ",\n";
  out << nested << "\"changed_deep_value_count\": "
      << signals.changed_deep_value_count << ",\n";
  out << nested << "\"added_top_level_keys\": "
      << inferlab_plan::json_string_array(signals.added_top_level_keys,
                                          indent + 2)
      << ",\n";
  out << nested << "\"removed_top_level_keys\": "
      << inferlab_plan::json_string_array(signals.removed_top_level_keys,
                                          indent + 2)
      << ",\n";
  out << nested << "\"changed_top_level_value_keys\": "
      << inferlab_plan::json_string_array(
             signals.changed_top_level_value_keys, indent + 2)
      << ",\n";
  out << nested << "\"added_deep_paths\": "
      << inferlab_plan::json_string_array(signals.added_deep_paths,
                                          indent + 2)
      << ",\n";
  out << nested << "\"removed_deep_paths\": "
      << inferlab_plan::json_string_array(signals.removed_deep_paths,
                                          indent + 2)
      << ",\n";
  out << nested << "\"changed_deep_value_paths\": "
      << inferlab_plan::json_string_array(signals.changed_deep_value_paths,
                                          indent + 2)
      << ",\n";
  out << nested << "\"size_delta_bytes\": " << signals.size_delta_bytes
      << ",\n";
  out << nested << "\"size_delta_direction\": "
      << inferlab_plan::json_string(signals.size_delta_direction) << ",\n";
  out << nested << "\"semantic_diff_decision\": "
      << inferlab_plan::json_string(signals.semantic_diff_decision) << ",\n";
  out << nested << "\"deep_semantic_diff_decision\": "
      << inferlab_plan::json_string(signals.deep_semantic_diff_decision)
      << ",\n";
  out << nested << "\"diff_decision\": "
      << inferlab_plan::json_string(signals.diff_decision) << ",\n";
  out << nested << "\"snapshot_to_release_focus\": "
      << snapshot_to_release_focus_json(signals, indent + 2) << ",\n";
  out << nested << "\"read_only\": true,\n";
  out << nested << "\"executes_commands\": false,\n";
  out << nested
      << "\"claim_boundary\": \"snapshot diff query reads configured snapshot files only to compute bounded size, fingerprint, top-level JSON object, and bounded JSON Pointer path differences; it does not execute commands, mutate files, render reports, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
