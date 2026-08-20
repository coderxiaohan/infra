#pragma once

#include "release_evidence_pipeline_repair_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

const std::vector<RepairPathGroupSpec>& repair_path_group_specs();

const RepairPathGroupSpec& require_repair_path_group_spec(
    const std::string& group_id);

bool is_native_core_audit_repair_category(const std::string& category);

std::vector<ActiveRepairPathGroup> active_repair_path_groups(
    const std::vector<NonGpuReviewRepairPath>& paths);

const NonGpuReviewRepairPath* first_repair_path_for_group(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const ActiveRepairPathGroup& group);

const ActiveRepairPathGroup* active_repair_path_group_for_id(
    const std::vector<ActiveRepairPathGroup>& groups,
    const std::string& group_id);

}  // namespace inferlab_release_pipeline
