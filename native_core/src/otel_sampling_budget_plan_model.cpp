#include "otel_sampling_budget_plan_model.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::otel_sampling_budget_plan {
namespace {

bool same_scenario(const OtelSamplingBudgetScenario& lhs,
                   const OtelSamplingBudgetScenario& rhs) {
  return lhs.name == rhs.name && lhs.backend == rhs.backend &&
         lhs.request_qps == rhs.request_qps &&
         lhs.spans_per_request == rhs.spans_per_request &&
         lhs.avg_span_bytes == rhs.avg_span_bytes &&
         lhs.sample_rate == rhs.sample_rate &&
         lhs.collector_capacity_spans_per_sec ==
             rhs.collector_capacity_spans_per_sec &&
         lhs.storage_budget_gb_per_day ==
             rhs.storage_budget_gb_per_day &&
         lhs.error_trace_boost == rhs.error_trace_boost &&
         lhs.tail_sampling_rate == rhs.tail_sampling_rate;
}

bool same_result(const OtelSamplingBudgetResult& lhs,
                 const OtelSamplingBudgetResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.generated_spans_per_sec == rhs.generated_spans_per_sec &&
         lhs.sampled_spans_per_sec == rhs.sampled_spans_per_sec &&
         lhs.collector_utilization == rhs.collector_utilization &&
         lhs.daily_storage_gb == rhs.daily_storage_gb &&
         lhs.storage_budget_utilization ==
             rhs.storage_budget_utilization &&
         lhs.recommended_sample_rate == rhs.recommended_sample_rate &&
         lhs.backpressure_risk_score == rhs.backpressure_risk_score &&
         lhs.sampling_policy == rhs.sampling_policy &&
         lhs.recommendation == rhs.recommendation;
}

void validate_scenarios(
    const std::vector<OtelSamplingBudgetScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "OTel sampling budget scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() || scenario.backend.empty()) {
      throw std::logic_error(
          "OTel sampling budget scenario identity is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "OTel sampling budget scenario names must be unique");
    }
  }
}

Summary summarize(
    const std::vector<OtelSamplingBudgetResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "OTel sampling budget records must not be empty");
  }
  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].backpressure_risk_score >
        records[result.max_risk_index].backpressure_risk_score) {
      result.max_risk_index = index;
    }
    if (records[index].daily_storage_gb >
        records[result.max_storage_index].daily_storage_gb) {
      result.max_storage_index = index;
    }
    if (records[index].collector_utilization >
        records[result.max_collector_index].collector_utilization) {
      result.max_collector_index = index;
    }
  }
  return result;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_otel_sampling_budget_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(
        estimate_otel_sampling_budget(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "OTel sampling budget scenario/record alignment is invalid");
  }
  if (plan.summary.max_risk_index >= plan.records.size() ||
      plan.summary.max_storage_index >= plan.records.size() ||
      plan.summary.max_collector_index >= plan.records.size()) {
    throw std::logic_error(
        "OTel sampling budget summary index is out of range");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_otel_sampling_budget(plan.scenarios[index]);
    if (!same_scenario(plan.records[index].scenario,
                       expected.scenario) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "OTel sampling budget record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "OTel sampling budget normalized names must be unique");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_risk_index !=
          expected_summary.max_risk_index ||
      plan.summary.max_storage_index !=
          expected_summary.max_storage_index ||
      plan.summary.max_collector_index !=
          expected_summary.max_collector_index) {
    throw std::logic_error(
        "OTel sampling budget summary is not canonical");
  }
}

}  // namespace inferlab::otel_sampling_budget_plan
