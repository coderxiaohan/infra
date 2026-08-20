#pragma once

#include "artifact_text_source.hpp"
#include "mature_product_profile_registry.hpp"
#include "repository_wiring_text.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::product_integration_inventory {

struct ArtifactSpec {
  std::string artifact_type;
  std::string target;
  std::string artifact;
  std::string report;
  std::string benchmark_entry;
  std::string refresh_command;
  std::string claim_scope;
};

struct FileAudit {
  std::string repository_path;
  std::string path_status = "missing";
  std::string read_status = "missing";
  bool regular_file = false;
  bool readable = false;
  std::size_t read_issue_count = 0U;
};

struct CommonRegistryAudit {
  std::size_t product_entry_count = 0U;
  bool product_present = false;
  bool display_name_matches = false;
  bool category_matches = false;
  bool artifact_matches = false;
  bool report_matches = false;
  bool config_matches = false;
  bool scope_matches = false;
  bool boundary_lists_present = false;
};

struct FinalReportAudit {
  std::size_t module_count = 0U;
  std::size_t result_occurrence_count = 0U;
  std::size_t report_occurrence_count = 0U;
  bool result_present = false;
  bool report_present = false;
  bool code_wiring_present = false;
};

struct InventoryRecord {
  const mature_product_profiles::ProductProfileSpec* profile = nullptr;
  bool default_wiring_expected = false;
  FileAudit adapter;
  FileAudit config;
  FileAudit report;
  bool adapter_product_binding_present = false;
  bool config_document_valid = false;
  bool config_product_binding_present = false;
  bool config_report_binding_present = false;
  CommonRegistryAudit common_registry;
  std::size_t artifact_catalog_source_occurrence_count = 0U;
  bool artifact_catalog_source_present = false;
  bool artifact_catalog_scope_matches = false;
  inferlab_repository_wiring::ProductProfileMatrixEntryMatch matrix;
  bool benchmark_matrix_present = false;
  bool benchmark_dry_run_command_present = false;
  bool benchmark_replay_command_present = false;
  bool benchmark_matrix_scope_matches = false;
  std::size_t summary_profile_call_count = 0U;
  bool summary_source_present = false;
  bool summary_scope_matches = false;
  bool prometheus_source_present = false;
  bool prometheus_scope_matches = false;
  FinalReportAudit final_report;
  bool final_report_result_scope_matches = false;
  bool report_artifact_reference_present = false;
  bool report_boundary_present = false;
  int passed_checks = 0;
  int total_checks = 0;
  double integration_score = 0.0;
  bool ready = false;
  std::vector<std::string> missing_surfaces;
};

enum class Decision {
  source_read_issues,
  wiring_gaps,
  ready,
};

struct InventoryPlan {
  std::vector<inferlab_artifact::TextSource> source_files;
  std::vector<InventoryRecord> records;
  std::size_t unique_report_file_count = 0U;
  std::size_t physical_text_read_count = 0U;
  std::size_t source_read_issue_count = 0U;
  std::size_t adapter_read_issue_count = 0U;
  std::size_t config_read_issue_count = 0U;
  std::size_t report_read_issue_count = 0U;
  std::size_t total_read_issue_count = 0U;
  int product_count = 0;
  int default_required_product_count = 0;
  int optional_on_demand_product_count = 0;
  int passed_records = 0;
  int failed_records = 0;
  int passed_checks = 0;
  int total_checks = 0;
  double integration_score = 0.0;
  Decision decision = Decision::wiring_gaps;
};

const ArtifactSpec& artifact_spec();
const mature_product_profiles::ProductProfileSpec& record_profile(
    const InventoryRecord& record);
const char* decision_status(Decision decision);
const char* record_status(const InventoryRecord& record);
const char* record_recommendation(const InventoryRecord& record);

InventoryPlan build_inventory(const std::filesystem::path& repo_root);

}  // namespace inferlab::product_integration_inventory
