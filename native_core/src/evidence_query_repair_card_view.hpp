#pragma once

#include "evidence_json_writer.hpp"
#include "evidence_query_repair_card_projection.hpp"

#include <string>

namespace inferlab_evidence_query {

inline std::string repair_cards_json(
    const RepairCardRenderProjection& projection,
    const int indent) {
  inferlab_evidence::RepairCardJsonOptions options;
  options.severity_key = "severity";
  options.repair_action_key = "why_blocked";
  options.include_location_counts = false;
  options.claim_boundary =
      "repair card is guidance only; it does not execute commands, mutate "
      "files, or produce benchmark/replay/execute evidence";
  return inferlab_evidence::render_repair_cards_json(
      projection.cards, indent, options, projection.extension_fields);
}

}  // namespace inferlab_evidence_query
