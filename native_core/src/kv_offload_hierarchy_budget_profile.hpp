#pragma once

#include "inferlab/advanced_serving_kv_tier.hpp"
#include "release_kv_budget_profile.hpp"

namespace inferlab {

inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const KvOffloadHierarchyPlan& plan);

}  // namespace inferlab
