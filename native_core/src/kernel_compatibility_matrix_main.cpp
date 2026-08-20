#include "kernel_compatibility_matrix_model.hpp"
#include "kernel_compatibility_matrix_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kernel_compatibility_matrix");
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
    const auto plan = inferlab::kernel_compatibility::build_plan(
        inferlab::kernel_compatibility::default_assumptions(),
        inferlab_high_value::microbench_operators(),
        inferlab::kernel_compatibility::default_profiles());
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::kernel_compatibility::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
