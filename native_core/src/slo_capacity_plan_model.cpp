#include "slo_capacity_plan_model.hpp"

#include "checked_index.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::slo_capacity_plan {
namespace {

bool same_scenario(const SloScenario& lhs, const SloScenario& rhs) {
  return lhs.qps == rhs.qps &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens;
}

bool same_result(const SloCapacityResult& lhs,
                 const SloCapacityResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.prefill_replicas == rhs.prefill_replicas &&
         lhs.decode_replicas == rhs.decode_replicas &&
         lhs.required_replicas == rhs.required_replicas &&
         lhs.estimated_hourly_cost == rhs.estimated_hourly_cost &&
         lhs.estimated_daily_cost == rhs.estimated_daily_cost &&
         lhs.prefill_gpu_ms_per_request ==
             rhs.prefill_gpu_ms_per_request &&
         lhs.decode_tokens_per_second_required ==
             rhs.decode_tokens_per_second_required &&
         lhs.replica_utilization == rhs.replica_utilization &&
         lhs.ttft_slo_feasible == rhs.ttft_slo_feasible &&
         lhs.tpot_slo_feasible == rhs.tpot_slo_feasible;
}

void validate_constants(const SloConstants& constants) {
  if (!std::isfinite(constants.ttft_slo_ms) ||
      constants.ttft_slo_ms <= 0.0 ||
      !std::isfinite(constants.tpot_slo_ms) ||
      constants.tpot_slo_ms <= 0.0 ||
      !std::isfinite(constants.gpu_hour_cost) ||
      constants.gpu_hour_cost < 0.0 ||
      !std::isfinite(constants.decode_tokens_per_second_per_gpu) ||
      constants.decode_tokens_per_second_per_gpu <= 0.0 ||
      !std::isfinite(constants.prefill_ms_per_token) ||
      constants.prefill_ms_per_token < 0.0) {
    throw std::logic_error("SLO capacity constants are invalid");
  }
}

void validate_scenarios(const std::vector<SloScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error("SLO capacity scenarios must not be empty");
  }
  for (std::size_t index = 0U; index < scenarios.size(); ++index) {
    const auto& scenario = scenarios[index];
    if (!std::isfinite(scenario.qps) || scenario.qps < 0.0 ||
        scenario.prompt_tokens < 0 || scenario.output_tokens < 0) {
      throw std::logic_error("SLO capacity scenario is invalid");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (same_scenario(scenario, scenarios[prior])) {
        throw std::logic_error(
            "SLO capacity scenarios must be unique");
      }
    }
  }
}

Summary summarize(const std::vector<SloCapacityResult>& records) {
  if (records.empty()) {
    throw std::logic_error("SLO capacity records must not be empty");
  }

  Summary result;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].required_replicas >
        records[result.max_replicas_index].required_replicas) {
      result.max_replicas_index = index;
    }
    if (records[index].estimated_hourly_cost >
        records[result.max_hourly_cost_index].estimated_hourly_cost) {
      result.max_hourly_cost_index = index;
    }
  }
  result.infeasible_scenarios = internal::checked_count_if(
      records.begin(), records.end(),
      [](const SloCapacityResult& record) {
        return !record.ttft_slo_feasible ||
               !record.tpot_slo_feasible;
      },
      "SLO capacity infeasible scenario count");
  return result;
}

}  // namespace

Plan build_plan(const SloConstants& constants) {
  validate_constants(constants);

  Plan plan;
  plan.constants = constants;
  plan.scenarios = default_slo_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(
        estimate_slo_capacity(scenario, plan.constants));
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
        "SLO capacity scenario/record alignment is invalid");
  }
  if (plan.summary.max_replicas_index >= plan.records.size() ||
      plan.summary.max_hourly_cost_index >= plan.records.size()) {
    throw std::logic_error(
        "SLO capacity summary index is out of range");
  }

  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_slo_capacity(plan.scenarios[index], plan.constants);
    if (!same_scenario(plan.records[index].scenario,
                       plan.scenarios[index]) ||
        !same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "SLO capacity record is not the canonical estimate");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.max_replicas_index !=
          expected_summary.max_replicas_index ||
      plan.summary.max_hourly_cost_index !=
          expected_summary.max_hourly_cost_index ||
      plan.summary.infeasible_scenarios !=
          expected_summary.infeasible_scenarios) {
    throw std::logic_error("SLO capacity summary is not canonical");
  }
}

inferlab_release_traffic::Binding release_traffic_profile_binding(
    const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_traffic::Observation> observations;
  observations.reserve(plan.scenarios.size());
  for (std::size_t index = 0U; index < plan.scenarios.size(); ++index) {
    const auto& scenario = plan.scenarios[index];
    observations.push_back(
        {"slo_scenario_" + std::to_string(index), scenario.qps,
         scenario.prompt_tokens, scenario.output_tokens,
         plan.constants.gpu_hour_cost});
  }
  return inferlab_release_traffic::bind_profile(
      "slo_capacity", observations);
}

}  // namespace inferlab::slo_capacity_plan
