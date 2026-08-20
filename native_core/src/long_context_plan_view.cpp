#include "long_context_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_kv_budget_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::long_context_plan {
namespace {

std::string scenario_json(const LongContextScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "long context scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"chunk_size\": " << scenario.chunk_size << ",\n";
  out << nested << "\"decode_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.decode_ms_per_token << ",\n";
  out << nested << "\"kv_bytes_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.kv_bytes_per_token << ",\n";
  out << nested << "\"kv_budget_gib\": " << std::fixed
      << std::setprecision(6) << scenario.kv_budget_gib << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"output_tokens\": " << scenario.output_tokens
      << ",\n";
  out << nested << "\"prefix_cache_hit_rate\": " << std::fixed
      << std::setprecision(6) << scenario.prefix_cache_hit_rate << ",\n";
  out << nested << "\"prefill_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.prefill_ms_per_token << ",\n";
  out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const LongContextResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "long context record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"chunk_count\": " << result.chunk_count << ",\n";
  out << nested << "\"chunked_prefill_ms\": " << std::fixed
      << std::setprecision(6) << result.chunked_prefill_ms << ",\n";
  out << nested << "\"decode_ms\": " << std::fixed
      << std::setprecision(6) << result.decode_ms << ",\n";
  out << nested << "\"full_prefill_ms\": " << std::fixed
      << std::setprecision(6) << result.full_prefill_ms << ",\n";
  out << nested << "\"kv_cache_gb\": " << std::fixed
      << std::setprecision(6) << result.kv_cache_gb << ",\n";
  out << nested << "\"prefix_cache_saved_ms\": " << std::fixed
      << std::setprecision(6) << result.prefix_cache_saved_ms << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"ttft_ms\": " << std::fixed
      << std::setprecision(6) << result.ttft_ms << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& max_kv =
      plan.records[plan.summary.max_kv_cache_index];
  const auto& best_ttft =
      plan.records[plan.summary.best_ttft_index];
  const auto kv_budget_profile = release_kv_budget_binding(plan);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_long_context_matrix\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"native long-context planning artifact only; does not run benchmarks, external products, or target-environment execution\",\n";
  out << "  \"release_kv_budget_profile\": "
      << inferlab_release_kv_budget::binding_json(kv_budget_profile, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_ttft_ms\": " << std::fixed
      << std::setprecision(6) << best_ttft.ttft_ms << ",\n";
  out << "    \"max_kv_cache_gb\": " << std::fixed
      << std::setprecision(6) << max_kv.kv_cache_gb << ",\n";
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
  out << "  ],\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::long_context_plan
