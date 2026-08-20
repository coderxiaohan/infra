#include "scheduler_runtime_policy_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_runtime_policy {
namespace {

std::string spaces(const int count) {
  return probe::probe_json_indent(count);
}

std::string policy_config_json(const PolicyConfig& config,
                               const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"enable_backpressure\": "
      << inferlab_plan::json_bool(config.enable_backpressure) << ",\n";
  out << nested << "\"max_active_requests\": "
      << config.max_active_requests << ",\n";
  out << nested << "\"name\": \"" << config.name << "\",\n";
  out << nested << "\"preempt_on_append_pressure\": "
      << inferlab_plan::json_bool(config.preempt_on_append_pressure)
      << ",\n";
  out << nested << "\"reserve_free_blocks\": "
      << config.reserve_free_blocks << "\n";
  out << pad << "}";
  return out.str();
}

std::string request_json(const PolicyResult& result, const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < result.requests.size(); ++i) {
    const auto& request = result.requests[i];
    const auto& metrics = result.request_metrics[i];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"decode_steps_per_token\": "
        << std::fixed << std::setprecision(6)
        << metrics.decode_steps_per_token << ",\n";
    out << spaces(indent + 4) << "\"finished_step\": "
        << metrics.finished_step << ",\n";
    out << spaces(indent + 4) << "\"first_decode_step\": "
        << metrics.first_decode_step << ",\n";
    out << spaces(indent + 4) << "\"first_scheduled_step\": "
        << metrics.first_scheduled_step << ",\n";
    out << spaces(indent + 4) << "\"generated_tokens\": "
        << metrics.generated_tokens << ",\n";
    out << spaces(indent + 4) << "\"max_decode_tokens\": "
        << metrics.max_decode_tokens << ",\n";
    out << spaces(indent + 4) << "\"preemptions\": "
        << request.preemptions << ",\n";
    out << spaces(indent + 4) << "\"prompt_tokens\": "
        << metrics.prompt_tokens << ",\n";
    out << spaces(indent + 4) << "\"queue_wait_steps\": "
        << metrics.queue_wait_steps << ",\n";
    out << spaces(indent + 4) << "\"recompute_tokens\": "
        << request.recompute_tokens << ",\n";
    out << spaces(indent + 4) << "\"request_id\": "
        << request.request.request_id << ",\n";
    out << spaces(indent + 4) << "\"state\": \""
        << probe::request_state_name(metrics.state) << "\",\n";
    out << spaces(indent + 4) << "\"submitted_step\": "
        << metrics.submitted_step << ",\n";
    out << spaces(indent + 4) << "\"time_to_first_token_steps\": "
        << metrics.time_to_first_token_steps << ",\n";
    out << spaces(indent + 4) << "\"total_latency_steps\": "
        << metrics.total_latency_steps << "\n";
    out << nested << "}"
        << (i + 1U == result.requests.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string trace_json(const std::vector<RuntimeStepStats>& trace,
                       const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < trace.size(); ++i) {
    const auto& step = trace[i];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"admitted\": " << step.admitted
        << ",\n";
    out << spaces(indent + 4) << "\"append_pressure_events\": "
        << step.append_pressure_events << ",\n";
    out << spaces(indent + 4) << "\"backpressure_deferred\": "
        << step.backpressure_deferred << ",\n";
    out << spaces(indent + 4) << "\"cache\": "
        << probe::allocation_stats_json(step.cache_stats, indent + 4)
        << ",\n";
    out << spaces(indent + 4) << "\"decoded\": " << step.decoded << ",\n";
    out << spaces(indent + 4) << "\"finished\": " << step.finished
        << ",\n";
    out << spaces(indent + 4) << "\"preempted\": " << step.preempted
        << ",\n";
    out << spaces(indent + 4) << "\"rejected\": " << step.rejected
        << ",\n";
    out << spaces(indent + 4) << "\"step\": " << step.step << "\n";
    out << nested << "}" << (i + 1U == trace.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string policy_result_json(const PolicyResult& result,
                               const std::size_t index,
                               const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"append_pressure_events\": "
      << result.append_pressure_events << ",\n";
  out << "      \"max_preemptions_per_request\": "
      << result.max_preemptions_per_request << ",\n";
  out << "      \"policy_config\": "
      << policy_config_json(result.config, 6) << ",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6) << ",\n";
  out << "      \"preempted_request_count\": "
      << result.preempted_request_count << ",\n";
  out << "      \"request_metrics\": " << request_json(result, 6) << ",\n";
  out << "      \"runtime_trace\": " << trace_json(result.trace, 6)
      << ",\n";
  out << "      \"simulation_step_budget\": " << result.step_budget
      << ",\n";
  out << "      \"total_backpressure_deferrals\": "
      << result.total_backpressure_deferrals << ",\n";
  out << "      \"total_preemptions\": " << result.total_preemptions
      << ",\n";
  out << "      \"total_recompute_tokens\": "
      << result.total_recompute_tokens << "\n";
  out << "    }" << (index + 1U == count ? "\n" : ",\n");
  return out.str();
}

std::string tradeoff_summary_json(const SweepPlan& plan) {
  const auto& summary = plan.tradeoff;
  const auto& best_finished =
      plan.results.at(summary.best_finished_policy_index);
  const auto& throughput =
      plan.results.at(summary.best_tokens_per_step_index);
  const auto& ttft =
      plan.results.at(summary.lowest_average_ttft_index);
  const auto& preemptions =
      plan.results.at(summary.highest_preemption_index);
  const auto& deferrals =
      plan.results.at(summary.highest_backpressure_deferral_index);
  std::ostringstream out;
  out << "{\n";
  out << "    \"best_finished_policy\": \""
      << best_finished.config.name << "\",\n";
  out << "    \"best_finished_request_count\": "
      << best_finished.profile.finished_request_count << ",\n";
  out << "    \"best_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << throughput.profile.tokens_per_step << ",\n";
  out << "    \"best_tokens_per_step_policy\": \""
      << throughput.config.name << "\",\n";
  out << "    \"highest_backpressure_deferral_policy\": \""
      << deferrals.config.name << "\",\n";
  out << "    \"highest_backpressure_deferrals\": "
      << deferrals.total_backpressure_deferrals << ",\n";
  out << "    \"highest_preemption_policy\": \""
      << preemptions.config.name << "\",\n";
  out << "    \"highest_preemptions\": "
      << preemptions.total_preemptions << ",\n";
  out << "    \"lowest_average_ttft_policy\": \""
      << ttft.config.name << "\",\n";
  out << "    \"lowest_average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << ttft.profile.average_ttft_steps << ",\n";
  out << "    \"policy_count\": " << plan.results.size() << ",\n";
  out << "    \"total_backpressure_deferrals\": "
      << summary.total_backpressure_deferrals << ",\n";
  out << "    \"total_preemptions\": " << summary.total_preemptions
      << ",\n";
  out << "    \"total_recompute_tokens\": "
      << summary.total_recompute_tokens << "\n";
  out << "  }";
  return out.str();
}

}  // namespace

std::string artifact_json(const SweepPlan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_scheduler_runtime_policy_sweep",
      "Executed deterministic scalar CPU backpressure/preemption scheduler "
      "simulation over a synthetic workload only; not measured product "
      "latency, throughput, capacity, reliability, or GPU runtime evidence.");
  out << "  \"base_workload\": "
      << probe::inference_workload_json(plan.workload, 2) << ",\n";
  out << "  \"policy_results\": [\n";
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    out << policy_result_json(plan.results[i], i, plan.results.size());
  }
  out << "  ],\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"scheduler_config\": "
      << probe::paged_kv_cache_config_json(plan.cache_config, 2) << ",\n";
  out << "  \"tradeoff_summary\": " << tradeoff_summary_json(plan)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_runtime_policy
