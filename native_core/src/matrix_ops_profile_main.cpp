#include "matrix_ops_profile_model.hpp"
#include "matrix_ops_profile_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <string>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--m", "--k", "--n", "--tile-m", "--tile-k",
     "--tile-n", "--softmax-rows", "--softmax-cols", "--output"},
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
    const inferlab::matrix_ops_profile::Request request{
        positive_arg(argc, argv, "--m", 4),
        positive_arg(argc, argv, "--k", 8),
        positive_arg(argc, argv, "--n", 6),
        positive_arg(argc, argv, "--tile-m", 2),
        positive_arg(argc, argv, "--tile-k", 4),
        positive_arg(argc, argv, "--tile-n", 3),
        positive_arg(argc, argv, "--softmax-rows", 4),
        positive_arg(argc, argv, "--softmax-cols", 6),
    };
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const std::string output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg)
            .generic_string();
    const auto plan = inferlab::matrix_ops_profile::build_plan(request);
    inferlab_plan::write_or_print(
        output_path, inferlab::matrix_ops_profile::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }
  return 0;
}
