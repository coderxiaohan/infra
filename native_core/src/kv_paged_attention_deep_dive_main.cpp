#include "kv_paged_attention_deep_dive_model.hpp"
#include "kv_paged_attention_deep_dive_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <string>
#include <utility>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kv_paged_attention_deep_dive");
  std::string output_path;
  try {
    const auto& cli_spec =
        inferlab_native_trace::trace_context_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli_spec)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);
    const std::filesystem::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const std::filesystem::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .string();
    auto trace_source =
        inferlab_native_trace::load_trace_context_source_from_cli(
            repo_root, argc, argv);

    const auto plan =
        inferlab::kv_paged_attention_deep_dive::build_plan(
            inferlab::kv_paged_attention_deep_dive::default_assumptions(),
            inferlab::kv_paged_attention_deep_dive::default_scenarios(),
            std::move(trace_source));
    inferlab_plan::write_or_print(
        output_path,
        inferlab::kv_paged_attention_deep_dive::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_native_trace::trace_context_cli_spec());
    return 1;
  }
  return 0;
}
