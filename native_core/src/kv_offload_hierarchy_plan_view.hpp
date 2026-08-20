#pragma once

#include "inferlab/advanced_serving_kv_tier.hpp"

#include <string>

namespace inferlab::kv_offload_hierarchy_plan_view {

std::string artifact_json(const KvOffloadHierarchyPlan& plan);

}  // namespace inferlab::kv_offload_hierarchy_plan_view
