#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"
#include "release_traffic_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::slo_capacity_plan {

struct Summary {
  std::size_t max_replicas_index = 0U;
  std::size_t max_hourly_cost_index = 0U;
  int infeasible_scenarios = 0;
};

struct Plan {
  SloConstants constants;
  std::vector<SloScenario> scenarios;
  std::vector<SloCapacityResult> records;
  Summary summary;
};

Plan build_plan(const SloConstants& constants);
void validate_plan(const Plan& plan);
inferlab_release_traffic::Binding release_traffic_profile_binding(
    const Plan& plan);

}  // namespace inferlab::slo_capacity_plan
