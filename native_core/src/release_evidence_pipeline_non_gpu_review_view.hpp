#pragma once

#include "release_evidence_pipeline_artifact_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string non_gpu_review_closure_json(
    const ReleasePipelineArtifactProjection& projection, int indent);

}  // namespace inferlab_release_pipeline
