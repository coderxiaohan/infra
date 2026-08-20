#include "evidence_query_filter_view.hpp"

#include "native_core_audit_field_view.hpp"
#include "plan_json_utils.hpp"

#include <sstream>

namespace inferlab_evidence_query {

std::string native_core_audit_filter_query_json(
    const NativeCoreAuditFilterSignals& signals, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"audit_register_present\": "
      << inferlab_plan::json_bool(signals.audit_register_present) << ",\n";
  out << nested << "\"audit_register_readable\": "
      << inferlab_plan::json_bool(signals.audit_register_readable) << ",\n";
  out << nested << "\"audit_register_read_status\": "
      << inferlab_plan::json_string(signals.audit_register_read_status)
      << ",\n";
  out << nested << "\"audit_summary_present\": "
      << inferlab_plan::json_bool(signals.audit_summary_present) << ",\n";
  out << nested << "\"read_issue_count\": " << signals.read_issue_count
      << ",\n";
  inferlab_evidence::write_native_core_audit_direct_catalog_fields(
      out, nested, signals);
  out << nested << "\"filter_decision\": "
      << inferlab_plan::json_string(signals.filter_decision) << ",\n";
  out << nested << "\"blocker_count\": "
      << native_core_audit_blocker_count(signals) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
