#include "release_evidence_pipeline_source_context.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

const inferlab_evidence::EvidenceSnapshot& managed_snapshot(
    const std::filesystem::path& results,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots,
    const std::string& catalog_id) {
  return snapshots.get(
      results /
      inferlab_high_value::require_managed_artifact(catalog_id).artifact);
}

const std::string& readable_document(
    const inferlab_evidence::EvidenceSnapshot& snapshot) {
  static const std::string empty_document;
  return snapshot.read_result.readable ? snapshot.read_result.text
                                       : empty_document;
}

std::string summary_or_root(
    const inferlab_evidence::EvidenceSnapshot& snapshot) {
  return inferlab_artifact::json_object_section_or_root_text(
      snapshot.read_result, "summary");
}

inferlab_evidence::NativeCoreAuditMergedProjection project_native_audit(
    const inferlab_evidence::EvidenceSnapshot& evidence_query,
    const inferlab_evidence::EvidenceSnapshot& audit_register) {
  return inferlab_evidence::merge_native_core_audit_projections(
      inferlab_evidence::project_native_core_audit_query(
          evidence_query.metadata, evidence_query.read_result),
      inferlab_evidence::project_native_core_audit_register(
          audit_register.metadata, audit_register.read_result));
}

std::size_t checked_present_source_count(
    const std::vector<const inferlab_evidence::EvidenceSnapshot*>& sources) {
  std::size_t count = 0;
  for (const auto* source : sources) {
    if (source == nullptr) {
      throw std::logic_error("release pipeline source snapshot is null");
    }
    if (!source->metadata.present) continue;
    count = inferlab::internal::checked_add_size(
        count, 1U, "release pipeline present source count");
  }
  return count;
}

std::size_t checked_read_issue_count(
    const std::vector<const inferlab_evidence::EvidenceSnapshot*>& sources) {
  std::size_t count = 0;
  for (const auto* source : sources) {
    if (source == nullptr) {
      throw std::logic_error("release pipeline source snapshot is null");
    }
    if (!inferlab_evidence::has_read_issue(source->read_result)) continue;
    count = inferlab::internal::checked_add_size(
        count, 1U, "release pipeline source read-issue count");
  }
  return count;
}

}  // namespace

ReleasePipelineSourceContext::ReleasePipelineSourceContext(
    const std::filesystem::path& results,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots)
    : evidence_query(
          &managed_snapshot(results, snapshots, "evidence_query_result")),
      native_audit(
          &managed_snapshot(results, snapshots, "native_core_audit_register")),
      evidence_query_document(readable_document(*evidence_query)),
      evidence_query_summary_projection(
          inferlab_evidence::strict_evidence_query_summary_or_root(
              evidence_query->read_result)),
      evidence_query_summary(evidence_query_summary_projection.text),
      native_audit_document(readable_document(*native_audit)),
      native_audit_summary(summary_or_root(*native_audit)),
      snapshot_diff(
          inferlab_evidence::project_evidence_query_snapshot_diff_signals(
              evidence_query->read_result,
              evidence_query_summary_projection.text)),
      navigation(
          inferlab_evidence::project_evidence_query_navigation_signals(
              evidence_query->read_result,
              evidence_query_summary_projection.text)),
      product_replay_readiness(
          inferlab_evidence::project_product_replay_readiness(
              results, snapshots)),
      native_audit_projection(
          project_native_audit(*evidence_query, *native_audit)) {
  const std::vector<const inferlab_evidence::EvidenceSnapshot*> sources = {
      evidence_query, native_audit};
  present_source_count = checked_present_source_count(sources);
  read_issue_count = checked_read_issue_count(sources);
}

}  // namespace inferlab_release_pipeline
