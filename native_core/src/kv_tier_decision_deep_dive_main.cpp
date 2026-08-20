#include "kv_tier_decision_deep_dive_model.hpp"
#include "kv_tier_decision_deep_dive_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kv_tier_decision_deep_dive");
  std::string output_path;
  try {
    const auto& cli_spec = inferlab_plan::repo_root_output_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli_spec)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);
    const std::filesystem::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::filesystem::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .string();

    const auto plan =
        inferlab::kv_tier_decision_deep_dive::build_plan(
            inferlab::kv_tier_decision_deep_dive::default_assumptions(),
            inferlab::kv_tier_decision_deep_dive::default_scenarios());
    inferlab_plan::write_or_print(
        output_path,
        inferlab::kv_tier_decision_deep_dive::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
