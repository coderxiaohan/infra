#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"
#include "release_kv_budget_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::long_context_plan {

struct Summary {
  std::size_t max_kv_cache_index = 0U;
  std::size_t best_ttft_index = 0U;
};

struct Plan {
  std::vector<LongContextScenario> scenarios;
  std::vector<LongContextResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);
inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan);

}  // namespace inferlab::long_context_plan
