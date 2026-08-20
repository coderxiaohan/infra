#include "release_evidence_pipeline_alignment_view.hpp"

#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string release_pipeline_alignment_dimension_json(
    const ReleasePipelineAlignmentProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string item = inferlab_plan::json_indent(indent + 4);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dimension_count\": " << projection.dimensions.size()
      << ",\n";
  out << nested << "\"ready_dimension_count\": "
      << projection.ready_dimension_count << ",\n";
  out << nested << "\"blocking_dimension_count\": "
      << projection.blocking_dimension_count << ",\n";
  out << nested << "\"advisory_dimension_count\": "
      << projection.advisory_dimension_count << ",\n";
  out << nested << "\"required_check_count\": "
      << projection.required_check_count << ",\n";
  out << nested << "\"satisfied_check_count\": "
      << projection.satisfied_check_count << ",\n";
  out << nested << "\"issue_count\": " << projection.issue_count << ",\n";
  out << nested << "\"dimensions\": [";
  for (std::size_t index = 0U; index < projection.dimensions.size(); ++index) {
    const auto& dimension = projection.dimensions[index];
    out << "\n" << item << "{\n";
    out << item << "  \"id\": "
        << inferlab_plan::json_string(dimension.id) << ",\n";
    out << item << "  \"profile_id\": "
        << inferlab_plan::json_string(dimension.profile_id) << ",\n";
    out << item << "  \"status\": "
        << inferlab_plan::json_string(dimension.status) << ",\n";
    out << item << "  \"release_blocking\": "
        << inferlab_plan::json_bool(dimension.release_blocking) << ",\n";
    out << item << "  \"ready\": "
        << inferlab_plan::json_bool(dimension.ready) << ",\n";
    out << item << "  \"cross_artifact_reconciled\": "
        << inferlab_plan::json_bool(dimension.cross_artifact_reconciled)
        << ",\n";
    out << item << "  \"required_check_count\": "
        << dimension.required_check_count << ",\n";
    out << item << "  \"satisfied_check_count\": "
        << dimension.satisfied_check_count << ",\n";
    out << item << "  \"blocker_category\": "
        << inferlab_plan::json_string(dimension.blocker_category) << ",\n";
    out << item << "  \"repair_environment\": "
        << inferlab_plan::json_string(dimension.repair_environment) << ",\n";
    out << item << "  \"issues\": "
        << inferlab_plan::json_string_array(dimension.issues, indent + 6)
        << "\n";
    out << item << "}"
        << (index + 1U == projection.dimensions.size() ? "" : ",");
  }
  if (!projection.dimensions.empty()) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"cross-artifact configuration alignment only; advisory dimensions do not independently block release and no dimension proves target execution or performance\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
