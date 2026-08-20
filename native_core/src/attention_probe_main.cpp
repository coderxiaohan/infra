#include "attention_probe_model.hpp"
#include "attention_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--heads", "--query-length", "--key-length",
     "--head-dim", "--causal-profile-length", "--output"},
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
    const inferlab::AttentionShape shape{
        positive_arg(argc, argv, "--heads", 2),
        positive_arg(argc, argv, "--query-length", 4),
        positive_arg(argc, argv, "--key-length", 6),
        positive_arg(argc, argv, "--head-dim", 8),
    };
    const auto causal_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--causal-profile-length", "", kCliOptions);
    const int causal_length =
        causal_arg.empty()
            ? shape.query_length
            : inferlab_plan::parse_standard_positive_int_arg(
                  "--causal-profile-length", causal_arg);
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output_arg)
            .generic_string();
    const auto plan = inferlab::attention_probe::build_plan(
        {shape, causal_length});
    inferlab_plan::write_or_print(
        output_path, inferlab::attention_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }
  return 0;
}
