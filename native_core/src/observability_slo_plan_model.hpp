#pragma once

#include "inferlab/advanced_serving_observability.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::observability_slo_plan {

struct Summary {
  std::size_t max_burn_index = 0U;
  int page_oncall_count = 0;
};

struct Plan {
  std::vector<ObservabilitySloScenario> scenarios;
  std::vector<ObservabilitySloResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);

}  // namespace inferlab::observability_slo_plan
