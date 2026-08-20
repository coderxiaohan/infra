#include "radix_prefix_cache_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::radix_prefix_cache_plan {
namespace {

bool same_scenario(const RadixPrefixCacheScenario& lhs,
                   const RadixPrefixCacheScenario& rhs) {
  return lhs.name == rhs.name && lhs.backend == rhs.backend &&
         lhs.requests == rhs.requests &&
         lhs.avg_prompt_tokens == rhs.avg_prompt_tokens &&
         lhs.shared_prefix_tokens == rhs.shared_prefix_tokens &&
         lhs.unique_suffix_tokens == rhs.unique_suffix_tokens &&
         lhs.prefix_reuse_rate == rhs.prefix_reuse_rate &&
         lhs.node_overhead_bytes == rhs.node_overhead_bytes &&
         lhs.kv_bytes_per_token == rhs.kv_bytes_per_token &&
         lhs.prefill_ms_per_token == rhs.prefill_ms_per_token &&
         lhs.radix_lookup_us_per_node == rhs.radix_lookup_us_per_node &&
         lhs.adapter_residency_limit == rhs.adapter_residency_limit &&
         lhs.prefix_cache_key_scope == rhs.prefix_cache_key_scope &&
         lhs.cross_adapter_prefix_sharing_allowed ==
             rhs.cross_adapter_prefix_sharing_allowed &&
         lhs.release_isolation_fixture == rhs.release_isolation_fixture;
}

bool same_result(const RadixPrefixCacheResult& lhs,
                 const RadixPrefixCacheResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.estimated_nodes == rhs.estimated_nodes &&
         lhs.reused_tokens == rhs.reused_tokens &&
         lhs.saved_prefill_ms == rhs.saved_prefill_ms &&
         lhs.lookup_overhead_ms == rhs.lookup_overhead_ms &&
         lhs.radix_metadata_mb == rhs.radix_metadata_mb &&
         lhs.saved_kv_cache_gb == rhs.saved_kv_cache_gb &&
         lhs.net_ttft_saving_ms == rhs.net_ttft_saving_ms &&
         lhs.cache_efficiency_score == rhs.cache_efficiency_score &&
         lhs.recommendation == rhs.recommendation &&
         lhs.shared_blocks == rhs.shared_blocks &&
         lhs.suffix_blocks == rhs.suffix_blocks &&
         lhs.split_count == rhs.split_count &&
         lhs.exact_prefix_hit_rate == rhs.exact_prefix_hit_rate &&
         lhs.hash_prefix_hit_rate == rhs.hash_prefix_hit_rate &&
         lhs.radix_prefix_hit_rate == rhs.radix_prefix_hit_rate &&
         lhs.block_sharing_ratio == rhs.block_sharing_ratio &&
         lhs.fragmentation_ratio == rhs.fragmentation_ratio &&
         lhs.eviction_cost_ms == rhs.eviction_cost_ms;
}

void validate_scenarios(
    const std::vector<RadixPrefixCacheScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "radix prefix cache scenarios must not be empty");
  }

  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty()) {
      throw std::logic_error(
          "radix prefix cache scenario name must not be empty");
    }
    if (scenario.backend.empty()) {
      throw std::logic_error(
          "radix prefix cache scenario backend must not be empty");
    }
    if (scenario.release_isolation_fixture &&
        (scenario.adapter_residency_limit <= 0 ||
         scenario.prefix_cache_key_scope.empty() ||
         scenario.cross_adapter_prefix_sharing_allowed)) {
      throw std::logic_error(
          "release radix-prefix isolation fixture is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "radix prefix cache scenario names must be unique");
    }
  }
}

Summary summarize(const std::vector<RadixPrefixCacheResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "radix prefix cache records must not be empty");
  }

  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].cache_efficiency_score >
        records[result.max_efficiency_index].cache_efficiency_score) {
      result.max_efficiency_index = index;
    }
    if (records[index].estimated_nodes >
        records[result.max_nodes_index].estimated_nodes) {
      result.max_nodes_index = index;
    }
  }
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_radix_prefix_cache_scenarios();
  validate_scenarios(plan.scenarios);

  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_radix_prefix_cache(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "radix prefix cache plan scenario/record alignment is invalid");
  }
  if (plan.summary.max_efficiency_index >= plan.records.size() ||
      plan.summary.max_nodes_index >= plan.records.size()) {
    throw std::logic_error(
        "radix prefix cache summary index is out of range");
  }

  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_radix_prefix_cache(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       plan.scenarios[index]) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "radix prefix cache record is not the canonical estimate");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_efficiency_index !=
          expected_summary.max_efficiency_index ||
      plan.summary.max_nodes_index != expected_summary.max_nodes_index) {
    throw std::logic_error(
        "radix prefix cache summary is not canonical");
  }
}

inferlab_release_lora_prefix_isolation::Binding release_isolation_binding(
    const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_lora_prefix_isolation::Observation>
      observations;
  for (const auto& record : plan.records) {
    if (!record.scenario.release_isolation_fixture) continue;
    const bool compatible = record.scenario.adapter_residency_limit > 0 &&
        !record.scenario.prefix_cache_key_scope.empty() &&
        !record.scenario.cross_adapter_prefix_sharing_allowed &&
        record.scenario.shared_prefix_tokens > 0;
    observations.push_back({
        record.scenario.name, record.scenario.prefix_cache_key_scope,
        record.scenario.adapter_residency_limit,
        record.scenario.cross_adapter_prefix_sharing_allowed, compatible,
        compatible ? "radix_namespace_isolation_reconciled"
                   : "radix_namespace_isolation_incomplete"});
  }
  return inferlab_release_lora_prefix_isolation::bind_profile(
      "radix_prefix_cache", observations);
}

}  // namespace inferlab::radix_prefix_cache_plan
