#pragma once

#include "product_profile_artifact_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::product_profile_contract_check {

struct ArtifactSpec {
  std::string artifact_type;
  std::string target;
  std::string artifact;
  std::string report;
  std::string refresh_command;
  std::string claim_scope;
};

enum class RecordDecision {
  pass,
  pending_missing_artifact,
  not_selected_optional,
  fail,
};

enum class AuditDecision {
  ok,
  pending,
  failed,
};

struct AuditRecord {
  std::filesystem::path resolved_path;
  mature_product_profiles::ProductProfileArtifactContract contract;
  RecordDecision decision = RecordDecision::fail;
  std::string recommendation;
};

struct AuditSummary {
  std::size_t product_count = 0U;
  std::size_t default_required_product_count = 0U;
  std::size_t optional_on_demand_product_count = 0U;
  std::size_t selected_product_count = 0U;
  std::size_t present_artifacts = 0U;
  std::size_t missing_artifacts = 0U;
  std::size_t required_missing_artifacts = 0U;
  std::size_t passed_records = 0U;
  std::size_t failed_records = 0U;
  std::size_t pending_records = 0U;
  std::size_t not_selected_optional_records = 0U;
  std::size_t readable_artifacts = 0U;
  std::size_t parsed_artifacts = 0U;
  std::size_t read_issue_count = 0U;
  std::size_t malformed_or_invalid_artifact_count = 0U;
  std::size_t boundary_pass_count = 0U;
  std::size_t execute_gate_pass_count = 0U;
  std::size_t mode_flag_pass_count = 0U;
  std::size_t claim_scope_pass_count = 0U;
  std::size_t product_evidence_eligible_count = 0U;
  std::size_t performance_claim_eligible_count = 0U;
  std::size_t total_required_fields = 0U;
  std::size_t total_present_fields = 0U;
  std::size_t total_required_checks = 0U;
  std::size_t total_satisfied_checks = 0U;
  std::size_t physical_artifact_read_count = 0U;
  double required_field_pass_rate = 0.0;
  double contract_coverage = 0.0;
};

struct AuditPlan {
  std::vector<AuditRecord> records;
  AuditSummary summary;
  bool strict_missing = false;
  AuditDecision decision = AuditDecision::failed;
};

const ArtifactSpec& artifact_spec();
const char* audit_status(AuditDecision decision);
const char* record_status(RecordDecision decision);

AuditPlan build_audit(const std::filesystem::path& results_root,
                      bool strict_missing);

}  // namespace inferlab::product_profile_contract_check
