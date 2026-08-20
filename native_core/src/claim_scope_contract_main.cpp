#include "claim_scope_contract_model.hpp"
#include "claim_scope_contract_view.hpp"
#include "plan_json_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--matrix", "--results", "--output", "--format"},
    {"--strict"},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_claim_scope_contract");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);

    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const fs::path matrix_arg = inferlab_plan::arg_value(
        argc, argv, "--matrix", "configs/benchmark_matrix.yaml");
    const fs::path results_arg =
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw");
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }

    const fs::path matrix_path =
        inferlab_plan::resolve_repo_relative_path(repo_root, matrix_arg);
    const fs::path results_root =
        inferlab_plan::resolve_repo_relative_path(repo_root, results_arg);
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    const auto audit = inferlab_claim_scope::evaluate_claim_scope_contract(
        matrix_path, results_root,
        inferlab_plan::has_flag(argc, argv, "--strict"));
    const inferlab_claim_scope::ClaimScopeContractViewContext view_context{
        repo_root, output_path, format};
    inferlab_plan::write_or_print(
        output_path.string(),
        inferlab_claim_scope::claim_scope_contract_artifact_json(
            audit, view_context));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
