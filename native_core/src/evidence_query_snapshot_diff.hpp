#pragma once

#include "artifact_metadata_reader.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

namespace fs = std::filesystem;

struct SnapshotDiffSignals {
  bool configured = false;
  bool before_configured = false;
  bool after_configured = false;
  bool before_present = false;
  bool after_present = false;
  bool before_regular_file = false;
  bool after_regular_file = false;
  bool before_readable = false;
  bool after_readable = false;
  bool content_compared = false;
  bool content_changed = false;
  bool semantic_compared = false;
  bool deep_semantic_compared = false;
  bool deep_semantic_truncated = false;
  bool before_json_object = false;
  bool after_json_object = false;
  std::string before_path;
  std::string after_path;
  std::string before_status = "not_configured";
  std::string after_status = "not_configured";
  std::string before_read_status = "not_configured";
  std::string after_read_status = "not_configured";
  std::string before_content_fingerprint;
  std::string after_content_fingerprint;
  std::size_t path_issue_count = 0;
  std::size_t read_issue_count = 0;
  std::size_t before_size_bytes = 0;
  std::size_t after_size_bytes = 0;
  std::size_t size_delta_bytes = 0;
  std::size_t before_top_level_key_count = 0;
  std::size_t after_top_level_key_count = 0;
  std::size_t common_top_level_key_count = 0;
  std::size_t added_top_level_key_count = 0;
  std::size_t removed_top_level_key_count = 0;
  std::size_t changed_top_level_value_count = 0;
  std::size_t before_deep_path_count = 0;
  std::size_t after_deep_path_count = 0;
  std::size_t common_deep_path_count = 0;
  std::size_t added_deep_path_count = 0;
  std::size_t removed_deep_path_count = 0;
  std::size_t changed_deep_value_count = 0;
  std::vector<std::string> added_top_level_keys;
  std::vector<std::string> removed_top_level_keys;
  std::vector<std::string> changed_top_level_value_keys;
  std::vector<std::string> added_deep_paths;
  std::vector<std::string> removed_deep_paths;
  std::vector<std::string> changed_deep_value_paths;
  std::string size_delta_direction = "none";
  std::string diff_decision = "snapshot_diff_not_configured";
  std::string semantic_diff_decision = "snapshot_diff_semantic_not_compared";
  std::string deep_semantic_diff_decision =
      "snapshot_diff_deep_semantic_not_compared";
};

