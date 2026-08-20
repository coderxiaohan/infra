#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::disaggregated_serving {

struct Summary {
  std::size_t best_ttft_index = 0U;
  std::size_t best_speedup_index = 0U;
};

struct Plan {
  DisaggregatedConstants constants;
  std::vector<DisaggregatedScenario> scenarios;
  std::vector<DisaggregatedResult> records;
  Summary summary;
};

Plan build_plan(const DisaggregatedConstants& constants);
void validate_plan(const Plan& plan);

}  // namespace inferlab::disaggregated_serving
