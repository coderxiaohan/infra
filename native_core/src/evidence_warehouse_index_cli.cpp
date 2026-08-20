#include "evidence_warehouse_index_cli.hpp"

#include "evidence_warehouse_index_model.hpp"
#include "evidence_warehouse_index_sqlite.hpp"
#include "evidence_warehouse_index_view.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace inferlab_evidence_warehouse {
namespace {

namespace fs = std::filesystem;

const inferlab_plan::CliOptionSpec& cli_spec() {
  static const inferlab_plan::CliOptionSpec spec{
      {"--repo-root", "--results", "--output", "--sqlite-output",
       "--format"},
      {},
  };
  return spec;
}

}  // namespace

int run_cli(const int argc, char** argv) {
  const auto& spec = cli_spec();
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_evidence_warehouse_index");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, spec)) return 0;
    inferlab_plan::validate_cli_args(argc, argv, spec);

    const fs::path repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", spec);
    const fs::path results_root = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--results", "results/raw", spec));
    const fs::path output_argument = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", spec);
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_argument);
    const fs::path sqlite_argument = inferlab_plan::arg_value_with_spec(
        argc, argv, "--sqlite-output", "", spec);
    const fs::path sqlite_output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           sqlite_argument);
    if (!output_path.empty() && !sqlite_output_path.empty() &&
        inferlab_plan::paths_alias(output_path, sqlite_output_path)) {
      throw std::invalid_argument(
          "--output and --sqlite-output must resolve to different paths");
    }
    const std::string format = inferlab_plan::arg_value_with_spec(
        argc, argv, "--format", "json", spec);
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }

    const auto plan =
        build_warehouse_plan(repo_root, results_root, output_path);
    const auto sqlite_status = write_sqlite_index(sqlite_output_path, plan);
    const std::string payload = warehouse_artifact_json(
        plan, sqlite_status, repo_root, results_root, output_path,
        sqlite_output_path, format);
    inferlab_plan::write_or_print(output_path.generic_string(), payload);
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, spec);
    return 1;
  }
  return 0;
}

}  // namespace inferlab_evidence_warehouse
