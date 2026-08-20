#include "plan_json_utils.hpp"
#include "slo_capacity_plan_model.hpp"
#include "slo_capacity_plan_view.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--ttft-slo-ms", "--tpot-slo-ms",
     "--gpu-hour-cost", "--decode-tokens-per-second-per-gpu",
     "--prefill-ms-per-token", "--output"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_slo_capacity_plan");
  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);

    inferlab::SloConstants constants;
    constants.ttft_slo_ms = inferlab_plan::parse_positive_double_arg(
        "--ttft-slo-ms",
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--ttft-slo-ms",
            inferlab_plan::fixed6(constants.ttft_slo_ms), kCliOptions));
    constants.tpot_slo_ms = inferlab_plan::parse_positive_double_arg(
        "--tpot-slo-ms",
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--tpot-slo-ms",
            inferlab_plan::fixed6(constants.tpot_slo_ms), kCliOptions));
    constants.gpu_hour_cost =
        inferlab_plan::parse_non_negative_double_arg(
            "--gpu-hour-cost",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--gpu-hour-cost",
                inferlab_plan::fixed6(constants.gpu_hour_cost),
                kCliOptions));
    constants.decode_tokens_per_second_per_gpu =
        inferlab_plan::parse_positive_double_arg(
            "--decode-tokens-per-second-per-gpu",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--decode-tokens-per-second-per-gpu",
                inferlab_plan::fixed6(
                    constants.decode_tokens_per_second_per_gpu),
                kCliOptions));
    constants.prefill_ms_per_token =
        inferlab_plan::parse_non_negative_double_arg(
            "--prefill-ms-per-token",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--prefill-ms-per-token",
                inferlab_plan::fixed6(constants.prefill_ms_per_token),
                kCliOptions));

    const std::string root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    const std::string output = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(root, output)
            .generic_string();
    const auto plan = inferlab::slo_capacity_plan::build_plan(constants);
    inferlab_plan::write_or_print(
        output_path, inferlab::slo_capacity_plan::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
