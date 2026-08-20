#include "scheduler_capacity_sweep_model.hpp"
#include "scheduler_capacity_sweep_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_scheduler_capacity_sweep");
  try {
    const auto& cli_options = inferlab_plan::repo_root_output_cli_spec();
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, cli_options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_options);
    const std::string repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", cli_options);
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", cli_options);
    const auto output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg);
    const auto plan =
        inferlab::scheduler_capacity_sweep::build_default_plan();
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::scheduler_capacity_sweep::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