namespace detail {

inline std::string snapshot_path_status(const fs::path& path,
                                        bool& present,
                                        bool& regular_file) {
  present = false;
  regular_file = false;
  if (path.empty()) return "not_configured";
  const std::string status = inferlab_artifact::path_probe_status(path);
  if (status == "missing") return "missing";
  if (status == "path_probe_error") return "path_probe_failed";
  present = true;
  regular_file = status == "file";
  return regular_file ? "present" : "not_regular_file";
}

inline std::map<std::string, std::string> top_level_json_value_fingerprints(
    const std::string& text, bool* parse_ok) {
  if (parse_ok != nullptr) *parse_ok = false;
  std::map<std::string, std::string> values;
  if (!inferlab_artifact::json_document_is_valid_object(text)) return values;
  std::size_t pos = inferlab_artifact::skip_json_space(text, 0);
  if (pos >= text.size() || text[pos] != '{') return values;
  ++pos;

  while (pos < text.size()) {
    pos = inferlab_artifact::skip_json_space(text, pos);
    if (pos < text.size() && text[pos] == '}') {
      const std::size_t trailing =
          inferlab_artifact::skip_json_space(text, pos + 1);
      if (parse_ok != nullptr) *parse_ok = trailing == text.size();
      return values;
    }
    if (pos >= text.size()) break;
    if (text[pos] == ',') {
      ++pos;
      continue;
    }
    if (text[pos] != '"') break;

    std::size_t close = std::string::npos;
    std::string key;
    if (!inferlab_artifact::parse_json_string_literal(text, pos, close, key)) {
      break;
    }
    std::size_t colon = inferlab_artifact::skip_json_space(text, close + 1);
    if (colon >= text.size() || text[colon] != ':') break;
    const std::size_t value_start =
        inferlab_artifact::skip_json_space(text, colon + 1);
    const std::size_t value_end =
        inferlab_artifact::find_json_value_end(text, value_start);
    if (value_end == std::string::npos || value_end < value_start) break;
    values[key] = inferlab_plan::hex_u64(
        inferlab_plan::fnv1a64(text.substr(value_start,
                                           value_end - value_start)));
    pos = inferlab_artifact::skip_json_space(text, value_end);
    if (pos < text.size() && text[pos] == ',') {
      pos = inferlab_artifact::skip_json_space(text, pos + 1);
      if (pos < text.size() && text[pos] == '}') break;
    }
  }
  return values;
}

inline void add_bounded_key(std::vector<std::string>& values,
                     const std::string& value) {
  if (values.size() < 20) values.push_back(value);
}

inline std::string json_pointer_escape(const std::string& value) {
  std::string escaped;
  for (const char ch : value) {
    if (ch == '~') {
      escaped += "~0";
    } else if (ch == '/') {
      escaped += "~1";
    } else {
      escaped.push_back(ch);
    }
  }
  return escaped;
}

inline std::string append_json_pointer(const std::string& path,
                                const std::string& component) {
  return path + "/" + json_pointer_escape(component);
}

inline void add_bounded_path(std::vector<std::string>& values,
                      const std::string& value) {
  if (values.size() < 20) values.push_back(value);
}

inline void add_deep_fingerprint(std::map<std::string, std::string>& values,
                          const std::string& path,
                          const std::string& fingerprint,
                          bool& truncated) {
  if (values.size() >= 256) {
    truncated = true;
    return;
  }
  values[path.empty() ? "/" : path] = fingerprint;
}

inline bool collect_deep_json_fingerprints(const std::string& text,
                                    std::size_t start,
                                    const std::string& path,
                                    const std::size_t depth,
                                    std::map<std::string, std::string>& values,
                                    bool& truncated,
                                    std::size_t& end) {
  start = inferlab_artifact::skip_json_space(text, start);
  if (start >= text.size()) return false;
  const char ch = text[start];
  if (depth >= 8 && (ch == '{' || ch == '[')) {
    const std::size_t value_end =
        inferlab_artifact::find_json_value_end(text, start);
    if (value_end == std::string::npos || value_end < start) return false;
    add_deep_fingerprint(values, path,
                         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(
                             text.substr(start, value_end - start))),
                         truncated);
    truncated = true;
    end = value_end;
    return true;
  }

  if (ch == '{') {
    std::size_t pos = inferlab_artifact::skip_json_space(text, start + 1);
    if (pos < text.size() && text[pos] == '}') {
      add_deep_fingerprint(values, path, "empty_object", truncated);
      end = pos + 1;
      return true;
    }
    while (pos < text.size()) {
      if (text[pos] != '"') return false;
      std::size_t close = std::string::npos;
      std::string key;
      if (!inferlab_artifact::parse_json_string_literal(text, pos, close, key)) {
        return false;
      }
      const std::size_t colon =
          inferlab_artifact::skip_json_space(text, close + 1);
      if (colon >= text.size() || text[colon] != ':') return false;
      const std::size_t value_start =
          inferlab_artifact::skip_json_space(text, colon + 1);
      const std::string child_path = append_json_pointer(path, key);
      std::size_t value_end = std::string::npos;
      if (!collect_deep_json_fingerprints(text, value_start, child_path,
                                          depth + 1, values, truncated,
                                          value_end)) {
        return false;
      }
      pos = inferlab_artifact::skip_json_space(text, value_end);
      if (pos < text.size() && text[pos] == ',') {
        pos = inferlab_artifact::skip_json_space(text, pos + 1);
        continue;
      }
      if (pos < text.size() && text[pos] == '}') {
        end = pos + 1;
        return true;
      }
      return false;
    }
    return false;
  }

  if (ch == '[') {
    std::size_t pos = inferlab_artifact::skip_json_space(text, start + 1);
    if (pos < text.size() && text[pos] == ']') {
      add_deep_fingerprint(values, path, "empty_array", truncated);
      end = pos + 1;
      return true;
    }
    std::size_t index = 0;
    while (pos < text.size()) {
      const std::string child_path =
          append_json_pointer(path, std::to_string(index));
      std::size_t value_end = std::string::npos;
      if (!collect_deep_json_fingerprints(text, pos, child_path, depth + 1,
                                          values, truncated, value_end)) {
        return false;
      }
      ++index;
      pos = inferlab_artifact::skip_json_space(text, value_end);
      if (pos < text.size() && text[pos] == ',') {
        pos = inferlab_artifact::skip_json_space(text, pos + 1);
        continue;
      }
      if (pos < text.size() && text[pos] == ']') {
        end = pos + 1;
        return true;
      }
      return false;
    }
    return false;
  }

