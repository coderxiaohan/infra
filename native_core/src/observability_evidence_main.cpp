#include "failure_drill_model.hpp"
#include "observability_evidence_view.hpp"
#include "observability_slo_plan_model.hpp"
#include "otel_sampling_budget_plan_model.hpp"
#include "otel_trace_plan_model.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_observability_evidence");
  try {
    const auto& options = inferlab_plan::repo_root_output_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, options);
    const std::string repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", options);
    const std::string output = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", options);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output)
            .generic_string();

    const auto slo = inferlab::observability_slo_plan::build_default_plan();
    const auto trace = inferlab::otel_trace_plan::build_default_plan();
    const auto sampling =
        inferlab::otel_sampling_budget_plan::build_default_plan();
    const auto drill = inferlab::failure_drill::build_plan(
        inferlab::failure_drill::default_assumptions(),
        inferlab::failure_drill::default_scenarios());
    inferlab_plan::write_or_print(
        output_path, inferlab::observability_evidence::artifact_json(
                         slo, trace, sampling, drill));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
