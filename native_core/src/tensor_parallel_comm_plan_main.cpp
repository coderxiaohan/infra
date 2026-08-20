#include "plan_json_utils.hpp"
#include "tensor_parallel_comm_plan_model.hpp"
#include "tensor_parallel_comm_plan_view.hpp"

#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_tensor_parallel_comm_plan");
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
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg);
    const auto plan = inferlab::tensor_parallel_comm::build_plan(
        inferlab::default_tensor_parallel_comm_scenarios());
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::tensor_parallel_comm::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
