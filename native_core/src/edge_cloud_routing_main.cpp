#include "edge_cloud_routing_model.hpp"
#include "edge_cloud_routing_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_edge_cloud_routing");
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
        argc, argv, "--config", "configs/edge_cloud_routing.example.json",
        cli_spec);
    const std::string mode_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--mode", "plan", cli_spec);
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", cli_spec);
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", cli_spec);
    if (format != "json") {
      throw std::invalid_argument("edge-cloud routing format must be json");
    }
    const auto mode = inferlab::edge_cloud_routing::parse_mode(mode_arg);
    if (mode == inferlab::edge_cloud_routing::Mode::kExecute &&
        output_arg.empty()) {
      throw std::invalid_argument(
          "edge-cloud execute mode requires --output so transport diagnostics cannot corrupt artifact JSON");
    }
    const std::filesystem::path resolved_config =
        inferlab_plan::resolve_repo_relative_path(repo_root, config_arg);
    const std::filesystem::path resolved_output =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    config_path = resolved_config.generic_string();
    output_path = resolved_output.generic_string();
    const auto artifact = inferlab::edge_cloud_routing::build_artifact(
        repo_root, resolved_config, mode, resolved_output);
    inferlab_plan::write_or_print(
        output_path,
        inferlab::edge_cloud_routing::artifact_json(artifact));
    return mode == inferlab::edge_cloud_routing::Mode::kExecute &&
                   artifact.summary.decision == "blocked"
               ? 2
               : 0;
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error, "error: ",
                                    {config_path, output_path});
    inferlab_plan::print_cli_usage(program, cli_spec);
    return 1;
  }
}
