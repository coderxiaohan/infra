#include "plan_json_utils.hpp"
#include "product_integration_inventory_model.hpp"
#include "product_integration_inventory_view.hpp"

#include <exception>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_product_integration_inventory");
  fs::path repo_root = ".";
  fs::path output;
  try {
    const auto& cli = inferlab_plan::repo_root_output_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli)) return 0;
    inferlab_plan::validate_cli_args(argc, argv, cli);
    repo_root = inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    output = inferlab_plan::resolve_optional_repo_relative_path(
        repo_root, inferlab_plan::arg_value(argc, argv, "--output", ""));

    const auto plan =
        inferlab::product_integration_inventory::build_inventory(repo_root);
    const inferlab::product_integration_inventory::ArtifactViewContext view{
        repo_root, output};
    inferlab_plan::write_or_print(
        output.empty() ? "" : output.string(),
        inferlab::product_integration_inventory::artifact_json(plan, view));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "error: ", {repo_root.string(), output.string()});
    inferlab_plan::print_cli_usage(
        program,
        inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
