#pragma once

#include "inferlab/advanced_serving_structured_delivery.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::streaming_sse_plan {

struct Summary {
  std::size_t max_events_index = 0U;
  std::size_t max_stall_index = 0U;
  std::size_t max_disconnects_index = 0U;
};

struct Plan {
  std::vector<StreamingSseScenario> scenarios;
  std::vector<StreamingSseResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);

}  // namespace inferlab::streaming_sse_plan
