#include "deep_dive_contract_check_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_deep_dive_contract {
namespace {

namespace fs = std::filesystem;

constexpr char kArtifactType[] = "native_deep_dive_contract_check";
constexpr char kTargetName[] = "inferlab_deep_dive_contract_check";
constexpr char kArtifactPath[] =
    "results/raw/native_deep_dive_contract_check.json";
constexpr char kRefreshCommand[] =
    "./build/inferlab_deep_dive_contract_check --repo-root . --results "
    "results/raw --output results/raw/native_deep_dive_contract_check.json "
    "--format json";
constexpr char kClaimScope[] =
    "checks existing InferLab native deep-dive and governance artifacts "
    "through catalog-derived, mature-product dispatch, retained fallback, "
    "and structural evidence "
    "contracts; does not run producers, benchmarks, native binaries, or "
    "external products";

const std::vector<std::string>& product_alignment() {
  static const std::vector<std::string> products = {
      "vLLM",       "SGLang",  "TensorRT-LLM", "FlashInfer",
      "LMCache",    "Mooncake", "Dynamo",       "KServe/llm-d",
      "Ray Serve",  "TGI",     "KTransformers"};
  return products;
}

std::string cli_context_json(const fs::path& repo_root,
                             const fs::path& results_root,
                             const fs::path& output_path,
                             const std::string& format,
                             const bool strict_missing,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": " << inferlab_plan::json_string(format)
      << ",\n";
  out << nested << "\"strict_missing\": "
      << inferlab_plan::json_bool(strict_missing) << ",\n";
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
  out << nested
      << "\"claim_boundary\": \"CLI context documents accepted read-only "
         "contract-check inputs only; it does not refresh artifacts, execute "
         "producers, import replay evidence, or prove benchmark readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const Record& record, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string resolved_path = record.resolved_path.generic_string();
  const std::string resolved_path_label =
      inferlab_plan::redacted_path_label(resolved_path);
  const std::string path_probe_error = inferlab_plan::redact_error_message(
      record.path_probe_error, {resolved_path});
  const std::string text_read_error = inferlab_plan::redact_error_message(
      record.text_read_error, {resolved_path});

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(record.target.artifact) << ",\n";
  out << nested << "\"base_contract_coverage\": "
      << inferlab_plan::fixed6(record.base_contract.coverage) << ",\n";
  out << nested << "\"base_contract_issue_count\": "
      << record.base_contract.issues.size() << ",\n";
  out << nested << "\"base_contract_issues\": "
      << inferlab_plan::json_string_array(record.base_contract.issues,
                                           indent + 2)
      << ",\n";
  out << nested << "\"base_contract_ready\": "
      << inferlab_plan::json_bool(record.base_contract.ready) << ",\n";
  out << nested << "\"canonical_wiring_complete\": "
      << inferlab_plan::json_bool(record.target.canonical_wiring_complete)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(record.target.claim_boundary) << ",\n";
  out << nested << "\"claim_scope_present\": "
      << inferlab_plan::json_bool(record.claim_scope_present) << ",\n";
  out << nested << "\"contract_score\": "
      << inferlab_plan::fixed6(record.contract_score) << ",\n";
  out << nested << "\"executed_false\": "
      << inferlab_plan::json_bool(record.executed_false) << ",\n";
  out << nested << "\"executed_true\": "
      << inferlab_plan::json_bool(record.executed_true) << ",\n";
  out << nested << "\"expected_kind\": "
      << inferlab_plan::json_string(record.target.expected_kind) << ",\n";
  out << nested << "\"failed_forbidden_checks\": "
      << inferlab_plan::json_string_array(record.failed_forbidden_checks,
                                           indent + 2)
      << ",\n";
  out << nested << "\"fallback_retained\": "
      << inferlab_plan::json_bool(record.target.fallback_retained) << ",\n";
  out << nested << "\"missing_checks\": "
      << inferlab_plan::json_string_array(record.missing_checks, indent + 2)
      << ",\n";
  out << nested << "\"metric_owning_sections\": "
      << inferlab_plan::json_string_array(
             record.target.metric_owning_sections, indent + 2)
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(record.target.name) << ",\n";
  out << nested << "\"observed_artifact_type\": "
      << inferlab_plan::json_string(record.observed_artifact_type) << ",\n";
  out << nested << "\"observed_mode\": "
      << inferlab_plan::json_string(record.observed_mode) << ",\n";
  out << nested << "\"parse_ok\": "
      << inferlab_plan::json_bool(record.parse_ok) << ",\n";
  out << nested << "\"passed_checks\": " << record.passed_checks
      << ",\n";
  out << nested << "\"path_probe_error\": "
      << inferlab_plan::json_string(path_probe_error) << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(record.path_probe_status) << ",\n";
  out << nested << "\"present\": "
      << inferlab_plan::json_bool(record.present) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommendation_text(record.recommendation))
      << ",\n";
  out << nested << "\"records_present\": "
      << inferlab_plan::json_bool(record.records_present) << ",\n";
  out << nested << "\"refresh_command\": "
      << inferlab_plan::json_string(record.target.refresh_command) << ",\n";
  out << nested << "\"replayed\": "
      << inferlab_plan::json_bool(record.replayed) << ",\n";
  out << nested << "\"required_checks\": " << record.required_checks
      << ",\n";
  out << nested << "\"resolved_path\": "
      << inferlab_plan::json_string(resolved_path_label) << ",\n";
  out << nested << "\"resolved_path_redacted\": "
      << inferlab_plan::json_bool(inferlab_plan::path_was_redacted(
             resolved_path, resolved_path_label))
      << ",\n";
  out << nested << "\"source\": "
      << inferlab_plan::json_string(target_source_name(record.target.source))
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record_status_name(record.status))
      << ",\n";
  out << nested << "\"text_read_error\": "
      << inferlab_plan::json_string(text_read_error) << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(record.text_read_status) << "\n";
  out << pad << "}";
  return out.str();
}

