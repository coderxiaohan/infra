#pragma once

#include "release_evidence_pipeline_repair_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

int repair_counter_value(std::size_t value, const char* label);
int count_non_gpu_repair_environment(
    const std::vector<NonGpuReviewRepairPath>& paths,
    inferlab_evidence::RepairEnvironment environment);
int count_non_gpu_target_or_validation_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths);
int count_non_gpu_current_repo_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths);
int repair_path_count_as_int(
    const std::vector<NonGpuReviewRepairPath>& paths);
int positive_delta(int left, int right);
int bounded_min(int left, int right);
int count_non_gpu_repair_category(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category);
int count_non_gpu_repair_paths_with_source_locations(
    const std::vector<NonGpuReviewRepairPath>& paths);
int count_non_gpu_repair_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths);
int count_non_gpu_repair_evidence_counters(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category = "");
int count_native_core_audit_repair_evidence_counters(
    const std::vector<NonGpuReviewRepairPath>& paths);
int count_non_gpu_repair_category_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category);
int count_non_gpu_repair_category_from_source(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category,
    const std::string& source);
int count_non_gpu_repair_category_source_location_gaps_from_source(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category,
    const std::string& source);
int count_native_core_audit_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths);
int count_native_core_audit_repair_path_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths);

}  // namespace inferlab_release_pipeline
