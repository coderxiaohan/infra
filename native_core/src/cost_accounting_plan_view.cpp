#include "cost_accounting_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_traffic_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::cost_accounting_plan {
namespace {

std::string scenario_json(const CostAccountingScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "cost-accounting scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cache_hit_rate\": " << std::fixed
      << std::setprecision(6) << scenario.cache_hit_rate << ",\n";
  out << nested << "\"gpu_hour_cost\": " << std::fixed
      << std::setprecision(6) << scenario.gpu_hour_cost << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"output_tokens\": " << scenario.output_tokens
      << ",\n";
  out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
      << ",\n";
  out << nested << "\"replicas\": " << scenario.replicas << ",\n";
  out << nested << "\"request_qps\": " << std::fixed
      << std::setprecision(6) << scenario.request_qps << ",\n";
  out << nested << "\"tokens_per_second_per_replica\": " << std::fixed
      << std::setprecision(6)
      << scenario.tokens_per_second_per_replica << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const CostAccountingResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "cost-accounting record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"billable_tokens_per_second\": " << std::fixed
      << std::setprecision(6) << result.billable_tokens_per_second << ",\n";
  out << nested << "\"cost_per_million_tokens\": " << std::fixed
      << std::setprecision(6) << result.cost_per_million_tokens << ",\n";
  out << nested << "\"daily_cost\": " << std::fixed
      << std::setprecision(6) << result.daily_cost << ",\n";
  out << nested << "\"effective_tokens_per_second\": " << std::fixed
      << std::setprecision(6) << result.effective_tokens_per_second
      << ",\n";
  out << nested << "\"hourly_cost\": " << std::fixed
      << std::setprecision(6) << result.hourly_cost << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"utilization\": " << std::fixed
      << std::setprecision(6) << result.utilization << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto traffic_profile = release_traffic_profile_binding(plan);
  const auto& max_cost =
      plan.records[plan.summary.max_daily_cost_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_cost_accounting_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ native planning/estimation artifact only; does not run benchmarks, product backends, CUDA kernels, or target-environment execution\",\n";
  out << "  \"release_traffic_profile\": "
      << inferlab_release_traffic::binding_json(traffic_profile, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"max_daily_cost\": " << std::fixed
      << std::setprecision(6) << max_cost.daily_cost << ",\n";
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

}  // namespace inferlab::cost_accounting_plan
