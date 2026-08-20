#include "evidence_query_source_projection.hpp"

#include "checked_index.hpp"
#include "evidence_catalog_flow_projection.hpp"
#include "evidence_domain_utils.hpp"
#include "evidence_fixture_corpus_model.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_warehouse_index_model.hpp"
#include "native_core_audit_register_projection.hpp"
#include "repository_evidence_consumer_registry.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {
namespace {

std::vector<inferlab_artifact::WarehouseArtifactRecord> fixture_scan_records(
    const std::filesystem::path& fixture_dir) {
  const auto corpus =
      inferlab_fixture_corpus::load_fixture_corpus(fixture_dir);
  std::vector<inferlab_artifact::WarehouseArtifactRecord> records;
  records.reserve(corpus.fixtures.empty() ? 1U : corpus.fixtures.size());
  for (const auto& fixture : corpus.fixtures) {
    inferlab_artifact::WarehouseArtifactRecord record;
    const std::string fixture_id = fixture.fixture_id.empty()
                                       ? fixture.filename
                                       : fixture.fixture_id;
    record.artifact = "fixture:" + fixture_id;
    record.present = fixture.regular_file && fixture.readable;
    record.matrix_present = true;
    record.summary_present = fixture.summary_present;
    record.prometheus_present = true;
    record.report_present =
        fixture.actual_blocker !=
        inferlab_evidence::BlockerCategory::kReportWiringMissing;
    record.final_report_present =
        fixture.actual_blocker !=
        inferlab_evidence::BlockerCategory::kReportWiringMissing;
    record.contract_present = fixture.contract_valid;
    record.freshness_present =
        fixture.actual_blocker !=
        inferlab_evidence::BlockerCategory::kFreshnessStale;

    record.metadata.source = fixture.path_label;
    record.metadata.present = record.present;
    record.metadata.parse_ok = fixture.document_valid;
    record.metadata.schema_present = fixture.schema_version_valid;
    record.metadata.summary_present = record.summary_present;
    record.metadata.claim_scope_present = fixture.claim_scope_present;
    record.metadata.executed_present = fixture.executed_present;
    record.metadata.replayed_present = fixture.replayed_present;
    record.metadata.executed = fixture.executed;
    record.metadata.replayed = fixture.replayed;
    record.metadata.blocker_count =
        fixture.actual_blocker == inferlab_evidence::BlockerCategory::kNone
            ? 0U
            : std::max<std::size_t>(1U, fixture.blockers.size());
    record.metadata.artifact_type = fixture.artifact_type;
    record.metadata.mode =
        "fixture_confidence:" +
        std::string(inferlab_evidence::evidence_level_name(
            fixture.actual_confidence));
    record.metadata.claim_scope = fixture.claim_scope;
    record.metadata.release_decision =
        inferlab_evidence::release_decision_name(
            fixture.actual_release_decision);
    record.metadata.warnings = fixture.contract_issues;
    record.evidence_projection.resolved = true;
    record.evidence_projection.blocker_category =
        inferlab_evidence::blocker_category_name(fixture.actual_blocker);
    record.evidence_projection.repair_environment =
        inferlab_evidence::repair_environment_name(
            fixture.actual_repair_environment);
    record.evidence_projection.confidence =
        inferlab_evidence::evidence_level_name(fixture.actual_confidence);
    record.evidence_projection.affected_claims =
        fixture.expected.forbidden_claims;
    records.push_back(std::move(record));
  }
  if (records.empty()) {
    inferlab_artifact::WarehouseArtifactRecord record;
    record.artifact = "fixture:manifest";
    record.present = corpus.manifest.regular_file && corpus.manifest.readable;
    record.metadata.source = corpus.manifest.path_label;
    record.metadata.present = record.present;
    record.metadata.parse_ok = corpus.manifest.document_valid;
    record.metadata.schema_present = corpus.manifest.schema_version_valid;
    record.metadata.blocker_count = 1U;
    record.metadata.artifact_type = "fixture_manifest";
    record.metadata.mode = "fixture_confidence:stale_or_blocked";
    record.metadata.release_decision = "blocked";
    record.metadata.warnings = corpus.manifest.contract_issues;
    record.evidence_projection.resolved = true;
    record.evidence_projection.blocker_category =
        record.present ? "schema_missing" : "artifact_missing";
    record.evidence_projection.repair_environment =
        inferlab_evidence::repair_environment(
            record.evidence_projection.blocker_category);
    record.evidence_projection.confidence = "stale_or_blocked";
    record.evidence_projection.affected_claims = {
        "fixture_query", "fixture_release_gate", "fixture_report_confidence"};
    records.push_back(std::move(record));
  }
  return records;
}

}  // namespace

