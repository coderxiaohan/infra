#include "inferlab/edge_cloud.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

constexpr std::uint64_t kMaximumBudgetPolicyAgeMs =
    7U * 24U * 60U * 60U * 1000U;
constexpr std::uint64_t kMaximumLocalEnergyEstimateUj =
    1000000000000000ULL;
constexpr double kMaximumCloudRequestCostUsd = 1000000.0;

bool any_gate_enabled(const EdgeCloudBudgetPolicyInput& input) {
  return input.battery_gate_enabled || input.local_energy_gate_enabled ||
         input.cloud_cost_gate_enabled;
}

bool any_observation_available(const EdgeCloudBudgetPolicyInput& input) {
  return input.battery_observation_available ||
         input.local_energy_estimate_available ||
         input.cloud_cost_estimate_available;
}

void require_unique_known_blockers(
    const std::vector<std::string>& blockers,
    const std::vector<std::string>& vocabulary,
    const char* field) {
  for (std::size_t index = 0U; index < blockers.size(); ++index) {
    if (std::find(vocabulary.begin(), vocabulary.end(), blockers[index]) ==
        vocabulary.end()) {
      throw std::invalid_argument(std::string(field) +
                                  " has an unknown blocker");
    }
    if (std::find(blockers.begin(), blockers.begin() + index,
                  blockers[index]) != blockers.begin() + index) {
      throw std::invalid_argument(std::string(field) +
                                  " blockers must be unique");
    }
  }
}

}  // namespace

void validate_edge_cloud_budget_policy_input(
    const EdgeCloudBudgetPolicyInput& input) {
  validate_edge_cloud_provenance_label(
      "edge-cloud budget provenance", input.provenance);
  if (!std::isfinite(input.minimum_local_battery_percent) ||
      input.minimum_local_battery_percent < 0.0 ||
      input.minimum_local_battery_percent > 100.0 ||
      input.maximum_local_energy_uj == 0U ||
      input.maximum_local_energy_uj > kMaximumLocalEnergyEstimateUj ||
      !std::isfinite(input.maximum_cloud_request_cost_usd) ||
      input.maximum_cloud_request_cost_usd <= 0.0 ||
      input.maximum_cloud_request_cost_usd >
          kMaximumCloudRequestCostUsd ||
      input.maximum_observation_age_ms == 0U ||
      input.maximum_observation_age_ms > kMaximumBudgetPolicyAgeMs ||
      input.observation_age_ms >
          kMaximumEdgeCloudObservationAgeMs) {
    throw std::invalid_argument(
        "edge-cloud budget thresholds are outside bounds");
  }
  if (!input.battery_gate_enabled &&
      (input.battery_observation_available || input.battery_percent != 0.0)) {
    throw std::invalid_argument(
        "disabled battery gate cannot carry an observation");
  }
  if (!input.local_energy_gate_enabled &&
      (input.local_energy_estimate_available ||
       input.estimated_local_energy_uj != 0U)) {
    throw std::invalid_argument(
        "disabled local-energy gate cannot carry an estimate");
  }
  if (!input.cloud_cost_gate_enabled &&
      (input.cloud_cost_estimate_available ||
       input.estimated_cloud_request_cost_usd != 0.0)) {
    throw std::invalid_argument(
        "disabled cloud-cost gate cannot carry an estimate");
  }
  if (input.battery_observation_available &&
      (!std::isfinite(input.battery_percent) ||
       input.battery_percent < 0.0 || input.battery_percent > 100.0)) {
    throw std::invalid_argument(
        "edge-cloud battery observation is outside bounds");
  }
  if (!input.battery_observation_available && input.battery_percent != 0.0) {
    throw std::invalid_argument(
        "missing battery observation must use a zero value");
  }
  if (input.local_energy_estimate_available &&
      input.estimated_local_energy_uj > kMaximumLocalEnergyEstimateUj) {
    throw std::invalid_argument(
        "edge-cloud local-energy estimate is outside bounds");
  }
  if (!input.local_energy_estimate_available &&
      input.estimated_local_energy_uj != 0U) {
    throw std::invalid_argument(
        "missing local-energy estimate must use a zero value");
  }
  if (input.cloud_cost_estimate_available &&
      (!std::isfinite(input.estimated_cloud_request_cost_usd) ||
       input.estimated_cloud_request_cost_usd < 0.0 ||
       input.estimated_cloud_request_cost_usd >
           kMaximumCloudRequestCostUsd)) {
    throw std::invalid_argument(
        "edge-cloud cloud-cost estimate is outside bounds");
  }
  if (!input.cloud_cost_estimate_available &&
      input.estimated_cloud_request_cost_usd != 0.0) {
    throw std::invalid_argument(
        "missing cloud-cost estimate must use a zero value");
  }
  if (!any_gate_enabled(input)) {
    if (any_observation_available(input) || input.observation_age_ms != 0U) {
      throw std::invalid_argument(
          "disabled edge-cloud budget policy cannot carry observations");
    }
  } else if (!any_observation_available(input) &&
             input.observation_age_ms != 0U) {
    throw std::invalid_argument(
        "budget policy without observations must use zero age");
  }
}

