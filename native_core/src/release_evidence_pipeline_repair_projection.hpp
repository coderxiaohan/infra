#pragma once

#include "release_evidence_pipeline_repair_model.hpp"
#include "release_evidence_pipeline_root_cause_projection.hpp"
#include "release_evidence_pipeline_source_context.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct ReleasePipelineBlockerRepairCardLinkProjection {
  ReleasePipelineBlockerRepairCardLinkProjection(
      const ReleasePipelineRootCauseSourceProjection& root_cause_value,
      const std::vector<NonGpuReviewRepairPath>& repair_paths);

  const ReleasePipelineRootCauseSourceProjection& root_cause;
  bool link_present = false;
  bool root_cause_repair_cards_present = false;
  int root_cause_repair_card_count = 0;
  int root_cause_current_repo_repair_card_count = 0;
  int root_cause_target_product_replay_repair_card_count = 0;
  int root_cause_target_gpu_execute_repair_card_count = 0;
  int root_cause_target_machine_repair_card_count = 0;
  std::string root_cause_repair_environment_split_status = "source_missing";
  bool root_cause_repair_environment_split_schema_gap = false;
  bool root_cause_repair_environment_legacy_fallback_used = false;
  int root_cause_flow_check_repair_card_count = 0;
  int total_release_repair_path_count = 0;
  int release_repair_path_count = 0;
  int release_current_repo_repair_path_count = 0;
  int release_target_or_validation_repair_path_count = 0;
  int repair_path_source_location_gap_count = 0;
  int linked_root_cause_to_release_path_count = 0;
  int linked_root_cause_repair_card_count = 0;
  int unmatched_release_repair_path_count = 0;
  int unmatched_root_cause_repair_card_count = 0;
  bool first_release_repair_path_present = false;
  std::string first_release_repair_path_id;
  std::string first_release_repair_path_category;
  std::string first_release_repair_environment;
  std::string first_release_repair_source_location;
  std::string recommended_action;
  bool current_repo_action_required = false;
  bool target_or_validation_action_required = false;
  std::string link_status = "missing_inputs";
  std::size_t upstream_source_contract_issue_count = 0;
  std::size_t link_contract_issue_count = 0;
  std::size_t source_contract_issue_count = 0;
};

struct ReleasePipelineRepairProjection {
  explicit ReleasePipelineRepairProjection(
      const ReleasePipelineSourceContext& sources);

  std::vector<NonGpuReviewRepairPath> paths;
  std::vector<ActiveRepairPathGroup> groups;
  std::size_t environment_group_count = 0;
  std::size_t category_group_count = 0;
  std::size_t current_repo_group_count = 0;
  std::size_t target_or_validation_group_count = 0;
};

}  // namespace inferlab_release_pipeline
