#include "edge_runtime_matrix_model.hpp"
#include "edge_runtime_matrix_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_edge_runtime_matrix");
  const inferlab_plan::CliOptionSpec cli_spec{
      {"--repo-root", "--config", "--mode", "--output", "--format"}, {}};
  std::string config_path;
  std::string output_path;
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli_spec)) return 0;
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);
    const std::filesystem::path repo_root =
        inferlab_plan::arg_value_with_spec(argc, argv, "--repo-root", ".",
                                           cli_spec);
    const std::string config_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--config", "configs/edge_runtime_matrix.example.json",
        cli_spec);
    const std::string mode_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--mode", "plan", cli_spec);
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", cli_spec);
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", cli_spec);
    if (format != "json") {
      throw std::invalid_argument("edge runtime matrix format must be json");
    }
    const auto mode = inferlab::edge_runtime_matrix::parse_mode(mode_arg);
    if (mode == inferlab::edge_runtime_matrix::Mode::kExecute &&
        output_arg.empty()) {
      throw std::invalid_argument(
          "edge runtime matrix execute mode requires --output so third-party runtime logs cannot corrupt artifact JSON");
    }
    config_path = inferlab_plan::resolve_repo_relative_path(repo_root,
                                                             config_arg)
                      .generic_string();
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .generic_string();
    const auto matrix = inferlab::edge_runtime_matrix::build_matrix(
        repo_root, config_path, mode, output_path);
    inferlab_plan::write_or_print(
        output_path, inferlab::edge_runtime_matrix::artifact_json(matrix));
    return matrix.mode == inferlab::edge_runtime_matrix::Mode::kExecute &&
                   matrix.summary.decision == "blocked"
               ? 2
               : 0;
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error, "error: ",
                                    {config_path, output_path});
    inferlab_plan::print_cli_usage(program, cli_spec);
    return 1;
  }
}
