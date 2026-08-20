#include "native_core_audit_register_file_view.hpp"

#include "native_core_audit_register_view_utils.hpp"
#include "plan_json_utils.hpp"

#include <sstream>

namespace inferlab_native_audit {
namespace {

std::string file_audit_json(const FileAudit& audit, const int indent) {
  const int nested_indent =
      checked_view_indent(indent, 2, "file audit nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const int priority_rank = review_priority_rank(audit);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"file\": " << inferlab_plan::json_string(audit.file)
      << ",\n";
  out << nested << "\"subsystem\": "
      << inferlab_plan::json_string(audit.subsystem) << ",\n";
  out << nested << "\"review_lane\": "
      << inferlab_plan::json_string(review_lane_for(audit)) << ",\n";
  out << nested << "\"audit_phase\": "
      << inferlab_plan::json_string(audit_phase_for(audit)) << ",\n";
  out << nested << "\"review_status\": "
      << inferlab_plan::json_string(audit.review_status) << ",\n";
  out << nested << "\"completion_state\": "
      << inferlab_plan::json_string(file_completion_state_for(audit))
      << ",\n";
  out << nested << "\"blocker_category\": "
      << inferlab_plan::json_string(file_completion_blocker_category_for(audit))
      << ",\n";
  out << nested << "\"completion_blocker_count\": "
      << file_completion_blocker_count_for(audit) << ",\n";
  out << nested << "\"priority\": "
      << inferlab_plan::json_string(review_priority_label(priority_rank))
      << ",\n";
  out << nested << "\"priority_rank\": " << priority_rank << ",\n";
  out << nested << "\"issues_found\": "
      << inferlab_plan::json_string_array(audit.issues_found, nested_indent)
      << ",\n";
  out << nested << "\"issues_fixed\": "
      << inferlab_plan::json_string_array(audit.issues_fixed, nested_indent)
      << ",\n";
  out << nested << "\"remaining_risk\": "
      << inferlab_plan::json_string(audit.remaining_risk) << ",\n";
  out << nested << "\"claim_boundary_risk\": "
      << inferlab_plan::json_string(audit.claim_boundary_risk) << ",\n";
  out << nested << "\"path_redaction_status\": "
      << inferlab_plan::json_string(audit.path_redaction_status) << ",\n";
  out << nested << "\"checked_arithmetic_status\": "
      << inferlab_plan::json_string(audit.checked_arithmetic_status) << ",\n";
  out << nested << "\"artifact_contract_status\": "
      << inferlab_plan::json_string(audit.artifact_contract_status) << ",\n";
  out << nested << "\"read_status\": "
      << inferlab_plan::json_string(audit.read_status) << ",\n";
  out << nested << "\"read_error\": "
      << inferlab_plan::json_string(audit.read_error) << ",\n";
  out << nested << "\"exact_wiring_locations\": "
      << inferlab_plan::json_string_array({audit.file}, nested_indent)
      << ",\n";
  out << nested << "\"exact_wiring_location_count\": 1,\n";
  out << nested << "\"source_location_gap_count\": 0,\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(current_repo_action_for_file(audit))
      << ",\n";
  out << nested << "\"target_machine_action\": "
      << inferlab_plan::json_string(target_machine_action_for_file(audit))
      << ",\n";
  out << nested << "\"review_notes\": "
      << inferlab_plan::json_string(audit.review_notes) << ",\n";
  out << nested
      << "\"claim_boundary\": \"file audit row is static source-review metadata only; it does not prove compile, test, native execution, benchmark, replay, execute, CUDA timing, or release readiness\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string files_json(const std::vector<FileAudit>& audits,
                       const int indent) {
  const int item_indent =
      checked_view_indent(indent, 2, "file audit item indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < audits.size(); ++index) {
    out << file_audit_json(audits[index], item_indent)
        << (index == audits.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace inferlab_native_audit
