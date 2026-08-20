#include "speculative_decode_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::speculative_decode_plan {
namespace {

bool same_scenario(const SpeculativeDecodeScenario& lhs,
                   const SpeculativeDecodeScenario& rhs) {
  return lhs.name == rhs.name &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.verify_window == rhs.verify_window &&
         lhs.acceptance_rate == rhs.acceptance_rate &&
         lhs.draft_tokens_per_second == rhs.draft_tokens_per_second &&
         lhs.target_tokens_per_second == rhs.target_tokens_per_second &&
         lhs.target_verify_ms_per_token ==
             rhs.target_verify_ms_per_token &&
         lhs.round_overhead_ms == rhs.round_overhead_ms;
}

bool same_result(const SpeculativeDecodeResult& lhs,
                 const SpeculativeDecodeResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.target_verify_rounds == rhs.target_verify_rounds &&
         lhs.draft_tokens == rhs.draft_tokens &&
         lhs.accepted_tokens == rhs.accepted_tokens &&
         lhs.rejected_tokens == rhs.rejected_tokens &&
         lhs.target_only_ms == rhs.target_only_ms &&
         lhs.speculative_ms == rhs.speculative_ms &&
         lhs.speedup == rhs.speedup &&
         lhs.tokens_per_second == rhs.tokens_per_second &&
         lhs.break_even_acceptance_rate ==
             rhs.break_even_acceptance_rate;
}

void validate_scenarios(
    const std::vector<SpeculativeDecodeScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "speculative decode scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty()) {
      throw std::logic_error(
          "speculative decode scenario name is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "speculative decode scenario names must be unique");
    }
  }
}

Summary summarize(const std::vector<SpeculativeDecodeResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "speculative decode records must not be empty");
  }
  Summary summary;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].speedup >
        records[summary.best_speedup_index].speedup) {
      summary.best_speedup_index = index;
    }
  }
  return summary;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_speculative_decode_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_speculative_decode(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "speculative decode scenario/record alignment is invalid");
  }
  if (plan.summary.best_speedup_index >= plan.records.size()) {
    throw std::logic_error(
        "speculative decode summary index is out of range");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected = estimate_speculative_decode(plan.scenarios[index]);
    if (!same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "speculative decode record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "speculative decode normalized names must be unique");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.best_speedup_index !=
      expected_summary.best_speedup_index) {
    throw std::logic_error(
        "speculative decode summary is not canonical");
  }
}

}  // namespace inferlab::speculative_decode_plan
