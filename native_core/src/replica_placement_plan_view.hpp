#pragma once

#include "inferlab/advanced_serving_deployment_planning.hpp"

#include <string>

namespace inferlab::replica_placement_view {

std::string artifact_json(const ReplicaPlacementPlan& plan);

}  // namespace inferlab::replica_placement_view
