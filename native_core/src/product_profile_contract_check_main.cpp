#include "plan_json_utils.hpp"
#include "product_profile_contract_check_model.hpp"
#include "product_profile_contract_check_view.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--results", "--output", "--format"},
    {"--strict"},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_product_profile_contract_check");
  fs::path repo_root = ".";
  fs::path results_root = "results/raw";
  fs::path output;
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    repo_root = inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    results_root = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw"));
    output = inferlab_plan::resolve_optional_repo_relative_path(
        repo_root, inferlab_plan::arg_value(argc, argv, "--output", ""));
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }
    const auto plan =
        inferlab::product_profile_contract_check::build_audit(
            results_root,
            inferlab_plan::has_flag(argc, argv, "--strict"));
    const inferlab::product_profile_contract_check::ArtifactViewContext view{
        repo_root, results_root, output, format};
    inferlab_plan::write_or_print(
        output.empty() ? "" : output.string(),
        inferlab::product_profile_contract_check::artifact_json(plan, view));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "error: ",
        {repo_root.string(), results_root.string(), output.string()});
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