  const std::size_t value_end =
      inferlab_artifact::find_json_value_end(text, start);
  if (value_end == std::string::npos || value_end < start) return false;
  add_deep_fingerprint(values, path,
                       inferlab_plan::hex_u64(inferlab_plan::fnv1a64(
                           text.substr(start, value_end - start))),
                       truncated);
  end = value_end;
  return true;
}

inline std::map<std::string, std::string> deep_json_value_fingerprints(
    const std::string& text, bool* parse_ok, bool* truncated) {
  if (parse_ok != nullptr) *parse_ok = false;
  if (truncated != nullptr) *truncated = false;
  std::map<std::string, std::string> values;
  bool local_truncated = false;
  const std::size_t start = inferlab_artifact::skip_json_space(text, 0);
  std::size_t end = std::string::npos;
  if (!collect_deep_json_fingerprints(text, start, "", 0, values,
                                      local_truncated, end)) {
    if (truncated != nullptr) *truncated = local_truncated;
    return values;
  }
  const std::size_t trailing = inferlab_artifact::skip_json_space(text, end);
  if (parse_ok != nullptr) *parse_ok = trailing == text.size();
  if (truncated != nullptr) *truncated = local_truncated;
  return values;
}

inline void fill_snapshot_deep_semantic_diff(SnapshotDiffSignals& signals,
                                      const std::string& before_text,
                                      const std::string& after_text) {
  bool before_parse_ok = false;
  bool after_parse_ok = false;
  bool before_truncated = false;
  bool after_truncated = false;
  const auto before_paths =
      deep_json_value_fingerprints(before_text, &before_parse_ok,
                                   &before_truncated);
  const auto after_paths =
      deep_json_value_fingerprints(after_text, &after_parse_ok,
                                   &after_truncated);
  signals.deep_semantic_truncated = before_truncated || after_truncated;
  if (!before_parse_ok || !after_parse_ok) {
    signals.deep_semantic_diff_decision =
        "snapshot_diff_deep_semantic_parse_error";
    return;
  }
  signals.deep_semantic_compared = true;
  signals.before_deep_path_count = before_paths.size();
  signals.after_deep_path_count = after_paths.size();

  for (const auto& entry : before_paths) {
    const auto after_iter = after_paths.find(entry.first);
    if (after_iter == after_paths.end()) {
      ++signals.removed_deep_path_count;
      add_bounded_path(signals.removed_deep_paths, entry.first);
      continue;
    }
    ++signals.common_deep_path_count;
    if (after_iter->second != entry.second) {
      ++signals.changed_deep_value_count;
      add_bounded_path(signals.changed_deep_value_paths, entry.first);
    }
  }
  for (const auto& entry : after_paths) {
    if (before_paths.find(entry.first) == before_paths.end()) {
      ++signals.added_deep_path_count;
      add_bounded_path(signals.added_deep_paths, entry.first);
    }
  }

  signals.deep_semantic_diff_decision =
      signals.deep_semantic_truncated
          ? "snapshot_diff_deep_semantic_truncated"
          : (signals.added_deep_path_count > 0 ||
                     signals.removed_deep_path_count > 0 ||
                     signals.changed_deep_value_count > 0
                 ? "snapshot_diff_deep_semantic_changed"
                 : "snapshot_diff_deep_semantic_unchanged");
}

