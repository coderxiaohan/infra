#include "claim_scope_contract_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_claim_scope {
namespace {

int nested_indent(const int indent) {
  return inferlab::internal::checked_add_int(
      indent, 2, "claim scope JSON nested indent");
}

std::string cli_context_json(const ClaimScopeContractAudit& audit,
                             const ClaimScopeContractViewContext& context,
                             const int indent) {
  const int child_indent = nested_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << child << "\"format\": "
      << inferlab_plan::json_string(context.format) << ",\n";
  out << child << "\"strict_missing\": "
      << inferlab_plan::json_bool(audit.strict_missing) << ",\n";
  out << child << "\"strict_cli_validation\": true,\n";
  out << child << "\"unsupported_positionals_rejected\": true,\n";
  out << child << "\"unsupported_format_rejected\": true,\n";
  out << child << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, child_indent) << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "repo_root", context.repo_root, child_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "matrix", audit.matrix_path, child_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "results_root", audit.results_root, child_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "output", context.output_path, child_indent)
      << ",\n";
  out << child
      << "\"claim_boundary\": \"CLI context documents accepted claim-scope contract inputs only; it audits artifact declarations and does not run producers, benchmarks, products, or report rendering\"\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const ClaimRecord& record, const int indent) {
  const int child_indent = nested_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  const std::string resolved_path = record.resolved_path.string();
  std::ostringstream out;
  out << pad << "{\n";
  out << child << "\"artifact\": "
      << inferlab_plan::json_string(record.artifact) << ",\n";
  out << child << "\"artifact_family\": "
      << inferlab_plan::json_string(record.artifact_family) << ",\n";
  out << child << "\"audit_type_present\": "
      << inferlab_plan::json_bool(record.audit_type_present) << ",\n";
  out << child << "\"byte_size\": " << record.byte_size << ",\n";
  out << child << "\"claim_scope_present\": "
      << inferlab_plan::json_bool(record.claim_scope_present) << ",\n";
  out << child << "\"claim_score\": "
      << inferlab_plan::fixed6(record.claim_score) << ",\n";
  out << child << "\"command_or_config_present\": "
      << inferlab_plan::json_bool(record.command_or_config_present) << ",\n";
  out << child << "\"declared_in_matrix\": "
      << inferlab_plan::json_bool(record.declared_in_matrix) << ",\n";
  out << child << "\"executed_false\": "
      << inferlab_plan::json_bool(record.executed_false) << ",\n";
  out << child << "\"executed_field_present\": "
      << inferlab_plan::json_bool(record.executed_field_present) << ",\n";
  out << child << "\"executed_field_valid\": "
      << inferlab_plan::json_bool(record.executed_field_valid) << ",\n";
  out << child << "\"executed_true\": "
      << inferlab_plan::json_bool(record.executed_true) << ",\n";
  out << child << "\"execution_kind_present\": "
      << inferlab_plan::json_bool(record.execution_kind_present) << ",\n";
  out << child << "\"external_boundary_present\": "
      << inferlab_plan::json_bool(record.external_boundary_present) << ",\n";
  out << child << "\"file_size_probe_error\": "
      << inferlab_plan::json_string(record.file_size_probe_error) << ",\n";
  out << child << "\"missing_claim_fields\": "
      << inferlab_plan::json_string_array(record.missing_claim_fields,
                                           child_indent)
      << ",\n";
  out << child << "\"mode_present\": "
      << inferlab_plan::json_bool(record.mode_present) << ",\n";
  out << child << "\"not_implemented_boundary_present\": "
      << inferlab_plan::json_bool(record.not_implemented_boundary_present)
      << ",\n";
  out << child << "\"passed_checks\": " << record.passed_checks << ",\n";
  out << child << "\"path_probe_error\": "
      << inferlab_plan::json_string(record.path_probe_error) << ",\n";
  out << child << "\"path_probe_status\": "
      << inferlab_plan::json_string(record.path_probe_status) << ",\n";
  out << child << "\"present\": "
      << inferlab_plan::json_bool(record.present) << ",\n";
  out << child << "\"raw_metrics_present\": "
      << inferlab_plan::json_bool(record.raw_metrics_present) << ",\n";
  out << child << "\"recommendation\": "
      << inferlab_plan::json_string(record.recommendation) << ",\n";
  out << child << "\"required_checks\": " << record.required_checks
      << ",\n";
  out << child << "\"resolved_path\": "
      << inferlab_plan::redacted_optional_path_json(resolved_path) << ",\n";
  out << child << "\"resolved_path_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(resolved_path)
      << ",\n";
  out << child << "\"risky_executed_without_evidence\": "
      << inferlab_plan::json_bool(record.risky_executed_without_evidence)
      << ",\n";
  out << child << "\"status\": "
      << inferlab_plan::json_string(record.status) << ",\n";
  out << child << "\"status_present\": "
      << inferlab_plan::json_bool(record.status_present) << ",\n";
  out << child << "\"text_read_error\": "
      << inferlab_plan::json_string(record.text_read_error) << ",\n";
  out << child << "\"text_read_status\": "
      << inferlab_plan::json_string(record.text_read_status) << ",\n";
  out << child << "\"valid_json_object\": "
      << inferlab_plan::json_bool(record.valid_json_object) << ",\n";
  out << child << "\"version_or_environment_present\": "
      << inferlab_plan::json_bool(record.version_or_environment_present)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string summary_json(const ClaimScopeContractAudit& audit,
                         const int indent) {
  const int child_indent = nested_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string child = inferlab_plan::json_indent(child_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << child << "\"artifact_count\": " << audit.records.size() << ",\n";
  out << child << "\"matrix_present\": "
      << inferlab_plan::json_bool(audit.matrix_source.present) << ",\n";
  out << child << "\"matrix_read_issue_count\": "
      << audit.matrix_source.read_issue_count << ",\n";
  out << child << "\"matrix_issue_count\": "
      << audit.matrix_source.issue_count << ",\n";
  out << child << "\"matrix_ignored_output_count\": "
      << audit.matrix_source.ignored_output_count << ",\n";
  out << child << "\"matrix_duplicate_output_count\": "
      << audit.matrix_source.duplicate_output_count << ",\n";
  out << child << "\"matrix_self_output_deferred_count\": "
      << audit.matrix_source.self_output_deferred_count << ",\n";
  out << child << "\"present_count\": " << audit.present_count << ",\n";
  out << child << "\"pass_count\": " << audit.pass_count << ",\n";
  out << child << "\"warn_count\": " << audit.warn_count << ",\n";
  out << child << "\"fail_count\": " << audit.fail_count << ",\n";
  out << child << "\"pending_count\": " << audit.pending_count << ",\n";
  out << child << "\"executed_true_count\": "
      << audit.executed_true_count << ",\n";
  out << child << "\"executed_false_count\": "
      << audit.executed_false_count << ",\n";
  out << child << "\"claim_scope_count\": " << audit.claim_scope_count
      << ",\n";
  out << child << "\"risky_executed_without_evidence_count\": "
      << audit.risky_executed_count << ",\n";
  out << child << "\"total_required_checks\": "
      << audit.total_required_checks << ",\n";
  out << child << "\"total_passed_checks\": "
      << audit.total_passed_checks << ",\n";
  out << child << "\"claim_contract_score\": "
      << inferlab_plan::fixed6(audit.claim_contract_score) << "\n";
  out << pad << "}";
  return out.str();
}

std::string records_json(const ClaimScopeContractAudit& audit,
                         const int indent) {
  const int child_indent = nested_indent(indent);
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[";
  if (!audit.records.empty()) out << "\n";
  for (std::size_t index = 0; index < audit.records.size(); ++index) {
    const bool is_last = index == audit.records.size() - 1U;
    out << record_json(audit.records[index], child_indent)
        << (is_last ? "\n" : ",\n");
  }
  if (!audit.records.empty()) out << pad;
  out << "]";
  return out.str();
}

}  // namespace

std::string claim_scope_contract_artifact_json(
    const ClaimScopeContractAudit& audit,
    const ClaimScopeContractViewContext& context) {
  if (context.format != "json") {
    throw std::invalid_argument(
        "claim-scope contract view only supports JSON output");
  }
  const std::string matrix_path = audit.matrix_path.string();
  const std::string results_root = audit.results_root.string();
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_claim_scope_contract\",\n";
  out << "  \"status\": " << inferlab_plan::json_string(audit.status)
      << ",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"execution_kind\": \"static_artifact_claim_scope_audit\",\n";
  out << "  \"claim_scope\": \"audits existing and planned InferLab result artifacts for execution semantics; does not run benchmarks or external products\",\n";
  out << "  \"audit_type\": \"claim_scope_contract\",\n";
  out << "  \"cli_context\": " << cli_context_json(audit, context, 2)
      << ",\n";
  out << "  \"matrix_path\": "
      << inferlab_plan::redacted_optional_path_json(matrix_path) << ",\n";
  out << "  \"matrix_path_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(matrix_path) << ",\n";
  out << "  \"matrix_read_status\": "
      << inferlab_plan::json_string(audit.matrix_source.read_status) << ",\n";
  out << "  \"matrix_read_error\": "
      << inferlab_plan::json_string(audit.matrix_source.read_error) << ",\n";
  out << "  \"matrix_readable\": "
      << inferlab_plan::json_bool(audit.matrix_source.readable) << ",\n";
  out << "  \"matrix_empty\": "
      << inferlab_plan::json_bool(audit.matrix_source.empty) << ",\n";
  out << "  \"matrix_issues\": "
      << inferlab_plan::json_string_array(audit.matrix_source.parse_issues, 2)
      << ",\n";
  out << "  \"results_root\": "
      << inferlab_plan::redacted_optional_path_json(results_root) << ",\n";
  out << "  \"results_root_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(results_root)
      << ",\n";
  out << "  \"strict_missing\": "
      << inferlab_plan::json_bool(audit.strict_missing) << ",\n";
  out << "  \"summary\": " << summary_json(audit, 2) << ",\n";
  out << "  \"records\": " << records_json(audit, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_claim_scope
