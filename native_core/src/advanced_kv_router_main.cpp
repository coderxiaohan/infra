#include "advanced_kv_router_artifact_model.hpp"
#include "advanced_kv_router_artifact_view.hpp"
#include "high_value_artifact_catalog.hpp"
#include "plan_json_utils.hpp"

#include <exception>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace {

const inferlab_plan::CliOptionSpec& router_cli_spec() {
  static const inferlab_plan::CliOptionSpec spec = [] {
    auto value = inferlab_native_trace::trace_context_cli_spec();
    value.value_options.push_back("--workers");
    inferlab_plan::validate_cli_option_spec(value);
    return value;
  }();
  return spec;
}

}  // namespace

int main(int argc, char** argv) {
  const char* program =
      argc > 0 && argv != nullptr && argv[0] != nullptr
          ? argv[0]
          : "inferlab";
  try {
    const auto& artifact_spec =
        inferlab_high_value::require_managed_artifact(
            inferlab::advanced_kv_router_artifact_view::kCatalogId);
    program = inferlab_plan::cli_program_name(
        argc, argv, artifact_spec.target.c_str());
    const auto& cli_spec = router_cli_spec();
    if (inferlab_plan::handle_cli_help(
            argc, argv, program, cli_spec)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_spec);
    const fs::path repo_root =
        inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    const int workers = inferlab_plan::parse_positive_int_arg(
        "--workers",
        inferlab_plan::arg_value(argc, argv, "--workers", "3"),
        inferlab::maximum_kv_aware_routing_workers());
    const fs::path output_arg =
        inferlab_plan::arg_value(argc, argv, "--output", "");
    const fs::path output_path =
        inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                           output_arg);
    auto trace_source =
        inferlab_native_trace::load_trace_context_source_from_cli(
            repo_root, argc, argv, cli_spec);
    const auto plan = inferlab::advanced_kv_router_artifact::build_plan(
        workers, inferlab::default_advanced_requests(),
        std::move(trace_source));
    inferlab_plan::write_or_print(
        output_path.generic_string(),
        inferlab::advanced_kv_router_artifact_view::artifact_json(plan));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(program, router_cli_spec());
    return 1;
  }
  return 0;
}
