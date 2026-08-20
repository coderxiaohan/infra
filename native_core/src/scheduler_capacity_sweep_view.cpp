#include "scheduler_capacity_sweep_view.hpp"

#include "plan_json_utils.hpp"
#include "scheduler_probe_json.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::scheduler_capacity_sweep {
namespace {

std::string capacity_result_json(const CapacityResult& result,
                                 const std::size_t index,
                                 const std::size_t count) {
  std::ostringstream out;
  out << "    {\n";
  out << "      \"num_blocks\": " << result.num_blocks << ",\n";
  out << "      \"pressure_profile\": "
      << probe::scheduler_pressure_profile_json(result.profile, 6)
      << ",\n";
  out << "      \"requests\": "
      << probe::request_lifecycle_metrics_json(result.requests, 6)
      << ",\n";
  out << "      \"simulation_step_budget\": " << result.step_budget
      << "\n";
  out << "    }" << (index + 1U == count ? "\n" : ",\n");
  return out.str();
}

std::string capacity_points_json(
    const std::vector<int>& capacity_points) {
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0U; i < capacity_points.size(); ++i) {
    out << capacity_points[i]
        << (i + 1U == capacity_points.size() ? "" : ", ");
  }
  out << "]";
  return out.str();
}

std::string threshold_summary_json(const SweepPlan& plan) {
  const auto& summary = plan.threshold;
  const auto& best_any =
      plan.results.at(summary.highest_tokens_per_step_index);
  const auto& saturated =
      plan.results.at(summary.highest_peak_cache_utilization_index);
  const auto& rejected =
      plan.results.at(summary.highest_rejected_index);
  const CapacityResult* first_ok = nullptr;
  const CapacityResult* best_ok = nullptr;
  if (summary.has_no_rejection) {
    first_ok = &plan.results.at(summary.minimum_no_rejection_index);
    best_ok = &plan.results.at(summary.best_no_rejection_index);
  }

  std::ostringstream out;
  out << "{\n";
  out << "    \"best_no_rejection_blocks\": "
      << (best_ok == nullptr ? -1 : best_ok->num_blocks) << ",\n";
  out << "    \"best_no_rejection_tokens_per_step\": ";
  if (best_ok == nullptr) {
    out << "0.000000";
  } else {
    out << std::fixed << std::setprecision(6)
        << best_ok->profile.tokens_per_step;
  }
  out << ",\n";
  out << "    \"capacity_point_count\": " << plan.results.size()
      << ",\n";
  out << "    \"highest_peak_cache_utilization\": " << std::fixed
      << std::setprecision(6)
      << saturated.profile.peak_cache_utilization << ",\n";
  out << "    \"highest_peak_cache_utilization_blocks\": "
      << saturated.num_blocks << ",\n";
  out << "    \"highest_tokens_per_step\": " << std::fixed
      << std::setprecision(6) << best_any.profile.tokens_per_step << ",\n";
  out << "    \"highest_tokens_per_step_blocks\": "
      << best_any.num_blocks << ",\n";
  out << "    \"max_rejected_blocks\": " << rejected.num_blocks << ",\n";
  out << "    \"max_rejected_requests\": "
      << rejected.profile.rejected_request_count << ",\n";
  out << "    \"minimum_no_rejection_blocks\": "
      << (first_ok == nullptr ? -1 : first_ok->num_blocks) << ",\n";
  out << "    \"minimum_no_rejection_tokens_per_step\": ";
  if (first_ok == nullptr) {
    out << "0.000000";
  } else {
    out << std::fixed << std::setprecision(6)
        << first_ok->profile.tokens_per_step;
  }
  out << ",\n";
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
      out, "native_scheduler_capacity_sweep",
      "Executed deterministic scalar CPU scheduler capacity sweep over a "
      "synthetic workload only; not measured product capacity, wall-clock "
      "latency, throughput, or GPU runtime evidence.");
  out << "  \"capacity_points\": [\n";
  for (std::size_t i = 0U; i < plan.results.size(); ++i) {
    out << capacity_result_json(
        plan.results[i], i, plan.results.size());
  }
  out << "  ],\n";
  out << "  \"sweep_config\": {\n";
  out << "    \"block_size_tokens\": " << plan.block_size_tokens << ",\n";
  out << "    \"bytes_per_token\": " << plan.bytes_per_token << ",\n";
  out << "    \"capacity_points\": "
      << capacity_points_json(plan.capacity_points) << ",\n";
  out << "    \"max_active_requests\": "
      << plan.max_active_requests << "\n";
  out << "  },\n";
  out << "  \"reference_simulation_only\": true,\n";
  out << "  \"threshold_summary\": "
      << threshold_summary_json(plan) << ",\n";
  out << "  \"workload\": "
      << probe::inference_workload_json(plan.workload, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_capacity_sweep
