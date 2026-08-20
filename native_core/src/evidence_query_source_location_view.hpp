#pragma once

#include "evidence_query_blocker_drilldown.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string source_location_category_coverage_json(
    const SourceLocationCategoryCoverage& coverage,
    const int indent) {
  const auto& counts = coverage.repair_card_counts;
  const auto& categories = coverage.categories;
  const auto& stats = coverage.stats;
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  bool first = true;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"category_count\": " << stats.category_count << ",\n";
  out << nested << "\"active_category_count\": "
      << stats.active_category_count << ",\n";
  out << nested << "\"source_exact_category_count\": "
      << stats.source_exact_category_count << ",\n";
  out << nested << "\"source_gap_category_count\": "
      << stats.source_gap_category_count << ",\n";
  out << nested << "\"current_repo_source_gap_category_count\": "
      << stats.current_repo_source_gap_category_count << ",\n";
  out << nested << "\"target_or_validation_source_gap_category_count\": "
      << stats.target_or_validation_source_gap_category_count << ",\n";
  out << nested << "\"source_gap_repair_card_count\": "
      << stats.source_gap_repair_card_count << ",\n";
  out << nested << "\"source_exactness_percent\": "
      << source_location_category_exactness_percent(stats) << ",\n";
  out << nested << "\"categories\": [";
  for (const auto& category : categories) {
    const auto count_it = counts.find(category);
    const std::size_t repair_card_count =
        count_it == counts.end() ? 0U : count_it->second;
    const bool active = repair_card_count > 0U;
    const auto locations = wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    const inferlab_evidence::RepairEnvironment environment =
        inferlab_evidence::repair_environment_kind(category);
    const bool current_repo =
        environment ==
        inferlab_evidence::RepairEnvironment::kCurrentRepoFixable;
    const std::vector<std::string> exact_locations =
        source_exact ? locations : std::vector<std::string>{};
    const std::vector<std::string> fallback_locations =
        source_exact ? std::vector<std::string>{} : locations;
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "  {\n";
    out << nested << "    \"category\": "
        << inferlab_plan::json_string(category) << ",\n";
    out << nested << "    \"known_category\": "
        << (is_known_blocker_category(category) ? "true" : "false")
        << ",\n";
    out << nested << "    \"active\": " << (active ? "true" : "false")
        << ",\n";
    out << nested << "    \"repair_card_count\": " << repair_card_count
        << ",\n";
    out << nested << "    \"repair_environment\": "
        << inferlab_plan::json_string(
               inferlab_evidence::repair_environment_name(environment))
        << ",\n";
    out << nested << "    \"source_resolution_status\": "
        << inferlab_plan::json_string(
               source_exact ? "exact_source_location_available"
                            : "source_location_mapping_missing")
        << ",\n";
    out << nested << "    \"source_exact\": "
        << (source_exact ? "true" : "false") << ",\n";
    out << nested << "    \"source_location_count\": "
        << exact_locations.size() << ",\n";
    out << nested << "    \"source_location_gap\": "
        << (source_exact ? "false" : "true") << ",\n";
    out << nested << "    \"current_repo_action_required\": "
        << (active && current_repo ? "true" : "false") << ",\n";
    out << nested << "    \"target_or_validation_action_required\": "
        << (active && !current_repo ? "true" : "false") << ",\n";
    out << nested << "    \"repair_action\": "
        << inferlab_plan::json_string(repair_action(category)) << ",\n";
    out << nested << "    \"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(exact_locations, indent + 4)
        << ",\n";
    out << nested << "    \"fallback_investigation_locations\": "
        << inferlab_plan::json_string_array(fallback_locations, indent + 4)
        << ",\n";
    out << nested << "    \"next_action\": "
        << inferlab_plan::json_string(
               !active
                   ? "no active blocker currently matched this category"
                   : (source_exact
                          ? "inspect exact_wiring_locations and matching repair cards"
                          : "add category-to-source mapping before claiming this category is source-exact"))
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"source-location category coverage is read-only mapping metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
    out << nested << "  }";
  }
  if (!first) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"source-location category coverage shows which blocker categories have exact source/config/report mappings; it does not execute commands, mutate files, refresh artifacts, or prove readiness\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