inline void fill_snapshot_semantic_diff(SnapshotDiffSignals& signals,
                                        const std::string& before_text,
                                        const std::string& after_text) {
  signals.before_json_object =
      inferlab_artifact::json_root_is_object(before_text);
  signals.after_json_object =
      inferlab_artifact::json_root_is_object(after_text);
  if (!signals.before_json_object || !signals.after_json_object) {
    signals.semantic_diff_decision = "snapshot_diff_semantic_not_json_object";
    return;
  }
  if (!inferlab_artifact::json_document_is_valid_object(before_text) ||
      !inferlab_artifact::json_document_is_valid_object(after_text)) {
    signals.semantic_diff_decision = "snapshot_diff_semantic_parse_error";
    signals.deep_semantic_diff_decision =
        "snapshot_diff_deep_semantic_parse_error";
    return;
  }

  bool before_parse_ok = false;
  bool after_parse_ok = false;
  const auto before_keys =
      top_level_json_value_fingerprints(before_text, &before_parse_ok);
  const auto after_keys =
      top_level_json_value_fingerprints(after_text, &after_parse_ok);
  if (!before_parse_ok || !after_parse_ok) {
    signals.semantic_diff_decision = "snapshot_diff_semantic_parse_error";
    return;
  }
  signals.semantic_compared = true;
  signals.before_top_level_key_count = before_keys.size();
  signals.after_top_level_key_count = after_keys.size();

  for (const auto& entry : before_keys) {
    const auto after_iter = after_keys.find(entry.first);
    if (after_iter == after_keys.end()) {
      ++signals.removed_top_level_key_count;
      add_bounded_key(signals.removed_top_level_keys, entry.first);
      continue;
    }
    ++signals.common_top_level_key_count;
    if (after_iter->second != entry.second) {
      ++signals.changed_top_level_value_count;
      add_bounded_key(signals.changed_top_level_value_keys, entry.first);
    }
  }
  for (const auto& entry : after_keys) {
    if (before_keys.find(entry.first) == before_keys.end()) {
      ++signals.added_top_level_key_count;
      add_bounded_key(signals.added_top_level_keys, entry.first);
    }
  }

  signals.semantic_diff_decision =
      signals.added_top_level_key_count > 0 ||
              signals.removed_top_level_key_count > 0 ||
              signals.changed_top_level_value_count > 0
          ? "snapshot_diff_semantic_changed"
          : "snapshot_diff_semantic_unchanged";
  fill_snapshot_deep_semantic_diff(signals, before_text, after_text);
}

}  // namespace detail

inline SnapshotDiffSignals read_snapshot_diff_signals(
    const fs::path& before_snapshot,
    const fs::path& after_snapshot) {
  SnapshotDiffSignals signals;
  signals.before_configured = !before_snapshot.empty();
  signals.after_configured = !after_snapshot.empty();
  signals.configured = signals.before_configured || signals.after_configured;
  signals.before_path =
      signals.before_configured ? before_snapshot.generic_string() : "";
  signals.after_path =
      signals.after_configured ? after_snapshot.generic_string() : "";
  signals.before_status =
      detail::snapshot_path_status(before_snapshot, signals.before_present,
                                   signals.before_regular_file);
  signals.after_status =
      detail::snapshot_path_status(after_snapshot, signals.after_present,
                                   signals.after_regular_file);
  std::string before_snapshot_text;
  std::string after_snapshot_text;
  if (signals.before_configured && signals.before_status != "present") {
    ++signals.path_issue_count;
  }
  if (signals.after_configured && signals.after_status != "present") {
    ++signals.path_issue_count;
  }
  if (signals.before_configured && signals.before_status == "present") {
    const auto before_read =
        inferlab_artifact::read_text_with_status(before_snapshot);
    signals.before_read_status = before_read.status;
    signals.before_readable = before_read.readable;
    if (before_read.readable) {
      before_snapshot_text = before_read.text;
      signals.before_size_bytes = before_read.text.size();
      signals.before_content_fingerprint =
          inferlab_plan::hex_u64(inferlab_plan::fnv1a64(before_read.text));
      signals.before_json_object =
          inferlab_artifact::json_root_is_object(before_read.text);
    } else {
      ++signals.read_issue_count;
    }
  }
  if (signals.after_configured && signals.after_status == "present") {
    const auto after_read =
        inferlab_artifact::read_text_with_status(after_snapshot);
    signals.after_read_status = after_read.status;
    signals.after_readable = after_read.readable;
    if (after_read.readable) {
      after_snapshot_text = after_read.text;
      signals.after_size_bytes = after_read.text.size();
      signals.after_content_fingerprint =
          inferlab_plan::hex_u64(inferlab_plan::fnv1a64(after_read.text));
      signals.after_json_object =
          inferlab_artifact::json_root_is_object(after_read.text);
    } else {
      ++signals.read_issue_count;
    }
  }
  signals.content_compared =
      signals.before_readable && signals.after_readable;
  if (signals.content_compared) {
    signals.content_changed =
        signals.before_size_bytes != signals.after_size_bytes ||
        signals.before_content_fingerprint !=
            signals.after_content_fingerprint;
    if (signals.before_size_bytes > signals.after_size_bytes) {
      signals.size_delta_bytes =
          signals.before_size_bytes - signals.after_size_bytes;
      signals.size_delta_direction = "decreased";
    } else if (signals.after_size_bytes > signals.before_size_bytes) {
      signals.size_delta_bytes =
          signals.after_size_bytes - signals.before_size_bytes;
      signals.size_delta_direction = "increased";
    } else {
      signals.size_delta_bytes = 0;
      signals.size_delta_direction = "none";
    }
    detail::fill_snapshot_semantic_diff(signals, before_snapshot_text,
                                        after_snapshot_text);
  }
  if (!signals.configured) {
    signals.diff_decision = "snapshot_diff_not_configured";
  } else if (!signals.before_configured || !signals.after_configured) {
    signals.diff_decision = "snapshot_diff_needs_before_and_after";
  } else if (signals.path_issue_count > 0) {
    signals.diff_decision = "snapshot_diff_path_repair_required";
  } else if (signals.read_issue_count > 0) {
    signals.diff_decision = "snapshot_diff_read_repair_required";
  } else if (signals.content_compared && signals.content_changed) {
    signals.diff_decision = "snapshot_diff_content_changed";
  } else if (signals.content_compared) {
    signals.diff_decision = "snapshot_diff_no_content_change";
  } else {
    signals.diff_decision = "snapshot_diff_inputs_ready";
  }
  return signals;
}

