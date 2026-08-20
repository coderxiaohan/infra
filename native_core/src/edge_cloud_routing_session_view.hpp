#pragma once

#include "inferlab/edge_cloud.hpp"

#include <string>

namespace inferlab::edge_cloud_routing {

std::string session_recovery_input_json(
    const EdgeCloudSessionRecoveryInput& input, int indent);
std::string session_recovery_assessment_json(
    const EdgeCloudSessionRecoveryAssessment& assessment, int indent);

}  // namespace inferlab::edge_cloud_routing
