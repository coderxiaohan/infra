#include "cuda_quant_microkernel_deep_dive_model.hpp"
#include "cuda_quant_microkernel_deep_dive_view.hpp"

#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_cuda_quant_microkernel_deep_dive");
  try {
    const auto& cli_spec =
        inferlab_native_trace::trace_context_cli_spec();
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, cli_spec)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg);
    auto trace_source =
        inferlab_native_trace::load_trace_context_source_from_cli(
            repo_root, argc, argv);
    const auto plan = inferlab::cuda_quant_microkernel::build_plan(
        inferlab::cuda_quant_microkernel::default_assumptions(),
        inferlab_high_value::microbench_operators(),
        inferlab::cuda_quant_microkernel::default_profiles(),
        std::move(trace_source));
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::cuda_quant_microkernel::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_native_trace::trace_context_cli_spec());
    return 1;
  }
  return 0;
}
