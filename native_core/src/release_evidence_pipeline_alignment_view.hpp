#pragma once

#include "release_evidence_pipeline_alignment_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string release_pipeline_alignment_dimension_json(
    const ReleasePipelineAlignmentProjection& projection,
    int indent);

}  // namespace inferlab_release_pipeline
