#include "lora_adapter_cache_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::lora_adapter_cache_plan {
namespace {

bool same_scenario(const LoraAdapterCacheScenario& lhs,
                   const LoraAdapterCacheScenario& rhs) {
  return lhs.name == rhs.name && lhs.backend == rhs.backend &&
         lhs.adapter_count == rhs.adapter_count &&
         lhs.active_adapters_per_minute ==
             rhs.active_adapters_per_minute &&
         lhs.cache_slots == rhs.cache_slots &&
         lhs.adapter_size_mb == rhs.adapter_size_mb &&
         lhs.base_model_memory_gb == rhs.base_model_memory_gb &&
         lhs.gpu_memory_gb == rhs.gpu_memory_gb &&
         lhs.adapter_load_ms == rhs.adapter_load_ms &&
         lhs.request_qps == rhs.request_qps &&
         lhs.hot_adapter_fraction == rhs.hot_adapter_fraction &&
         lhs.prefix_cache_key_scope == rhs.prefix_cache_key_scope &&
         lhs.cross_adapter_prefix_sharing_allowed ==
             rhs.cross_adapter_prefix_sharing_allowed &&
         lhs.release_isolation_fixture == rhs.release_isolation_fixture;
}

bool same_result(const LoraAdapterCacheResult& lhs,
                 const LoraAdapterCacheResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.adapter_memory_mb == rhs.adapter_memory_mb &&
         lhs.gpu_memory_headroom_gb == rhs.gpu_memory_headroom_gb &&
         lhs.cache_hit_rate == rhs.cache_hit_rate &&
         lhs.evictions_per_minute == rhs.evictions_per_minute &&
         lhs.load_penalty_p99_ms == rhs.load_penalty_p99_ms &&
         lhs.mixed_batch_fragmentation_score ==
             rhs.mixed_batch_fragmentation_score &&
         lhs.adapter_pressure_score == rhs.adapter_pressure_score &&
         lhs.cache_policy == rhs.cache_policy &&
         lhs.recommendation == rhs.recommendation;
}

void validate_scenarios(
    const std::vector<LoraAdapterCacheScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "LoRA adapter cache scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() || scenario.backend.empty()) {
      throw std::logic_error(
          "LoRA adapter cache scenario identity is invalid");
    }
    if (scenario.release_isolation_fixture &&
        (scenario.prefix_cache_key_scope.empty() ||
         scenario.cross_adapter_prefix_sharing_allowed ||
         scenario.cache_slots <= 0)) {
      throw std::logic_error(
          "release LoRA adapter-cache fixture is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "LoRA adapter cache scenario names must be unique");
    }
  }
}

Summary summarize(
    const std::vector<LoraAdapterCacheResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "LoRA adapter cache records must not be empty");
  }
  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].cache_hit_rate >
        records[result.best_cache_hit_index].cache_hit_rate) {
      result.best_cache_hit_index = index;
    }
    if (records[index].adapter_pressure_score >
        records[result.max_adapter_pressure_index]
            .adapter_pressure_score) {
      result.max_adapter_pressure_index = index;
    }
    if (records[index].evictions_per_minute >
        records[result.max_evictions_index].evictions_per_minute) {
      result.max_evictions_index = index;
    }
  }
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_lora_adapter_cache_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_lora_adapter_cache(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "LoRA adapter cache scenario/record alignment is invalid");
  }
  if (plan.summary.best_cache_hit_index >= plan.records.size() ||
      plan.summary.max_adapter_pressure_index >= plan.records.size() ||
      plan.summary.max_evictions_index >= plan.records.size()) {
    throw std::logic_error(
        "LoRA adapter cache summary index is out of range");
  }
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_lora_adapter_cache(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       plan.scenarios[index]) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "LoRA adapter cache record is not the canonical estimate");
    }
  }
  const auto expected_summary = summarize(plan.records);
  if (plan.summary.best_cache_hit_index !=
          expected_summary.best_cache_hit_index ||
      plan.summary.max_adapter_pressure_index !=
          expected_summary.max_adapter_pressure_index ||
      plan.summary.max_evictions_index !=
          expected_summary.max_evictions_index) {
    throw std::logic_error(
        "LoRA adapter cache summary is not canonical");
  }
}

inferlab_release_lora_prefix_isolation::Binding release_isolation_binding(
    const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_lora_prefix_isolation::Observation>
      observations;
  for (const auto& record : plan.records) {
    if (!record.scenario.release_isolation_fixture) continue;
    const bool compatible = record.scenario.cache_slots > 0 &&
        !record.scenario.prefix_cache_key_scope.empty() &&
        !record.scenario.cross_adapter_prefix_sharing_allowed;
    observations.push_back({
        record.scenario.name, record.scenario.prefix_cache_key_scope,
        record.scenario.cache_slots,
        record.scenario.cross_adapter_prefix_sharing_allowed, compatible,
        compatible ? "adapter_cache_residency_reconciled"
                   : "adapter_cache_residency_incomplete"});
  }
  return inferlab_release_lora_prefix_isolation::bind_profile(
      "lora_adapter_cache", observations);
}

}  // namespace inferlab::lora_adapter_cache_plan
