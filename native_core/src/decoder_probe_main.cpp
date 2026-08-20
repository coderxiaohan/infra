#include "decoder_probe_model.hpp"
#include "decoder_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

using inferlab_plan::parse_standard_positive_int_arg;

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--seq-len", "--hidden-size", "--heads", "--ffn-size",
     "--output"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program =
      inferlab_plan::cli_program_name(argc, argv, "inferlab_decoder_probe");
  auto config = inferlab::decoder_probe::default_config();
  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    config.seq_len = parse_standard_positive_int_arg(
        "--seq-len", inferlab_plan::arg_value_with_spec(
                         argc, argv, "--seq-len",
                         std::to_string(config.seq_len), kCliOptions));
    config.hidden_size = parse_standard_positive_int_arg(
        "--hidden-size", inferlab_plan::arg_value_with_spec(
                             argc, argv, "--hidden-size",
                             std::to_string(config.hidden_size), kCliOptions));
    config.num_heads = parse_standard_positive_int_arg(
        "--heads", inferlab_plan::arg_value_with_spec(
                       argc, argv, "--heads",
                       std::to_string(config.num_heads), kCliOptions));
    config.ffn_intermediate_size = parse_standard_positive_int_arg(
        "--ffn-size", inferlab_plan::arg_value_with_spec(
                          argc, argv, "--ffn-size",
                          std::to_string(config.ffn_intermediate_size),
                          kCliOptions));
    const std::string output = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output).generic_string();
    const auto plan = inferlab::decoder_probe::build_plan(config);
    inferlab_plan::write_or_print(
        output_path, inferlab::decoder_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
