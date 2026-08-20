#include "inferlab/shape_trace.hpp"

#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

using inferlab_plan::parse_standard_positive_int_arg;

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--batch-size", "--seq-len", "--decode-steps",
     "--hidden-size", "--num-layers", "--num-heads", "--head-dim",
     "--dtype", "--output"},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  inferlab::TransformerShapeConfig config;
  std::string output_path;

  try {
    if (inferlab_plan::handle_cli_help(argc, argv, argv[0], kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root =
        inferlab_plan::arg_value_with_spec(argc, argv, "--repo-root", ".",
                                           kCliOptions);
    config.batch_size =
        parse_standard_positive_int_arg(
            "--batch-size",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--batch-size", std::to_string(config.batch_size),
                kCliOptions));
    config.seq_len =
        parse_standard_positive_int_arg(
            "--seq-len",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--seq-len", std::to_string(config.seq_len),
                kCliOptions));
    config.decode_steps =
        parse_standard_positive_int_arg(
            "--decode-steps",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--decode-steps",
                std::to_string(config.decode_steps), kCliOptions));
    config.hidden_size =
        parse_standard_positive_int_arg(
            "--hidden-size",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--hidden-size",
                std::to_string(config.hidden_size), kCliOptions));
    config.num_layers =
        parse_standard_positive_int_arg(
            "--num-layers",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--num-layers", std::to_string(config.num_layers),
                kCliOptions));
    config.num_heads =
        parse_standard_positive_int_arg(
            "--num-heads",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--num-heads", std::to_string(config.num_heads),
                kCliOptions));
    config.head_dim =
        parse_standard_positive_int_arg(
            "--head-dim",
            inferlab_plan::arg_value_with_spec(
                argc, argv, "--head-dim", std::to_string(config.head_dim),
                kCliOptions));
    const std::string dtype_arg =
        inferlab_plan::arg_value_with_spec(argc, argv, "--dtype", "",
                                           kCliOptions);
    if (!dtype_arg.empty()) {
      config.dtype = inferlab::parse_dtype(dtype_arg);
    }
    const std::string output_arg =
        inferlab_plan::arg_value_with_spec(argc, argv, "--output", "",
                                           kCliOptions);
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .generic_string();

    const auto payload = inferlab::to_json(inferlab::build_trace(config));
    inferlab_plan::write_or_print(output_path, payload);
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }

  return 0;
}
