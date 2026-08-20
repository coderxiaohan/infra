#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"

#include <string>

namespace inferlab::gateway_policy_matrix_plan_view {

std::string artifact_json(const GatewayPolicyMatrixPlan& plan);

}  // namespace inferlab::gateway_policy_matrix_plan_view
