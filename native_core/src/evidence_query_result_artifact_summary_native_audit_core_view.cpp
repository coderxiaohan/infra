#include "evidence_query_result_artifact_sections.hpp"

#include "native_core_audit_field_view.hpp"
#include "plan_json_utils.hpp"

namespace inferlab_evidence_query::detail {

void append_result_artifact_summary_native_audit_core(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection) {
  const auto& signals = projection.request.native_core_audit;
  static const bool keys_validated = [] {
    inferlab_evidence::validate_native_core_audit_output_keys(
        {"native_core_audit_filter_query_present",
         "native_core_audit_register_present",
         "native_core_audit_register_readable",
         "native_core_audit_register_read_status",
         "native_core_audit_filter_read_issue_count",
         "native_core_audit_blocker_count",
         "native_core_audit_filter_decision"},
        "Evidence Query summary");
    return true;
  }();
  (void)keys_validated;

  out << "    \"native_core_audit_filter_query_present\": true,\n";
  out << "    \"native_core_audit_register_present\": "
      << inferlab_plan::json_bool(signals.audit_register_present) << ",\n";
  out << "    \"native_core_audit_register_readable\": "
      << inferlab_plan::json_bool(signals.audit_register_readable) << ",\n";
  out << "    \"native_core_audit_register_read_status\": "
      << inferlab_plan::json_string(signals.audit_register_read_status)
      << ",\n";
  out << "    \"native_core_audit_filter_read_issue_count\": "
      << signals.read_issue_count << ",\n";
  out << "    \"native_core_audit_blocker_count\": "
      << native_core_audit_blocker_count(signals) << ",\n";
  inferlab_evidence::write_native_core_audit_catalog_fields(out, "    ",
                                                            signals);
  out << "    \"native_core_audit_filter_decision\": "
      << inferlab_plan::json_string(signals.filter_decision) << ",\n";
}

}  // namespace inferlab_evidence_query::detail
