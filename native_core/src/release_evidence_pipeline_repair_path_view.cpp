#include "release_evidence_pipeline_repair_path_view.hpp"

#include "evidence_json_writer.hpp"

#include <utility>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

inferlab_evidence::RepairCard repair_card_for_path(
    const NonGpuReviewRepairPath& path) {
  inferlab_evidence::RepairCard card;
  card.id = path.id;
  card.artifact = path.source;
  card.category = path.category;
  card.repair_environment = path.repair_environment;
  card.severity = path.severity;
  card.issue = path.trigger;
  card.repair_action = path.recommended_action;
  card.exact_wiring_locations = path.source_locations;
  card.exact_wiring_location_count = card.exact_wiring_locations.size();
  card.source_location_gap_count =
      card.exact_wiring_locations.empty() ? 1U : 0U;
  return card;
}

}  // namespace

std::string non_gpu_review_repair_paths_json(
    const std::vector<NonGpuReviewRepairPath>& paths, const int indent) {
  std::vector<inferlab_evidence::RepairCard> cards;
  std::vector<std::vector<inferlab_evidence::RepairCardJsonField>>
      extension_fields;
  cards.reserve(paths.size());
  extension_fields.reserve(paths.size());
  for (const auto& path : paths) {
    cards.push_back(repair_card_for_path(path));
    const auto& card = cards.back();
    std::vector<inferlab_evidence::RepairCardJsonField> fields;
    fields.push_back(inferlab_evidence::evidence_json_string_field(
        "command", path.command));
    fields.push_back(inferlab_evidence::evidence_json_string_array_field(
        "source_locations", card.exact_wiring_locations));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "source_location_count", card.exact_wiring_location_count));
    fields.push_back(
        card.exact_wiring_locations.empty()
            ? inferlab_evidence::evidence_json_null_field(
                  "first_source_location")
            : inferlab_evidence::evidence_json_string_field(
                  "first_source_location",
                  card.exact_wiring_locations.front()));
    fields.push_back(
        inferlab_evidence::evidence_json_named_signed_counter_array_field(
            "evidence_counters", path.evidence_counters));
    fields.push_back(inferlab_evidence::evidence_json_unsigned_field(
        "evidence_counter_count", path.evidence_counters.size()));
    extension_fields.push_back(std::move(fields));
  }

  inferlab_evidence::RepairCardJsonOptions options;
  options.artifact_key = "source";
  options.severity_key = "severity";
  options.issue_key = "trigger";
  options.repair_action_key = "recommended_action";
  options.include_affected_claims = false;
  options.include_exact_wiring_locations = false;
  options.include_location_counts = false;
  options.include_claim_boundary = false;
  return inferlab_evidence::render_repair_cards_json(
      cards, indent, options, extension_fields);
}

}  // namespace inferlab_release_pipeline
