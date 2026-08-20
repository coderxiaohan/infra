#pragma once

#include "release_evidence_pipeline_repair_model.hpp"

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

std::string non_gpu_review_repair_paths_json(
    const std::vector<NonGpuReviewRepairPath>& paths,
    int indent);

}  // namespace inferlab_release_pipeline
