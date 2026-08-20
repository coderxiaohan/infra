#include "radix_prefix_cache_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_lora_prefix_isolation_profile_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::radix_prefix_cache_plan {
namespace {

std::string scenario_json(const RadixPrefixCacheScenario& scenario,
                          const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(
          indent, 2, "radix prefix cache scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"avg_prompt_tokens\": "
      << scenario.avg_prompt_tokens << ",\n";
  out << nested << "\"adapter_residency_limit\": "
      << scenario.adapter_residency_limit << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"cross_adapter_prefix_sharing_allowed\": "
      << inferlab_plan::json_bool(
             scenario.cross_adapter_prefix_sharing_allowed)
      << ",\n";
  out << nested << "\"kv_bytes_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.kv_bytes_per_token << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"node_overhead_bytes\": " << std::fixed
      << std::setprecision(6) << scenario.node_overhead_bytes << ",\n";
  out << nested << "\"prefix_reuse_rate\": " << std::fixed
      << std::setprecision(6) << scenario.prefix_reuse_rate << ",\n";
  out << nested << "\"prefill_ms_per_token\": " << std::fixed
      << std::setprecision(6) << scenario.prefill_ms_per_token << ",\n";
  out << nested << "\"prefix_cache_key_scope\": "
      << inferlab_plan::json_string(scenario.prefix_cache_key_scope)
      << ",\n";
  out << nested << "\"radix_lookup_us_per_node\": " << std::fixed
      << std::setprecision(6) << scenario.radix_lookup_us_per_node << ",\n";
  out << nested << "\"requests\": " << scenario.requests << ",\n";
  out << nested << "\"release_isolation_fixture\": "
      << inferlab_plan::json_bool(scenario.release_isolation_fixture)
      << ",\n";
  out << nested << "\"shared_prefix_tokens\": "
      << scenario.shared_prefix_tokens << ",\n";
  out << nested << "\"unique_suffix_tokens\": "
      << scenario.unique_suffix_tokens << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const RadixPrefixCacheResult& result,
                        const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "radix prefix cache record JSON indent");
  const auto nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"block_sharing_ratio\": " << std::fixed
      << std::setprecision(6) << result.block_sharing_ratio << ",\n";
  out << nested << "\"cache_efficiency_score\": " << std::fixed
      << std::setprecision(6) << result.cache_efficiency_score << ",\n";
  out << nested << "\"cache_hit_table\": {\n";
  out << nested << "  \"exact_prefix_hit_rate\": " << std::fixed
      << std::setprecision(6) << result.exact_prefix_hit_rate << ",\n";
  out << nested << "  \"hash_prefix_hit_rate\": " << std::fixed
      << std::setprecision(6) << result.hash_prefix_hit_rate << ",\n";
  out << nested << "  \"radix_prefix_hit_rate\": " << std::fixed
      << std::setprecision(6) << result.radix_prefix_hit_rate << "\n";
  out << nested << "},\n";
  out << nested << "\"eviction_cost_ms\": " << std::fixed
      << std::setprecision(6) << result.eviction_cost_ms << ",\n";
  out << nested << "\"estimated_nodes\": " << result.estimated_nodes
      << ",\n";
  out << nested << "\"fragmentation_ratio\": " << std::fixed
      << std::setprecision(6) << result.fragmentation_ratio << ",\n";
  out << nested << "\"lookup_overhead_ms\": " << std::fixed
      << std::setprecision(6) << result.lookup_overhead_ms << ",\n";
  out << nested << "\"net_ttft_saving_ms\": " << std::fixed
      << std::setprecision(6) << result.net_ttft_saving_ms << ",\n";
  out << nested << "\"paged_kv_blocks\": {\n";
  out << nested << "  \"shared_blocks\": " << result.shared_blocks << ",\n";
  out << nested << "  \"suffix_blocks\": " << result.suffix_blocks << ",\n";
  out << nested << "  \"split_count\": " << result.split_count << "\n";
  out << nested << "},\n";
  out << nested << "\"radix_metadata_mb\": " << std::fixed
      << std::setprecision(6) << result.radix_metadata_mb << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation) << ",\n";
  out << nested << "\"reused_tokens\": " << std::fixed
      << std::setprecision(6) << result.reused_tokens << ",\n";
  out << nested << "\"saved_kv_cache_gb\": " << std::fixed
      << std::setprecision(6) << result.saved_kv_cache_gb << ",\n";
  out << nested << "\"saved_prefill_ms\": " << std::fixed
      << std::setprecision(6) << result.saved_prefill_ms << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"tree_snapshot\": {\n";
  out << nested << "  \"root\": \"system_prompt\",\n";
  out << nested << "  \"shared_prefix_tokens\": "
      << result.scenario.shared_prefix_tokens << ",\n";
  out << nested << "  \"branches\": " << result.split_count << ",\n";
  out << nested << "  \"leaf_suffix_tokens\": "
      << result.scenario.unique_suffix_tokens << "\n";
  out << nested << "}\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto isolation_binding = release_isolation_binding(plan);
  const auto& max_efficiency =
      plan.records[plan.summary.max_efficiency_index];
  const auto& max_nodes = plan.records[plan.summary.max_nodes_index];
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_radix_prefix_cache_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ native radix/prefix cache planning artifact only; does not run product backends, benchmarks, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  out << "    \"max_efficiency_score\": " << std::fixed
      << std::setprecision(6) << max_efficiency.cache_efficiency_score
      << ",\n";
  out << "    \"max_efficiency_scenario\": "
      << inferlab_plan::json_string(max_efficiency.scenario.name) << ",\n";
  out << "    \"max_node_count\": " << max_nodes.estimated_nodes << ",\n";
  out << "    \"max_node_scenario\": "
      << inferlab_plan::json_string(max_nodes.scenario.name) << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], 4)
          << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
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

}  // namespace inferlab::radix_prefix_cache_plan
