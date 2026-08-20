#include "dependency_governance_cli.hpp"

#include "dependency_governance_model.hpp"
#include "high_value_artifact_catalog.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace inferlab::dependency_governance {
namespace {

const char* fallback_program_name(const ArtifactView view) {
  switch (view) {
    case ArtifactView::policy_gate:
      return "inferlab_third_party_dependency_gate";
    case ArtifactView::sbom_gate:
      return "inferlab_dependency_sbom_gate";
  }
  throw std::logic_error("unknown dependency governance CLI view");
}

}  // namespace

int run_cli(const int argc, char** argv, const ArtifactView view) {
  const char* program = "inferlab_dependency_governance";
  try {
    program = inferlab_plan::cli_program_name(
        argc, argv, fallback_program_name(view));
    const auto& cli = inferlab_plan::repo_root_output_format_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli)) return 0;
    inferlab_plan::validate_cli_args(argc, argv, cli);
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument(
          "unsupported --format value: " + format + "; expected json");
    }
    const std::filesystem::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::filesystem::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const std::filesystem::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                          output_arg);
    const auto plan = build_plan(
        inferlab_high_value::dependency_specs(),
        inferlab_high_value::require_managed_artifact(
            "third_party_dependency_gate"),
        inferlab_high_value::require_managed_artifact(
            "dependency_sbom_gate"));
    inferlab_plan::write_or_print(output_path.generic_string(),
                                  artifact_json(plan, view));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_format_cli_spec());
    return 1;
  }
  return 0;
}

}  // namespace inferlab::dependency_governance
