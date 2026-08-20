#include "slo_capacity_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_traffic_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::slo_capacity_plan {
namespace {

std::string scenario_json(const SloScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "SLO capacity scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"output_tokens\": " << scenario.output_tokens
      << ",\n";
  out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
      << ",\n";
  out << nested << "\"qps\": " << std::fixed
      << std::setprecision(6) << scenario.qps << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const SloCapacityResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "SLO capacity record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"decode_replicas\": "
      << result.decode_replicas << ",\n";
  out << nested << "\"decode_tokens_per_second_required\": "
      << std::fixed << std::setprecision(6)
      << result.decode_tokens_per_second_required << ",\n";
  out << nested << "\"estimated_daily_cost\": " << std::fixed
      << std::setprecision(6) << result.estimated_daily_cost << ",\n";
  out << nested << "\"estimated_hourly_cost\": " << std::fixed
      << std::setprecision(6) << result.estimated_hourly_cost << ",\n";
  out << nested << "\"prefill_gpu_ms_per_request\": " << std::fixed
      << std::setprecision(6) << result.prefill_gpu_ms_per_request
      << ",\n";
  out << nested << "\"prefill_replicas\": "
      << result.prefill_replicas << ",\n";
  out << nested << "\"replica_utilization\": " << std::fixed
      << std::setprecision(6) << result.replica_utilization << ",\n";
  out << nested << "\"required_replicas\": "
      << result.required_replicas << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"tpot_slo_feasible\": "
      << inferlab_plan::json_bool(result.tpot_slo_feasible) << ",\n";
  out << nested << "\"ttft_slo_feasible\": "
      << inferlab_plan::json_bool(result.ttft_slo_feasible) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto traffic_profile = release_traffic_profile_binding(plan);
  const auto& max_replicas =
      plan.records[plan.summary.max_replicas_index];
  const auto& max_cost =
      plan.records[plan.summary.max_hourly_cost_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_slo_capacity_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"native SLO/capacity planning artifact only; does not run benchmarks, external products, or target-environment execution\",\n";
  out << "  \"release_traffic_profile\": "
      << inferlab_release_traffic::binding_json(traffic_profile, 2)
      << ",\n";
  out << "  \"constants\": {\n";
  out << "    \"decode_tokens_per_second_per_gpu\": " << std::fixed
      << std::setprecision(6)
      << plan.constants.decode_tokens_per_second_per_gpu << ",\n";
  out << "    \"gpu_hour_cost\": " << std::fixed
      << std::setprecision(6) << plan.constants.gpu_hour_cost << ",\n";
  out << "    \"prefill_ms_per_token\": " << std::fixed
      << std::setprecision(6) << plan.constants.prefill_ms_per_token
      << ",\n";
  out << "    \"tpot_slo_ms\": " << std::fixed
      << std::setprecision(6) << plan.constants.tpot_slo_ms << ",\n";
  out << "    \"ttft_slo_ms\": " << std::fixed
      << std::setprecision(6) << plan.constants.ttft_slo_ms << "\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"infeasible_scenarios\": "
      << plan.summary.infeasible_scenarios << ",\n";
  out << "    \"max_hourly_cost\": " << std::fixed
      << std::setprecision(6) << max_cost.estimated_hourly_cost << ",\n";
  out << "    \"max_required_replicas\": "
      << max_replicas.required_replicas << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    if (index != 0U) {
      out << ",\n";
    }
    out << record_json(plan.records[index], 4);
  }
  out << "\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::slo_capacity_plan
