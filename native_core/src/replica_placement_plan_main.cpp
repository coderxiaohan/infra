#include "inferlab/advanced_serving_deployment_planning.hpp"
#include "plan_json_utils.hpp"
#include "replica_placement_plan_view.hpp"

#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_replica_placement_plan");
  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program,
            inferlab_plan::repo_root_output_cli_spec())) {
      return 0;
    }
    inferlab_plan::validate_cli_args(
        argc, argv, inferlab_plan::repo_root_output_cli_spec());
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    const auto plan = inferlab::build_replica_placement_plan(
        inferlab::default_replica_placement_assumptions(),
        inferlab::default_replica_placement_scenarios());
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::replica_placement_view::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
