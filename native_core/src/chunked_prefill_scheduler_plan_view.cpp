#include "chunked_prefill_scheduler_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_kv_budget_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::chunked_prefill_scheduler_plan {
namespace {

std::string scenario_json(
    const ChunkedPrefillSchedulerScenario& scenario,
    const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "chunked prefill scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"chunk_launch_overhead_ms\": " << std::fixed
      << std::setprecision(6) << scenario.chunk_launch_overhead_ms << ",\n";
  out << nested << "\"chunk_size\": " << scenario.chunk_size << ",\n";
  out << nested << "\"concurrent_decode_streams\": "
      << scenario.concurrent_decode_streams << ",\n";
  out << nested << "\"decode_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.decode_ms_per_token << ",\n";
  out << nested << "\"kv_budget_gb\": " << std::fixed
      << std::setprecision(6) << scenario.kv_budget_gb << ",\n";
  out << nested << "\"kv_bytes_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.kv_bytes_per_token << ",\n";
  out << nested << "\"max_prefill_batch_tokens\": "
      << scenario.max_prefill_batch_tokens << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"output_tokens\": " << scenario.output_tokens << ",\n";
  out << nested << "\"prefix_cache_hit_rate\": " << std::fixed
      << std::setprecision(6) << scenario.prefix_cache_hit_rate << ",\n";
  out << nested << "\"prefill_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.prefill_ms_per_token << ",\n";
  out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens << ",\n";
  out << nested << "\"scheduler_tick_ms\": " << std::fixed
      << std::setprecision(6) << scenario.scheduler_tick_ms << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const ChunkedPrefillSchedulerResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "chunked prefill record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  const auto timeline_pad = inferlab_plan::json_indent(
      internal::checked_add_int(
          nested_indent, 2, "chunked prefill timeline JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"chunk_count\": " << result.chunk_count << ",\n";
  out << nested << "\"chunk_policy\": "
      << inferlab_plan::json_string(result.chunk_policy) << ",\n";
  out << nested << "\"decode_maximal_batch_size\": "
      << result.decode_maximal_batch_size << ",\n";
  out << nested << "\"decode_starvation_risk\": " << std::fixed
      << std::setprecision(6) << result.decode_starvation_risk << ",\n";
  out << nested << "\"decode_stall_ms\": " << std::fixed
      << std::setprecision(6) << result.decode_stall_ms << ",\n";
  out << nested << "\"effective_prompt_tokens\": "
      << result.effective_prompt_tokens << ",\n";
  out << nested << "\"first_chunk_ttft_ms\": " << std::fixed
      << std::setprecision(6) << result.first_chunk_ttft_ms << ",\n";
  out << nested << "\"jain_fairness_estimate\": " << std::fixed
      << std::setprecision(6) << result.jain_fairness_estimate << ",\n";
  out << nested << "\"kv_budget_utilization\": " << std::fixed
      << std::setprecision(6) << result.kv_budget_utilization << ",\n";
  out << nested << "\"kv_cache_gb\": " << std::fixed
      << std::setprecision(6) << result.kv_cache_gb << ",\n";
  out << nested << "\"long_context_pressure_score\": " << std::fixed
      << std::setprecision(6) << result.long_context_pressure_score << ",\n";
  out << nested << "\"p99_risk_score\": " << std::fixed
      << std::setprecision(6) << result.p99_risk_score << ",\n";
  out << nested << "\"prefill_waves\": " << result.prefill_waves << ",\n";
  out << nested << "\"prefill_chunk_timeline\": [\n";
  for (std::size_t index = 0U;
       index < result.prefill_chunk_timeline.size(); ++index) {
    out << timeline_pad
        << inferlab_plan::json_string(
               result.prefill_chunk_timeline[index])
        << (index == result.prefill_chunk_timeline.size() - 1U
                ? "\n"
                : ",\n");
  }
  out << nested << "],\n";
  out << nested << "\"prefix_cache_saved_ms\": " << std::fixed
      << std::setprecision(6) << result.prefix_cache_saved_ms << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation) << ",\n";
  out << nested << "\"sarathi_stall_free_score\": " << std::fixed
      << std::setprecision(6) << result.sarathi_stall_free_score << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"scheduler_gap_ms\": " << std::fixed
      << std::setprecision(6) << result.scheduler_gap_ms << ",\n";
  out << nested << "\"throughput_efficiency\": " << std::fixed
      << std::setprecision(6) << result.throughput_efficiency << ",\n";
  out << nested << "\"total_prefill_ms\": " << std::fixed
      << std::setprecision(6) << result.total_prefill_ms << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& best_ttft = plan.records[plan.summary.best_ttft_index];
  const auto& best_stall =
      plan.records[plan.summary.best_stall_free_index];
  const auto& max_pressure =
      plan.records[plan.summary.max_pressure_index];
  const auto kv_budget_profile = release_kv_budget_binding(plan);
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_chunked_prefill_scheduler_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"native chunked-prefill scheduler planning artifact only; does not run benchmarks, external products, or target-environment execution\",\n";
  out << "  \"release_kv_budget_profile\": "
      << inferlab_release_kv_budget::binding_json(kv_budget_profile, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_first_chunk_ttft_ms\": " << std::fixed
      << std::setprecision(6) << best_ttft.first_chunk_ttft_ms << ",\n";
  out << "    \"best_sarathi_stall_free_scenario\": "
      << inferlab_plan::json_string(best_stall.scenario.name) << ",\n";
  out << "    \"best_sarathi_stall_free_score\": " << std::fixed
      << std::setprecision(6) << best_stall.sarathi_stall_free_score << ",\n";
  out << "    \"max_long_context_pressure_score\": " << std::fixed
      << std::setprecision(6) << max_pressure.long_context_pressure_score
      << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::chunked_prefill_scheduler_plan