inline std::size_t snapshot_deep_change_count(
    const SnapshotDiffSignals& signals) {
  return signals.added_deep_path_count + signals.removed_deep_path_count +
         signals.changed_deep_value_count;
}

inline bool snapshot_release_review_required(
    const SnapshotDiffSignals& signals) {
  return signals.semantic_diff_decision ==
             "snapshot_diff_semantic_not_json_object" ||
         signals.semantic_diff_decision ==
             "snapshot_diff_semantic_parse_error" ||
         signals.deep_semantic_diff_decision ==
             "snapshot_diff_deep_semantic_parse_error" ||
         signals.deep_semantic_truncated ||
         snapshot_deep_change_count(signals) > 0U;
}

inline std::string snapshot_release_blocker_reason(
    const SnapshotDiffSignals& signals) {
  if (!signals.configured) return "snapshot_diff_not_configured";
  if (!signals.before_configured || !signals.after_configured) {
    return "snapshot_diff_needs_before_and_after";
  }
  if (signals.path_issue_count > 0U) return "snapshot_diff_path_repair_required";
  if (signals.read_issue_count > 0U) return "snapshot_diff_read_repair_required";
  if (signals.semantic_diff_decision ==
          "snapshot_diff_semantic_not_json_object" ||
      signals.semantic_diff_decision ==
          "snapshot_diff_semantic_parse_error" ||
      signals.deep_semantic_diff_decision ==
      "snapshot_diff_deep_semantic_parse_error") {
    return "snapshot_diff_parse_error";
  }
  if (signals.deep_semantic_truncated) return "snapshot_diff_truncated";
  if (snapshot_deep_change_count(signals) > 0U) {
    return "snapshot_diff_deep_path_changed";
  }
  return "snapshot_diff_no_release_review_needed";
}

inline std::string snapshot_release_link_status(
    const SnapshotDiffSignals& signals) {
  const std::string reason = snapshot_release_blocker_reason(signals);
  if (reason == "snapshot_diff_not_configured") return "not_configured";
  if (reason == "snapshot_diff_needs_before_and_after" ||
      reason == "snapshot_diff_path_repair_required" ||
      reason == "snapshot_diff_read_repair_required") {
    return "snapshot_inputs_blocked";
  }
  if (snapshot_release_review_required(signals)) {
    return "release_review_required";
  }
  return "no_release_review_needed";
}

inline std::string snapshot_release_next_action(
    const SnapshotDiffSignals& signals) {
  const std::string reason = snapshot_release_blocker_reason(signals);
  if (reason == "snapshot_diff_not_configured") {
    return "configure --before and --after snapshots before using snapshot-to-release review";
  }
  if (reason == "snapshot_diff_needs_before_and_after") {
    return "provide both before and after snapshot paths";
  }
  if (reason == "snapshot_diff_path_repair_required") {
    return "fix snapshot path existence/type before comparing evidence state";
  }
  if (reason == "snapshot_diff_read_repair_required") {
    return "fix snapshot readability before deriving release review guidance";
  }
  if (reason == "snapshot_diff_parse_error") {
    return "inspect snapshot JSON parse errors before trusting release-diff guidance";
  }
  if (reason == "snapshot_diff_truncated") {
    return "narrow snapshots or raise bounded diff limits before treating the diff as exhaustive";
  }
  if (reason == "snapshot_diff_deep_path_changed") {
    return "review changed JSON Pointer paths and map them to release blockers before report_ready";
  }
  return "no snapshot-to-release repair action is currently required";
}

}  // namespace inferlab_evidence_query
