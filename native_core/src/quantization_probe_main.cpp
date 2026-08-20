#include "quantization_probe_model.hpp"
#include "quantization_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

using inferlab_plan::parse_standard_positive_int_arg;

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--rows", "--cols", "--group-size", "--output"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_quantization_probe");
  auto config = inferlab::quantization_probe::default_config();

  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--repo-root", ".", kCliOptions);
    config.rows = parse_standard_positive_int_arg(
        "--rows", inferlab_plan::arg_value_with_spec(
                      argc, argv, "--rows", std::to_string(config.rows),
                      kCliOptions));
    config.cols = parse_standard_positive_int_arg(
        "--cols", inferlab_plan::arg_value_with_spec(
                      argc, argv, "--cols", std::to_string(config.cols),
                      kCliOptions));
    config.group_size = parse_standard_positive_int_arg(
        "--group-size",
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--group-size",
            std::to_string(config.group_size), kCliOptions));
    const std::string output_arg =
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg)
            .generic_string();
    const auto plan = inferlab::quantization_probe::build_plan(config);
    inferlab_plan::write_or_print(
        output_path,
        inferlab::quantization_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }

  return 0;
}
