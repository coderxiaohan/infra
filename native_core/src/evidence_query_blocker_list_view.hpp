#pragma once

#include "evidence_query_blocker_list_projection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_evidence_query {

inline std::string blockers_json(const BlockerListProjection& projection,
                                 const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < projection.blocker_records.size();
       ++index) {
    const auto& record = projection.blocker_records[index];
    if (index > 0U) out << ",";
    out << "\n" << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(record.artifact) << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(record.category) << ",\n";
    out << nested << "  \"status\": "
        << inferlab_plan::json_string(record.status) << ",\n";
    out << nested
        << "  \"message\": \"artifact is missing, lacks required evidence metadata, needs replay/execute evidence, or is not wired into reports/gates\",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(record.repair_environment) << ",\n";
    out << nested << "  \"repair_action\": "
        << inferlab_plan::json_string(record.repair_action) << ",\n";
    out << nested << "  \"evidence_confidence\": "
        << inferlab_plan::json_string(record.evidence_confidence) << "\n";
    out << nested << "}";
  }
  if (!projection.blocker_records.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string top_blockers_json(const BlockerListProjection& projection,
                                     const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < projection.ranked_categories.size();
       ++index) {
    const std::string& category = projection.ranked_categories[index];
    const auto& aggregate = projection.categories.at(category);
    if (index > 0U) out << ",";
    out << "\n" << nested << "{\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "  \"count\": " << aggregate.count << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(aggregate.repair_environment) << ",\n";
    out << nested << "  \"repair_action\": "
        << inferlab_plan::json_string(aggregate.repair_action) << "\n";
    out << nested << "}";
  }
  if (!projection.ranked_categories.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string repair_plan_json(const BlockerListProjection& projection,
                                    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << "[";
  std::size_t step = 0;
  for (const auto& category : projection.repair_plan_categories) {
    step = inferlab::internal::checked_add_size(
        step, 1U, "blocker repair-plan step");
    const auto& aggregate = projection.categories.at(category);
    if (step > 1U) out << ",";
    out << "\n" << nested << "{\n";
    out << nested << "  \"step\": " << step << ",\n";
    out << nested << "  \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "  \"repair_environment\": "
        << inferlab_plan::json_string(aggregate.repair_environment) << ",\n";
    out << nested << "  \"repair_action\": "
        << inferlab_plan::json_string(aggregate.repair_action) << ",\n";
    out << nested << "  \"artifacts\": "
        << inferlab_plan::json_string_array(aggregate.artifacts, indent + 2)
        << "\n";
    out << nested << "}";
  }
  if (!projection.repair_plan_categories.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string environment_split_json(
    const BlockerListProjection& projection,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"current_repo_fixable\": "
      << inferlab_plan::json_string_array(
             projection.current_repo_fixable_artifacts, indent + 2)
      << ",\n";
  out << nested << "\"current_server_when_validation_enabled\": "
      << inferlab_plan::json_string_array(
             projection.current_server_validation_artifacts, indent + 2)
      << ",\n";
  out << nested << "\"target_machine_required\": "
      << inferlab_plan::json_string_array(
             projection.target_machine_required_artifacts, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
