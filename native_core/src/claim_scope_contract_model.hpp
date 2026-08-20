#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_claim_scope {

struct ClaimRecord {
  std::string artifact;
  std::string artifact_family;
  std::filesystem::path resolved_path;
  bool declared_in_matrix = false;
  bool present = false;
  std::uintmax_t byte_size = 0;
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string file_size_probe_error;
  std::string text_read_status = "not_checked";
  std::string text_read_error;
  bool valid_json_object = false;
  bool executed_field_present = false;
  bool executed_field_valid = false;
  bool executed_true = false;
  bool executed_false = false;
  bool execution_kind_present = false;
  bool claim_scope_present = false;
  bool mode_present = false;
  bool audit_type_present = false;
  bool status_present = false;
  bool external_boundary_present = false;
  bool not_implemented_boundary_present = false;
  bool command_or_config_present = false;
  bool version_or_environment_present = false;
  bool raw_metrics_present = false;
  bool risky_executed_without_evidence = false;
  std::size_t required_checks = 0;
  std::size_t passed_checks = 0;
  double claim_score = 0.0;
  std::vector<std::string> missing_claim_fields;
  std::string status;
  std::string recommendation;
};

struct MatrixSourceState {
  bool present = false;
  bool readable = false;
  bool empty = false;
  std::string read_status = "not_checked";
  std::string read_error;
  std::size_t read_issue_count = 0;
  std::size_t ignored_output_count = 0;
  std::size_t duplicate_output_count = 0;
  std::size_t self_output_deferred_count = 0;
  std::size_t issue_count = 0;
  std::vector<std::string> parse_issues;
};

struct ClaimScopeContractAudit {
  std::filesystem::path matrix_path;
  std::filesystem::path results_root;
  bool strict_missing = false;
  MatrixSourceState matrix_source;
  std::vector<ClaimRecord> records;
  std::size_t present_count = 0;
  std::size_t pass_count = 0;
  std::size_t warn_count = 0;
  std::size_t fail_count = 0;
  std::size_t pending_count = 0;
  std::size_t executed_true_count = 0;
  std::size_t executed_false_count = 0;
  std::size_t claim_scope_count = 0;
  std::size_t risky_executed_count = 0;
  std::size_t total_required_checks = 0;
  std::size_t total_passed_checks = 0;
  double claim_contract_score = 0.0;
  std::string status;
};

ClaimScopeContractAudit evaluate_claim_scope_contract(
    const std::filesystem::path& matrix_path,
    const std::filesystem::path& results_root,
    bool strict_missing);

}  // namespace inferlab_claim_scope