std::string contract_sources_json(const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested
      << "\"catalog_managed\": \"catalog_derived_views.hpp::contract_views\",\n";
  out << nested
      << "\"core_deep_dive_catalog\": \"deep_dive_core_artifact_registry.hpp projected through the high-value catalog\",\n";
  out << nested
      << "\"mature_product_dispatch\": \"mature_product_artifact_dispatch.hpp plus artifact_contract_fixture.cpp field/metric extension\",\n";
  out << nested
      << "\"deep_dive_wiring_registry\": \"deep_dive_evidence_bundle_fixture.hpp/.cpp plus artifact_contract_fixture.cpp field/metric extension\",\n";
  out << nested
      << "\"contract_extensions\": \"artifact_contract_fixture.cpp::default_contract_extensions\",\n";
  out << nested
      << "\"structural_evaluator\": \"artifact_evidence_contract.hpp/.cpp\",\n";
  out << nested
      << "\"metric_registry\": \"catalog_required_metrics.cpp and bounded domain appenders\",\n";
  out << nested
      << "\"metric_owner_contract\": \"deep_dive_contract_check_model.cpp bounded top-level owning-section registry\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan,
                          const fs::path& repo_root,
                          const fs::path& results_root,
                          const fs::path& output_path,
                          const std::string& format) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"" << kArtifactType << "\",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, kArtifactType, kClaimScope, product_alignment(), kTargetName,
      kArtifactPath, kRefreshCommand);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"audit_type\": \"native_deep_dive_contract_check\",\n";
  out << "  \"claim_scope\": " << inferlab_plan::json_string(kClaimScope)
      << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"strict_missing\": "
      << inferlab_plan::json_bool(plan.strict_missing) << ",\n";
  out << "  \"cli_context\": "
      << cli_context_json(repo_root, results_root, output_path, format,
                          plan.strict_missing, 2)
      << ",\n";
  out << "  \"contract_sources\": " << contract_sources_json(2) << ",\n";
  out << "  \"policy\": {\n";
  out << "    \"maximum_required_field_count\": "
      << plan.policy.maximum_required_field_count << ",\n";
  out << "    \"maximum_required_metric_count\": "
      << plan.policy.maximum_required_metric_count << ",\n";
  out << "    \"maximum_target_count\": "
      << plan.policy.maximum_target_count << ",\n";
  out << "    \"warning_coverage_threshold\": "
      << inferlab_plan::fixed6(plan.policy.warning_coverage_threshold)
      << "\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"canonical_wiring_complete_target_count\": "
      << plan.summary.canonical_wiring_complete_target_count << ",\n";
  out << "    \"catalog_managed_target_count\": "
      << plan.summary.catalog_managed_target_count << ",\n";
  out << "    \"contract_score\": "
      << inferlab_plan::fixed6(plan.summary.contract_score) << ",\n";
  out << "    \"core_deep_dive_extension_target_count\": "
      << plan.summary.core_deep_dive_extension_target_count << ",\n";
  out << "    \"fail_count\": " << plan.summary.fail_count << ",\n";
  out << "    \"fallback_retained_target_count\": "
      << plan.summary.fallback_retained_target_count << ",\n";
  out << "    \"gate_decision\": "
      << inferlab_plan::json_string(
             gate_decision_name(plan.summary.gate_decision))
      << ",\n";
  out << "    \"legacy_fallback_target_count\": "
      << plan.summary.legacy_fallback_target_count << ",\n";
  out << "    \"deep_dive_wiring_registry_target_count\": "
      << plan.summary.deep_dive_wiring_registry_target_count << ",\n";
  out << "    \"mature_product_dispatch_target_count\": "
      << plan.summary.mature_product_dispatch_target_count << ",\n";
  out << "    \"pass_count\": " << plan.summary.pass_count << ",\n";
  out << "    \"pending_count\": " << plan.summary.pending_count
      << ",\n";
  out << "    \"present_count\": " << plan.summary.present_count << ",\n";
  out << "    \"target_count\": " << plan.summary.target_count << ",\n";
  out << "    \"warn_count\": " << plan.summary.warn_count << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t i = 0; i < plan.records.size(); ++i) {
    out << record_json(plan.records[i], 4)
        << (i + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(plan.blockers, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_deep_dive_contract
