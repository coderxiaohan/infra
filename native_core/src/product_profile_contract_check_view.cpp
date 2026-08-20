#include "product_profile_contract_check_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab::product_profile_contract_check {
namespace {

using mature_product_profiles::contract_profile;

std::string cli_context_json(const AuditPlan& plan,
                             const ArtifactViewContext& context,
                             const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product profile contract CLI JSON indentation");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": "
      << inferlab_plan::json_string(context.format) << ",\n";
  out << nested << "\"strict_missing\": "
      << inferlab_plan::json_bool(plan.strict_missing) << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, nested_indent) << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "repo_root", context.repo_root, nested_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "results_root", context.results_root, nested_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "output", context.output, nested_indent)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(artifact_spec().claim_scope) << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const AuditRecord& record, const int indent) {
  const auto& contract = record.contract;
  const auto& profile = contract_profile(contract);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product profile contract record JSON indentation");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"adapter_boundary_present\": "
      << inferlab_plan::json_bool(contract.adapter_boundary_present) << ",\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(profile.artifact) << ",\n";
  out << nested << "\"boundary_score\": "
      << inferlab_plan::fixed6(contract.boundary_score) << ",\n";
  out << nested << "\"byte_size\": " << contract.byte_size << ",\n";
  out << nested << "\"can_claim_product_performance\": "
      << inferlab_plan::json_bool(contract.can_claim_product_performance)
      << ",\n";
  out << nested << "\"category\": "
      << inferlab_plan::json_string(profile.category) << ",\n";
  out << nested << "\"category_value_present\": "
      << inferlab_plan::json_bool(contract.category_value_present) << ",\n";
  out << nested << "\"claim_scope_consistent\": "
      << inferlab_plan::json_bool(contract.claim_scope_consistent) << ",\n";
  out << nested << "\"claim_scope_present\": "
      << inferlab_plan::json_bool(contract.claim_scope_present) << ",\n";
  out << nested << "\"config_snapshot_present\": "
      << inferlab_plan::json_bool(contract.config_snapshot_present) << ",\n";
  out << nested << "\"contract_coverage\": "
      << inferlab_plan::fixed6(contract.contract_coverage) << ",\n";
  out << nested << "\"contract_ready\": "
      << inferlab_plan::json_bool(contract.contract_ready) << ",\n";
  out << nested << "\"execute_gate_present\": "
      << inferlab_plan::json_bool(contract.execute_gate_present) << ",\n";
  out << nested << "\"executed\": "
      << inferlab_plan::json_bool(contract.executed) << ",\n";
  out << nested << "\"executed_present\": "
      << inferlab_plan::json_bool(contract.executed_present) << ",\n";
  out << nested << "\"invalid_type_fields\": "
      << inferlab_plan::json_string_array(contract.invalid_type_fields,
                                           nested_indent)
      << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(contract.issues, nested_indent)
      << ",\n";
  out << nested << "\"local_optimization_scope_present\": "
      << inferlab_plan::json_bool(
             contract.local_optimization_scope_present)
      << ",\n";
  out << nested << "\"missing_fields\": "
      << inferlab_plan::json_string_array(contract.missing_fields,
                                           nested_indent)
      << ",\n";
  out << nested << "\"mode\": "
      << inferlab_plan::json_string(contract.mode) << ",\n";
  out << nested << "\"mode_flags_consistent\": "
      << inferlab_plan::json_bool(contract.mode_flags_consistent) << ",\n";
  out << nested << "\"mode_status_consistent\": "
      << inferlab_plan::json_bool(contract.mode_status_consistent) << ",\n";
  out << nested << "\"not_implemented_boundary_present\": "
      << inferlab_plan::json_bool(
             contract.not_implemented_boundary_present)
      << ",\n";
  out << nested << "\"official_sources_present\": "
      << inferlab_plan::json_bool(contract.official_sources_present) << ",\n";
  out << nested << "\"parse_ok\": "
      << inferlab_plan::json_bool(contract.parse_ok) << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(contract.path_probe_status) << ",\n";
  out << nested << "\"planned_commands_present\": "
      << inferlab_plan::json_bool(contract.planned_commands_present) << ",\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(contract.present) << ",\n";
  out << nested << "\"present_field_count\": "
      << contract.present_field_count << ",\n";
  out << nested << "\"product\": "
      << inferlab_plan::json_string(profile.id) << ",\n";
  out << nested << "\"profile_scope\": "
      << inferlab_plan::json_string(
             mature_product_profiles::profile_scope_name(profile.scope))
      << ",\n";
  out << nested << "\"default_required\": "
      << inferlab_plan::json_bool(
             mature_product_profiles::profile_is_default_required(profile))
      << ",\n";
  out << nested << "\"product_boundary_present\": "
      << inferlab_plan::json_bool(contract.product_boundary_present) << ",\n";
  out << nested << "\"product_evidence_eligible\": "
      << inferlab_plan::json_bool(contract.product_evidence_eligible) << ",\n";
  out << nested << "\"product_name_value_present\": "
      << inferlab_plan::json_bool(contract.product_name_value_present)
      << ",\n";
  out << nested << "\"product_value_present\": "
      << inferlab_plan::json_bool(contract.product_value_present) << ",\n";
  out << nested << "\"profile_status\": "
      << inferlab_plan::json_string(contract.status) << ",\n";
  out << nested << "\"raw_metrics_present\": "
      << inferlab_plan::json_bool(contract.raw_metrics_present) << ",\n";
  out << nested << "\"readable\": "
      << inferlab_plan::json_bool(contract.readable) << ",\n";
  out << nested << "\"real_performance_claim\": "
      << inferlab_plan::json_string(contract.real_performance_claim) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(record.recommendation) << ",\n";
  out << nested << "\"replay_evidence_consistent\": "
      << inferlab_plan::json_bool(contract.replay_evidence_consistent)
      << ",\n";
  out << nested << "\"replay_metric_evidence_present\": "
      << inferlab_plan::json_bool(contract.replay_metric_evidence_present)
      << ",\n";
  out << nested << "\"replayed\": "
      << inferlab_plan::json_bool(contract.replayed) << ",\n";
  out << nested << "\"replayed_present\": "
      << inferlab_plan::json_bool(contract.replayed_present) << ",\n";
  out << nested << "\"required_check_count\": "
      << contract.required_check_count << ",\n";
  out << nested << "\"required_field_count\": "
      << contract.required_field_count << ",\n";
  out << nested << "\"required_field_pass_rate\": "
      << inferlab_plan::fixed6(contract.required_field_pass_rate) << ",\n";
  out << nested << "\"resolved_path\": "
      << inferlab_plan::json_string(
             inferlab_plan::redacted_path_label(
                 record.resolved_path.generic_string()))
      << ",\n";
  out << nested << "\"resolved_path_redacted\": true,\n";
  out << nested << "\"satisfied_check_count\": "
      << contract.satisfied_check_count << ",\n";
  out << nested << "\"schema_profile_v1\": "
      << inferlab_plan::json_bool(contract.schema_profile_v1) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record_status(record.decision)) << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(contract.text_read_status) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const AuditPlan& plan,
                          const ArtifactViewContext& context) {
  const auto& summary = plan.summary;
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_spec().artifact_type) << ",\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(audit_status(plan.decision)) << ",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"audit_type\": \"product_profile_artifact_contract_check\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(artifact_spec().claim_scope) << ",\n";
  out << "  \"cli_context\": "
      << cli_context_json(plan, context, 2) << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "results_root", context.results_root, 2)
      << ",\n";
  out << "  \"strict_missing\": "
      << inferlab_plan::json_bool(plan.strict_missing) << ",\n";
  out << "  \"execution_policy\": \"static artifact contract audit only; execute/replay eligibility is read from existing product artifacts and this target never launches a product\",\n";
  out << "  \"recommended_next_command\": "
      << inferlab_plan::json_string(artifact_spec().refresh_command) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"product_count\": " << summary.product_count << ",\n";
  out << "    \"default_required_product_count\": "
      << summary.default_required_product_count << ",\n";
  out << "    \"optional_on_demand_product_count\": "
      << summary.optional_on_demand_product_count << ",\n";
  out << "    \"selected_product_count\": "
      << summary.selected_product_count << ",\n";
  out << "    \"present_artifacts\": " << summary.present_artifacts
      << ",\n";
  out << "    \"missing_artifacts\": " << summary.missing_artifacts
      << ",\n";
  out << "    \"required_missing_artifacts\": "
      << summary.required_missing_artifacts << ",\n";
  out << "    \"passed_records\": " << summary.passed_records << ",\n";
  out << "    \"failed_records\": " << summary.failed_records << ",\n";
  out << "    \"pending_records\": " << summary.pending_records << ",\n";
  out << "    \"not_selected_optional_records\": "
      << summary.not_selected_optional_records << ",\n";
  out << "    \"readable_artifacts\": " << summary.readable_artifacts
      << ",\n";
  out << "    \"parsed_artifacts\": " << summary.parsed_artifacts
      << ",\n";
  out << "    \"read_issue_count\": " << summary.read_issue_count
      << ",\n";
  out << "    \"malformed_or_invalid_artifact_count\": "
      << summary.malformed_or_invalid_artifact_count << ",\n";
  out << "    \"required_field_pass_rate\": "
      << inferlab_plan::fixed6(summary.required_field_pass_rate) << ",\n";
  out << "    \"contract_coverage\": "
      << inferlab_plan::fixed6(summary.contract_coverage) << ",\n";
  out << "    \"boundary_pass_count\": "
      << summary.boundary_pass_count << ",\n";
  out << "    \"execute_gate_pass_count\": "
      << summary.execute_gate_pass_count << ",\n";
  out << "    \"mode_flag_pass_count\": "
      << summary.mode_flag_pass_count << ",\n";
  out << "    \"claim_scope_pass_count\": "
      << summary.claim_scope_pass_count << ",\n";
  out << "    \"product_evidence_eligible_count\": "
      << summary.product_evidence_eligible_count << ",\n";
  out << "    \"performance_claim_eligible_count\": "
      << summary.performance_claim_eligible_count << ",\n";
  out << "    \"total_required_fields\": "
      << summary.total_required_fields << ",\n";
  out << "    \"total_present_fields\": "
      << summary.total_present_fields << ",\n";
  out << "    \"total_required_checks\": "
      << summary.total_required_checks << ",\n";
  out << "    \"total_satisfied_checks\": "
      << summary.total_satisfied_checks << ",\n";
  out << "    \"physical_artifact_read_count\": "
      << summary.physical_artifact_read_count << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], 4)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::product_profile_contract_check
