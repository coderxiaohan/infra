#include "kv_fragmentation_probe_model.hpp"
#include "kv_fragmentation_probe_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--num-blocks", "--block-size-tokens",
     "--bytes-per-token", "--output"},
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
      argc, argv, "inferlab_kv_fragmentation_probe");
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

    inferlab::PagedKVCacheConfig config{12, 4, 128};
    config.num_blocks = positive_arg(
        argc, argv, "--num-blocks", config.num_blocks);
    config.block_size_tokens = positive_arg(
        argc, argv, "--block-size-tokens", config.block_size_tokens);
    config.bytes_per_token = positive_arg(
        argc, argv, "--bytes-per-token", config.bytes_per_token);
    const auto plan =
        inferlab::kv_fragmentation_probe::build_plan(config);
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::kv_fragmentation_probe::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
