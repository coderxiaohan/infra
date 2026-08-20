#include "native_core_audit_register_source.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "native_core_audit_register_count_utils.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace inferlab_native_audit {
namespace {

using inferlab_plan::contains;

std::string repo_relative(const std::filesystem::path& repo_root,
                          const std::filesystem::path& path) {
  std::error_code error;
  const auto rel = std::filesystem::relative(path, repo_root, error);
  if (!error && !rel.empty()) return rel.generic_string();
  return inferlab_plan::redacted_path_label(path.generic_string());
}

std::string subsystem_for(const std::string& file) {
  const std::string name = std::filesystem::path(file).filename().string();
  if (contains(file, "include/")) return "public_header";
  if (name == "checked_index.hpp" || name == "plan_json_utils.hpp" ||
      name == "artifact_metadata_reader.hpp" ||
      name == "high_value_artifact_catalog.hpp" ||
      name == "catalog_derived_views.hpp") {
    return "shared_infrastructure";
  }
  if (contains(name, "kv") || contains(name, "cache")) return "kv_cache";
  if (contains(name, "scheduler") || contains(name, "batch")) {
    return "scheduler_batching";
  }
  if (contains(name, "attention") || contains(name, "paged")) {
    return "attention_paged_kv";
  }
  if (contains(name, "matrix") || contains(name, "gemm")) {
    return "matrix_ops";
  }
  if (contains(name, "norm")) return "normalization";
  if (contains(name, "rope")) return "rope";
  if (contains(name, "quant")) return "quantization";
  if (contains(name, "decoder")) return "decoder";
  if (contains(name, "gguf")) return "gguf";
  if (contains(name, "evidence") || contains(name, "release") ||
      contains(name, "catalog") || contains(name, "fixture") ||
      contains(name, "contract") || contains(name, "freshness") ||
      contains(name, "query")) {
    return "evidence_release_governance";
  }
  if (has_suffix(name, "_main.cpp")) return "artifact_probe_main";
  return "native_core_misc";
}

bool is_audit_priority_file(const std::string& file) {
  return contains(file, "checked_index.hpp") ||
         contains(file, "plan_json_utils.hpp") ||
         contains(file, "artifact_metadata_reader.hpp") ||
         contains(file, "high_value_artifact_catalog.hpp") ||
         contains(file, "catalog_derived_views.hpp") ||
         contains(file, "evidence_query_main.cpp") ||
         contains(file, "release_evidence_pipeline_main.cpp") ||
         contains(file, "artifact_catalog_single_source_main.cpp");
}

std::string path_redaction_status_for(const std::string& text,
                                      const std::string& file) {
  if (contains(text, "redacted_path_label") ||
      contains(text, "redacted_path_json_fields") ||
      contains(text, "redact_error_message")) {
    return "uses_shared_redaction_helper";
  }
  if (contains(text, ".generic_string()") || contains(text, ".string()")) {
    return "needs_path_output_review";
  }
  if (contains(text, "report_cli_error")) {
    return "uses_shared_error_redaction_helper";
  }
  if (has_suffix(file, ".hpp")) return "not_path_emitter";
  return "no_path_output_detected";
}

std::string checked_arithmetic_status_for(const std::string& text,
                                          const std::string& subsystem) {
  if (contains(text, "checked_") || contains(text, "parse_bounded") ||
      contains(text, "parse_positive") ||
      contains(text, "parse_non_negative")) {
    return "uses_checked_or_bounded_helpers";
  }
  if (subsystem == "matrix_ops" || subsystem == "attention_paged_kv" ||
      subsystem == "kv_cache" || subsystem == "scheduler_batching" ||
      subsystem == "decoder" || subsystem == "quantization") {
    return "needs_numeric_boundary_review";
  }
  return "not_numeric_hot_path";
}

std::string artifact_contract_delegation_status_for(const std::string& text) {
  if (contains(text, "_runner.hpp") && contains(text, "::run(")) {
    return "artifact_contract_delegated_to_runner";
  }
  if (contains(text, "_request_projection.hpp") &&
      contains(text, "_artifact_view.hpp") &&
      contains(text, "artifact_json(") && contains(text, "write_or_print")) {
    return "artifact_contract_delegated_to_request_projection_view";
  }
  return "";
}

std::string artifact_contract_status_for(const std::string& text,
                                         const std::string& file) {
  if (!has_suffix(file, "_main.cpp")) return "not_artifact_entrypoint";
  const bool has_common_metadata =
      contains(text, "write_common_dry_run_metadata") ||
      contains(text, "write_common_native_probe_contract");
  const std::string delegation_status =
      artifact_contract_delegation_status_for(text);
  if (!delegation_status.empty()) return delegation_status;
  const bool has_claim_scope = contains(text, "\"claim_scope\"") ||
                               has_common_metadata;
  const bool has_mode = contains(text, "\"mode\"") || has_common_metadata;
  const bool has_cli_validation = contains(text, "validate_cli_args");
  if (has_claim_scope && has_mode && has_cli_validation) {
    return "artifact_contract_fields_and_cli_detected";
  }
  return "needs_artifact_contract_review";
}

std::vector<std::string> issues_for(const std::string& text,
                                    const std::string& file,
                                    const std::string& path_redaction_status,
                                    const std::string& checked_status,
                                    const std::string& contract_status,
                                    const bool readable) {
  std::vector<std::string> issues;
  if (!readable) {
    issues.push_back("source_unreadable");
    return issues;
  }
  if (path_redaction_status == "needs_path_output_review") {
    issues.push_back("path_output_needs_redaction_review");
  }
  if (checked_status == "needs_numeric_boundary_review") {
    issues.push_back("numeric_hot_path_needs_checked_arithmetic_review");
  }
  if (contract_status == "needs_artifact_contract_review") {
    issues.push_back("artifact_main_needs_cli_or_contract_review");
  }
  if (contract_status == "artifact_contract_delegated_to_runner") {
    issues.push_back("artifact_main_delegated_runner_requires_review");
  }
  if (contract_status ==
      "artifact_contract_delegated_to_request_projection_view") {
    issues.push_back(
        "artifact_main_delegated_projection_view_requires_review");
  }
  const bool uses_common_dry_run_metadata =
      contains(text, "write_common_dry_run_metadata");
  const bool uses_common_native_probe_contract =
      contains(text, "write_common_native_probe_contract");
  const bool delegates_artifact_contract =
      contract_status == "artifact_contract_delegated_to_runner" ||
      contract_status ==
          "artifact_contract_delegated_to_request_projection_view";
  const bool emits_not_run =
      uses_common_dry_run_metadata || contains(text, "\"not_run\"");
  const bool emits_executed = uses_common_dry_run_metadata ||
                              uses_common_native_probe_contract ||
                              contains(text, "\"executed\"");
  const bool emits_replayed = uses_common_dry_run_metadata ||
                              uses_common_native_probe_contract ||
                              contains(text, "\"replayed\"");
  if (has_suffix(file, "_main.cpp") && !emits_not_run &&
      !uses_common_native_probe_contract && !delegates_artifact_contract) {
    issues.push_back("artifact_main_should_emit_not_run_boundary");
  }
  if (has_suffix(file, "_main.cpp") && !emits_executed &&
      !delegates_artifact_contract) {
    issues.push_back("artifact_main_should_emit_executed_flag");
  }
  if (has_suffix(file, "_main.cpp") && !emits_replayed &&
      !delegates_artifact_contract) {
    issues.push_back("artifact_main_should_emit_replayed_flag");
  }
  return issues;
}

}  // namespace

