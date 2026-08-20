#include "artifact_schema_registry_model.hpp"
#include "artifact_schema_registry_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_artifact_schema_registry");
  try {
    const auto& cli_options =
        inferlab_plan::repo_root_output_format_cli_spec();
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, cli_options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_options);
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", cli_options);
    if (format != "json") {
      throw std::invalid_argument(
          "unsupported --format value: " + format + "; expected json");
    }
    const std::filesystem::path repo_root =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--repo-root", ".", cli_options);
    const std::filesystem::path output_arg =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--output", "", cli_options);
    const auto output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg);
    const auto plan =
        inferlab::artifact_schema_registry::build_plan(repo_root);
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::artifact_schema_registry::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "inferlab_artifact_schema_registry failed: ");
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_format_cli_spec());
    return 1;
  }
  return 0;
}
