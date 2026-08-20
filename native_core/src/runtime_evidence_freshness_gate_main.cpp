#include "plan_json_utils.hpp"
#include "runtime_evidence_freshness_gate_model.hpp"
#include "runtime_evidence_freshness_gate_view.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--results", "--reference-age-hours", "--output",
     "--format"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* executable = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_runtime_evidence_freshness_gate");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, executable, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }

    const fs::path results_root = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw"));
    const fs::path output_path =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path resolved_output_path =
        output_path.empty()
            ? fs::path{}
            : inferlab_plan::resolve_repo_relative_path(repo_root, output_path);
    const std::string reference_age_arg = inferlab_plan::arg_value(
        argc, argv, "--reference-age-hours", "");
    const double reference_age_hours =
        reference_age_arg.empty()
            ? -1.0
            : inferlab_plan::parse_non_negative_double_arg(
                  "--reference-age-hours", reference_age_arg);

    const inferlab_freshness_gate::RuntimeEvidenceFreshnessGateRequest request(
        results_root, reference_age_hours);
    const std::string artifact =
        inferlab_freshness_gate::runtime_evidence_freshness_gate_artifact_json(
            request, repo_root, resolved_output_path, reference_age_arg, format);
    inferlab_plan::write_or_print(resolved_output_path.generic_string(),
                                  artifact);
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(executable, kCliOptions);
    return 1;
  }
  return 0;
}