EvidenceQuerySourceProjection project_evidence_query_sources(
    const std::filesystem::path& warehouse_path,
    const std::filesystem::path& repo_root,
    const std::filesystem::path& results_path,
    const std::filesystem::path& fixture_dir,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  EvidenceQuerySourceProjection projection;
  projection.fixture_used = !fixture_dir.empty();
  if (projection.fixture_used) {
    projection.warehouse.path = warehouse_path.string();
    projection.warehouse.warnings.push_back(
        "warehouse input not read because fixture corpus was selected");
    projection.records = fixture_scan_records(fixture_dir);
  } else {
    projection.warehouse =
        inferlab_artifact::read_warehouse_index(warehouse_path);
    if (projection.warehouse.parsed) {
      projection.records = projection.warehouse.records;
    } else {
      projection.scan_sources =
          inferlab_evidence_warehouse::load_lineage_sources(repo_root);
      projection.scan_source_read_issue_count =
          inferlab_artifact::source_read_issue_count(projection.scan_sources);
      const auto lineage =
          inferlab_evidence_warehouse::project_managed_artifact_lineage(
              repo_root, results_path, projection.scan_sources, snapshots,
              "evidence_query_result");
      projection.records =
          inferlab_evidence_warehouse::copy_warehouse_records(lineage);
      for (auto& source : projection.scan_sources) source.text.clear();
    }
  }
  resolve_evidence_projections(projection.records);
  std::sort(projection.records.begin(), projection.records.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.artifact < rhs.artifact;
            });
  projection.record_count = inferlab::internal::checked_int_from_size(
      projection.records.size(), "evidence query record count");
  projection.fixture_record_count =
      projection.fixture_used ? projection.record_count : 0;
  projection.warehouse_used =
      projection.warehouse.parsed && !projection.fixture_used;
  projection.direct_scan_used =
      !projection.fixture_used && !projection.warehouse.parsed;
  return projection;
}

CatalogFlowFilterSignals project_catalog_flow_filter_signals(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  const auto& catalog_snapshot = snapshots.get(
      results_path /
      inferlab_repository_dag::require_direct_consumer_artifact(
          "evidence_query", "catalog")
          .artifact);
  inferlab_evidence::CatalogFlowProjectionOptions options;
  options.fallback_removal.infer_catalog_section_presence = true;
  options.fallback_removal.unknown_primary_environment_when_present = true;
  CatalogFlowFilterSignals signals;
  static_cast<inferlab_evidence::CatalogFlowSignals&>(signals) =
      inferlab_evidence::project_catalog_flow_signals(
          catalog_snapshot.read_result, options);
  if (!signals.catalog_artifact_present) {
    signals.filter_decision = "catalog_flow_catalog_missing";
  } else if (signals.read_issue_count > 0U) {
    signals.filter_decision = "catalog_flow_catalog_invalid";
  } else if (signals.discrepancy_count > 0U || signals.input_gap_count > 0U ||
             signals.missing_surface_count > 0U ||
             signals.missing_step_count > 0U ||
             signals.consumer_field_coverage_field_gap_count > 0U) {
    signals.filter_decision = "catalog_flow_repair_required";
  } else if (
      signals.surface_parity_refresh_required_count > 0U ||
      signals.surface_migration_fallback_count > 0U ||
      signals.surface_consumer_fallback_removal_blocked_count > 0U ||
      signals.surface_consumer_parity_refresh_required_count > 0U ||
      signals.fallback_removal_queue_blocked_count > 0U ||
      signals.fallback_removal_queue_parity_refresh_required_count > 0U ||
      inferlab_evidence::catalog_flow_fallback_removal_shortest_path_active(
          signals) ||
      signals.consumer_field_coverage_parity_refresh_required_count > 0U) {
    signals.filter_decision =
        "catalog_flow_surface_parity_refresh_required";
  } else if (signals.metadata_policy_card_count > 0U) {
    signals.filter_decision = "catalog_flow_metadata_policy_warning";
  } else {
    signals.filter_decision = "catalog_flow_clear";
  }
  return signals;
}

NativeCoreAuditFilterSignals project_native_core_audit_filter_signals(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  const auto& audit_snapshot =
      snapshots.get(
          results_path /
          inferlab_repository_dag::require_direct_consumer_artifact(
              "evidence_query", "native_audit")
              .artifact);
  auto projection = inferlab_evidence::project_native_core_audit_register(
      audit_snapshot.metadata, audit_snapshot.read_result);
  NativeCoreAuditFilterSignals signals;
  static_cast<inferlab_evidence::NativeCoreAuditProjectionSignals&>(signals) =
      std::move(projection.signals);
  signals.filter_decision = std::move(projection.filter_decision);
  return signals;
}

}  // namespace inferlab_evidence_query
