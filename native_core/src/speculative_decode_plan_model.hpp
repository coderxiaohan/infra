#pragma once

#include "inferlab/advanced_serving_generation_strategy.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::speculative_decode_plan {

struct Summary {
  std::size_t best_speedup_index = 0U;
};

struct Plan {
  std::vector<SpeculativeDecodeScenario> scenarios;
  std::vector<SpeculativeDecodeResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);

}  // namespace inferlab::speculative_decode_plan
