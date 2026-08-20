#include "release_evidence_pipeline_artifact_sections.hpp"

#include "native_core_audit_field_view.hpp"
#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include <ostream>
#include <string>

namespace inferlab_release_pipeline::detail {
namespace {

void validate_native_audit_summary_keys() {
  static const bool validated = [] {
    inferlab_evidence::validate_native_core_audit_output_keys(
        {"native_core_audit_filter_query_present",
         "native_core_audit_register_present",
         "native_core_audit_register_readable",
         "native_core_audit_register_read_status",
         "native_core_audit_blocker_count",
         "native_core_audit_source_contract_issue_count",
         "native_core_audit_filter_read_issue_count",
         "native_core_audit_filter_decision"},
        "release pipeline summary");
    return true;
  }();
  (void)validated;
}

}  // namespace

void append_release_pipeline_summary_native_audit_and_close(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection) {
  const auto& request = projection.request;
  const auto& audit = request.native_audit_closure;
  const auto& signals = audit.signals;
  validate_native_audit_summary_keys();

  append_repair_metric_fields(out, "native_core_audit",
                              projection.repair_summary.native_core_audit);
  append_repair_metric_fields(
      out, "native_core_audit_scope",
      projection.repair_summary.native_core_audit_scope);
  append_repair_metric_fields(
      out, "native_core_audit_backlog",
      projection.repair_summary.native_core_audit_backlog);
  append_bool_field(out, "native_core_audit_filter_query_present",
                    signals.filter_query_present);
  append_bool_field(out, "native_core_audit_register_present",
                    signals.audit_register_present ||
                        audit.audit_register_direct_present);
  append_bool_field(out, "native_core_audit_register_readable",
                    signals.audit_register_readable);
  append_string_field(out, "native_core_audit_register_read_status",
                      signals.audit_register_read_status);
  inferlab_evidence::write_native_core_audit_catalog_fields(
      out, "    ", signals);
  append_count_field(out, "native_core_audit_blocker_count",
                     audit.blocker_count);
  append_count_field(out, "native_core_audit_source_contract_issue_count",
                     audit.source_contract_issue_count);
  append_count_field(out, "native_core_audit_filter_read_issue_count",
                     signals.read_issue_count);
  append_string_field(out, "native_core_audit_filter_decision",
                      audit.filter_decision.empty() ? "unknown"
                                                    : audit.filter_decision);

  out << evidence_query_operation_summary_fields_json(
      request.evidence_query_closure, 4);
  out << "    \"evidence_confidence\": "
      << inferlab_plan::json_string(request.decision.evidence_confidence)
      << "\n";
  out << "  },\n";
}

}  // namespace inferlab_release_pipeline::detail
