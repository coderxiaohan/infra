#include "release_evidence_pipeline_repair_metrics.hpp"

#include "checked_index.hpp"
#include "release_evidence_pipeline_repair_group_projection.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

void checked_increment(int& value, const char* name) {
  value = inferlab::internal::checked_add_int(value, 1, name);
}

}  // namespace

int repair_counter_value(const std::size_t value, const char* label) {
  return inferlab::internal::checked_int_from_size(value, label);
}

int count_non_gpu_repair_environment(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const inferlab_evidence::RepairEnvironment environment) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.repair_environment == environment) {
      checked_increment(count, "release pipeline repair-environment count");
    }
  }
  return count;
}

int count_non_gpu_target_or_validation_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (inferlab_evidence::repair_environment_is_target_or_validation(
            path.repair_environment)) {
      checked_increment(
          count, "release pipeline target-or-validation repair-path count");
    }
  }
  return count;
}

int count_non_gpu_current_repo_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (inferlab_evidence::repair_environment_is_current_repo(
            path.repair_environment)) {
      checked_increment(count,
                        "release pipeline current-repo repair-path count");
    }
  }
  return count;
}

int repair_path_count_as_int(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  return inferlab::internal::checked_int_from_size(
      paths.size(), "release pipeline repair-path count");
}

int positive_delta(const int left, const int right) {
  return left > right ? left - right : 0;
}

int bounded_min(const int left, const int right) {
  return left < right ? left : right;
}

int count_non_gpu_repair_category(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.category == category) {
      checked_increment(count, "release pipeline repair-category count");
    }
  }
  return count;
}

int count_non_gpu_repair_paths_with_source_locations(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (!path.source_locations.empty()) {
      checked_increment(
          count, "release pipeline repair path with source-location count");
    }
  }
  return count;
}

int count_non_gpu_repair_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.source_locations.empty()) {
      checked_increment(
          count, "release pipeline repair source-location gap count");
    }
  }
  return count;
}

int count_non_gpu_repair_evidence_counters(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category) {
  std::size_t count = 0;
  for (const auto& path : paths) {
    if (!category.empty() && path.category != category) continue;
    count = inferlab::internal::checked_add_size(
        count, path.evidence_counters.size(),
        "release pipeline repair evidence-counter count");
  }
  return inferlab::internal::checked_int_from_size(
      count, "release pipeline repair evidence-counter count");
}

int count_native_core_audit_repair_evidence_counters(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  const int backlog =
      count_non_gpu_repair_evidence_counters(paths,
                                             "native_core_audit_backlog");
  const int scope = count_non_gpu_repair_evidence_counters(
      paths, "native_core_audit_scope_blocked");
  return inferlab::internal::checked_add_int(
      backlog, scope,
      "release pipeline native-core audit evidence-counter count");
}

int count_non_gpu_repair_category_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.category == category && path.source_locations.empty()) {
      checked_increment(
          count, "release pipeline category source-location gap count");
    }
  }
  return count;
}

int count_non_gpu_repair_category_from_source(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category,
    const std::string& source) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.category == category && path.source == source) {
      checked_increment(count,
                        "release pipeline category/source repair-path count");
    }
  }
  return count;
}

int count_non_gpu_repair_category_source_location_gaps_from_source(
    const std::vector<NonGpuReviewRepairPath>& paths,
    const std::string& category,
    const std::string& source) {
  int count = 0;
  for (const auto& path : paths) {
    if (path.category == category && path.source == source &&
        path.source_locations.empty()) {
      checked_increment(
          count,
          "release pipeline category/source source-location gap count");
    }
  }
  return count;
}

int count_native_core_audit_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (is_native_core_audit_repair_category(path.category)) {
      checked_increment(count,
                        "release pipeline native-core audit repair-path count");
    }
  }
  return count;
}

int count_native_core_audit_repair_path_source_location_gaps(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  int count = 0;
  for (const auto& path : paths) {
    if (is_native_core_audit_repair_category(path.category) &&
        path.source_locations.empty()) {
      checked_increment(
          count,
          "release pipeline native-core audit source-location gap count");
    }
  }
  return count;
}

}  // namespace inferlab_release_pipeline
