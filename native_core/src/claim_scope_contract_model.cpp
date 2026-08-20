#include "claim_scope_contract_model.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "claim_scope_contract_matrix.hpp"
#include "plan_json_utils.hpp"

#include <string>
#include <utility>

namespace inferlab_claim_scope {
namespace {

using inferlab_plan::contains;

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}

std::string artifact_family(const std::string& artifact) {
  if (artifact == "product_replay_semantic_events.json") {
    return "product_replay_semantics";
  }
  if (starts_with(artifact, "product_")) return "product_profile";
  if (starts_with(artifact, "native_product_")) {
    return "native_product_contract";
  }
  if (artifact == "native_release_evidence_pipeline.json") {
    return "native_release_gate";
  }
  if (starts_with(artifact, "native_") &&
      (contains(artifact, "deep_dive") ||
       contains(artifact, "replay_alignment"))) {
    return "native_deep_dive";
  }
  if (starts_with(artifact, "native_") &&
      (contains(artifact, "_plan") || contains(artifact, "_simulator") ||
       contains(artifact, "router") || contains(artifact, "matrix"))) {
    return "native_plan_or_simulation";
  }
  if (contains(artifact, "vllm") || contains(artifact, "trtllm") ||
      contains(artifact, "llama_cpp") ||
      contains(artifact, "android_edge")) {
    return "external_runtime_profile";
  }
  if (contains(artifact, "benchmark") || contains(artifact, "profile") ||
      contains(artifact, "quality") || contains(artifact, "comparison")) {
    return "benchmark_evidence";
  }
  return "local_artifact";
}

void require_field(ClaimRecord& record, const bool passed,
                   const std::string& field_name) {
  record.required_checks = inferlab::internal::checked_add_size(
      record.required_checks, 1U, "claim scope required check count");
  if (passed) {
    record.passed_checks = inferlab::internal::checked_add_size(
        record.passed_checks, 1U, "claim scope passed check count");
  } else {
    record.missing_claim_fields.push_back(field_name);
  }
}

bool semantic_key_present(const std::string& text, const std::string& key) {
  return inferlab_artifact::contains_semantic_key_fallback(text, key);
}

bool bool_value_equals(const std::string& text, const std::string& key,
                       const bool expected) {
  return inferlab_artifact::json_bool_value_equals(text, key, expected);
}

bool nonempty_string_value(const std::string& text, const std::string& key,
                           std::string* value = nullptr) {
  std::string parsed;
  const bool valid =
      inferlab_artifact::extract_semantic_string_key_fallback(text, key,
                                                               parsed) &&
      !parsed.empty();
  if (value != nullptr) *value = valid ? parsed : std::string{};
  return valid;
}

void project_semantic_signals(ClaimRecord& record, const std::string& text) {
  record.executed_field_present = semantic_key_present(text, "executed");
  record.executed_true = bool_value_equals(text, "executed", true);
  record.executed_false = bool_value_equals(text, "executed", false);
  record.executed_field_valid =
      record.executed_true || record.executed_false;
  record.execution_kind_present =
      nonempty_string_value(text, "execution_kind");
  std::string claim_scope;
  record.claim_scope_present =
      nonempty_string_value(text, "claim_scope", &claim_scope);
  record.mode_present = nonempty_string_value(text, "mode");
  record.audit_type_present = nonempty_string_value(text, "audit_type");
  record.status_present = nonempty_string_value(text, "status");
  record.external_boundary_present =
      semantic_key_present(text, "external_product_boundary") ||
      contains(claim_scope, "external product") ||
      contains(claim_scope, "mature product") ||
      contains(claim_scope, "product core remains external");
  record.not_implemented_boundary_present =
      semantic_key_present(text, "not_implemented_by_inferlab") ||
      contains(claim_scope, "not implemented") ||
      contains(claim_scope, "not_implemented");
  record.command_or_config_present =
      semantic_key_present(text, "command") ||
      semantic_key_present(text, "planned_commands") ||
      semantic_key_present(text, "config_snapshot") ||
      semantic_key_present(text, "config");
  record.version_or_environment_present =
      semantic_key_present(text, "version_capture") ||
      semantic_key_present(text, "environment_preflight") ||
      semantic_key_present(text, "system") ||
      semantic_key_present(text, "tools");
  record.raw_metrics_present = semantic_key_present(text, "raw_metrics") ||
                               semantic_key_present(text, "metrics");
  record.risky_executed_without_evidence =
      record.executed_true && !record.version_or_environment_present &&
      !record.raw_metrics_present && !record.command_or_config_present;
}

void apply_family_requirements(ClaimRecord& record, const std::string& text) {
  require_field(record,
                !record.executed_field_present ||
                    record.executed_field_valid,
                "executed_must_be_boolean_when_present");
  require_field(record, record.status_present, "status");
  if (record.artifact_family == "product_profile") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record, record.mode_present, "mode");
    require_field(record, record.not_implemented_boundary_present,
                  "not_implemented_by_inferlab");
    require_field(record, record.command_or_config_present,
                  "planned_commands_or_config_snapshot");
    require_field(record, record.version_or_environment_present,
                  "version_capture_or_environment_preflight");
  } else if (record.artifact_family == "native_plan_or_simulation") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record,
                  record.execution_kind_present || record.audit_type_present ||
                      record.claim_scope_present || record.mode_present,
                  "execution_kind_or_audit_type_or_claim_scope");
    require_field(record,
                  !record.executed_true ||
                      !record.risky_executed_without_evidence,
                  "executed_true_requires_command_metrics_or_environment_evidence");
  } else if (record.artifact_family == "native_product_contract") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record,
                  record.audit_type_present || record.claim_scope_present,
                  "audit_type_or_claim_scope");
    require_field(record, record.external_boundary_present ||
                              record.not_implemented_boundary_present,
                  "external_product_boundary");
  } else if (record.artifact_family == "native_deep_dive") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record,
                  record.claim_scope_present || record.audit_type_present,
                  "claim_scope_or_audit_type");
    require_field(record, !record.executed_true,
                  "deep_dive_must_not_use_execute_semantics");
  } else if (record.artifact_family == "native_release_gate") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record, record.mode_present, "mode");
    require_field(record,
                  record.claim_scope_present || record.audit_type_present,
                  "claim_scope_or_audit_type");
    require_field(
        record,
        !inferlab_artifact::json_object_section_or_empty(text, "summary")
             .empty(),
        "summary_object");
    require_field(record, !record.executed_true,
                  "release_gate_must_not_use_execute_semantics");
  } else if (record.artifact_family == "product_replay_semantics") {
    require_field(record, record.executed_field_valid, "executed");
    require_field(record, record.claim_scope_present, "claim_scope");
    require_field(
        record,
        !inferlab_artifact::json_array_section_or_empty(text, "records")
             .empty(),
        "records_array");
    require_field(record, inferlab_artifact::contains_key(text, "event_type"),
                  "event_type");
    require_field(record, !record.executed_true,
                  "semantic_events_must_not_use_execute_semantics");
  } else if (record.artifact_family == "external_runtime_profile") {
    require_field(record,
                  record.executed_field_valid || record.mode_present,
                  "executed_or_mode");
    require_field(record,
                  !record.executed_true ||
                      record.version_or_environment_present,
                  "executed_true_requires_version_or_environment");
  } else {
    require_field(record,
                  !record.executed_true ||
                      !record.risky_executed_without_evidence,
                  "executed_true_requires_evidence");
  }
}

