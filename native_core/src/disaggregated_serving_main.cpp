#include "disaggregated_serving_model.hpp"
#include "disaggregated_serving_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--prefill-ms-per-token", "--decode-ms-per-token",
     "--kv-bytes-per-token", "--output"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  inferlab::DisaggregatedConstants constants;
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_disaggregated_serving");

  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    constants.prefill_ms_per_token = inferlab_plan::parse_non_negative_double_arg(
        "--prefill-ms-per-token",
        inferlab_plan::arg_value(argc, argv, "--prefill-ms-per-token",
                                 inferlab_plan::fixed6(constants.prefill_ms_per_token)));
    constants.decode_ms_per_token = inferlab_plan::parse_non_negative_double_arg(
        "--decode-ms-per-token",
        inferlab_plan::arg_value(argc, argv, "--decode-ms-per-token",
                                 inferlab_plan::fixed6(constants.decode_ms_per_token)));
    constants.kv_bytes_per_token = inferlab_plan::parse_non_negative_double_arg(
        "--kv-bytes-per-token",
        inferlab_plan::arg_value(argc, argv, "--kv-bytes-per-token",
                                 inferlab_plan::fixed6(constants.kv_bytes_per_token)));
    const std::string output =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output)
            .generic_string();
    const auto plan =
        inferlab::disaggregated_serving::build_plan(constants);
    inferlab_plan::write_or_print(
        output_path,
        inferlab::disaggregated_serving::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
