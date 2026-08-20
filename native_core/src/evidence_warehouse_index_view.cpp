#include "evidence_warehouse_index_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "catalog_derived_views.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_evidence_warehouse {
namespace {

namespace fs = std::filesystem;

std::string bool_json(const bool value) {
  return inferlab_plan::json_bool(value);
}

std::string dependency_json(
    const inferlab_high_value::DependencySpec& dependency,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(dependency.name) << ",\n";
  out << nested << "\"version\": "
      << inferlab_plan::json_string(dependency.pinned_version) << ",\n";
  out << nested << "\"source\": "
      << inferlab_plan::json_string(dependency.source) << ",\n";
  out << nested << "\"license\": "
      << inferlab_plan::json_string(dependency.license) << ",\n";
  out << nested << "\"cmake_option\": "
      << inferlab_plan::json_string(dependency.cmake_option) << ",\n";
  out << nested << "\"required\": " << bool_json(dependency.required)
      << ",\n";
  out << nested << "\"optional\": " << bool_json(dependency.optional)
      << ",\n";
  out << nested << "\"fallback\": "
      << inferlab_plan::json_string(dependency.fallback) << ",\n";
  out << nested << "\"claim_impact\": "
      << inferlab_plan::json_string(dependency.claim_impact) << "\n";
  out << pad << "}";
  return out.str();
}

std::string cli_context_json(const fs::path& repo_root,
                             const fs::path& results_root,
                             const fs::path& output_path,
                             const fs::path& sqlite_output_path,
                             const std::string& format,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": " << inferlab_plan::json_string(format)
      << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, indent + 2) << ",\n";
  out << inferlab_artifact::cli_path_field_json("repo_root", repo_root,
                                                 indent + 2)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json("results_root", results_root,
                                                 indent + 2)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json("output", output_path,
                                                 indent + 2)
      << ",\n";
  out << inferlab_artifact::cli_path_field_json(
             "sqlite_output", sqlite_output_path, indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"CLI context records requested repository, "
         "artifact and optional SQLite destinations only; it does not prove "
         "producer execution, artifact freshness, replay, performance, or "
         "release readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string source_files_json(
    const std::vector<inferlab_artifact::TextSource>& sources,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  if (!sources.empty()) out << "\n";
  for (std::size_t index = 0U; index < sources.size(); ++index) {
    const auto& source = sources[index];
    const std::string path_label =
        inferlab_plan::redacted_path_label(source.path);
    out << nested << "{\n";
    out << nested << "  \"id\": " << inferlab_plan::json_string(source.id)
        << ",\n";
    out << nested << "  \"path\": "
        << inferlab_plan::json_string(path_label) << ",\n";
    out << nested << "  \"path_redacted\": "
        << bool_json(inferlab_plan::path_was_redacted(source.path,
                                                       path_label))
        << ",\n";
    out << nested << "  \"present\": " << bool_json(source.present)
        << ",\n";
    out << nested << "  \"readable\": " << bool_json(source.readable)
        << ",\n";
    out << nested << "  \"read_status\": "
        << inferlab_plan::json_string(source.status) << ",\n";
    out << nested << "  \"read_issue_count\": "
        << source.read_issue_count << "\n";
    out << nested << "}" << (index == sources.size() - 1U ? "\n" : ",\n");
  }
  if (!sources.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string managed_artifacts_json(
    const std::vector<ManagedArtifactLineage>& records,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  if (!records.empty()) out << "\n";
  for (std::size_t index = 0U; index < records.size(); ++index) {
    const auto& record = records[index];
    if (record.spec == nullptr) {
      throw std::logic_error("warehouse view received a record without catalog metadata");
    }
    const auto& spec = *record.spec;
    const auto& warehouse = record.warehouse_record;
    out << nested << "{\n";
    out << nested << "  \"id\": " << inferlab_plan::json_string(spec.id)
        << ",\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(spec.artifact) << ",\n";
    out << nested << "  \"target\": "
        << inferlab_plan::json_string(spec.target) << ",\n";
    out << nested << "  \"report\": "
        << inferlab_plan::json_string(spec.report) << ",\n";
    out << nested << "  \"evidence_level\": "
        << inferlab_plan::json_string(spec.evidence_level) << ",\n";
    out << nested << "  \"claim_boundary\": "
        << inferlab_plan::json_string(
               inferlab_catalog::registry_claim_boundary(spec))
        << ",\n";
    out << nested << "  \"artifact_path_status\": "
        << inferlab_plan::json_string(record.artifact_path_status) << ",\n";
    out << nested << "  \"current_output_deferred\": "
        << bool_json(record.current_output_deferred) << ",\n";
    out << nested << "  \"present\": " << bool_json(warehouse.present)
        << ",\n";
    out << nested << "  \"producer_target_present\": "
        << bool_json(record.producer_target_present) << ",\n";
    out << nested << "  \"matrix_present\": "
        << bool_json(warehouse.matrix_present) << ",\n";
    out << nested << "  \"summary_present\": "
        << bool_json(warehouse.summary_present) << ",\n";
    out << nested << "  \"prometheus_present\": "
        << bool_json(warehouse.prometheus_present) << ",\n";
    out << nested << "  \"report_present\": "
        << bool_json(warehouse.report_present) << ",\n";
    out << nested << "  \"final_report_present\": "
        << bool_json(warehouse.final_report_present) << ",\n";
    out << nested << "  \"contract_present\": "
        << bool_json(warehouse.contract_present) << ",\n";
    out << nested << "  \"freshness_present\": "
        << bool_json(warehouse.freshness_present) << ",\n";
    out << nested << "  \"evidence_bundle_present\": "
        << bool_json(record.evidence_bundle_present) << ",\n";
    out << nested << "  \"ready_edge_count\": "
        << record.ready_edge_count << ",\n";
    out << nested << "  \"required_edge_count\": "
        << record.required_edge_count << ",\n";
    out << nested << "  \"metadata\": "
        << inferlab_artifact::metadata_json(warehouse.metadata, indent + 2)
        << "\n";
    out << nested << "}"
        << (index == records.size() - 1U ? "\n" : ",\n");
  }
  if (!records.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string scanned_artifacts_json(
  const std::vector<inferlab_artifact::Metadata>& artifacts,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[";
  if (!artifacts.empty()) out << "\n";
  for (std::size_t index = 0U; index < artifacts.size(); ++index) {
    out << inferlab_artifact::metadata_json(artifacts[index], indent + 2)
        << (index == artifacts.size() - 1U ? "\n" : ",\n");
  }
  if (!artifacts.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string scan_issues_json(const std::vector<RawArtifactScanIssue>& issues,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  if (!issues.empty()) out << "\n";
  for (std::size_t index = 0U; index < issues.size(); ++index) {
    const auto& issue = issues[index];
    out << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(issue.artifact) << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(issue.category) << ",\n";
    out << nested << "  \"message\": "
        << inferlab_plan::json_string(issue.message) << "\n";
    out << nested << "}"
        << (index == issues.size() - 1U ? "\n" : ",\n");
  }
  if (!issues.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string lineage_edges_json(
    const std::vector<ManagedArtifactLineage>& records,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  bool first = true;
  for (const auto& record : records) {
    for (const auto& edge : record.edges) {
      out << (first ? "\n" : ",\n");
      first = false;
      out << nested << "{\n";
      out << nested << "  \"artifact\": "
          << inferlab_plan::json_string(edge.artifact) << ",\n";
      out << nested << "  \"relation\": "
          << inferlab_plan::json_string(edge.relation) << ",\n";
      out << nested << "  \"target\": "
          << inferlab_plan::json_string(edge.target) << ",\n";
      out << nested << "  \"authority\": "
          << inferlab_plan::json_string(edge.authority) << ",\n";
      out << nested << "  \"present\": " << bool_json(edge.present)
          << ",\n";
      out << nested << "  \"gap_category\": "
          << inferlab_plan::json_string(edge.gap_category) << ",\n";
      out << nested << "  \"repair_action\": "
          << inferlab_plan::json_string(edge.repair_action) << "\n";
      out << nested << "}";
    }
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

std::string blockers_json(const std::vector<WarehouseBlocker>& blockers,
                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[";
  if (!blockers.empty()) out << "\n";
  for (std::size_t index = 0U; index < blockers.size(); ++index) {
    const auto& blocker = blockers[index];
    out << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(blocker.artifact) << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(blocker.category) << ",\n";
    out << nested << "  \"message\": "
        << inferlab_plan::json_string(blocker.message) << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(blocker.repair_environment) << ",\n";
    out << nested << "  \"exact_wiring_location\": "
        << inferlab_plan::json_string(blocker.exact_wiring_location)
        << ",\n";
    out << nested << "  \"required_action\": "
        << inferlab_plan::json_string(blocker.required_action) << "\n";
    out << nested << "}"
        << (index == blockers.size() - 1U ? "\n" : ",\n");
  }
  if (!blockers.empty()) out << pad;
  out << "]";
  return out.str();
}

std::string dependency_runtime_json(
    const WarehousePlan& plan,
    const SqliteWriteStatus& sqlite_status,
    const fs::path& sqlite_output_path,
    const int indent) {
  if (plan.json_dependency == nullptr || plan.sqlite_dependency == nullptr) {
    throw std::logic_error("warehouse dependency metadata is incomplete");
  }
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string sqlite_output = sqlite_output_path.generic_string();
  const std::string sqlite_label =
      inferlab_plan::redacted_path_label(sqlite_output);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"nlohmann_json_enabled\": "
#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
      << "true,\n";
#else
      << "false,\n";
#endif
  out << nested << "\"structured_parser_artifact_count\": "
      << plan.summary.structured_parser_artifact_count << ",\n";
  out << nested << "\"sqlite_requested\": "
      << bool_json(sqlite_status.requested) << ",\n";
  out << nested << "\"sqlite_enabled\": "
      << bool_json(sqlite_status.enabled) << ",\n";
  out << nested << "\"sqlite_written\": "
      << bool_json(sqlite_status.written) << ",\n";
  out << nested << "\"sqlite_status\": "
      << inferlab_plan::json_string(sqlite_status.status) << ",\n";
  out << nested << "\"sqlite_runtime_version\": "
      << inferlab_plan::json_string(sqlite_status.runtime_version) << ",\n";
  out << nested << "\"sqlite_record_count\": "
      << sqlite_status.record_count << ",\n";
  out << nested << "\"sqlite_output\": "
      << inferlab_plan::json_string(sqlite_label) << ",\n";
  out << nested << "\"sqlite_output_redacted\": "
      << bool_json(inferlab_plan::path_was_redacted(sqlite_output,
                                                     sqlite_label))
      << ",\n";
  out << nested << "\"sqlite_error\": "
      << inferlab_plan::json_string(sqlite_status.error) << ",\n";
  out << nested << "\"fallback\": "
      << inferlab_plan::json_string(sqlite_status.fallback) << ",\n";
  out << nested << "\"dependencies\": [\n";
  out << dependency_json(*plan.json_dependency, indent + 4) << ",\n";
  out << dependency_json(*plan.sqlite_dependency, indent + 4) << "\n";
  out << nested << "]\n";
  out << pad << "}";
  return out.str();
}

std::string summary_json(const WarehouseSummary& summary, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact_count\": " << summary.artifact_count
      << ",\n";
  out << nested << "\"managed_artifact_count\": "
      << summary.managed_artifact_count << ",\n";
  out << nested << "\"indexed_artifact_count\": "
      << summary.indexed_artifact_count << ",\n";
  out << nested << "\"parse_ready_artifact_count\": "
      << summary.parse_ready_artifact_count << ",\n";
  out << nested << "\"structured_parser_artifact_count\": "
      << summary.structured_parser_artifact_count << ",\n";
  out << nested << "\"current_output_deferred_count\": "
      << summary.current_output_deferred_count << ",\n";
  out << nested << "\"missing_target_count\": "
      << summary.missing_target_count << ",\n";
  out << nested << "\"missing_report_count\": "
      << summary.missing_report_count << ",\n";
  out << nested << "\"missing_matrix_count\": "
      << summary.missing_matrix_count << ",\n";
  out << nested << "\"missing_summary_count\": "
      << summary.missing_summary_count << ",\n";
  out << nested << "\"missing_prometheus_count\": "
      << summary.missing_prometheus_count << ",\n";
  out << nested << "\"missing_final_report_count\": "
      << summary.missing_final_report_count << ",\n";
  out << nested << "\"missing_contract_count\": "
      << summary.missing_contract_count << ",\n";
  out << nested << "\"missing_freshness_count\": "
      << summary.missing_freshness_count << ",\n";
  out << nested << "\"missing_bundle_count\": "
      << summary.missing_bundle_count << ",\n";
  out << nested << "\"source_file_count\": "
      << summary.source_file_count << ",\n";
  out << nested << "\"source_read_issue_count\": "
      << summary.source_read_issue_count << ",\n";
  out << nested << "\"raw_scan_issue_count\": "
      << summary.raw_scan_issue_count << ",\n";
  out << nested << "\"physical_artifact_read_count\": "
      << summary.physical_artifact_read_count << ",\n";
  out << nested << "\"ready_edge_count\": "
      << summary.ready_edge_count << ",\n";
  out << nested << "\"required_edge_count\": "
      << summary.required_edge_count << ",\n";
  out << nested << "\"blocker_count\": " << summary.blocker_count
      << ",\n";
  out << nested << "\"lineage_score\": "
      << inferlab_plan::fixed6(summary.lineage_score) << ",\n";
  out << nested << "\"inventory_state\": "
      << inferlab_plan::json_string(
             inventory_state_name(summary.inventory_state))
      << ",\n";
  out << nested << "\"warehouse_decision\": "
      << inferlab_plan::json_string(
             warehouse_decision_name(summary.decision))
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string warehouse_artifact_json(
    const WarehousePlan& plan, const SqliteWriteStatus& sqlite_status,
    const fs::path& repo_root, const fs::path& results_root,
    const fs::path& output_path, const fs::path& sqlite_output_path,
    const std::string& format) {
  if (plan.spec == nullptr) {
    throw std::logic_error("warehouse plan has no catalog artifact identity");
  }
  const auto& spec = *plan.spec;
  const std::string claim_scope =
      inferlab_catalog::registry_claim_boundary(spec);
  const std::string artifact_type =
      inferlab_catalog::registry_artifact_type(spec);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"schema\": {\n";
  out << "    \"name\": " << inferlab_plan::json_string(artifact_type)
      << ",\n";
  out << "    \"version\": \"v1\"\n";
  out << "  },\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_type) << ",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(
             warehouse_decision_name(plan.summary.decision))
      << ",\n";
  out << "  \"generated_at\": "
      << inferlab_plan::json_string(inferlab_plan::current_utc_timestamp())
      << ",\n";
  out << "  \"claim_scope\": " << inferlab_plan::json_string(claim_scope)
      << ",\n";
  out << "  \"claim_scope_detail\": "
      << inferlab_plan::common_claim_scope_detail_json(claim_scope, 2)
      << ",\n";
  out << "  \"environment\": {\n";
  out << "    \"requires_gpu_for_index_generation\": false,\n";
  out << "    \"requires_product_environment_for_index_generation\": false,\n";
  out << "    \"runtime_validation_status\": \"not_run_per_user_request\"\n";
  out << "  },\n";
  out << "  \"official_sources\": [],\n";
  out << "  \"refresh_commands\": "
      << inferlab_plan::json_string_array({spec.command}, 2) << ",\n";
  out << "  \"migration_commands\": "
      << inferlab_plan::json_string_array({spec.command}, 2) << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << ",\n";
  out << "  \"cli_context\": "
      << cli_context_json(repo_root, results_root, output_path,
                          sqlite_output_path, format, 2)
      << ",\n";
  out << "  \"dependency_runtime\": "
      << dependency_runtime_json(plan, sqlite_status, sqlite_output_path, 2)
      << ",\n";
  out << "  \"summary\": " << summary_json(plan.summary, 2) << ",\n";
  out << "  \"source_files\": "
      << source_files_json(plan.lineage.sources, 2) << ",\n";
  out << "  \"raw_scan\": {\n";
  out << "    \"claim_boundary\": "
      << inferlab_plan::json_string(
             "Raw scan status describes inventory completeness only. Scan "
             "issues neither prove nor independently block catalog lineage, "
             "artifact semantics, replay, execute, performance, or release "
             "readiness.")
      << ",\n";
  out << "    \"directory_status\": "
      << inferlab_plan::json_string(plan.raw_scan.directory_status) << ",\n";
  out << "    \"skipped_non_json_count\": "
      << plan.raw_scan.skipped_non_json_count << ",\n";
  out << "    \"skipped_current_output_count\": "
      << plan.raw_scan.skipped_current_output_count << ",\n";
  out << "    \"issue_count\": " << plan.raw_scan.issues.size() << ",\n";
  out << "    \"issues\": " << scan_issues_json(plan.raw_scan.issues, 4)
      << "\n";
  out << "  },\n";
  out << "  \"artifacts\": "
      << managed_artifacts_json(plan.lineage.records, 2) << ",\n";
  out << "  \"scanned_raw_artifacts\": "
      << scanned_artifacts_json(plan.raw_scan.artifacts, 2) << ",\n";
  out << "  \"lineage_edges\": "
      << lineage_edges_json(plan.lineage.records, 2) << ",\n";
  out << "  \"blockers\": " << blockers_json(plan.blockers, 2) << ",\n";
  out << "  \"pending_artifact_refresh_commands\": "
      << inferlab_plan::json_string_array(
             plan.pending_artifact_refresh_commands, 2)
      << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_evidence_warehouse
