#pragma once

#include "artifact_metadata_reader.hpp"
#include "artifact_text_source.hpp"
#include "evidence_query_consumer_filter_signals.hpp"
#include "evidence_release_policy.hpp"
#include "native_core_audit_projection.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace inferlab_evidence_query {

struct EvidenceQuerySourceProjection {
  inferlab_artifact::WarehouseIndex warehouse;
  std::vector<inferlab_artifact::TextSource> scan_sources;
  std::vector<inferlab_artifact::WarehouseArtifactRecord> records;
  std::size_t scan_source_read_issue_count = 0;
  int record_count = 0;
  int fixture_record_count = 0;
  bool fixture_used = false;
  bool warehouse_used = false;
  bool direct_scan_used = false;
};

EvidenceQuerySourceProjection project_evidence_query_sources(
    const std::filesystem::path& warehouse_path,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& results_path,
    const std::filesystem::path& fixture_dir,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots);

CatalogFlowFilterSignals project_catalog_flow_filter_signals(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots);

NativeCoreAuditFilterSignals project_native_core_audit_filter_signals(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots);

}  // namespace inferlab_evidence_query
