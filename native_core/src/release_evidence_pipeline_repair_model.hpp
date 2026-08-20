#pragma once

#include "evidence_domain_types.hpp"
#include "evidence_json_writer.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct RepairPathGroupSpec {
  std::string id;
  std::string grouping;
  std::string key;
  std::string label;
  std::string priority;
  std::string claim_boundary;
};

struct NonGpuReviewRepairPath {
  std::string id;
  std::string source;
  std::string category;
  inferlab_evidence::RepairEnvironment repair_environment =
      inferlab_evidence::RepairEnvironment::kUnknown;
  std::string severity;
  std::string trigger;
  std::string recommended_action;
  std::string command;
  std::vector<std::string> source_locations;
  std::vector<inferlab_evidence::EvidenceNamedCounter> evidence_counters;
};

inline constexpr std::size_t kNoRepairPathIndex =
    std::numeric_limits<std::size_t>::max();

struct ActiveRepairPathGroup {
  RepairPathGroupSpec spec;
  std::size_t step = 0;
  std::size_t path_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t evidence_counter_count = 0;
  std::size_t first_path_index = kNoRepairPathIndex;
  bool current_repo_action_required = false;
  bool target_or_validation_action_required = false;
  std::string action_owner = "review_only";
};

}  // namespace inferlab_release_pipeline
