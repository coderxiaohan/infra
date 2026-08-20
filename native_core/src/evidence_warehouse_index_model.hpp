#pragma once

#include "artifact_metadata_reader.hpp"
#include "artifact_text_source.hpp"
#include "evidence_release_policy.hpp"
#include "high_value_artifact_catalog.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_evidence_warehouse {

enum class WarehouseDecision {
  kLineageReady,
  kBlocked,
};

enum class InventoryState {
  kEmpty,
  kPartial,
  kComplete,
};

const char* warehouse_decision_name(WarehouseDecision decision);
const char* inventory_state_name(InventoryState state);

struct LineageEdge {
  std::string artifact;
  std::string relation;
  std::string target;
  std::string authority;
  bool present = false;
  std::string gap_category;
  std::string repair_action;
};

struct ManagedArtifactLineage {
  const inferlab_high_value::ManagedArtifact* spec = nullptr;
  inferlab_artifact::WarehouseArtifactRecord warehouse_record;
  std::string artifact_path_status = "missing";
  bool current_output_deferred = false;
  bool producer_target_present = false;
  bool evidence_bundle_present = false;
  std::size_t ready_edge_count = 0U;
  std::size_t required_edge_count = 0U;
  std::vector<LineageEdge> edges;
};

struct ManagedLineageProjection {
  std::vector<inferlab_artifact::TextSource> sources;
  std::vector<ManagedArtifactLineage> records;
  std::size_t source_read_issue_count = 0U;
  std::size_t ready_edge_count = 0U;
  std::size_t required_edge_count = 0U;
};

struct RawArtifactScanIssue {
  std::string artifact;
  std::string category;
  std::string message;
};

struct RawArtifactScan {
  std::string directory_status = "missing";
  std::vector<inferlab_artifact::Metadata> artifacts;
  std::vector<RawArtifactScanIssue> issues;
  std::size_t skipped_non_json_count = 0U;
  std::size_t skipped_current_output_count = 0U;
};

struct WarehouseBlocker {
  std::string artifact;
  std::string category;
  std::string message;
  std::string repair_environment;
  std::string exact_wiring_location;
  std::string required_action;
};

struct WarehouseSummary {
  std::size_t artifact_count = 0U;
  std::size_t managed_artifact_count = 0U;
  std::size_t indexed_artifact_count = 0U;
  std::size_t parse_ready_artifact_count = 0U;
  std::size_t structured_parser_artifact_count = 0U;
  std::size_t current_output_deferred_count = 0U;
  std::size_t missing_target_count = 0U;
  std::size_t missing_report_count = 0U;
  std::size_t missing_matrix_count = 0U;
  std::size_t missing_summary_count = 0U;
  std::size_t missing_prometheus_count = 0U;
  std::size_t missing_final_report_count = 0U;
  std::size_t missing_contract_count = 0U;
  std::size_t missing_freshness_count = 0U;
  std::size_t missing_bundle_count = 0U;
  std::size_t source_file_count = 0U;
  std::size_t source_read_issue_count = 0U;
  std::size_t raw_scan_issue_count = 0U;
  std::size_t physical_artifact_read_count = 0U;
  std::size_t ready_edge_count = 0U;
  std::size_t required_edge_count = 0U;
  std::size_t blocker_count = 0U;
  double lineage_score = 0.0;
  InventoryState inventory_state = InventoryState::kEmpty;
  WarehouseDecision decision = WarehouseDecision::kBlocked;
};

struct WarehousePlan {
  const inferlab_high_value::ManagedArtifact* spec = nullptr;
  const inferlab_high_value::DependencySpec* json_dependency = nullptr;
  const inferlab_high_value::DependencySpec* sqlite_dependency = nullptr;
  ManagedLineageProjection lineage;
  RawArtifactScan raw_scan;
  WarehouseSummary summary;
  std::vector<WarehouseBlocker> blockers;
  std::vector<std::string> pending_artifact_refresh_commands;
  std::vector<std::string> recommended_next_commands;
};

std::vector<inferlab_artifact::TextSource> load_lineage_sources(
    const std::filesystem::path& repo_root);

ManagedLineageProjection project_managed_artifact_lineage(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& results_root,
    const std::vector<inferlab_artifact::TextSource>& sources,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots,
    const std::string& current_output_artifact_id = {});

std::vector<inferlab_artifact::WarehouseArtifactRecord>
copy_warehouse_records(const ManagedLineageProjection& projection);

WarehousePlan build_warehouse_plan(
    const std::filesystem::path& repo_root,
    const std::filesystem::path& results_root,
    const std::filesystem::path& output_path);

}  // namespace inferlab_evidence_warehouse
