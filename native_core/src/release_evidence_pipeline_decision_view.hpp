#pragma once

#include "release_evidence_pipeline_decision_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string release_pipeline_decision_summary_fields_json(
    const ReleasePipelineDecisionProjection& projection,
    int indent);

std::string release_pipeline_source_contract_gate_json(
    const ReleasePipelineDecisionProjection& projection,
    int indent);

std::string release_pipeline_replay_evidence_dimension_json(
    const ReleasePipelineDecisionProjection& projection,
    int indent);

}  // namespace inferlab_release_pipeline
