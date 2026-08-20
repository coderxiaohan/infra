#include "inferlab/continuous_batch_scheduler.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace {

using inferlab_plan::parse_standard_positive_int_arg;

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--num-blocks", "--block-size-tokens",
     "--bytes-per-token", "--max-active-requests", "--output"},
    {},
};

std::string trace_json(
    const std::vector<inferlab::SchedulerStepStats>& trace,
    const std::vector<inferlab::RequestLifecycleMetrics>& requests,
    const inferlab::SchedulerPressureProfile& profile,
    const std::size_t step_budget) {
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_scheduler_trace",
      "Executed deterministic scalar CPU continuous-batching scheduler "
      "reference only; not product serving, wall-clock latency, throughput "
      "benchmark, or GPU runtime evidence.");
  out << "  \"pressure_profile\": "
      << inferlab::probe::scheduler_pressure_profile_json(profile, 2)
      << ",\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"requests\": "
      << inferlab::probe::request_lifecycle_metrics_json(requests, 2)
      << ",\n";
  out << "  \"simulation_step_budget\": " << step_budget << ",\n";
  out << "  \"steps\": [\n";
  for (std::size_t i = 0; i < trace.size(); ++i) {
    const auto& step = trace[i];
    out << "    {\n";
    out << "      \"admitted\": " << step.admitted << ",\n";
    out << "      \"backpressure_deferred\": "
        << step.backpressure_deferred << ",\n";
    out << "      \"cache\": "
        << inferlab::probe::allocation_stats_json(step.cache_stats, 6)
        << ",\n";
    out << "      \"decoded\": " << step.decoded << ",\n";
    out << "      \"finished\": " << step.finished << ",\n";
    out << "      \"rejected\": " << step.rejected << ",\n";
    out << "      \"step\": " << step.step << "\n";
    out << "    }" << (i + 1 == trace.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace

int main(int argc, char** argv) {
  inferlab::PagedKVCacheConfig cache_config{8, 8, 128};
  int max_active_requests = 2;
  std::string output_path;

  try {
    if (inferlab_plan::handle_cli_help(argc, argv, argv[0], kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    const std::string repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    cache_config.num_blocks = parse_standard_positive_int_arg(
        "--num-blocks", inferlab_plan::arg_value_with_spec(
                            argc, argv, "--num-blocks",
                            std::to_string(cache_config.num_blocks),
                            kCliOptions));
    cache_config.block_size_tokens = parse_standard_positive_int_arg(
        "--block-size-tokens", inferlab_plan::arg_value_with_spec(
                                   argc, argv, "--block-size-tokens",
                                   std::to_string(cache_config.block_size_tokens),
                                   kCliOptions));
    cache_config.bytes_per_token = parse_standard_positive_int_arg(
        "--bytes-per-token", inferlab_plan::arg_value_with_spec(
                                 argc, argv, "--bytes-per-token",
                                 std::to_string(cache_config.bytes_per_token),
                                 kCliOptions));
    max_active_requests = parse_standard_positive_int_arg(
        "--max-active-requests", inferlab_plan::arg_value_with_spec(
                                     argc, argv, "--max-active-requests",
                                     std::to_string(max_active_requests),
                                     kCliOptions));
    const std::string output_arg = inferlab_plan::arg_value_with_spec(
        argc, argv, "--output", "", kCliOptions);
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .generic_string();

    const std::vector<inferlab::InferenceRequest> workload = {
        {1, 6, 3},
        {2, 9, 2},
        {3, 4, 4},
    };
    const auto execution = inferlab::probe::run_continuous_scheduler_probe(
        cache_config, max_active_requests, workload);
    const auto payload = trace_json(execution.trace, execution.requests,
                                    execution.profile,
                                    execution.step_budget);
    inferlab_plan::write_or_print(output_path, payload);
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(error);
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }

  return 0;
}
