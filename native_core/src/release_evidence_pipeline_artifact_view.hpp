#pragma once

#include "release_evidence_pipeline_request.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string render_release_evidence_pipeline_artifact(
    const ReleaseEvidencePipelineRequest& request);

}  // namespace inferlab_release_pipeline
