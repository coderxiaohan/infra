#include "engine_memory_planner_model.hpp"
#include "engine_memory_planner_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--output", "--gpu-memory-gb"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_engine_memory_planner");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    const double gpu_memory_gib = inferlab_plan::parse_positive_double_arg(
        "--gpu-memory-gb",
        inferlab_plan::arg_value(argc, argv, "--gpu-memory-gb", "24.0"));
    const auto plan = inferlab::engine_memory_planner::build_plan(
        inferlab::engine_memory_planner::default_assumptions(),
        inferlab::engine_memory_planner::default_profiles(gpu_memory_gib));
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::engine_memory_planner::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
