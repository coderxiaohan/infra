#include "product_integration_inventory_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "artifact_text_source_view.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::product_integration_inventory {
namespace {

std::string file_audit_json(const FileAudit& file, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product integration file-audit view indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"repository_path\": "
      << inferlab_plan::json_string(file.repository_path) << ",\n";
  out << nested << "\"path_status\": "
      << inferlab_plan::json_string(file.path_status) << ",\n";
  out << nested << "\"read_status\": "
      << inferlab_plan::json_string(file.read_status) << ",\n";
  out << nested << "\"regular_file\": "
      << inferlab_plan::json_bool(file.regular_file) << ",\n";
  out << nested << "\"readable\": "
      << inferlab_plan::json_bool(file.readable) << ",\n";
  out << nested << "\"read_issue_count\": "
      << file.read_issue_count << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const InventoryRecord& record, const int indent) {
  const auto& profile = record_profile(record);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product integration record view indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const std::string adapter_source =
      mature_product_profiles::profile_source_file(profile);
  const std::string entry_module =
      mature_product_profiles::profile_module(profile);
  const std::string config_path =
      mature_product_profiles::profile_config_path(profile);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"adapter_script\": "
      << inferlab_plan::json_string(entry_module) << ",\n";
  out << nested << "\"adapter_source_file\": "
      << inferlab_plan::json_string(adapter_source) << ",\n";
  out << nested << "\"entry_module\": "
      << inferlab_plan::json_string(entry_module) << ",\n";
  out << nested << "\"adapter_script_exists\": "
      << inferlab_plan::json_bool(record.adapter.regular_file) << ",\n";
  out << nested << "\"adapter_readable\": "
      << inferlab_plan::json_bool(record.adapter.readable) << ",\n";
  out << nested << "\"adapter_product_binding_present\": "
      << inferlab_plan::json_bool(
             record.adapter_product_binding_present)
      << ",\n";
  out << nested << "\"adapter_file_audit\": "
      << file_audit_json(record.adapter, nested_indent) << ",\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(profile.artifact) << ",\n";
  out << nested << "\"artifact_catalog_source_occurrence_count\": "
      << record.artifact_catalog_source_occurrence_count << ",\n";
  out << nested << "\"artifact_catalog_source_present\": "
      << inferlab_plan::json_bool(record.artifact_catalog_source_present)
      << ",\n";
  out << nested << "\"artifact_catalog_scope_matches\": "
      << inferlab_plan::json_bool(record.artifact_catalog_scope_matches)
      << ",\n";
  out << nested << "\"benchmark_entry\": "
      << inferlab_plan::json_string(profile.benchmark_entry) << ",\n";
  out << nested << "\"benchmark_entry_count\": "
      << record.matrix.entry_count << ",\n";
  out << nested << "\"benchmark_duplicate_field_count\": "
      << record.matrix.duplicate_field_count << ",\n";
  out << nested << "\"benchmark_dry_run_command_present\": "
      << inferlab_plan::json_bool(
             record.benchmark_dry_run_command_present)
      << ",\n";
  out << nested << "\"benchmark_replay_command_present\": "
      << inferlab_plan::json_bool(record.benchmark_replay_command_present)
      << ",\n";
  out << nested << "\"benchmark_matrix_present\": "
      << inferlab_plan::json_bool(record.benchmark_matrix_present)
      << ",\n";
  out << nested << "\"benchmark_matrix_scope_matches\": "
      << inferlab_plan::json_bool(record.benchmark_matrix_scope_matches)
      << ",\n";
  out << nested << "\"category\": "
      << inferlab_plan::json_string(profile.category) << ",\n";
  out << nested << "\"common_registry_artifact_present\": "
      << inferlab_plan::json_bool(record.common_registry.artifact_matches)
      << ",\n";
  out << nested << "\"common_registry_boundary_present\": "
      << inferlab_plan::json_bool(
             record.common_registry.boundary_lists_present)
      << ",\n";
  out << nested << "\"common_registry_present\": "
      << inferlab_plan::json_bool(record.common_registry.product_present)
      << ",\n";
  out << nested << "\"common_registry_entry_count\": "
      << record.common_registry.product_entry_count << ",\n";
  out << nested << "\"common_registry_display_name_present\": "
      << inferlab_plan::json_bool(
             record.common_registry.display_name_matches)
      << ",\n";
  out << nested << "\"common_registry_category_present\": "
      << inferlab_plan::json_bool(record.common_registry.category_matches)
      << ",\n";
  out << nested << "\"common_registry_report_present\": "
      << inferlab_plan::json_bool(record.common_registry.report_matches)
      << ",\n";
  out << nested << "\"common_registry_config_present\": "
      << inferlab_plan::json_bool(record.common_registry.config_matches)
      << ",\n";
  out << nested << "\"common_registry_scope_matches\": "
      << inferlab_plan::json_bool(record.common_registry.scope_matches)
      << ",\n";
  out << nested << "\"config_exists\": "
      << inferlab_plan::json_bool(record.config.regular_file) << ",\n";
  out << nested << "\"config_path\": "
      << inferlab_plan::json_string(config_path) << ",\n";
  out << nested << "\"config_readable\": "
      << inferlab_plan::json_bool(record.config.readable) << ",\n";
  out << nested << "\"config_document_valid\": "
      << inferlab_plan::json_bool(record.config_document_valid) << ",\n";
  out << nested << "\"config_product_binding_present\": "
      << inferlab_plan::json_bool(record.config_product_binding_present)
      << ",\n";
  out << nested << "\"config_report_binding_present\": "
      << inferlab_plan::json_bool(record.config_report_binding_present)
      << ",\n";
  out << nested << "\"config_file_audit\": "
      << file_audit_json(record.config, nested_indent) << ",\n";
  out << nested << "\"final_report_module_count\": "
      << record.final_report.module_count << ",\n";
  out << nested << "\"final_report_result_occurrence_count\": "
      << record.final_report.result_occurrence_count << ",\n";
  out << nested << "\"final_report_report_occurrence_count\": "
      << record.final_report.report_occurrence_count << ",\n";
  out << nested << "\"final_report_report_present\": "
      << inferlab_plan::json_bool(record.final_report.report_present)
      << ",\n";
  out << nested << "\"final_report_result_present\": "
      << inferlab_plan::json_bool(record.final_report.result_present)
      << ",\n";
  out << nested << "\"final_report_result_scope_matches\": "
      << inferlab_plan::json_bool(
             record.final_report_result_scope_matches)
      << ",\n";
  out << nested << "\"final_report_code_wiring_present\": "
      << inferlab_plan::json_bool(record.final_report.code_wiring_present)
      << ",\n";
  out << nested << "\"integration_score\": "
      << inferlab_plan::fixed6(record.integration_score) << ",\n";
  out << nested << "\"missing_surfaces\": "
      << inferlab_plan::json_string_array(record.missing_surfaces,
                                          nested_indent)
      << ",\n";
  out << nested << "\"passed_checks\": " << record.passed_checks
      << ",\n";
  out << nested << "\"product\": "
      << inferlab_plan::json_string(profile.id) << ",\n";
  out << nested << "\"profile_scope\": "
      << inferlab_plan::json_string(
             mature_product_profiles::profile_scope_name(profile.scope))
      << ",\n";
  out << nested << "\"default_wiring_expected\": "
      << inferlab_plan::json_bool(record.default_wiring_expected)
      << ",\n";
  out << nested << "\"product_name\": "
      << inferlab_plan::json_string(profile.display_name) << ",\n";
  out << nested << "\"prometheus_source_present\": "
      << inferlab_plan::json_bool(record.prometheus_source_present)
      << ",\n";
  out << nested << "\"prometheus_scope_matches\": "
      << inferlab_plan::json_bool(record.prometheus_scope_matches)
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(record_recommendation(record))
      << ",\n";
  out << nested << "\"report_artifact_reference_present\": "
      << inferlab_plan::json_bool(
             record.report_artifact_reference_present)
      << ",\n";
  out << nested << "\"report_boundary_present\": "
      << inferlab_plan::json_bool(record.report_boundary_present)
      << ",\n";
  out << nested << "\"report_exists\": "
      << inferlab_plan::json_bool(record.report.regular_file) << ",\n";
  out << nested << "\"report_path\": "
      << inferlab_plan::json_string(profile.report) << ",\n";
  out << nested << "\"report_readable\": "
      << inferlab_plan::json_bool(record.report.readable) << ",\n";
  out << nested << "\"report_read_status\": "
      << inferlab_plan::json_string(record.report.read_status) << ",\n";
  out << nested << "\"report_read_issue_count\": "
      << record.report.read_issue_count << ",\n";
  out << nested << "\"report_file_audit\": "
      << file_audit_json(record.report, nested_indent) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record_status(record)) << ",\n";
  out << nested << "\"summary_profile_call_count\": "
      << record.summary_profile_call_count << ",\n";
  out << nested << "\"summary_source_present\": "
      << inferlab_plan::json_bool(record.summary_source_present) << ",\n";
  out << nested << "\"summary_scope_matches\": "
      << inferlab_plan::json_bool(record.summary_scope_matches) << ",\n";
  out << nested << "\"total_checks\": " << record.total_checks << "\n";
  out << pad << "}";
  return out.str();
}

