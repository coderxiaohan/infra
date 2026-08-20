#include "native_core_audit_register_manifest.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "native_core_audit_register_count_utils.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_native_audit {
namespace {

constexpr std::size_t kMaxReviewOverrideCount = 4096U;

std::string normalize_manifest_file(std::string file) {
  std::replace(file.begin(), file.end(), '\\', '/');
  while (file.rfind("./", 0) == 0) file.erase(0, 2);
  return file;
}

bool review_status_supported(const std::string& status) {
  return status == "reviewed" || status == "reviewed_with_fix" ||
         status == "deferred_with_reason";
}

bool review_status_closes_file(const std::string& status) {
  return status == "reviewed" || status == "reviewed_with_fix";
}

bool normalize_audited_file_identity(std::string& file) {
  file = normalize_manifest_file(std::move(file));
  if (file.empty() || file.find(':') != std::string::npos) return false;
  const std::filesystem::path path(file);
  if (path.is_absolute() || path.lexically_normal().generic_string() != file) {
    return false;
  }
  for (const auto& component : path) {
    if (component == "." || component == "..") return false;
  }
  const bool in_audit_scope =
      file.rfind("native_core/include/", 0) == 0 ||
      file.rfind("native_core/src/", 0) == 0;
  const bool supported_source =
      file.size() >= 2U &&
      (file.compare(file.size() - 2U, 2U, ".h") == 0 ||
       (file.size() >= 4U &&
        (file.compare(file.size() - 4U, 4U, ".hpp") == 0 ||
         file.compare(file.size() - 4U, 4U, ".cpp") == 0)));
  return in_audit_scope && supported_source;
}

bool read_optional_string(const std::string& object, const std::string& key,
                          std::string& value, std::string& invalid_reason) {
  value.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(object, key)) {
    return true;
  }
  if (inferlab_artifact::extract_top_level_string_key_fallback(object, key,
                                                               value)) {
    return true;
  }
  invalid_reason = "invalid_" + key + "_type";
  return false;
}

bool read_required_string(const std::string& object, const std::string& key,
                          std::string& value, std::string& invalid_reason) {
  value.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(object, key)) {
    invalid_reason = "missing_" + key;
    return false;
  }
  if (!inferlab_artifact::extract_top_level_string_key_fallback(object, key,
                                                                value)) {
    invalid_reason = "invalid_" + key + "_type";
    return false;
  }
  if (value.empty()) {
    invalid_reason = "empty_" + key;
    return false;
  }
  return true;
}

bool read_optional_string_array(const std::string& object,
                                const std::string& key,
                                std::vector<std::string>& values,
                                std::string& invalid_reason) {
  values.clear();
  if (!inferlab_artifact::contains_top_level_key_fallback(object, key)) {
    return true;
  }
  const std::string section =
      inferlab_artifact::json_array_section_or_empty(object, key);
  if (!section.empty() &&
      inferlab_artifact::parse_string_array_items_fallback(section, values)) {
    std::vector<std::string> seen;
    seen.reserve(values.size());
    for (const auto& value : values) {
      if (value.empty()) {
        invalid_reason = "empty_" + key + "_item";
        return false;
      }
      if (std::find(seen.begin(), seen.end(), value) != seen.end()) {
        invalid_reason = "duplicate_" + key + "_item";
        return false;
      }
      seen.push_back(value);
    }
    return true;
  }
  invalid_reason = "invalid_" + key + "_type";
  return false;
}

ReviewOverride parse_review_override(const std::string& object) {
  ReviewOverride entry;
  if (!inferlab_artifact::json_document_is_valid_object(object)) {
    entry.invalid_reason = "invalid_override_object";
    return entry;
  }
  if (!read_required_string(object, "file", entry.file,
                            entry.invalid_reason)) {
    return entry;
  }
  if (!normalize_audited_file_identity(entry.file)) {
    entry.file = inferlab_plan::redacted_path_label(entry.file);
    entry.invalid_reason = "invalid_file_identity";
    return entry;
  }
  if (!read_required_string(object, "review_status", entry.review_status,
                            entry.invalid_reason)) {
    return entry;
  }
  if (!read_optional_string_array(object, "issues_fixed", entry.issues_fixed,
                                  entry.invalid_reason) ||
      !read_optional_string(object, "remaining_risk", entry.remaining_risk,
                            entry.invalid_reason) ||
      !read_optional_string(object, "claim_boundary_risk",
                            entry.claim_boundary_risk,
                            entry.invalid_reason) ||
      !read_optional_string(object, "path_redaction_status",
                            entry.path_redaction_status,
                            entry.invalid_reason) ||
      !read_optional_string(object, "checked_arithmetic_status",
                            entry.checked_arithmetic_status,
                            entry.invalid_reason) ||
      !read_optional_string(object, "artifact_contract_status",
                            entry.artifact_contract_status,
                            entry.invalid_reason) ||
      !read_optional_string(object, "review_notes", entry.review_notes,
                            entry.invalid_reason)) {
    return entry;
  }
  if (!review_status_supported(entry.review_status)) {
    entry.invalid_reason = "unsupported_review_status";
    return entry;
  }
  if (review_status_closes_file(entry.review_status) &&
      entry.review_notes.empty()) {
    entry.invalid_reason = "closing_status_requires_review_notes";
    return entry;
  }
  entry.valid = true;
  return entry;
}

}  // namespace