void finalize_record(ClaimRecord& record) {
  record.claim_score =
      record.required_checks == 0U
          ? 0.0
          : static_cast<double>(record.passed_checks) /
                static_cast<double>(record.required_checks);
  if (record.risky_executed_without_evidence) {
    record.status = "fail";
    record.recommendation =
        "do_not_claim_real_execution_without_command_metrics_or_environment_evidence";
  } else if (record.claim_score >= 1.0) {
    record.status = "pass";
    record.recommendation =
        "claim_scope_ready_for_summary_and_final_report";
  } else if (record.claim_score >= 0.6) {
    record.status = "warn";
    record.recommendation =
        "add_missing_claim_scope_fields_before_using_as_release_evidence";
  } else {
    record.status = "fail";
    record.recommendation =
        "artifact_claim_scope_incomplete_fix_before_performance_or_product_claims";
  }
}

ClaimRecord audit_artifact(const std::string& artifact,
                           const std::filesystem::path& results_root,
                           const bool strict_missing) {
  ClaimRecord record;
  record.artifact = artifact;
  record.artifact_family = artifact_family(artifact);
  record.declared_in_matrix = true;
  record.resolved_path = results_root / artifact;

  const auto read_result =
      inferlab_artifact::read_text_with_status(record.resolved_path);
  record.present = read_result.present;
  record.path_probe_status = read_result.status;
  record.path_probe_error = read_result.error;
  record.text_read_status = read_result.status;
  record.text_read_error = read_result.error;
  if (!record.present) {
    require_field(record, false, "artifact_present");
    const bool probe_failed = read_result.status == "unreadable_file_probe";
    record.status = probe_failed || strict_missing
                        ? "fail"
                        : "pending_missing_artifact";
    record.recommendation =
        probe_failed
            ? "investigate_artifact_path_probe_error_before_release_claims"
            : (strict_missing
                   ? "generate_declared_artifact_before_release_claims"
                   : "pending_until_dry_run_execute_or_native_audit_generates_artifact");
    return record;
  }
  if (!read_result.readable) {
    require_field(record, false, "artifact_readable");
    record.status = "fail";
    record.recommendation =
        "repair_unreadable_artifact_before_release_claims";
    return record;
  }

  const std::string& text = read_result.text;
  record.byte_size = static_cast<std::uintmax_t>(text.size());
  record.valid_json_object =
      inferlab_artifact::json_document_is_valid_object(text);
  if (!record.valid_json_object) {
    require_field(record, false, "valid_json_object");
    record.status = "fail";
    record.recommendation =
        "repair_malformed_or_duplicate_key_artifact_before_release_claims";
    return record;
  }

  project_semantic_signals(record, text);
  apply_family_requirements(record, text);
  finalize_record(record);
  return record;
}