std::string records_json(const InventoryPlan& plan, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product integration records view indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[";
  if (!plan.records.empty()) out << "\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], nested_indent)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  if (!plan.records.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string cli_context_json(const ArtifactViewContext& context,
                             const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "product integration CLI view indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << inferlab_artifact::cli_path_field_json(
             "repo_root", context.repo_root, nested_indent)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "output", context.output, nested_indent)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const InventoryPlan& plan,
                          const ArtifactViewContext& context) {
  const auto& spec = artifact_spec();
  const std::string repo_root_label =
      inferlab_plan::redacted_path_label(context.repo_root.string());
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(spec.artifact_type) << ",\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(decision_status(plan.decision))
      << ",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"audit_type\": \"product_integration_inventory\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(spec.claim_scope) << ",\n";
  out << "  \"repo_root\": "
      << inferlab_plan::json_string(repo_root_label) << ",\n";
  out << "  \"repo_root_redacted\": "
      << inferlab_plan::json_bool(inferlab_plan::path_was_redacted(
             context.repo_root.string(), repo_root_label))
      << ",\n";
  out << "  \"execution_policy\": "
      << inferlab_plan::json_string(
             "static repository wiring audit only; does not run external products, profile scripts, replay, benchmark, or GPU workloads")
      << ",\n";
  out << "  \"cli_context\": " << cli_context_json(context, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"product_count\": " << plan.product_count << ",\n";
  out << "    \"default_required_product_count\": "
      << plan.default_required_product_count << ",\n";
  out << "    \"optional_on_demand_product_count\": "
      << plan.optional_on_demand_product_count << ",\n";
  out << "    \"passed_records\": " << plan.passed_records << ",\n";
  out << "    \"failed_records\": " << plan.failed_records << ",\n";
  out << "    \"passed_checks\": " << plan.passed_checks << ",\n";
  out << "    \"total_checks\": " << plan.total_checks << ",\n";
  out << "    \"source_file_count\": " << plan.source_files.size()
      << ",\n";
  out << "    \"unique_report_file_count\": "
      << plan.unique_report_file_count << ",\n";
  out << "    \"physical_text_read_count\": "
      << plan.physical_text_read_count << ",\n";
  out << "    \"source_read_issue_count\": "
      << plan.source_read_issue_count << ",\n";
  out << "    \"adapter_read_issue_count\": "
      << plan.adapter_read_issue_count << ",\n";
  out << "    \"config_read_issue_count\": "
      << plan.config_read_issue_count << ",\n";
  out << "    \"report_read_issue_count\": "
      << plan.report_read_issue_count << ",\n";
  out << "    \"total_read_issue_count\": "
      << plan.total_read_issue_count << ",\n";
  out << "    \"integration_score\": "
      << inferlab_plan::fixed6(plan.integration_score) << "\n";
  out << "  },\n";
  out << "  \"source_files\": "
      << inferlab_artifact::text_source_metadata_json(plan.source_files, 2)
      << ",\n";
  out << "  \"records\": " << records_json(plan, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({spec.refresh_command}, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::product_integration_inventory
