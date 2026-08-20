#include "plan_json_utils.hpp"
#include "product_replay_semantic_events_fixture.hpp"
#include "product_replay_semantic_events_model.hpp"
#include "product_replay_semantic_events_view.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--results", "--sample", "--output", "--format",
     inferlab_trace_clock::clock_calibration_cli_option()},
    {},
};

}  // namespace

int main(int argc, char** argv) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, "inferlab_product_replay_semantic_events");
  std::filesystem::path repo_root = ".";
  std::filesystem::path results_root = "results/raw";
  std::filesystem::path output;
  std::vector<inferlab::product_replay_semantic_events::ReplaySampleRequest>
      sample_requests;
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, program, kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    repo_root = inferlab_plan::arg_value(argc, argv, "--repo-root", ".");
    results_root = inferlab_plan::resolve_repo_relative_path(
        repo_root,
        inferlab_plan::arg_value(argc, argv, "--results", "results/raw"));
    output = inferlab_plan::resolve_optional_repo_relative_path(
        repo_root, inferlab_plan::arg_value(argc, argv, "--output", ""));
    const std::string format =
        inferlab_plan::arg_value(argc, argv, "--format", "json");
    if (format != "json") {
      throw std::invalid_argument("unsupported --format value: " + format +
                                  "; expected json");
    }

    const auto explicit_samples =
        inferlab_plan::arg_values(argc, argv, "--sample");
    if (explicit_samples.empty()) {
      for (const auto& sample :
           inferlab::product_replay_semantic_events::default_sample_specs()) {
        sample_requests.push_back(
            {inferlab_plan::resolve_repo_relative_path(
                 repo_root, sample.repository_path),
             inferlab::product_replay_semantic_events::SampleInputOrigin::
                 default_fixture,
             sample.product_profile_id});
      }
    } else {
      for (const auto& sample : explicit_samples) {
        sample_requests.push_back(
            {inferlab_plan::resolve_repo_relative_path(repo_root, sample),
             inferlab::product_replay_semantic_events::SampleInputOrigin::
                 explicit_replay,
             ""});
      }
    }

    auto clock_calibration =
        inferlab_trace_clock::load_clock_calibration_source_from_cli(
            repo_root, argc, argv, kCliOptions);
    const auto plan =
        inferlab::product_replay_semantic_events::build_semantic_events_plan(
            results_root, sample_requests, std::move(clock_calibration));
    const inferlab::product_replay_semantic_events::
        SemanticEventsArtifactViewContext context{
            repo_root, results_root, output, format};
    inferlab_plan::write_or_print(
        output.empty() ? "" : output.string(),
        inferlab::product_replay_semantic_events::
            semantic_events_artifact_json(plan, context));
  } catch (const std::exception& error) {
    std::vector<std::string> known_paths = {
        repo_root.string(), results_root.string(), output.string()};
    for (const auto& sample : sample_requests) {
      known_paths.push_back(sample.path.string());
    }
    inferlab_plan::report_cli_error(error, "error: ", known_paths);
    inferlab_plan::print_cli_usage(program, kCliOptions);
    return 1;
  }
  return 0;
}
