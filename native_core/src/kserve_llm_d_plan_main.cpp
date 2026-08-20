#include "kserve_llmd_plan_model.hpp"
#include "kserve_llmd_plan_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--output", "--report"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kserve_llm_d_plan");

  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--repo-root", ".", kCliOptions);
    const std::string output_arg =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--output", "", kCliOptions);
    const std::string report_arg =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--report", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg)
            .generic_string();
    const std::string report_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, report_arg)
            .generic_string();

    const auto plan = inferlab::kserve_llmd_plan::build_default_plan();
    inferlab_plan::write_or_print(
        output_path,
        inferlab_kserve_view::artifact_json(plan));
    if (!report_path.empty()) {
      inferlab_plan::write_or_print(
          report_path, inferlab_kserve_view::deployment_report(plan));
    }
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }

  return 0;
}
