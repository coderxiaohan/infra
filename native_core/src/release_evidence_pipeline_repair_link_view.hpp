#pragma once

#include "release_evidence_pipeline_repair_projection.hpp"
#include "release_evidence_pipeline_snapshot_diff_link_projection.hpp"

#include <string>

namespace inferlab_release_pipeline {

std::string snapshot_diff_release_link_json(
    const ReleasePipelineSnapshotDiffReleaseLinkProjection& projection,
    int indent);

std::string blocker_repair_card_link_json(
    const ReleasePipelineBlockerRepairCardLinkProjection& projection,
    int indent);

std::string blocker_repair_card_link_summary_fields_json(
    const ReleasePipelineBlockerRepairCardLinkProjection& projection,
    int indent);

}  // namespace inferlab_release_pipeline
