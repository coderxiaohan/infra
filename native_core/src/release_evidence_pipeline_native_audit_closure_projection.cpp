#include "release_evidence_pipeline_native_audit_closure_projection.hpp"

#include "native_core_audit_field_catalog.hpp"

#include <string>

namespace inferlab_release_pipeline {
namespace {

std::string closure_filter_decision(
    const inferlab_evidence::NativeCoreAuditMergedProjection& audit) {
  if (!audit.filter_decision.empty()) return audit.filter_decision;
  if (inferlab_evidence::native_core_audit_has_meaningful_string(
          audit.signals.audit_decision)) {
    return audit.signals.audit_decision;
  }
  return "unknown";
}

std::size_t direct_audit_source_contract_issue_count(
    const ReleasePipelineSourceContext& sources) {
  const auto& snapshot = *sources.native_audit;
  const bool source_issue =
      inferlab_evidence::has_read_issue(snapshot.read_result) ||
      (snapshot.metadata.present &&
       (!snapshot.metadata.parse_ok ||
        !sources.native_audit_projection.signals.audit_summary_present));
  return source_issue ? 1U : 0U;
}

}  // namespace

ReleasePipelineNativeAuditClosureProjection::
    ReleasePipelineNativeAuditClosureProjection(
        const ReleasePipelineSourceContext& sources)
    : evidence_query_artifact(
          inferlab_high_value::require_managed_artifact(
              "evidence_query_result")),
      audit_register_artifact(
          inferlab_high_value::require_managed_artifact(
              "native_core_audit_register")),
      signals(sources.native_audit_projection.signals),
      evidence_query_direct_present(sources.evidence_query->metadata.present),
      audit_register_direct_present(sources.native_audit->metadata.present),
      audit_register_readable(sources.native_audit->read_result.readable),
      audit_register_read_status(sources.native_audit->read_result.status),
      blocker_count(sources.native_audit_projection.blocker_count),
      source_contract_issue_count(
          direct_audit_source_contract_issue_count(sources)),
      filter_decision(
          closure_filter_decision(sources.native_audit_projection)) {}

}  // namespace inferlab_release_pipeline
