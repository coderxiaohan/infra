#include "release_evidence_pipeline_native_audit_closure_view.hpp"

#include "checked_index.hpp"
#include "native_core_audit_field_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string native_audit_closure_json(
    const ReleasePipelineNativeAuditClosureProjection& projection,
    const int indent) {
  const auto& signals = projection.signals;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline native audit closure field indent");
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const std::string summary_pad = inferlab_plan::json_indent(
      inferlab::internal::checked_add_int(
          field_indent, 2,
          "release pipeline native audit closure summary indent"));

  std::ostringstream out;
  out << pad << "{\n";
  out << field_pad << "\"source\": "
      << inferlab_plan::json_string(
             projection.evidence_query_artifact.artifact)
      << ",\n";
  out << field_pad << "\"direct_artifact_present\": "
      << inferlab_plan::json_bool(projection.evidence_query_direct_present)
      << ",\n";
  out << field_pad << "\"filter_query_present\": "
      << inferlab_plan::json_bool(signals.filter_query_present) << ",\n";
  out << field_pad << "\"source_artifact\": "
      << inferlab_plan::json_string(projection.audit_register_artifact.artifact)
      << ",\n";
  out << field_pad << "\"audit_register_direct_present\": "
      << inferlab_plan::json_bool(projection.audit_register_direct_present)
      << ",\n";
  out << field_pad << "\"audit_register_readable\": "
      << inferlab_plan::json_bool(projection.audit_register_readable)
      << ",\n";
  out << field_pad << "\"audit_register_read_status\": "
      << inferlab_plan::json_string(projection.audit_register_read_status)
      << ",\n";
  out << field_pad << "\"audit_summary_present\": "
      << inferlab_plan::json_bool(signals.audit_summary_present) << ",\n";
  out << field_pad << "\"audit_blocker_count\": "
      << projection.blocker_count << ",\n";
  out << field_pad << "\"source_contract_issue_count\": "
      << projection.source_contract_issue_count << ",\n";
  out << field_pad << "\"summary\": {\n";
  inferlab_evidence::write_native_core_audit_direct_catalog_fields(
      out, summary_pad, signals);
  out << summary_pad << "\"filter_decision\": "
      << inferlab_plan::json_string(projection.filter_decision) << "\n";
  out << field_pad << "},\n";
  out << field_pad << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(
             {projection.audit_register_artifact.command,
              "close or explicitly defer open native_core review rows",
              projection.evidence_query_artifact.command},
             field_indent)
      << ",\n";
  out << field_pad << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(
             {"after static audit blockers are closed, run target-machine native validation before execute claims"},
             field_indent)
      << ",\n";
  out << field_pad
      << "\"claim_boundary\": \"native-core audit closure summarizes static audit-register evidence only; it does not compile, execute, benchmark, or prove runtime correctness\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