void validate_edge_cloud_budget_assessment(
    const EdgeCloudBudgetAssessment& assessment) {
  const std::vector<std::string> local_vocabulary = {
      "battery_observation_missing",
      "battery_below_local_reserve",
      "local_energy_estimate_missing",
      "local_energy_exceeds_budget",
      "local_budget_observation_stale",
  };
  const std::vector<std::string> cloud_vocabulary = {
      "cloud_cost_estimate_missing",
      "cloud_cost_exceeds_budget",
      "cloud_cost_observation_stale",
  };
  require_unique_known_blockers(assessment.local_blockers,
                                local_vocabulary, "local budget");
  require_unique_known_blockers(assessment.cloud_blockers,
                                cloud_vocabulary, "cloud budget");
  if (assessment.local_eligible != assessment.local_blockers.empty() ||
      assessment.cloud_eligible != assessment.cloud_blockers.empty()) {
    throw std::invalid_argument(
        "edge-cloud budget eligibility does not match blockers");
  }
  std::string expected_status;
  if (assessment.local_eligible && assessment.cloud_eligible) {
    expected_status = assessment.observation_age_ms.has_value()
                          ? "both_eligible"
                          : "not_required";
  } else if (!assessment.local_eligible && assessment.cloud_eligible) {
    expected_status = "local_ineligible";
  } else if (assessment.local_eligible && !assessment.cloud_eligible) {
    expected_status = "cloud_ineligible";
  } else {
    expected_status = "no_eligible_path";
  }
  if (assessment.status != expected_status) {
    throw std::invalid_argument(
        "edge-cloud budget assessment status is inconsistent");
  }
}

EdgeCloudBudgetAssessment assess_edge_cloud_budget(
    const EdgeCloudBudgetPolicyInput& input) {
  validate_edge_cloud_budget_policy_input(input);
  EdgeCloudBudgetAssessment assessment;
  if (!any_gate_enabled(input)) {
    validate_edge_cloud_budget_assessment(assessment);
    return assessment;
  }
  if (any_observation_available(input)) {
    assessment.observation_age_ms = input.observation_age_ms;
  }
  const bool stale = any_observation_available(input) &&
                     input.observation_age_ms >
                         input.maximum_observation_age_ms;
  if (input.battery_gate_enabled) {
    if (!input.battery_observation_available) {
      assessment.local_blockers.push_back(
          "battery_observation_missing");
    } else if (stale) {
      assessment.local_blockers.push_back(
          "local_budget_observation_stale");
    } else if (input.battery_percent <
               input.minimum_local_battery_percent) {
      assessment.local_blockers.push_back(
          "battery_below_local_reserve");
    }
  }
  if (input.local_energy_gate_enabled) {
    if (!input.local_energy_estimate_available) {
      assessment.local_blockers.push_back(
          "local_energy_estimate_missing");
    } else if (stale) {
      if (std::find(assessment.local_blockers.begin(),
                    assessment.local_blockers.end(),
                    "local_budget_observation_stale") ==
          assessment.local_blockers.end()) {
        assessment.local_blockers.push_back(
            "local_budget_observation_stale");
      }
    } else if (input.estimated_local_energy_uj >
               input.maximum_local_energy_uj) {
      assessment.local_blockers.push_back(
          "local_energy_exceeds_budget");
    }
  }
  if (input.cloud_cost_gate_enabled) {
    if (!input.cloud_cost_estimate_available) {
      assessment.cloud_blockers.push_back(
          "cloud_cost_estimate_missing");
    } else if (stale) {
      assessment.cloud_blockers.push_back(
          "cloud_cost_observation_stale");
    } else if (input.estimated_cloud_request_cost_usd >
               input.maximum_cloud_request_cost_usd) {
      assessment.cloud_blockers.push_back(
          "cloud_cost_exceeds_budget");
    }
  }
  assessment.local_eligible = assessment.local_blockers.empty();
  assessment.cloud_eligible = assessment.cloud_blockers.empty();
  if (assessment.local_eligible && assessment.cloud_eligible) {
    assessment.status = "both_eligible";
  } else if (!assessment.local_eligible && assessment.cloud_eligible) {
    assessment.status = "local_ineligible";
  } else if (assessment.local_eligible && !assessment.cloud_eligible) {
    assessment.status = "cloud_ineligible";
  } else {
    assessment.status = "no_eligible_path";
  }
  validate_edge_cloud_budget_assessment(assessment);
  return assessment;
}

}  // namespace inferlab
