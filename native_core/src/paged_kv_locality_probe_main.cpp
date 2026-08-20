#include "paged_kv_locality_probe_model.hpp"
#include "paged_kv_locality_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--tokens", "--num-heads", "--head-dim",
     "--block-size-tokens", "--output"},
    {},
};

int positive_arg(int argc, char** argv, const char* name,
                 const int fallback) {
  return inferlab_plan::parse_standard_positive_int_arg(
      name, inferlab_plan::arg_value_with_spec(
                argc, argv, name, std::to_string(fallback), kCliOptions));
}

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_paged_kv_locality_probe");
  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const fs::path repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    const fs::path output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(
            repo_root, output_arg);

    inferlab::paged_kv_locality_probe::Request request;
    request.token_count = positive_arg(
        argc, argv, "--tokens", request.token_count);
    request.shape.num_heads = positive_arg(
        argc, argv, "--num-heads", request.shape.num_heads);
    request.shape.head_dim = positive_arg(
        argc, argv, "--head-dim", request.shape.head_dim);
    request.shape.block_size_tokens = positive_arg(
        argc, argv, "--block-size-tokens",
        request.shape.block_size_tokens);
    const auto plan =
        inferlab::paged_kv_locality_probe::build_plan(request);
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::paged_kv_locality_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