bool has_suffix(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

FileAudit audit_file(const std::filesystem::path& repo_root,
                     const std::filesystem::path& path) {
  const auto read_result = inferlab_artifact::read_text_with_status(path);
  const bool readable = read_result.readable;
  const std::string& text = read_result.text;
  const std::string rel = repo_relative(repo_root, path);
  FileAudit audit;
  audit.file = rel;
  audit.read_status = read_result.status;
  audit.read_error = read_result.error;
  audit.subsystem = subsystem_for(rel);
  audit.path_redaction_status = path_redaction_status_for(text, rel);
  audit.checked_arithmetic_status =
      checked_arithmetic_status_for(text, audit.subsystem);
  audit.artifact_contract_status = artifact_contract_status_for(text, rel);
  audit.issues_found =
      issues_for(text, rel, audit.path_redaction_status,
                 audit.checked_arithmetic_status, audit.artifact_contract_status,
                 readable);
  if (!readable) {
    audit.review_status = "deferred_with_reason";
    audit.remaining_risk = "source file could not be read on current server";
  } else if (audit.issues_found.empty() && is_audit_priority_file(rel)) {
    audit.review_status = "reviewed";
    audit.remaining_risk =
        "static audit checks found no immediate catalog/query/release blocker; build/runtime validation not run";
  } else if (audit.issues_found.empty()) {
    audit.review_status = "deferred_with_reason";
    audit.remaining_risk =
        "static scan clean but file still needs human code review before full-audit closure";
  } else {
    audit.review_status = "deferred_with_reason";
    audit.remaining_risk =
        "static audit found review items that should be fixed or manually triaged";
  }
  audit.claim_boundary_risk =
      has_suffix(rel, "_main.cpp") && !contains(text, "claim_scope") &&
              !contains(text, "write_common_dry_run_metadata")
          ? "claim_scope_boundary_needs_review"
          : "claim_boundary_present_or_not_artifact";
  audit.review_notes =
      "implementation-only audit register entry; no compile/test/native execution performed";
  return audit;
}

NativeSourceScan scan_native_sources(
    const std::filesystem::path& repo_root) {
  NativeSourceScan result;
  const std::vector<std::string> roots = {"native_core/include",
                                          "native_core/src"};
  result.roots.reserve(roots.size());
  for (const auto& root_name : roots) {
    const auto root_path = repo_root / root_name;
    SourceRootStatus status;
    status.root = root_name;

    std::error_code error;
    status.present = std::filesystem::exists(root_path, error);
    if (error) {
      status.status = "path_probe_failed";
      status.error = inferlab_plan::redact_error_message(
          error.message(), {root_path.generic_string()});
      result.roots.push_back(std::move(status));
      continue;
    }
    if (!status.present) {
      status.status = "missing";
      result.roots.push_back(std::move(status));
      continue;
    }
    status.directory = std::filesystem::is_directory(root_path, error);
    if (error) {
      status.status = "path_probe_failed";
      status.error = inferlab_plan::redact_error_message(
          error.message(), {root_path.generic_string()});
      result.roots.push_back(std::move(status));
      continue;
    }
    if (!status.directory) {
      status.status = "not_directory";
      result.roots.push_back(std::move(status));
      continue;
    }

    const std::size_t first_file_index = result.files.size();
    std::filesystem::recursive_directory_iterator it(
        root_path, std::filesystem::directory_options::none, error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
      status.status = "directory_unreadable";
      status.error = inferlab_plan::redact_error_message(
          error.message(), {root_path.generic_string()});
      result.roots.push_back(std::move(status));
      continue;
    }

    bool scan_failed = false;
    for (; it != end;) {
      error.clear();
      const bool regular = it->is_regular_file(error);
      if (error) {
        scan_failed = true;
      } else if (regular) {
        const std::string path = it->path().generic_string();
        if (!contains(path, "/tests/") &&
            (has_suffix(path, ".cpp") || has_suffix(path, ".hpp") ||
             has_suffix(path, ".h"))) {
          result.files.push_back(it->path());
        }
      }
      if (scan_failed) break;
      it.increment(error);
      if (error) {
        scan_failed = true;
        break;
      }
    }
    if (scan_failed) {
      result.files.resize(first_file_index);
      status.status = "recursive_scan_failed";
      status.error = inferlab_plan::redact_error_message(
          error.message(), {root_path.generic_string()});
      result.roots.push_back(std::move(status));
      continue;
    }

    status.readable = true;
    status.status = "ready";
    result.roots.push_back(std::move(status));
  }
  std::sort(result.files.begin(), result.files.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.generic_string() < rhs.generic_string();
            });
  return result;
}

std::size_t source_root_issue_count(
    const std::vector<SourceRootStatus>& statuses) {
  std::size_t count = 0U;
  for (const auto& status : statuses) {
    if (status.status != "ready") {
      checked_increment_count(count, "native audit source root issue count");
    }
  }
  return count;
}

std::size_t source_root_error_count(
    const std::vector<SourceRootStatus>& statuses) {
  std::size_t count = 0U;
  for (const auto& status : statuses) {
    if (!status.error.empty()) {
      checked_increment_count(count, "native audit source root error count");
    }
  }
  return count;
}

std::size_t audit_scope_blocker_count(
    const std::vector<FileAudit>& audits,
    const std::vector<SourceRootStatus>& statuses) {
  return audits.empty() || source_root_issue_count(statuses) > 0U ? 1U : 0U;
}

}  // namespace inferlab_native_audit
