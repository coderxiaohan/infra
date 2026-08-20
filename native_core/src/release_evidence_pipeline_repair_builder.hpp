#pragma once

#include "release_evidence_pipeline_repair_model.hpp"

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

class RepairPathCollector {
 public:
  RepairPathCollector() = default;
  RepairPathCollector(const RepairPathCollector&) = delete;
  RepairPathCollector& operator=(const RepairPathCollector&) = delete;
  RepairPathCollector(RepairPathCollector&&) = delete;
  RepairPathCollector& operator=(RepairPathCollector&&) = delete;

  NonGpuReviewRepairPath& add(
      std::string id,
      std::string source,
      std::string category,
      inferlab_evidence::RepairEnvironment environment,
      std::string severity,
      std::string trigger,
      std::string action,
      std::string command);

  NonGpuReviewRepairPath& back();
  std::vector<NonGpuReviewRepairPath> take_paths();

 private:
  std::vector<NonGpuReviewRepairPath> paths_;
};

}  // namespace inferlab_release_pipeline
