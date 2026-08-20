#include "scheduler_fairness_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_fairness {
namespace {

std::string spaces(const int count) {
  return probe::probe_json_indent(count);
}

std::string fairness_json(const FairnessMetrics& fairness,
                          const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"average_queue_wait_steps\": " << std::fixed
      << std::setprecision(6) << fairness.average_queue_wait_steps << ",\n";
  out << nested << "\"average_total_latency_steps\": " << std::fixed
      << std::setprecision(6) << fairness.average_total_latency_steps
      << ",\n";
  out << nested << "\"jain_queue_wait_fairness\": " << std::fixed
      << std::setprecision(6) << fairness.jain_queue_wait_fairness << ",\n";
  out << nested << "\"max_queue_wait_steps\": "
      << fairness.max_queue_wait_steps << ",\n";
  out << nested << "\"max_total_latency_steps\": " << std::fixed
      << std::setprecision(6) << fairness.max_total_latency_steps << ",\n";
  out << nested << "\"queue_wait_stddev_steps\": " << std::fixed
      << std::setprecision(6) << fairness.queue_wait_stddev_steps << ",\n";
  out << nested << "\"starvation_threshold_steps\": "
      << fairness.starvation_threshold_steps << ",\n";
  out << nested << "\"starved_request_count\": "
      << fairness.starved_request_count << ",\n";
  out << nested << "\"worst_queue_wait_request_id\": "
      << fairness.worst_queue_wait_request_id << "\n";
  out << pad << "}";
  return out.str();
}

std::string workload_json(const std::vector<ArrivalRequest>& workload,
                          const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < workload.size(); ++i) {
    const auto& item = workload[i];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"max_decode_tokens\": "
        << item.request.max_decode_tokens << ",\n";
    out << spaces(indent + 4) << "\"prompt_tokens\": "
        << item.request.prompt_tokens << ",\n";
    out << spaces(indent + 4) << "\"request_id\": "
        << item.request.request_id << ",\n";
    out << spaces(indent + 4) << "\"submit_step\": " << item.submit_step
        << ",\n";
    out << spaces(indent + 4) << "\"total_tokens\": "
        << probe::inference_request_total_tokens(item.request) << "\n";
    out << nested << "}" << (i + 1U == workload.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string policy_result_json(const PolicyResult& result,
                               const std::size_t index,
                               const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"fairness\": " << fairness_json(result.fairness, 6)
      << ",\n";
  out << "      \"policy\": \"" << policy_name(result.policy) << "\",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6) << ",\n";
  out << "      \"request_metrics\": "
      << probe::request_lifecycle_metrics_json(
             result.lifecycle_metrics, 6)
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
  const auto& starvation =
      plan.results.at(summary.lowest_starvation_index);
  const auto& worst =
      plan.results.at(summary.worst_starvation_index);
  const auto& jain =
      plan.results.at(summary.highest_jain_fairness_index);
  const auto& balanced =
      plan.results.at(summary.balanced_policy_index);
  std::ostringstream out;
  out << "{\n";
  out << "    \"balanced_jain_floor\": " << std::fixed
      << std::setprecision(6) << summary.balanced_jain_floor << ",\n";
  out << "    \"balanced_policy\": \"" << policy_name(balanced.policy)
      << "\",\n";
  out << "    \"balanced_policy_average_queue_wait_steps\": " << std::fixed
      << std::setprecision(6)
      << balanced.fairness.average_queue_wait_steps << ",\n";
  out << "    \"balanced_policy_jain_queue_wait_fairness\": " << std::fixed
      << std::setprecision(6)
      << balanced.fairness.jain_queue_wait_fairness << ",\n";
  out << "    \"best_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << throughput.profile.tokens_per_step << ",\n";
  out << "    \"best_tokens_per_step_policy\": \""
      << policy_name(throughput.policy) << "\",\n";
  out << "    \"lowest_average_ttft_policy\": \""
      << policy_name(ttft.policy) << "\",\n";
  out << "    \"lowest_average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << ttft.profile.average_ttft_steps << ",\n";
  out << "    \"highest_jain_fairness_policy\": \""
      << policy_name(jain.policy) << "\",\n";
  out << "    \"highest_jain_queue_wait_fairness\": " << std::fixed
      << std::setprecision(6) << jain.fairness.jain_queue_wait_fairness
      << ",\n";
  out << "    \"lowest_starvation_policy\": \""
      << policy_name(starvation.policy) << "\",\n";
  out << "    \"lowest_starved_request_count\": "
      << starvation.fairness.starved_request_count << ",\n";
  out << "    \"lowest_starvation_max_queue_wait_steps\": "
      << starvation.fairness.max_queue_wait_steps << ",\n";
  out << "    \"policy_count\": " << plan.results.size() << ",\n";
  out << "    \"worst_starvation_policy\": \""
      << policy_name(worst.policy) << "\",\n";
  out << "    \"worst_starved_request_count\": "
      << worst.fairness.starved_request_count << ",\n";
  out << "    \"worst_starvation_max_queue_wait_steps\": "
      << worst.fairness.max_queue_wait_steps << "\n";
  out << "  }";
  return out.str();
}

std::string scheduler_config_json(const SweepPlan& plan) {
  std::ostringstream out;
  out << "{\n";
  out << "    \"block_size_tokens\": "
      << plan.cache_config.block_size_tokens << ",\n";
  out << "    \"bytes_per_token\": " << plan.cache_config.bytes_per_token
      << ",\n";
  out << "    \"max_active_requests\": " << plan.max_active_requests
      << ",\n";
  out << "    \"num_blocks\": " << plan.cache_config.num_blocks << ",\n";
  out << "    \"starvation_guard_threshold_steps\": "
      << plan.starvation_guard_threshold_steps << ",\n";
  out << "    \"starvation_threshold_steps\": "
      << plan.starvation_threshold_steps << "\n";
  out << "  }";
  return out.str();
}

}  // namespace

std::string artifact_json(const SweepPlan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_scheduler_fairness_sweep",
      "Executed deterministic scalar CPU fairness-policy scheduler "
      "simulation over synthetic arrivals only; not measured product "
      "fairness, latency, throughput, capacity, or GPU runtime evidence.");
  out << "  \"arrival_workload\": " << workload_json(plan.workload, 2)
      << ",\n";
  out << "  \"policy_results\": [\n";
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    out << policy_result_json(plan.results[i], i, plan.results.size());
  }
  out << "  ],\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"scheduler_config\": " << scheduler_config_json(plan)
      << ",\n";
  out << "  \"tradeoff_summary\": " << tradeoff_summary_json(plan) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_fairness
