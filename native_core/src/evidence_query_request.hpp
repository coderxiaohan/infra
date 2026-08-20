#pragma once

#include "evidence_query_artifact_projection.hpp"
#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_blocker_list_projection.hpp"
#include "evidence_query_cli_parser.hpp"
#include "evidence_query_cross_artifact.hpp"
#include "evidence_query_operation_guidance_projection.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_native_trace_projection.hpp"
#include "evidence_query_release_source_projection.hpp"
#include "evidence_query_repair_card_projection.hpp"
#include "evidence_query_repair_collection_projection.hpp"
#include "evidence_query_repair_source_projection.hpp"
#include "evidence_query_selection.hpp"
#include "evidence_query_snapshot_diff.hpp"
#include "evidence_query_source_projection.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct EvidenceQueryRequest {
  EvidenceQueryRequest(
      std::filesystem::path warehouse_path_value,
      std::filesystem::path repo_root_value,
      std::filesystem::path results_path_value,
      std::filesystem::path fixture_dir_value,
      std::string query_value,
      std::string artifact_filter_value,
      std::string blocker_filter_value,
      std::filesystem::path before_snapshot_value,
      std::filesystem::path after_snapshot_value,
      std::filesystem::path selector_repair_card_output_value,
      std::string selector_repair_card_export_write_status_value);

  EvidenceQueryRequest(const EvidenceQueryRequest&) = delete;
  EvidenceQueryRequest& operator=(const EvidenceQueryRequest&) = delete;
  EvidenceQueryRequest(EvidenceQueryRequest&&) = delete;
  EvidenceQueryRequest& operator=(EvidenceQueryRequest&&) = delete;

  std::filesystem::path warehouse_path;
  std::filesystem::path repo_root;
  std::filesystem::path results_path;
  std::filesystem::path fixture_dir;
  std::string query;
  std::string artifact_filter;
  std::string blocker_filter;
  std::filesystem::path before_snapshot;
  std::filesystem::path after_snapshot;
  std::filesystem::path selector_repair_card_output;
  std::string selector_repair_card_export_write_status;

  inferlab_evidence::EvidenceSnapshotRepository snapshots;
  EvidenceQuerySourceProjection source;
  QueryOperation query_operation;
  QueryArtifactProjection query_artifacts;
  BlockerListProjection blocker_list;
  EvidenceQueryReleaseSourceProjection release_sources;
  CatalogFlowFilterSignals catalog_flow;
  NativeCoreAuditFilterSignals native_core_audit;
  SnapshotDiffSignals snapshot_diff;
  std::vector<RepairPathSummary> shortest_paths;
  RepairCollectionProjection repair_collection;
  std::vector<RepairCardGroup> repair_groups;
  RepairCardRenderProjection repair_cards;
  RepairCardSourceExactnessProjection source_exactness;
  BlockerDrilldown blocker_drilldown;
  SourceLocationCategoryCoverage source_location_coverage;
  SelectorResolution selector_resolution;
  OperationGuidanceProjection operation_guidance;
  SelectedOperationProjection selected_operation;
  ProductReplaySemanticLinkageSignals product_replay_linkage;
  inferlab_deep_dive_trace::TraceTokenJoinProjection native_trace_correlation;
  std::vector<CrossArtifactBlockerFamily> cross_artifact_families;
  ShowArtifactSelection show_artifact_selection;
  std::size_t filter_source_read_issue_count = 0;
  bool query_ready = false;
};

}  // namespace inferlab_evidence_query
