#include "cost_accounting_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::cost_accounting_plan {
namespace {

bool same_scenario(const CostAccountingScenario& lhs,
                   const CostAccountingScenario& rhs) {
  return lhs.name == rhs.name &&
         lhs.request_qps == rhs.request_qps &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.replicas == rhs.replicas &&
         lhs.gpu_hour_cost == rhs.gpu_hour_cost &&
         lhs.cache_hit_rate == rhs.cache_hit_rate &&
         lhs.tokens_per_second_per_replica ==
             rhs.tokens_per_second_per_replica;
}

bool same_result(const CostAccountingResult& lhs,
                 const CostAccountingResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.billable_tokens_per_second == rhs.billable_tokens_per_second &&
         lhs.effective_tokens_per_second ==
             rhs.effective_tokens_per_second &&
         lhs.hourly_cost == rhs.hourly_cost &&
         lhs.daily_cost == rhs.daily_cost &&
         lhs.cost_per_million_tokens == rhs.cost_per_million_tokens &&
         lhs.utilization == rhs.utilization;
}

void validate_scenarios(
    const std::vector<CostAccountingScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "cost-accounting scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty()) {
      throw std::logic_error(
          "cost-accounting scenario name is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "cost-accounting scenario names must be unique");
    }
  }
}

Summary summarize(const std::vector<CostAccountingResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "cost-accounting records must not be empty");
  }
  Summary summary;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].daily_cost >
        records[summary.max_daily_cost_index].daily_cost) {
      summary.max_daily_cost_index = index;
    }
  }
  return summary;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_cost_accounting_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_cost_accounting(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "cost-accounting scenario/record alignment is invalid");
  }
  if (plan.summary.max_daily_cost_index >= plan.records.size()) {
    throw std::logic_error(
        "cost-accounting summary index is out of range");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected = estimate_cost_accounting(plan.scenarios[index]);
    if (!same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "cost-accounting record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "cost-accounting normalized names must be unique");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_daily_cost_index !=
      expected_summary.max_daily_cost_index) {
    throw std::logic_error(
        "cost-accounting summary is not canonical");
  }
}

inferlab_release_traffic::Binding release_traffic_profile_binding(
    const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_traffic::Observation> observations;
  observations.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    const auto& scenario = record.scenario;
    observations.push_back(
        {scenario.name, scenario.request_qps, scenario.prompt_tokens,
         scenario.output_tokens, scenario.gpu_hour_cost});
  }
  return inferlab_release_traffic::bind_profile(
      "cost_accounting", observations);
}

}  // namespace inferlab::cost_accounting_plan
