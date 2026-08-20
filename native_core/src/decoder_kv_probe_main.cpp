#include "decoder_kv_probe_model.hpp"
#include "decoder_kv_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--prompt-tokens", "--decode-steps", "--hidden-size",
     "--heads", "--ffn-size", "--block-size-tokens", "--output"},
    {},
};

int positive_arg(int argc, char** argv, const char* name,
                 const int default_value) {
  return inferlab_plan::parse_standard_positive_int_arg(
      name, inferlab_plan::arg_value_with_spec(
                argc, argv, name, std::to_string(default_value), kCliOptions));
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, argv[0], kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    inferlab::DecoderBlockConfig config;
    config.seq_len = positive_arg(argc, argv, "--prompt-tokens", 5);
    config.hidden_size = positive_arg(argc, argv, "--hidden-size", 16);
    config.num_heads = positive_arg(argc, argv, "--heads", 4);
    config.ffn_intermediate_size =
        positive_arg(argc, argv, "--ffn-size", 24);
    const inferlab::decoder_kv_probe::Request request{
        config,
        positive_arg(argc, argv, "--decode-steps", 3),
        positive_arg(argc, argv, "--block-size-tokens", 4),
    };
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output_arg)
            .generic_string();
    const auto plan = inferlab::decoder_kv_probe::build_plan(request);
    inferlab_plan::write_or_print(
        output_path, inferlab::decoder_kv_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }
  return 0;
}
