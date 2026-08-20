#include "release_evidence_pipeline_cli_view.hpp"

#include "artifact_metadata_reader.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string pipeline_cli_context_json(
    const ReleaseEvidencePipelineRequest& request,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string results_string = request.results.string();
  const std::string repo_root_string = request.repo_root.string();
  const std::string output_string = request.output.string();
  const std::string repo_root_label =
      inferlab_plan::redacted_path_label(repo_root_string);
  const std::string results_label =
      inferlab_plan::redacted_path_label(results_string);
  const std::string output_label =
      request.output.empty()
          ? ""
          : inferlab_plan::redacted_path_label(output_string);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": "
      << inferlab_plan::json_string(request.format) << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, indent + 2) << ",\n";
  out << nested << "\"repo_root_path\": "
      << inferlab_plan::json_string(repo_root_label) << ",\n";
  out << nested << "\"repo_root_path_redacted\": "
      << inferlab_plan::json_bool(inferlab_plan::path_was_redacted(
             repo_root_string, repo_root_label))
      << ",\n";
  out << nested << "\"repo_root_path_status\": "
      << inferlab_plan::json_string(
             inferlab_artifact::path_probe_status(request.repo_root))
      << ",\n";
  out << nested << "\"results_path\": "
      << inferlab_plan::json_string(results_label) << ",\n";
  out << nested << "\"results_path_redacted\": "
      << inferlab_plan::json_bool(
             inferlab_plan::path_was_redacted(results_string, results_label))
      << ",\n";
  out << nested << "\"results_path_status\": "
      << inferlab_plan::json_string(
             inferlab_artifact::path_probe_status(request.results))
      << ",\n";
  out << nested << "\"output_path\": "
      << (request.output.empty() ? "null"
                                 : inferlab_plan::json_string(output_label))
      << ",\n";
  out << nested << "\"output_path_redacted\": "
      << inferlab_plan::json_bool(
             !request.output.empty() && inferlab_plan::path_was_redacted(
                                            output_string, output_label))
      << ",\n";
  out << nested << "\"output_path_status\": "
      << inferlab_plan::json_string(
             inferlab_artifact::path_probe_status(request.output))
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"CLI context documents accepted dry-run release-pipeline inputs only; it does not execute refresh commands, run validation, or prove release readiness\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
