#pragma once

#include "release_evidence_pipeline_repair_builder.hpp"
#include "release_evidence_pipeline_root_cause_projection.hpp"
#include "release_evidence_pipeline_source_context.hpp"

namespace inferlab_release_pipeline {

void append_evidence_query_repair_paths(
    const ReleasePipelineSourceContext& sources,
    RepairPathCollector& paths);

void append_native_audit_repair_paths(
    const ReleasePipelineSourceContext& sources,
    RepairPathCollector& paths);

}  // namespace inferlab_release_pipeline
