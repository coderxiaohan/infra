#include "runtime_evidence_freshness_gate_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>

namespace inferlab_freshness_gate {
namespace {

std::string cli_context_json(const std::filesystem::path& repo_root,
                             const std::filesystem::path& results_root,
                             const std::filesystem::path& output,
                             const std::string& reference_age_arg,
                             const std::string& format, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": " << inferlab_plan::json_string(format)
      << ",\n";
  out << nested << "\"reference_age_hours\": "
      << (reference_age_arg.empty()
              ? "null"
              : inferlab_plan::json_string(reference_age_arg))
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
  out << inferlab_artifact::cli_path_field_json("output", output, indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"CLI context documents accepted freshness-gate inputs only; it does not refresh artifacts, execute producers, or prove benchmark/replay/execute readiness\"\n";
  out << pad << "}";
  return out.str();
}

std::string nullable_fixed6(const bool present, const double value) {
  return present ? inferlab_plan::fixed6(value) : "null";
}

std::string record_json(const FreshnessRecord& record, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"age_hours\": "
      << nullable_fixed6(record.age_known, record.age_hours) << ",\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(record.target.artifact) << ",\n";
  out << nested << "\"consumer\": "
      << inferlab_plan::json_string(record.target.consumer) << ",\n";
  out << nested << "\"evidence_class\": "
      << inferlab_plan::json_string(record.target.evidence_class) << ",\n";
  out << nested << "\"freshness_score\": "
      << inferlab_plan::fixed6(record.freshness_score) << ",\n";
  out << nested << "\"freshness_state\": "
      << inferlab_plan::json_string(record.freshness_state) << ",\n";
  out << nested << "\"max_age_hours\": " << record.target.max_age_hours
      << ",\n";
  out << nested << "\"mtime_probe_error\": "
      << inferlab_plan::json_string(record.mtime_probe_error) << ",\n";
  out << nested << "\"mtime_probe_status\": "
      << inferlab_plan::json_string(record.mtime_probe_status) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(record.target.name) << ",\n";
  out << nested << "\"path_probe_error\": "
      << inferlab_plan::json_string(record.path_probe_error) << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(record.path_probe_status) << ",\n";
  out << nested << "\"present\": " << inferlab_plan::json_bool(record.present)
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(record.recommendation) << ",\n";
  out << nested << "\"refresh_command\": "
      << inferlab_plan::json_string(record.target.refresh_command) << ",\n";
  out << nested << "\"release_blocking\": "
      << inferlab_plan::json_bool(record.release_blocking) << ",\n";
  out << nested << "\"requires_execute\": "
      << inferlab_plan::json_bool(record.target.requires_execute) << ",\n";
  out << nested << "\"requires_hardware\": "
      << inferlab_plan::json_bool(record.target.requires_hardware) << ",\n";
  const std::string resolved_path = record.resolved_path.generic_string();
  const std::string resolved_path_label =
      inferlab_plan::redacted_path_label(resolved_path);
  out << nested << "\"resolved_path\": "
      << inferlab_plan::json_string(resolved_path_label) << ",\n";
  out << nested << "\"resolved_path_redacted\": "
      << inferlab_plan::json_bool(
             inferlab_plan::path_was_redacted(resolved_path,
                                              resolved_path_label))
      << ",\n";
  out << nested << "\"stale\": " << inferlab_plan::json_bool(record.stale)
      << ",\n";
  out << nested << "\"stale_policy\": "
      << inferlab_plan::json_string(record.target.stale_policy) << ",\n";
  out << nested << "\"text_read_error\": "
      << inferlab_plan::json_string(record.text_read_error) << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(record.text_read_status) << "\n";
  out << pad << "}";
  return out.str();
}

void write_summary(std::ostringstream& out,
                   const RuntimeEvidenceFreshnessGateRequest& request) {
  const FreshnessSummary& summary = request.summary();
  out << "  \"summary\": {\n";
  out << "    \"execute_required_count\": "
      << summary.execute_required_count << ",\n";
  out << "    \"fresh_count\": " << summary.fresh_count << ",\n";
  out << "    \"freshness_score\": "
      << inferlab_plan::fixed6(summary.freshness_score) << ",\n";
  out << "    \"hardware_required_count\": "
      << summary.hardware_required_count << ",\n";
  out << "    \"missing_count\": " << summary.missing_count << ",\n";
  out << "    \"most_urgent_target\": "
      << inferlab_plan::json_string(summary.most_urgent_target) << ",\n";
  out << "    \"path_probe_failed_count\": "
      << summary.path_probe_failed_count << ",\n";
  out << "    \"present_count\": " << summary.present_count << ",\n";
  out << "    \"release_blocking_count\": "
      << summary.release_blocking_count << ",\n";
  out << "    \"release_decision\": "
      << inferlab_plan::json_string(summary.release_decision) << ",\n";
  out << "    \"registry_freshness_target_count\": "
      << request.target_projection().registry_target_count << ",\n";
  out << "    \"registry_consumed_freshness_target_count\": "
      << request.target_projection().registry_consumed_target_count << ",\n";
  out << "    \"stale_count\": " << summary.stale_count << ",\n";
  out << "    \"stale_or_missing_count\": "
      << summary.stale_or_missing_count << ",\n";
  out << "    \"target_count\": " << request.records().size() << ",\n";
  out << "    \"unknown_age_count\": " << summary.unknown_age_count
      << ",\n";
  out << "    \"unreadable_count\": " << summary.unreadable_count << "\n";
  out << "  },\n";
}

}  // namespace

std::string runtime_evidence_freshness_gate_artifact_json(
    const RuntimeEvidenceFreshnessGateRequest& request,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& resolved_output_path,
    const std::string& reference_age_arg, const std::string& format) {
  const std::string output = resolved_output_path.empty()
                                 ? ""
                                 : resolved_output_path.generic_string();
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, "native_runtime_evidence_freshness_gate",
      "artifact freshness gate only; does not run builds, tests, CUDA, products, benchmarks, report rendering, or endpoint probes",
      {"vLLM", "SGLang", "TensorRT-LLM", "FlashInfer", "LMCache",
       "Mooncake", "Dynamo", "KServe/llm-d", "Ray Serve", "TGI",
       "KTransformers"},
      "inferlab_runtime_evidence_freshness_gate",
      output.empty() ? "results/raw/native_runtime_evidence_freshness_gate.json"
                     : output,
      "./build/inferlab_runtime_evidence_freshness_gate --repo-root . --results results/raw --output results/raw/native_runtime_evidence_freshness_gate.json --format json");
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"audit_type\": \"runtime_evidence_freshness_gate\",\n";
  out << "  \"claim_scope\": \"artifact_freshness_policy\",\n";
  out << "  \"cli_context\": "
      << cli_context_json(repo_root, request.results_root(),
                          resolved_output_path, reference_age_arg, format, 2)
      << ",\n";
  const std::string results_root_path = request.results_root().generic_string();
  const std::string results_root_label =
      inferlab_plan::redacted_path_label(results_root_path);
  out << "  \"results_root\": "
      << inferlab_plan::json_string(results_root_label) << ",\n";
  out << "  \"results_root_redacted\": "
      << inferlab_plan::json_bool(inferlab_plan::path_was_redacted(
             results_root_path, results_root_label))
      << ",\n";
  write_summary(out, request);
  out << "  \"records\": [\n";
  for (std::size_t i = 0; i < request.records().size(); ++i) {
    out << record_json(request.records()[i], 4)
        << (i + 1U == request.records().size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab_freshness_gate
