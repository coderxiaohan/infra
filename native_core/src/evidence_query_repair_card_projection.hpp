#pragma once

#include "checked_index.hpp"
#include "evidence_json_writer.hpp"
#include "evidence_query_repair_groups.hpp"
#include "evidence_query_repair_policy.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {

struct RepairCardRenderProjection {
  std::vector<inferlab_evidence::RepairCard> cards;
  std::vector<std::vector<inferlab_evidence::RepairCardJsonField>>
      extension_fields;
};

inline RepairCardRenderProjection repair_card_render_projection(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  RepairCardRenderProjection projection;
  std::size_t card_id = 1U;
  for (const auto& record : records) {
    if (!repair_card_record_matches(record, operation, artifact_filter,
                                    blocker_filter)) {
      continue;
    }
    const std::string& category = blocker_category(record);
    const std::vector<std::string> locations =
        wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    const std::vector<std::string> exact_locations =
        source_exact ? locations : std::vector<std::string>{};
    const std::vector<std::string> fallback_locations =
        source_exact ? std::vector<std::string>{} : locations;

    inferlab_evidence::RepairCard card;
    card.id = "repair_card_" + std::to_string(card_id);
    card_id = inferlab::internal::checked_add_size(
        card_id, 1U, "repair-card render id");
    card.artifact = record.artifact;
    card.category = category;
    card.repair_environment =
        inferlab_evidence::repair_environment_kind(category);
    card.severity = "high";
    card.repair_action = repair_action(category);
    card.affected_claims = affected_claims(record);
    card.exact_wiring_locations = exact_locations;
    card.exact_wiring_location_count = exact_locations.size();
    card.source_location_gap_count = source_exact ? 0U : 1U;
    projection.cards.push_back(std::move(card));
    projection.extension_fields.push_back(
        {inferlab_evidence::repair_card_string_field(
             "source_resolution_status",
             source_exact ? "exact_source_location_available"
                          : "source_location_mapping_missing"),
         inferlab_evidence::repair_card_boolean_field("source_exact",
                                                       source_exact),
         inferlab_evidence::repair_card_unsigned_field(
             "source_location_count", exact_locations.size()),
         inferlab_evidence::repair_card_boolean_field(
             "source_location_gap", !source_exact),
         inferlab_evidence::repair_card_string_array_field(
             "fallback_investigation_locations", fallback_locations),
         inferlab_evidence::repair_card_string_field(
             "recommended_command",
             "refresh the owning artifact producer when validation is enabled")});
  }
  return projection;
}

}  // namespace inferlab_evidence_query
