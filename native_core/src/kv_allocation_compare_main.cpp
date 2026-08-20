#include "kv_allocation_compare_model.hpp"
#include "kv_allocation_compare_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--max-requests", "--max-seq-len",
     "--block-size-tokens", "--bytes-per-token", "--output"},
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
    const inferlab::kv_allocation_compare::Config config{
        positive_arg(argc, argv, "--max-requests", 4),
        positive_arg(argc, argv, "--max-seq-len", 64),
        positive_arg(argc, argv, "--block-size-tokens", 8),
        positive_arg(argc, argv, "--bytes-per-token", 128),
    };
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root, output_arg)
            .generic_string();
    const auto plan = inferlab::kv_allocation_compare::build_plan(config);
    inferlab_plan::write_or_print(
        output_path, inferlab::kv_allocation_compare::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }
  return 0;
}
