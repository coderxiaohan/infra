#include "scheduler_pressure_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_pressure_sweep {
namespace {

std::string scenario_json(const ScenarioResult& result,
                          const std::size_t index,
                          const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"cache_config\": "
      << probe::paged_kv_cache_config_json(result.config.cache_config, 6)
      << ",\n";
  out << "      \"max_active_requests\": "
      << result.config.max_active_requests << ",\n";
  out << "      \"name\": "
      << inferlab_plan::json_string(result.config.name) << ",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6) << ",\n";
  out << "      \"requests\": "
      << probe::request_lifecycle_metrics_json(result.requests, 6) << ",\n";
  out << "      \"simulation_step_budget\": "
      << result.step_budget << "\n";
  out << "    }" << (index + 1U == count ? "\n" : ",\n");
  return out.str();
}

std::string tradeoff_json(const SweepPlan& plan) {
  const auto& summary = plan.tradeoff;
  const auto& throughput =
      plan.results[summary.best_tokens_per_step_index];
  const auto& ttft =
      plan.results[summary.lowest_average_ttft_index];
  const auto& pressure =
      plan.results[summary.highest_peak_used_blocks_index];
  const auto& rejected =
      plan.results[summary.highest_rejected_index];
  std::ostringstream out;
  out << "{\n";
  out << "    \"best_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << throughput.profile.tokens_per_step << ",\n";
  out << "    \"best_tokens_per_step_scenario\": "
      << inferlab_plan::json_string(throughput.config.name) << ",\n";
  out << "    \"highest_peak_used_blocks\": "
      << pressure.profile.peak_used_blocks << ",\n";
  out << "    \"highest_peak_used_blocks_scenario\": "
      << inferlab_plan::json_string(pressure.config.name) << ",\n";
  out << "    \"lowest_average_ttft_scenario\": "
      << inferlab_plan::json_string(ttft.config.name) << ",\n";
  out << "    \"lowest_average_ttft_rejection_filter\": "
      << inferlab_plan::json_string(
             summary.lowest_ttft_filtered_to_no_rejection
                 ? "no_rejections"
                 : "all_scenarios")
      << ",\n";
  out << "    \"lowest_average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << ttft.profile.average_ttft_steps << ",\n";
  out << "    \"max_rejected_request_scenario\": "
      << inferlab_plan::json_string(rejected.config.name) << ",\n";
  out << "    \"max_rejected_requests\": "
      << rejected.profile.rejected_request_count << ",\n";
  out << "    \"scenario_count\": " << plan.results.size() << ",\n";
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
      out, "native_scheduler_pressure_sweep",
      "Executed deterministic scalar CPU scheduler pressure scenarios over "
      "a synthetic workload only; not measured product latency, throughput, "
      "capacity, or GPU runtime evidence.");
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"scenarios\": [\n";
  for (std::size_t index = 0U; index < plan.results.size(); ++index) {
    out << scenario_json(plan.results[index], index, plan.results.size());
  }
  out << "  ],\n";
  out << "  \"tradeoff_summary\": " << tradeoff_json(plan) << ",\n";
  out << "  \"workload\": "
      << probe::inference_workload_json(plan.workload, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_pressure_sweep
