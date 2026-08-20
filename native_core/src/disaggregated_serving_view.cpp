#include "disaggregated_serving_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::disaggregated_serving {
namespace {

std::string scenario_json(const DisaggregatedScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "disaggregated serving scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"arrival_qps\": " << std::fixed
      << std::setprecision(6) << scenario.arrival_qps << ",\n";
  out << nested << "\"bandwidth_gbps\": " << std::fixed
      << std::setprecision(6) << scenario.bandwidth_gbps << ",\n";
  out << nested << "\"decode_workers\": "
      << scenario.decode_workers << ",\n";
  out << nested << "\"output_tokens\": "
      << scenario.output_tokens << ",\n";
  out << nested << "\"prefill_workers\": "
      << scenario.prefill_workers << ",\n";
  out << nested << "\"prompt_tokens\": "
      << scenario.prompt_tokens << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const DisaggregatedResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "disaggregated serving record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"aggregate_baseline_ms\": " << std::fixed
      << std::setprecision(6) << result.aggregate_baseline_ms << ",\n";
  out << nested << "\"decode_ms\": " << std::fixed
      << std::setprecision(6) << result.decode_ms << ",\n";
  out << nested << "\"end_to_end_ms\": " << std::fixed
      << std::setprecision(6) << result.end_to_end_ms << ",\n";
  out << nested << "\"kv_transfer_ms\": " << std::fixed
      << std::setprecision(6) << result.kv_transfer_ms << ",\n";
  out << nested << "\"prefill_ms\": " << std::fixed
      << std::setprecision(6) << result.prefill_ms << ",\n";
  out << nested << "\"queue_penalty_ms\": " << std::fixed
      << std::setprecision(6) << result.queue_penalty_ms << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"speedup_vs_aggregate\": " << std::fixed
      << std::setprecision(6) << result.speedup_vs_aggregate << ",\n";
  out << nested << "\"tpot_ms\": " << std::fixed
      << std::setprecision(6) << result.tpot_ms << ",\n";
  out << nested << "\"ttft_ms\": " << std::fixed
      << std::setprecision(6) << result.ttft_ms << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& best_ttft =
      plan.records[plan.summary.best_ttft_index];
  const auto& best_speedup =
      plan.records[plan.summary.best_speedup_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"native disaggregated-serving planning artifact only; does not run benchmarks, external products, or target-environment execution\",\n";
  out << "  \"constants\": {\n";
  out << "    \"decode_ms_per_token\": " << std::fixed
      << std::setprecision(6)
      << plan.constants.decode_ms_per_token << ",\n";
  out << "    \"kv_bytes_per_token\": " << std::fixed
      << std::setprecision(6)
      << plan.constants.kv_bytes_per_token << ",\n";
  out << "    \"prefill_ms_per_token\": " << std::fixed
      << std::setprecision(6)
      << plan.constants.prefill_ms_per_token << "\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"best_speedup_vs_aggregate\": " << std::fixed
      << std::setprecision(6)
      << best_speedup.speedup_vs_aggregate << ",\n";
  out << "    \"best_ttft_ms\": " << std::fixed
      << std::setprecision(6) << best_ttft.ttft_ms << ",\n";
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

}  // namespace inferlab::disaggregated_serving