ReviewManifest read_review_manifest(
    const std::filesystem::path& manifest_path) {
  ReviewManifest manifest;
  manifest.configured = !manifest_path.empty();
  if (!manifest.configured) return manifest;

  manifest.path_label =
      inferlab_plan::redacted_path_label(manifest_path.generic_string());
  const auto read_result =
      inferlab_artifact::read_text_with_status(manifest_path);
  manifest.present = read_result.present;
  manifest.readable = read_result.readable;
  manifest.read_status = read_result.status;
  manifest.read_error = read_result.error;
  if (!read_result.readable) return manifest;

  if (!inferlab_artifact::json_document_is_valid_object(read_result.text)) {
    manifest.read_status = "invalid_json_object";
    manifest.read_error =
        "review manifest must be a strict JSON object with unique keys";
    return manifest;
  }
  if (!inferlab_artifact::contains_top_level_key_fallback(
          read_result.text, "review_overrides")) {
    manifest.read_status = "missing_review_overrides";
    manifest.read_error = "review_overrides array is required";
    return manifest;
  }

  const std::string section = inferlab_artifact::json_array_section_or_empty(
      read_result.text, "review_overrides");
  if (section.empty()) {
    manifest.read_status = "invalid_review_overrides_type";
    manifest.read_error = "review_overrides must be an array";
    return manifest;
  }

  const auto values = inferlab_artifact::top_level_json_values(section);
  if (values.size() > kMaxReviewOverrideCount) {
    manifest.read_status = "review_override_limit_exceeded";
    manifest.read_error = "review_overrides exceeds the bounded row limit";
    return manifest;
  }
  std::vector<std::string> seen_files;
  manifest.overrides.reserve(values.size());
  for (const auto& value : values) {
    const std::size_t start = inferlab_artifact::skip_json_space(value, 0);
    ReviewOverride entry;
    if (start >= value.size() || value[start] != '{') {
      entry.invalid_reason = "override_must_be_object";
    } else {
      entry = parse_review_override(value);
    }
    if (entry.valid &&
        std::find(seen_files.begin(), seen_files.end(), entry.file) !=
            seen_files.end()) {
      entry.valid = false;
      entry.invalid_reason = "duplicate_file_override";
    }
    if (entry.valid) seen_files.push_back(entry.file);
    if (!entry.valid) {
      checked_increment_count(manifest.invalid_override_count,
                      "review manifest invalid override count");
    }
    manifest.overrides.push_back(std::move(entry));
  }
  manifest.parse_ok = manifest.invalid_override_count == 0U;
  if (!manifest.parse_ok) {
    manifest.read_status = "invalid_review_overrides";
    manifest.read_error =
        "review_overrides contains invalid or duplicate entries";
  }
  return manifest;
}

void apply_review_manifest_overrides(std::vector<FileAudit>& audits,
                                     ReviewManifest& manifest) {
  if (!manifest.configured || !manifest.readable || !manifest.parse_ok) return;

  manifest.applied_override_count = 0U;
  manifest.unmatched_override_count = 0U;
  manifest.closed_file_count = 0U;
  for (auto& entry : manifest.overrides) entry.applied = false;

  std::vector<std::size_t> matched_audit_indexes;
  matched_audit_indexes.reserve(manifest.overrides.size());
  for (auto& entry : manifest.overrides) {
    std::size_t match_count = 0U;
    std::size_t matched_index = 0U;
    for (std::size_t index = 0; index < audits.size(); ++index) {
      if (audits[index].file != entry.file) continue;
      matched_index = index;
      checked_increment_count(match_count, "review manifest file match count");
    }
    if (match_count != 1U) {
      entry.valid = false;
      entry.invalid_reason = match_count == 0U ? "unmatched_file"
                                               : "ambiguous_file_match";
      checked_increment_count(manifest.unmatched_override_count,
                      "review manifest unmatched override count");
      checked_increment_count(manifest.invalid_override_count,
                      "review manifest invalid override count");
    } else {
      matched_audit_indexes.push_back(matched_index);
    }
  }

  if (manifest.unmatched_override_count != 0U) {
    manifest.parse_ok = false;
    manifest.read_status = "unmatched_review_overrides";
    manifest.read_error =
        "all review overrides must resolve to exactly one audited file";
    return;
  }

  for (std::size_t index = 0; index < manifest.overrides.size(); ++index) {
    auto& entry = manifest.overrides[index];
    auto& audit = audits[matched_audit_indexes[index]];
    entry.applied = true;
    checked_increment_count(manifest.applied_override_count,
                    "review manifest applied override count");
    audit.review_status = entry.review_status;
    if (review_status_closes_file(entry.review_status)) {
      audit.issues_fixed = entry.issues_fixed.empty() ? audit.issues_found
                                                      : entry.issues_fixed;
      audit.issues_found.clear();
      checked_increment_count(manifest.closed_file_count,
                      "review manifest closed file count");
    } else if (!entry.issues_fixed.empty()) {
      audit.issues_fixed = entry.issues_fixed;
    }
    if (!entry.remaining_risk.empty()) {
      audit.remaining_risk = entry.remaining_risk;
    } else if (review_status_closes_file(entry.review_status)) {
      audit.remaining_risk =
          "closed by explicit review manifest override; validation still not run";
    }
    if (!entry.claim_boundary_risk.empty()) {
      audit.claim_boundary_risk = entry.claim_boundary_risk;
    } else if (review_status_closes_file(entry.review_status)) {
      audit.claim_boundary_risk = "claim_boundary_reviewed_by_manifest";
    }
    if (!entry.path_redaction_status.empty()) {
      audit.path_redaction_status = entry.path_redaction_status;
    }
    if (!entry.checked_arithmetic_status.empty()) {
      audit.checked_arithmetic_status = entry.checked_arithmetic_status;
    }
    if (!entry.artifact_contract_status.empty()) {
      audit.artifact_contract_status = entry.artifact_contract_status;
    }
    if (!entry.review_notes.empty()) {
      audit.review_notes = "review_manifest_override: " + entry.review_notes;
    }
  }
}

}  // namespace inferlab_native_audit
