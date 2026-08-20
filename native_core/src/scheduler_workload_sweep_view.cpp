#include "scheduler_workload_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_workload_sweep {
namespace {

std::string workload_result_json(const WorkloadResult& result,
                                 const std::size_t index,
                                 const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"name\": "
      << inferlab_plan::json_string(result.workload.name) << ",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6)
      << ",\n";
  out << "      \"prompt_to_decode_ratio\": " << std::fixed
      << std::setprecision(6) << result.prompt_to_decode_ratio << ",\n";
  out << "      \"prompt_tokens\": " << result.prompt_tokens << ",\n";
  out << "      \"requested_decode_tokens\": "
      << result.requested_decode_tokens << ",\n";
  out << "      \"request_count\": " << result.workload.requests.size()
      << ",\n";
  out << "      \"request_metrics\": "
      << probe::request_lifecycle_metrics_json(result.requests, 6)
      << ",\n";
  out << "      \"requests\": "
      << probe::inference_workload_json(result.workload.requests, 6)
      << ",\n";
  out << "      \"simulation_step_budget\": " << result.step_budget
      << "\n";
  out << "    }" << (index + 1U == count ? "\n" : ",\n");
  return out.str();
}

std::string tradeoff_summary_json(const SweepPlan& plan) {
  const auto& summary = plan.tradeoff;
  const auto& throughput =
      plan.results.at(summary.best_tokens_per_step_index);
  const auto& ttft =
      plan.results.at(summary.lowest_average_ttft_index);
  const auto& queue =
      plan.results.at(summary.highest_average_queue_wait_index);
  const auto& pressure =
      plan.results.at(summary.highest_peak_used_blocks_index);
  const auto& rejected =
      plan.results.at(summary.highest_rejected_index);
  std::ostringstream out;
  out << "{\n";
  out << "    \"best_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << throughput.profile.tokens_per_step << ",\n";
  out << "    \"best_tokens_per_step_workload\": "
      << inferlab_plan::json_string(throughput.workload.name) << ",\n";
  out << "    \"highest_average_queue_wait_steps\": " << std::fixed
      << std::setprecision(6) << queue.profile.average_queue_wait_steps
      << ",\n";
  out << "    \"highest_average_queue_wait_workload\": "
      << inferlab_plan::json_string(queue.workload.name) << ",\n";
  out << "    \"highest_peak_used_blocks\": "
      << pressure.profile.peak_used_blocks << ",\n";
  out << "    \"highest_peak_used_blocks_workload\": "
      << inferlab_plan::json_string(pressure.workload.name) << ",\n";
  out << "    \"lowest_average_ttft_rejection_filter\": \""
      << (summary.lowest_ttft_filtered_to_no_rejection
              ? "no_rejections"
              : "all_workloads")
      << "\",\n";
  out << "    \"lowest_average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << ttft.profile.average_ttft_steps << ",\n";
  out << "    \"lowest_average_ttft_workload\": "
      << inferlab_plan::json_string(ttft.workload.name) << ",\n";
  out << "    \"max_rejected_request_workload\": "
      << inferlab_plan::json_string(rejected.workload.name) << ",\n";
  out << "    \"max_rejected_requests\": "
      << rejected.profile.rejected_request_count << ",\n";
  out << "    \"total_rejected_requests\": "
      << summary.total_rejected_requests << ",\n";
  out << "    \"workload_count\": " << plan.results.size() << "\n";
  out << "  }";
  return out.str();
}

}  // namespace

std::string artifact_json(const SweepPlan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_scheduler_workload_sweep",
      "Executed deterministic scalar CPU scheduler workload sweep over "
      "synthetic request mixes only; not measured product latency, "
      "throughput, capacity, or GPU runtime evidence.");
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"scheduler_config\": "
      << probe::scheduler_config_json(
             plan.cache_config, plan.max_active_requests, 2)
      << ",\n";
  out << "  \"tradeoff_summary\": "
      << tradeoff_summary_json(plan) << ",\n";
  out << "  \"workloads\": [\n";
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    out << workload_result_json(
        plan.results[i], i, plan.results.size());
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_workload_sweep
