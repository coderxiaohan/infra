#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"
#include "release_kv_budget_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::chunked_prefill_scheduler_plan {

struct Summary {
  std::size_t best_ttft_index = 0U;
  std::size_t best_stall_free_index = 0U;
  std::size_t max_pressure_index = 0U;
};

struct Plan {
  std::vector<ChunkedPrefillSchedulerScenario> scenarios;
  std::vector<ChunkedPrefillSchedulerResult> records;
  Summary summary;
};

Plan build_default_plan();

void validate_plan(const Plan& plan);
inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const Plan& plan);

}  // namespace inferlab::chunked_prefill_scheduler_plan
