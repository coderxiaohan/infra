#include "disaggregated_serving_model.hpp"

#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace inferlab::disaggregated_serving {
namespace {

using ScenarioIdentity =
    std::tuple<int, int, int, int, double, double>;

bool same_scenario(const DisaggregatedScenario& lhs,
                   const DisaggregatedScenario& rhs) {
  return lhs.prefill_workers == rhs.prefill_workers &&
         lhs.decode_workers == rhs.decode_workers &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.arrival_qps == rhs.arrival_qps &&
         lhs.bandwidth_gbps == rhs.bandwidth_gbps;
}

bool same_result(const DisaggregatedResult& lhs,
                 const DisaggregatedResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.prefill_ms == rhs.prefill_ms &&
         lhs.decode_ms == rhs.decode_ms &&
         lhs.kv_transfer_ms == rhs.kv_transfer_ms &&
         lhs.queue_penalty_ms == rhs.queue_penalty_ms &&
         lhs.ttft_ms == rhs.ttft_ms &&
         lhs.tpot_ms == rhs.tpot_ms &&
         lhs.end_to_end_ms == rhs.end_to_end_ms &&
         lhs.aggregate_baseline_ms == rhs.aggregate_baseline_ms &&
         lhs.speedup_vs_aggregate == rhs.speedup_vs_aggregate;
}

ScenarioIdentity scenario_identity(
    const DisaggregatedScenario& scenario) {
  return {scenario.prefill_workers, scenario.decode_workers,
          scenario.prompt_tokens, scenario.output_tokens,
          scenario.arrival_qps, scenario.bandwidth_gbps};
}

void validate_constants(const DisaggregatedConstants& constants) {
  if (!std::isfinite(constants.prefill_ms_per_token) ||
      constants.prefill_ms_per_token < 0.0 ||
      !std::isfinite(constants.decode_ms_per_token) ||
      constants.decode_ms_per_token < 0.0 ||
      !std::isfinite(constants.kv_bytes_per_token) ||
      constants.kv_bytes_per_token < 0.0) {
    throw std::logic_error(
        "disaggregated serving constants must be finite and non-negative");
  }
}

void validate_scenarios(
    const std::vector<DisaggregatedScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "disaggregated serving scenarios must not be empty");
  }
  std::set<ScenarioIdentity> identities;
  for (const auto& scenario : scenarios) {
    if (!std::isfinite(scenario.arrival_qps) ||
        !std::isfinite(scenario.bandwidth_gbps)) {
      throw std::logic_error(
          "disaggregated serving scenario rates must be finite");
    }
    if (!identities.insert(scenario_identity(scenario)).second) {
      throw std::logic_error(
          "disaggregated serving scenarios must be unique");
    }
  }
}

Summary summarize(const std::vector<DisaggregatedResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "disaggregated serving records must not be empty");
  }
  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].ttft_ms <
        records[result.best_ttft_index].ttft_ms) {
      result.best_ttft_index = index;
    }
    if (records[index].speedup_vs_aggregate >
        records[result.best_speedup_index].speedup_vs_aggregate) {
      result.best_speedup_index = index;
    }
  }
  return result;
}

}  // namespace

Plan build_plan(const DisaggregatedConstants& constants) {
  Plan plan;
  plan.constants = constants;
  validate_constants(plan.constants);
  plan.scenarios = default_disaggregated_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(
        estimate_disaggregated_serving(scenario, plan.constants));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_constants(plan.constants);
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "disaggregated serving scenario/record alignment is invalid");
  }
  if (plan.summary.best_ttft_index >= plan.records.size() ||
      plan.summary.best_speedup_index >= plan.records.size()) {
    throw std::logic_error(
        "disaggregated serving summary index is out of range");
  }
  std::set<ScenarioIdentity> normalized_identities;
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_disaggregated_serving(plan.scenarios[index],
                                       plan.constants);
    if (!same_scenario(plan.records[index].scenario,
                       expected.scenario) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "disaggregated serving record is not the canonical estimate");
    }
    if (!normalized_identities
             .insert(scenario_identity(expected.scenario))
             .second) {
      throw std::logic_error(
          "disaggregated serving normalized scenarios must be unique");
    }
  }
  const auto expected_summary = summarize(plan.records);
  if (plan.summary.best_ttft_index !=
          expected_summary.best_ttft_index ||
      plan.summary.best_speedup_index !=
          expected_summary.best_speedup_index) {
    throw std::logic_error(
        "disaggregated serving summary is not canonical");
  }
}

}  // namespace inferlab::disaggregated_serving
