#pragma once

#include "release_evidence_pipeline_repair_model.hpp"

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

std::string repair_path_groups_json(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::vector<ActiveRepairPathGroup>& groups,
    int indent);

std::string repair_path_group_plan_json(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::vector<ActiveRepairPathGroup>& groups,
    int indent);

}  // namespace inferlab_release_pipeline