void increment_if(std::size_t& value, const bool condition,
                  const char* name) {
  if (!condition) return;
  value = inferlab::internal::checked_add_size(value, 1U, name);
}

void summarize_records(ClaimScopeContractAudit& audit) {
  for (const auto& record : audit.records) {
    increment_if(audit.present_count, record.present,
                 "claim scope present artifact count");
    increment_if(audit.pass_count, record.status == "pass",
                 "claim scope pass count");
    increment_if(audit.warn_count, record.status == "warn",
                 "claim scope warning count");
    increment_if(audit.fail_count, record.status == "fail",
                 "claim scope fail count");
    increment_if(audit.pending_count,
                 record.status == "pending_missing_artifact",
                 "claim scope pending count");
    increment_if(audit.executed_true_count, record.executed_true,
                 "claim scope executed true count");
    increment_if(audit.executed_false_count, record.executed_false,
                 "claim scope executed false count");
    increment_if(audit.claim_scope_count, record.claim_scope_present,
                 "claim scope field count");
    increment_if(audit.risky_executed_count,
                 record.risky_executed_without_evidence,
                 "claim scope risky execute count");
    audit.total_required_checks = inferlab::internal::checked_add_size(
        audit.total_required_checks, record.required_checks,
        "claim scope total required check count");
    audit.total_passed_checks = inferlab::internal::checked_add_size(
        audit.total_passed_checks, record.passed_checks,
        "claim scope total passed check count");
  }
  audit.claim_contract_score =
      audit.total_required_checks == 0U
          ? 0.0
          : static_cast<double>(audit.total_passed_checks) /
                static_cast<double>(audit.total_required_checks);
  if (!audit.matrix_source.parse_issues.empty()) {
    audit.status = "source_invalid";
  } else if (audit.fail_count > 0U) {
    audit.status = "failed";
  } else if (audit.warn_count > 0U) {
    audit.status = "warning";
  } else if (audit.pending_count > 0U) {
    audit.status = "pending";
  } else if (audit.records.empty()) {
    audit.status = "source_invalid";
  } else {
    audit.status = "ok";
  }
}

}  // namespace

ClaimScopeContractAudit evaluate_claim_scope_contract(
    const std::filesystem::path& matrix_path,
    const std::filesystem::path& results_root,
    const bool strict_missing) {
  ClaimScopeContractAudit audit;
  audit.matrix_path = matrix_path;
  audit.results_root = results_root;
  audit.strict_missing = strict_missing;

  const auto read_result = inferlab_artifact::read_text_with_status(matrix_path);
  audit.matrix_source.present = read_result.present;
  audit.matrix_source.readable = read_result.readable;
  audit.matrix_source.empty = read_result.empty;
  audit.matrix_source.read_status = read_result.status;
  audit.matrix_source.read_error = read_result.error;
  audit.matrix_source.read_issue_count =
      !read_result.present || !read_result.readable || read_result.empty ? 1U
                                                                        : 0U;

  ClaimScopeMatrixInventory inventory;
  if (!read_result.present) {
    inventory.issues.push_back("benchmark_matrix_missing");
  } else if (!read_result.readable) {
    inventory.issues.push_back("benchmark_matrix_unreadable");
  } else if (read_result.empty) {
    inventory.issues.push_back("benchmark_matrix_empty");
  } else {
    inventory = parse_claim_scope_matrix(read_result.text);
    if (inventory.artifacts.empty()) {
      inventory.issues.push_back(
          "benchmark_matrix_has_no_results_raw_outputs");
    }
  }

  audit.matrix_source.ignored_output_count = inventory.ignored_output_count;
  audit.matrix_source.duplicate_output_count = inventory.duplicate_output_count;
  audit.matrix_source.parse_issues = std::move(inventory.issues);
  audit.matrix_source.issue_count = audit.matrix_source.parse_issues.size();
  audit.records.reserve(inventory.artifacts.size());
  for (const auto& artifact : inventory.artifacts) {
    if (artifact == kClaimScopeContractArtifact) {
      audit.matrix_source.self_output_deferred_count =
          inferlab::internal::checked_add_size(
              audit.matrix_source.self_output_deferred_count, 1U,
              "claim scope self-output deferred count");
      continue;
    }
    audit.records.push_back(
        audit_artifact(artifact, results_root, strict_missing));
  }
  summarize_records(audit);
  return audit;
}

}  // namespace inferlab_claim_scope
