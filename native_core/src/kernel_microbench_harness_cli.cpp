#include "kernel_microbench_harness_cli.hpp"

#include "kernel_microbench_harness_model.hpp"
#include "kernel_microbench_harness_runner.hpp"
#include "kernel_microbench_harness_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace inferlab::kernel_microbench {
namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--output", "--operator", "--case-profile",
     "--backend", "--benchmark-min-time-ms", "--benchmark-repetitions",
     "--format"},
    {"--execute"},
};

HarnessOptions parse_options(const int argc, char** argv) {
  HarnessOptions options;
  options.selected_operator =
      inferlab_plan::arg_value(argc, argv, "--operator", "all");
  const std::string profile =
      inferlab_plan::arg_value(argc, argv, "--case-profile", "smoke");
  options.all_profiles = profile == "all";
  if (!options.all_profiles) {
    options.selected_profile = parse_case_profile(profile);
  }
  options.backend = parse_execution_backend(
      inferlab_plan::arg_value(argc, argv, "--backend", "cuda_runtime"));
  options.execute_requested =
      inferlab_plan::has_flag(argc, argv, "--execute");
  options.benchmark_min_time_ms = inferlab_plan::parse_positive_double_arg(
      "--benchmark-min-time-ms",
      inferlab_plan::arg_value(argc, argv, "--benchmark-min-time-ms", "20"),
      600000.0);
  options.benchmark_repetitions = inferlab_plan::parse_positive_int_arg(
      "--benchmark-repetitions",
      inferlab_plan::arg_value(argc, argv, "--benchmark-repetitions", "5"),
      100);
  return options;
}

}  // namespace

int run_harness_cli(const int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kernel_microbench_harness");
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
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

    auto plan = build_harness_plan(parse_options(argc, argv),
                                   compiled_harness_capabilities());
    plan.repo_root_label =
        inferlab_plan::redacted_path_label(repo_root.generic_string());
    plan.output_path_label = output_path.empty()
                                 ? ""
                                 : inferlab_plan::redacted_path_label(
                                       output_path.generic_string());
    if (plan.options.execute_requested) execute_harness_plan(plan);
    inferlab_plan::write_or_print(output_path.generic_string(),
                                  harness_artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "inferlab_kernel_microbench_harness failed: ");
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}

}  // namespace inferlab::kernel_microbench
