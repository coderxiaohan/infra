#include "observability_slo_plan_model.hpp"

#include "checked_index.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::observability_slo_plan {
namespace {

bool same_scenario(const ObservabilitySloScenario& lhs,
                   const ObservabilitySloScenario& rhs) {
  return lhs.name == rhs.name &&
         lhs.request_qps == rhs.request_qps &&
         lhs.error_rate == rhs.error_rate &&
         lhs.p99_ttft_ms == rhs.p99_ttft_ms &&
         lhs.p99_tpot_ms == rhs.p99_tpot_ms &&
         lhs.ttft_slo_ms == rhs.ttft_slo_ms &&
         lhs.tpot_slo_ms == rhs.tpot_slo_ms &&
         lhs.error_budget_percent == rhs.error_budget_percent &&
         lhs.observation_window_minutes ==
             rhs.observation_window_minutes &&
         lhs.scrape_interval_s == rhs.scrape_interval_s;
}

bool same_result(const ObservabilitySloResult& lhs,
                 const ObservabilitySloResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.samples_per_window == rhs.samples_per_window &&
         lhs.ttft_violation_ratio == rhs.ttft_violation_ratio &&
         lhs.tpot_violation_ratio == rhs.tpot_violation_ratio &&
         lhs.error_budget_burn_rate == rhs.error_budget_burn_rate &&
         lhs.composite_slo_risk == rhs.composite_slo_risk &&
         lhs.page_oncall == rhs.page_oncall &&
         lhs.ticket_required == rhs.ticket_required;
}

void validate_scenarios(
    const std::vector<ObservabilitySloScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "observability SLO scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty()) {
      throw std::logic_error(
          "observability SLO scenario identity is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "observability SLO scenario names must be unique");
    }
  }
}

Summary summarize(
    const std::vector<ObservabilitySloResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "observability SLO records must not be empty");
  }
  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].error_budget_burn_rate >
        records[result.max_burn_index].error_budget_burn_rate) {
      result.max_burn_index = index;
    }
  }
  result.page_oncall_count = internal::checked_count_if(
      records.begin(), records.end(),
      [](const auto& record) { return record.page_oncall; },
      "observability SLO page count");
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_observability_slo_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_observability_slo(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "observability SLO scenario/record alignment is invalid");
  }
  if (plan.summary.max_burn_index >= plan.records.size()) {
    throw std::logic_error(
        "observability SLO summary index is out of range");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_observability_slo(plan.scenarios[index]);
    if (!same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "observability SLO record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "observability SLO normalized names must be unique");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_burn_index !=
          expected_summary.max_burn_index ||
      plan.summary.page_oncall_count !=
          expected_summary.page_oncall_count) {
    throw std::logic_error(
        "observability SLO summary is not canonical");
  }
}

}  // namespace inferlab::observability_slo_plan
