#include "long_context_plan_model.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::long_context_plan {
namespace {

bool same_scenario(const LongContextScenario& lhs,
                   const LongContextScenario& rhs) {
  return lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.chunk_size == rhs.chunk_size &&
         lhs.prefix_cache_hit_rate == rhs.prefix_cache_hit_rate &&
         lhs.prefill_ms_per_token == rhs.prefill_ms_per_token &&
         lhs.decode_ms_per_token == rhs.decode_ms_per_token &&
         lhs.kv_bytes_per_token == rhs.kv_bytes_per_token &&
         lhs.kv_budget_gib == rhs.kv_budget_gib &&
         lhs.name == rhs.name;
}

bool same_result(const LongContextResult& lhs,
                 const LongContextResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.chunk_count == rhs.chunk_count &&
         lhs.full_prefill_ms == rhs.full_prefill_ms &&
         lhs.chunked_prefill_ms == rhs.chunked_prefill_ms &&
         lhs.prefix_cache_saved_ms == rhs.prefix_cache_saved_ms &&
         lhs.ttft_ms == rhs.ttft_ms &&
         lhs.decode_ms == rhs.decode_ms &&
         lhs.kv_cache_gb == rhs.kv_cache_gb;
}

void validate_scenarios(
    const std::vector<LongContextScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "long context scenarios must not be empty");
  }
  for (std::size_t index = 0U; index < scenarios.size(); ++index) {
    const auto& scenario = scenarios[index];
    if (scenario.prompt_tokens < 0 || scenario.output_tokens < 0 ||
        scenario.chunk_size <= 0 ||
        !std::isfinite(scenario.prefix_cache_hit_rate) ||
        scenario.prefix_cache_hit_rate < 0.0 ||
        scenario.prefix_cache_hit_rate > 1.0 ||
        !std::isfinite(scenario.prefill_ms_per_token) ||
        scenario.prefill_ms_per_token < 0.0 ||
        !std::isfinite(scenario.decode_ms_per_token) ||
        scenario.decode_ms_per_token < 0.0 ||
        !std::isfinite(scenario.kv_bytes_per_token) ||
        scenario.kv_bytes_per_token < 0.0 ||
        !std::isfinite(scenario.kv_budget_gib) ||
        scenario.kv_budget_gib <= 0.0) {
      throw std::logic_error("long context scenario is invalid");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (same_scenario(scenario, scenarios[prior])) {
        throw std::logic_error(
            "long context scenarios must be unique");
      }
    }
  }
}

Summary summarize(const std::vector<LongContextResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "long context records must not be empty");
  }

  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].kv_cache_gb >
        records[result.max_kv_cache_index].kv_cache_gb) {
      result.max_kv_cache_index = index;
    }
    if (records[index].ttft_ms <
        records[result.best_ttft_index].ttft_ms) {
      result.best_ttft_index = index;
    }
  }
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_long_context_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_long_context(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "long context scenario/record alignment is invalid");
  }
  if (plan.summary.max_kv_cache_index >= plan.records.size() ||
      plan.summary.best_ttft_index >= plan.records.size()) {
    throw std::logic_error(
        "long context summary index is out of range");
  }

  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected = estimate_long_context(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       plan.scenarios[index]) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "long context record is not the canonical estimate");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_kv_cache_index !=
          expected_summary.max_kv_cache_index ||
      plan.summary.best_ttft_index !=
          expected_summary.best_ttft_index) {
    throw std::logic_error(
        "long context summary is not canonical");
  }
}

inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan) {
  std::vector<inferlab_release_kv_budget::Observation> observations;
  observations.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    const auto& scenario = record.scenario;
    if (scenario.name.empty()) continue;
    const bool compatible = record.kv_cache_gb <= scenario.kv_budget_gib;
    observations.push_back(
        {scenario.name, scenario.kv_budget_gib, scenario.prompt_tokens,
         scenario.output_tokens, scenario.chunk_size, compatible,
         compatible ? "fits_local_kv_budget"
                    : "exceeds_local_kv_budget"});
  }
  return inferlab_release_kv_budget::bind_profile("long_context_matrix",
                                                   observations);
}

}  // namespace inferlab::long_context_plan
