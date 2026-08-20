#pragma once

#include "inferlab/advanced_serving_lora.hpp"
#include "release_lora_prefix_isolation_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::lora_adapter_cache_plan {

struct Summary {
  std::size_t best_cache_hit_index = 0U;
  std::size_t max_adapter_pressure_index = 0U;
  std::size_t max_evictions_index = 0U;
};

struct Plan {
  std::vector<LoraAdapterCacheScenario> scenarios;
  std::vector<LoraAdapterCacheResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);
inferlab_release_lora_prefix_isolation::Binding release_isolation_binding(
    const Plan& plan);

}  // namespace inferlab::lora_adapter_cache_plan
