#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_artifact_view.hpp"
#include "release_evidence_pipeline_request.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    const char* program_name = inferlab_plan::cli_program_name(
        argc, argv, "inferlab_release_evidence_pipeline");
    if (inferlab_plan::handle_cli_help(
            argc, argv, program_name,
            inferlab_plan::repo_root_results_output_format_cli_spec())) {
      return 0;
    }
    inferlab_plan::validate_cli_args(
        argc, argv,
        inferlab_plan::repo_root_results_output_format_cli_spec());
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument(
          "unsupported --format value: " + format + "; expected json");
    }
    const fs::path results = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw"));
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output = inferlab_plan::resolve_optional_repo_relative_path(
        repo_root, output_arg);
    const inferlab_release_pipeline::ReleaseEvidencePipelineRequest request(
        repo_root, results, output, format);
    inferlab_plan::write_or_print(
        output.string(),
        inferlab_release_pipeline::render_release_evidence_pipeline_artifact(
            request));
  } catch (const std::exception& exc) {
    inferlab_plan::report_cli_error(
        exc, "inferlab_release_evidence_pipeline failed: ");
    return 1;
  }
  return 0;
}
