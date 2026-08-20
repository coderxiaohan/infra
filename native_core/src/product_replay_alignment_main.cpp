#include "plan_json_utils.hpp"
#include "product_replay_alignment_model.hpp"
#include "product_replay_alignment_view.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_product_replay_alignment");
  std::filesystem::path repo_root = ".";
  std::filesystem::path results_root = "results/raw";
  std::filesystem::path output;
  try {
    const auto& cli =
        inferlab_plan::repo_root_results_output_format_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli)) return 0;
    inferlab_plan::validate_cli_args(argc, argv, cli);
    repo_root = inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    results_root = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw"));
    output = inferlab_plan::resolve_optional_repo_relative_path(
        repo_root, inferlab_plan::arg_value(argc, argv, "--output", ""));
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }
    const auto plan =
        inferlab::product_replay_alignment::build_alignment_plan(
            results_root);
    const inferlab::product_replay_alignment::AlignmentArtifactViewContext
        context{repo_root, results_root, output, format};
    inferlab_plan::write_or_print(
        output.empty() ? "" : output.string(),
        inferlab::product_replay_alignment::alignment_artifact_json(
            plan, context));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "error: ",
        {repo_root.string(), results_root.string(), output.string()});
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_results_output_format_cli_spec());
    return 1;
  }
  return 0;
}
