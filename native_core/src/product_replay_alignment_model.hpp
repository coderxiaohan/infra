#pragma once

#include "artifact_evidence_contract.hpp"
#include "product_profile_artifact_contract.hpp"
#include "product_replay_alignment_fixture.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::product_replay_alignment {

struct ProductArtifactState {
  const mature_product_profiles::ProductProfileSpec* profile = nullptr;
  std::filesystem::path resolved_path;
  mature_product_profiles::ProductProfileArtifactContract contract;
  bool read_issue = false;
};

struct NativeArtifactState {
  NativeArtifactIdentity identity;
  std::filesystem::path resolved_path;
  inferlab_evidence::ArtifactContractProjection contract;
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string text_read_status = "not_checked";
  std::string text_read_error;
  bool regular_file = false;
  bool exact_artifact_type = false;
  bool required_fields_present = false;
  bool required_metrics_present = false;
  bool deep_dive_type_present = false;
  bool contract_ready = false;
  bool read_issue = false;
  std::vector<std::string> missing_required_fields;
  std::vector<std::string> missing_required_metrics;
  std::vector<std::string> issues;
};

struct AlignmentRecord {
  const AlignmentRuleSpec* rule = nullptr;
  std::vector<std::size_t> product_artifact_indices;
  std::vector<std::size_t> native_artifact_indices;
  std::size_t product_present_count = 0U;
  std::size_t product_contract_ready_count = 0U;
  std::size_t product_replayed_count = 0U;
  std::size_t product_executed_count = 0U;
  std::size_t product_evidence_count = 0U;
  std::size_t native_present_count = 0U;
  std::size_t native_contract_ready_count = 0U;
  std::size_t read_issue_count = 0U;
  double product_coverage = 0.0;
  double product_contract_coverage = 0.0;
  double replay_evidence_coverage = 0.0;
  double native_coverage = 0.0;
  double structural_alignment_coverage = 0.0;
  double alignment_score = 0.0;
  bool product_contract_complete = false;
  bool native_contract_complete = false;
  bool full_product_evidence_coverage = false;
  std::string status;
  std::string recommendation;
};

struct AlignmentSummary {
  std::size_t rule_count = 0U;
  std::size_t aligned_rule_count = 0U;
  std::size_t fully_aligned_rule_count = 0U;
  std::size_t dry_run_rule_count = 0U;
  std::size_t pending_rule_count = 0U;
  std::size_t blocked_rule_count = 0U;
  std::size_t unique_product_artifact_count = 0U;
  std::size_t unique_native_artifact_count = 0U;
  std::size_t product_contract_ready_count = 0U;
  std::size_t native_contract_ready_count = 0U;
  std::size_t replay_evidence_product_count = 0U;
  std::size_t execute_evidence_product_count = 0U;
  std::size_t read_issue_count = 0U;
  std::size_t physical_artifact_read_count = 0U;
  std::size_t best_record_index = 0U;
  std::string best_alignment_rule;
  double best_alignment_score = 0.0;
  std::string alignment_decision = "blocked";
};

struct AlignmentPlan {
  std::vector<ProductArtifactState> product_artifacts;
  std::vector<NativeArtifactState> native_artifacts;
  std::vector<AlignmentRecord> records;
  std::vector<std::string> product_display_names;
  std::vector<std::string> recommended_next_commands;
  AlignmentSummary summary;
  std::string claim_boundary;
};

const mature_product_profiles::ProductProfileSpec& product_profile(
    const ProductArtifactState& state);
const AlignmentRuleSpec& alignment_rule(const AlignmentRecord& record);
AlignmentPlan build_alignment_plan(
    const std::filesystem::path& results_root);

}  // namespace inferlab::product_replay_alignment
