#pragma once

#include "inferlab/advanced_serving_reuse_cache.hpp"
#include "release_lora_prefix_isolation_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::radix_prefix_cache_plan {

struct Summary {
  std::size_t max_efficiency_index = 0U;
  std::size_t max_nodes_index = 0U;
};

struct Plan {
  std::vector<RadixPrefixCacheScenario> scenarios;
  std::vector<RadixPrefixCacheResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);
inferlab_release_lora_prefix_isolation::Binding release_isolation_binding(
    const Plan& plan);

}  // namespace inferlab::radix_prefix_cache_plan
