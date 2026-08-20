#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_native_audit {

struct FileAudit {
  std::string file;
  std::string subsystem;
  std::string review_status;
  std::vector<std::string> issues_found;
  std::vector<std::string> issues_fixed;
  std::string remaining_risk;
  std::string claim_boundary_risk;
  std::string path_redaction_status;
  std::string checked_arithmetic_status;
  std::string artifact_contract_status;
  std::string review_notes;
  std::string read_status = "not_attempted";
  std::string read_error;
};

struct IssueSpec {
  std::string id;
  std::string title;
  std::string repair_environment;
  std::string severity;
  std::string current_repo_action;
  std::string target_machine_action;
};

struct SourceRootStatus {
  std::string root;
  bool present = false;
  bool directory = false;
  bool readable = false;
  std::string status = "missing";
  std::string error;
};

struct NativeSourceScan {
  std::vector<SourceRootStatus> roots;
  std::vector<std::filesystem::path> files;
};

struct ReviewOverride {
  std::string file;
  std::string review_status;
  std::vector<std::string> issues_fixed;
  std::string remaining_risk;
  std::string claim_boundary_risk;
  std::string path_redaction_status;
  std::string checked_arithmetic_status;
  std::string artifact_contract_status;
  std::string review_notes;
  std::string invalid_reason;
  bool valid = false;
  bool applied = false;
};

struct ReviewManifest {
  bool configured = false;
  bool present = false;
  bool readable = false;
  bool parse_ok = false;
  std::string path_label = "not_configured";
  std::string read_status = "not_configured";
  std::string read_error;
  std::vector<ReviewOverride> overrides;
  std::size_t invalid_override_count = 0;
  std::size_t applied_override_count = 0;
  std::size_t unmatched_override_count = 0;
  std::size_t closed_file_count = 0;
};

}  // namespace inferlab_native_audit
