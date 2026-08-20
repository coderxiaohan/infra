#pragma once

#include "inferlab/advanced_serving_control_plane.hpp"
#include "release_traffic_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::cost_accounting_plan {

struct Summary {
  std::size_t max_daily_cost_index = 0U;
};

struct Plan {
  std::vector<CostAccountingScenario> scenarios;
  std::vector<CostAccountingResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);
inferlab_release_traffic::Binding release_traffic_profile_binding(
    const Plan& plan);

}  // namespace inferlab::cost_accounting_plan
