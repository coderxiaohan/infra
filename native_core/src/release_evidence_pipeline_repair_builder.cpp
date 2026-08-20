#include "release_evidence_pipeline_repair_builder.hpp"

#include "release_evidence_pipeline_repair_source_policy.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_release_pipeline {

NonGpuReviewRepairPath& RepairPathCollector::add(
    std::string id, std::string source, std::string category,
    const inferlab_evidence::RepairEnvironment environment,
    std::string severity, std::string trigger, std::string action,
    std::string command) {
  const auto locations = repair_path_source_locations_for(category, source);
  paths_.push_back({std::move(id), std::move(source), std::move(category),
                    environment, std::move(severity), std::move(trigger),
                    std::move(action), std::move(command), locations, {}});
  return paths_.back();
}

NonGpuReviewRepairPath& RepairPathCollector::back() {
  if (paths_.empty()) {
    throw std::logic_error("release pipeline repair path collector is empty");
  }
  return paths_.back();
}

std::vector<NonGpuReviewRepairPath> RepairPathCollector::take_paths() {
  return std::move(paths_);
}

}  // namespace inferlab_release_pipeline
