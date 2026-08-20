#include "scheduler_admission_policy_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_admission_policy {
namespace {

std::string ordered_request_ids_json(
    const std::vector<InferenceRequest>& workload) {
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0U; i < workload.size(); ++i) {
    out << workload[i].request_id
        << (i + 1U == workload.size() ? "" : ", ");
  }
  out << "]";
  return out.str();
}

std::string policy_result_json(const PolicyResult& result,
                               const std::size_t index,
                               const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"ordered_request_ids\": "
      << ordered_request_ids_json(result.ordered_workload) << ",\n";
  out << "      \"ordered_workload\": "
      << probe::inference_workload_json(result.ordered_workload, 6)
      << ",\n";
  out << "      \"policy\": \"" << policy_name(result.policy) << "\",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6) << ",\n";
  out << "      \"request_metrics\": "
      << probe::request_lifecycle_metrics_json(result.requests, 6) << ",\n";
  out << "      \"simulation_step_budget\": " << result.step_budget
      << "\n";
  out << "    }" << (index + 1U == count ? "\n" : ",\n");
  return out.str();
}

std::string tradeoff_summary_json(const SweepPlan& plan) {
  const auto& summary = plan.tradeoff;
  const auto& best_finished =
      plan.results.at(summary.best_finished_policy_index);
  const auto& throughput =
      plan.results.at(summary.best_tokens_per_step_index);
  const auto& queue =
      plan.results.at(summary.lowest_average_queue_wait_index);
  const auto& ttft =
      plan.results.at(summary.lowest_average_ttft_index);
  const auto& rejected =
      plan.results.at(summary.highest_rejected_index);
  std::ostringstream out;
  out << "{\n";
  out << "    \"best_finished_policy\": \""
      << policy_name(best_finished.policy) << "\",\n";
  out << "    \"best_finished_request_count\": "
      << best_finished.profile.finished_request_count << ",\n";
  out << "    \"best_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << throughput.profile.tokens_per_step << ",\n";
  out << "    \"best_tokens_per_step_policy\": \""
      << policy_name(throughput.policy) << "\",\n";
  out << "    \"lowest_average_queue_wait_policy\": \""
      << policy_name(queue.policy) << "\",\n";
  out << "    \"lowest_average_queue_wait_steps\": " << std::fixed
      << std::setprecision(6) << queue.profile.average_queue_wait_steps
      << ",\n";
  out << "    \"lowest_average_ttft_policy\": \""
      << policy_name(ttft.policy) << "\",\n";
  out << "    \"lowest_average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << ttft.profile.average_ttft_steps << ",\n";
  out << "    \"max_rejected_policy\": \""
      << policy_name(rejected.policy) << "\",\n";
  out << "    \"max_rejected_requests\": "
      << rejected.profile.rejected_request_count << ",\n";
  out << "    \"policy_count\": " << plan.results.size() << ",\n";
  out << "    \"total_rejected_requests\": "
      << summary.total_rejected_requests << "\n";
  out << "  }";
  return out.str();
}

}  // namespace

std::string artifact_json(const SweepPlan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_scheduler_admission_policy_sweep",
      "Executed deterministic scalar CPU admission-order sweep over a "
      "synthetic workload only; not measured product scheduling, latency, "
      "throughput, capacity, or GPU runtime evidence.");
  out << "  \"base_workload\": "
      << probe::inference_workload_json(plan.workload, 2) << ",\n";
  out << "  \"policy_results\": [\n";
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    out << policy_result_json(plan.results[i], i, plan.results.size());
  }
  out << "  ],\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"scheduler_config\": "
      << probe::scheduler_config_json(
             plan.cache_config, plan.max_active_requests, 2)
      << ",\n";
  out << "  \"tradeoff_summary\": " << tradeoff_summary_json(plan)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_admission_policy
