#include "deep_dive_contract_check_model.hpp"
#include "deep_dive_contract_check_view.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--results", "--output", "--format"},
    {"--strict-missing"},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_deep_dive_contract_check");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::filesystem::path repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    const std::filesystem::path results_root =
        inferlab_plan::resolve_repo_relative_path(
            repo_root, inferlab_plan::arg_value_with_spec(
                           argc, argv, "--results", "results/raw",
                           kCliOptions));
    const std::filesystem::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, inferlab_plan::arg_value_with_spec(
                           argc, argv, "--output", "", kCliOptions));
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", kCliOptions);
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }
    const bool strict_missing =
        inferlab_plan::has_flag(argc, argv, "--strict-missing");
    const auto plan = inferlab_deep_dive_contract::build_plan(
        inferlab_deep_dive_contract::default_policy(), results_root,
        strict_missing);
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab_deep_dive_contract::artifact_json(
            plan, repo_root, results_root, output_path, format));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
