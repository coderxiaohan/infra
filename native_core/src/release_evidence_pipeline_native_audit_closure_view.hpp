#pragma once

#include "release_evidence_pipeline_native_audit_closure_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string native_audit_closure_json(
    const ReleasePipelineNativeAuditClosureProjection& projection,
    int indent);

}  // namespace inferlab_release_pipeline
