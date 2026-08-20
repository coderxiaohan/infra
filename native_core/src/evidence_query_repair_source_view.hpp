#pragma once

#include "checked_index.hpp"
#include "evidence_query_repair_source_projection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string repair_card_source_exactness_json(
    const RepairCardSourceExactnessProjection& projection,
    const int indent) {
  const RepairCardSourceExactnessStats& stats = projection.stats;
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<std::string> empty_locations;
  bool first = true;
  std::size_t card_id = 1U;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"repair_card_count\": " << stats.repair_card_count
      << ",\n";
  out << nested << "\"source_exact_card_count\": "
      << stats.source_exact_card_count << ",\n";
  out << nested << "\"source_gap_card_count\": "
      << stats.source_gap_card_count << ",\n";
  out << nested << "\"current_repo_source_exact_card_count\": "
      << stats.current_repo_source_exact_card_count << ",\n";
  out << nested << "\"current_repo_source_gap_card_count\": "
      << stats.current_repo_source_gap_card_count << ",\n";
  out << nested << "\"target_or_validation_source_exact_card_count\": "
      << stats.target_or_validation_source_exact_card_count << ",\n";
  out << nested << "\"target_or_validation_source_gap_card_count\": "
      << stats.target_or_validation_source_gap_card_count << ",\n";
  out << nested << "\"source_exactness_percent\": "
      << repair_card_source_exactness_percent(stats) << ",\n";
  out << nested << "\"embedded_card_limit\": "
      << projection.row_limit << ",\n";
  out << nested << "\"truncated\": "
      << (projection.truncated ? "true" : "false") << ",\n";
  out << nested << "\"cards\": [";
  for (const auto& row : projection.rows) {
    if (!first) out << ",";
    first = false;
    const std::vector<std::string>& exact_locations =
        row.source_exact ? row.wiring_locations : empty_locations;
    const std::vector<std::string>& fallback_locations =
        row.source_exact ? empty_locations : row.wiring_locations;
    out << "\n" << nested << "  {\n";
    out << nested << "    \"id\": "
        << inferlab_plan::json_string("repair_card_source_exactness_" +
                                      std::to_string(card_id))
        << ",\n";
    card_id = inferlab::internal::checked_add_size(
        card_id, 1U, "repair-card source-exactness display id");
    out << nested << "    \"artifact\": "
        << inferlab_plan::json_string(row.artifact) << ",\n";
    out << nested << "    \"category\": "
        << inferlab_plan::json_string(row.category) << ",\n";
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(row.repair_environment) << ",\n";
    out << nested << "    \"source_resolution_status\": "
        << inferlab_plan::json_string(
               row.source_exact ? "exact_source_location_available"
                                : "source_location_mapping_missing")
        << ",\n";
    out << nested << "    \"source_exact\": "
        << (row.source_exact ? "true" : "false") << ",\n";
    out << nested << "    \"source_location_count\": "
        << exact_locations.size() << ",\n";
    out << nested << "    \"source_location_gap\": "
        << (row.source_exact ? "false" : "true") << ",\n";
    out << nested << "    \"current_repo_action_required\": "
        << (row.current_repo_action_required ? "true" : "false") << ",\n";
    out << nested << "    \"target_or_validation_action_required\": "
        << (row.current_repo_action_required ? "false" : "true") << ",\n";
    out << nested << "    \"repair_action\": "
        << inferlab_plan::json_string(repair_action(row.category)) << ",\n";
    out << nested << "    \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(exact_locations, indent + 2)
        << ",\n";
    out << nested << "    \"fallback_investigation_locations\": "
        << inferlab_plan::json_string_array(fallback_locations, indent + 2)
        << ",\n";
    out << nested << "    \"next_action\": "
        << inferlab_plan::json_string(
               row.source_exact
                   ? "inspect exact_wiring_locations before changing code or report wiring"
                   : "add category-to-source mapping before presenting this repair as source-exact")
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"repair-card source exactness is read-only location coverage metadata; it does not execute repairs, mutate files, refresh artifacts, or prove benchmark/replay/execute evidence\"\n";
    out << nested << "  }";
  }
  if (!first) out << "\n" << nested;
  out << "],\n";
  out << nested << "\"emitted_card_count\": " << projection.rows.size()
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"repair-card source exactness explains whether repair cards have concrete source/config/report locations; it does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
