#include "lora_adapter_cache_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_lora_prefix_isolation_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::lora_adapter_cache_plan {
namespace {

std::string scenario_json(const LoraAdapterCacheScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "LoRA adapter cache scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"active_adapters_per_minute\": "
      << scenario.active_adapters_per_minute << ",\n";
  out << nested << "\"adapter_count\": "
      << scenario.adapter_count << ",\n";
  out << nested << "\"adapter_load_ms\": " << std::fixed
      << std::setprecision(6) << scenario.adapter_load_ms << ",\n";
  out << nested << "\"adapter_size_mb\": " << std::fixed
      << std::setprecision(6) << scenario.adapter_size_mb << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"base_model_memory_gb\": " << std::fixed
      << std::setprecision(6) << scenario.base_model_memory_gb << ",\n";
  out << nested << "\"cache_slots\": " << scenario.cache_slots << ",\n";
  out << nested << "\"cross_adapter_prefix_sharing_allowed\": "
      << inferlab_plan::json_bool(
             scenario.cross_adapter_prefix_sharing_allowed)
      << ",\n";
  out << nested << "\"gpu_memory_gb\": " << std::fixed
      << std::setprecision(6) << scenario.gpu_memory_gb << ",\n";
  out << nested << "\"hot_adapter_fraction\": " << std::fixed
      << std::setprecision(6) << scenario.hot_adapter_fraction << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"prefix_cache_key_scope\": "
      << inferlab_plan::json_string(scenario.prefix_cache_key_scope)
      << ",\n";
  out << nested << "\"release_isolation_fixture\": "
      << inferlab_plan::json_bool(scenario.release_isolation_fixture)
      << ",\n";
  out << nested << "\"request_qps\": " << std::fixed
      << std::setprecision(6) << scenario.request_qps << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const LoraAdapterCacheResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "LoRA adapter cache record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"adapter_memory_mb\": " << std::fixed
      << std::setprecision(6) << result.adapter_memory_mb << ",\n";
  out << nested << "\"adapter_pressure_score\": " << std::fixed
      << std::setprecision(6) << result.adapter_pressure_score << ",\n";
  out << nested << "\"cache_hit_rate\": " << std::fixed
      << std::setprecision(6) << result.cache_hit_rate << ",\n";
  out << nested << "\"cache_policy\": "
      << inferlab_plan::json_string(result.cache_policy) << ",\n";
  out << nested << "\"evictions_per_minute\": " << std::fixed
      << std::setprecision(6) << result.evictions_per_minute << ",\n";
  out << nested << "\"gpu_memory_headroom_gb\": " << std::fixed
      << std::setprecision(6) << result.gpu_memory_headroom_gb << ",\n";
  out << nested << "\"load_penalty_p99_ms\": " << std::fixed
      << std::setprecision(6) << result.load_penalty_p99_ms << ",\n";
  out << nested << "\"mixed_batch_fragmentation_score\": " << std::fixed
      << std::setprecision(6)
      << result.mixed_batch_fragmentation_score << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation) << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto isolation_binding = release_isolation_binding(plan);
  const auto& best_hit =
      plan.records[plan.summary.best_cache_hit_index];
  const auto& max_pressure =
      plan.records[plan.summary.max_adapter_pressure_index];
  const auto& max_evictions =
      plan.records[plan.summary.max_evictions_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_lora_adapter_cache_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ native LoRA cache planning artifact only; does not run product backends, benchmarks, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_cache_hit_rate\": " << std::fixed
      << std::setprecision(6) << best_hit.cache_hit_rate << ",\n";
  out << "    \"best_cache_hit_scenario\": "
      << inferlab_plan::json_string(best_hit.scenario.name) << ",\n";
  out << "    \"max_adapter_pressure_scenario\": "
      << inferlab_plan::json_string(max_pressure.scenario.name) << ",\n";
  out << "    \"max_adapter_pressure_score\": " << std::fixed
      << std::setprecision(6) << max_pressure.adapter_pressure_score
      << ",\n";
  out << "    \"max_evictions_per_minute\": " << std::fixed
      << std::setprecision(6) << max_evictions.evictions_per_minute
      << ",\n";
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
  out << "  \"release_lora_prefix_isolation_profile\": "
      << inferlab_release_lora_prefix_isolation::binding_json(
             isolation_binding, 2)
      << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::lora_adapter_cache_plan
