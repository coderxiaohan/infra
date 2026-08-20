#include "artifact_catalog_single_source_model.hpp"
#include "artifact_catalog_single_source_views.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  try {
    const auto& cli_spec = inferlab_plan::repo_root_output_format_cli_spec();
    const char* program = inferlab_plan::cli_program_name(
        argc, argv, "inferlab_artifact_catalog_single_source");
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli_spec)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);

    const fs::path repo_root =
        inferlab_plan::arg_value_with_spec(argc, argv, "--repo-root", ".",
                                           cli_spec);
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", cli_spec);
    if (format != "json") {
      throw std::invalid_argument(
          "unsupported --format value: " + format + "; expected json");
    }
    const fs::path output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", cli_spec);
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    const std::string output =
        output_path.empty() ? "" : output_path.generic_string();
    const auto projection =
        inferlab_catalog_single_source::build_catalog_single_source_projection();
    inferlab_plan::write_or_print(
        output,
        inferlab_catalog_single_source::render_catalog_single_source_artifact(
            projection, output_arg.generic_string(), repo_root, output_path,
            format));
  } catch (const std::exception& exc) {
    inferlab_plan::report_cli_error(
        exc, "inferlab_artifact_catalog_single_source failed: ");
    return 1;
  }
  return 0;
}
