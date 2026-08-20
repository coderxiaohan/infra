#pragma once

#include "evidence_domain_utils.hpp"
#include "release_evidence_pipeline_stage.hpp"

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

inferlab_evidence::MetadataBlockerPolicy pipeline_metadata_policy();

std::string repair_category(
    const PipelineStage& stage,
    const inferlab_evidence::EvidenceSnapshot& snapshot);

std::string repair_action(const PipelineStage& stage,
                          const std::string& category);

std::string stages_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string input_artifacts_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string blockers_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string release_dimensions_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string grouped_blockers_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const std::string& category_filter,
    int indent);

std::string repair_plan_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string repair_order_json(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    int indent);

std::string shortest_path_json(const std::string& target, int indent);

}  // namespace inferlab_release_pipeline
