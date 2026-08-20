#pragma once

#include "release_evidence_pipeline_request.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string pipeline_cli_context_json(
    const ReleaseEvidencePipelineRequest& request,
    int indent);

}  // namespace inferlab_release_pipeline
