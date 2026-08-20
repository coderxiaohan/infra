#include "lora_adapter_cache_plan_model.hpp"
#include "lora_adapter_cache_plan_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_lora_adapter_cache_plan");
  try {
    const auto& options = inferlab_plan::repo_root_output_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, options);
    const std::string root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", options);
    const std::string output = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", options);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(root, output)
            .generic_string();
    const auto plan =
        inferlab::lora_adapter_cache_plan::build_default_plan();
    inferlab_plan::write_or_print(
        output_path,
        inferlab::lora_adapter_cache_plan::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_output_cli_spec());
    return 1;
  }
  return 0;
}
