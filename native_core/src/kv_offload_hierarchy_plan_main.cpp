#include "inferlab/advanced_serving_kv_tier.hpp"
#include "kv_offload_hierarchy_plan_view.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec& cli_options() {
  static const inferlab_plan::CliOptionSpec options{
      {"--repo-root", "--output", "--hbm-gb", "--dram-gb",
       "--nvme-gb", "--remote-gb"},
      {},
  };
  return options;
}

double capacity_arg(int argc, char** argv, const char* name,
                    const double fallback) {
  return inferlab_plan::parse_non_negative_double_arg(
      name, inferlab_plan::arg_value(
                argc, argv, name, inferlab_plan::fixed6(fallback)));
}

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_kv_offload_hierarchy_plan");
  try {
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, cli_options())) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_options());
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                            output_arg);
    const auto tiers = inferlab::default_kv_offload_tiers(
        capacity_arg(argc, argv, "--hbm-gb", 24.0),
        capacity_arg(argc, argv, "--dram-gb", 256.0),
        capacity_arg(argc, argv, "--nvme-gb", 2048.0),
        capacity_arg(argc, argv, "--remote-gb", 8192.0));
    const auto plan = inferlab::build_kv_offload_hierarchy_plan(
        inferlab::default_kv_offload_hierarchy_assumptions(), tiers,
        inferlab::default_kv_offload_workloads(),
        inferlab::kv_offload_policy_profiles());
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::kv_offload_hierarchy_plan_view::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, cli_options());
    return 1;
  }
  return 0;
}
