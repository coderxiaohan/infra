#pragma once

#include "checked_index.hpp"
#include "evidence_json_writer.hpp"
#include "evidence_query_selection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string selector_repair_cards_json(
    const std::vector<SelectorRepairCard>& cards,
    const int indent) {
  return inferlab_evidence::repair_cards_json(
      cards, indent,
      "selector repair card is read-only triage metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence");
}

inline std::string selector_repair_card_export_status(
    const SelectorResolution& resolution) {
  if (resolution.repair_card_count == 0) return "no_selector_repair_cards";
  if (resolution.repair_card_truncated) return "embedded_sample_truncated";
  return "embedded_cards_complete";
}

inline std::size_t selector_repair_card_environment_count(
    const SelectorResolution& resolution,
    const std::string& repair_environment) {
  std::size_t count = 0;
  for (const auto& card : resolution.all_repair_cards) {
    if (inferlab_evidence::repair_environment_name(card.repair_environment) ==
        repair_environment) {
      count = inferlab::internal::checked_add_size(
          count, 1U, "selector repair-card environment count");
    }
  }
  return count;
}

inline std::size_t selector_repair_card_source_location_gap_card_count(
    const SelectorResolution& resolution) {
  std::size_t count = 0;
  for (const auto& card : resolution.all_repair_cards) {
    if (card.source_location_gap_count > 0U) {
      count = inferlab::internal::checked_add_size(
          count, 1U, "selector repair-card source-location gap count");
    }
  }
  return count;
}

inline bool selector_repair_card_external_export_written(
    const std::string& external_export_write_status) {
  return external_export_write_status == "written_before_primary_output";
}

inline bool selector_repair_card_external_file_required(
    const SelectorResolution& resolution,
    const std::string& external_export_write_status) {
  return resolution.repair_card_truncated &&
         !selector_repair_card_external_export_written(
             external_export_write_status);
}

inline std::string selector_repair_card_export_blocker_reason(
    const SelectorResolution& resolution,
    const std::string& external_export_path,
    const std::string& external_export_write_status) {
  if (resolution.resolution_status == "selector_not_found") {
    return "selector_not_found";
  }
  if (resolution.ambiguous) return "selector_ambiguous";
  if (resolution.repair_card_count == 0U) return "no_selector_repair_cards";
  if (selector_repair_card_external_file_required(
          resolution, external_export_write_status)) {
    if (!external_export_path.empty() &&
        !selector_repair_card_external_export_written(
            external_export_write_status)) {
      return "external_export_not_written";
    }
    return "embedded_sample_truncated";
  }
  if (selector_repair_card_source_location_gap_card_count(resolution) > 0U) {
    return "source_location_gaps";
  }
  return "none";
}

inline std::string selector_repair_card_export_review_status(
    const SelectorResolution& resolution,
    const std::string& external_export_path,
    const std::string& external_export_write_status) {
  if (resolution.resolution_status == "selector_not_found") {
    return "selector_not_found";
  }
  if (resolution.ambiguous) return "selector_ambiguous";
  if (resolution.repair_card_count == 0U) return "no_cards";
  if (selector_repair_card_external_file_required(
          resolution, external_export_write_status)) {
    return external_export_path.empty() ? "external_export_required"
                                        : "external_export_failed";
  }
  if (selector_repair_card_source_location_gap_card_count(resolution) > 0U) {
    return "source_location_gaps_present";
  }
  if (selector_repair_card_external_export_written(
          external_export_write_status)) {
    return "external_export_written";
  }
  return "embedded_cards_complete";
}

inline std::string selector_repair_card_export_focus_next_action(
    const SelectorResolution& resolution,
    const std::string& external_export_path,
    const std::string& external_export_write_status) {
  const std::string blocker_reason =
      selector_repair_card_export_blocker_reason(
          resolution, external_export_path, external_export_write_status);
  if (blocker_reason == "selector_not_found") {
    return "run list_artifacts or use an exact artifact/blocker selector before exporting selector repair cards";
  }
  if (blocker_reason == "selector_ambiguous") {
    return "narrow selector to one exact artifact id or blocker category before trusting focused repair cards";
  }
  if (blocker_reason == "external_export_not_written") {
    return "inspect selector repair-card output path and rerun export on a validation-enabled machine";
  }
  if (blocker_reason == "embedded_sample_truncated") {
    return "configure --selector-repair-card-output or narrow the selector before release review";
  }
  if (blocker_reason == "source_location_gaps") {
    return "add source-location mappings for selector repair cards before claiming current-repo repair closure";
  }
  if (resolution.repair_card_count == 0U) {
    return "no selector repair cards are available; inspect selector resolution and blocker filters";
  }
  return "review the complete embedded or external selector repair-card artifact";
}

inline std::string selector_repair_card_export_review_focus_json(
    const SelectorResolution& resolution,
    const int indent,
    const std::string& external_export_path,
    const std::string& external_export_write_status) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const bool external_export_configured = !external_export_path.empty();
  const bool external_export_written =
      selector_repair_card_external_export_written(
          external_export_write_status);
  const bool external_file_required =
      selector_repair_card_external_file_required(
          resolution, external_export_write_status);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"review_status\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_review_status(
                 resolution, external_export_path,
                 external_export_write_status))
      << ",\n";
  out << nested << "\"blocker_reason\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_blocker_reason(
                 resolution, external_export_path,
                 external_export_write_status))
      << ",\n";
  out << nested << "\"external_file_required\": "
      << (external_file_required ? "true" : "false") << ",\n";
  out << nested << "\"external_export_configured\": "
      << (external_export_configured ? "true" : "false") << ",\n";
  out << nested << "\"external_export_written\": "
      << (external_export_written ? "true" : "false") << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << nested << "\"resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"selector_ambiguous\": "
      << (resolution.ambiguous ? "true" : "false") << ",\n";
  out << nested << "\"total_match_count\": "
      << resolution.total_match_count << ",\n";
  out << nested << "\"repair_card_count\": "
      << resolution.repair_card_count << ",\n";
  out << nested << "\"embedded_card_count\": "
      << resolution.repair_card_emitted_count << ",\n";
  out << nested << "\"full_export_count\": "
      << resolution.all_repair_cards.size() << ",\n";
  out << nested << "\"current_repo_card_count\": "
      << selector_repair_card_environment_count(resolution,
                                                "current_repo_fixable")
      << ",\n";
  out << nested << "\"current_server_card_count\": "
      << selector_repair_card_environment_count(
             resolution, "current_server_when_validation_enabled")
      << ",\n";
  out << nested << "\"target_machine_card_count\": "
      << selector_repair_card_environment_count(
             resolution, "target_gpu_or_execute_machine")
      << ",\n";
  out << nested << "\"target_product_card_count\": "
      << selector_repair_card_environment_count(
             resolution, "target_product_or_replay_evidence")
      << ",\n";
  out << nested << "\"source_location_gap_card_count\": "
      << selector_repair_card_source_location_gap_card_count(resolution)
      << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(
             selector_repair_card_export_focus_next_action(
                 resolution, external_export_path,
                 external_export_write_status))
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"selector repair-card export focus is read-only release-review metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string selector_repair_card_export_json(
    const SelectorResolution& resolution,
    const int indent,
    const std::string& external_export_path = "",
    const std::string& external_export_write_status = "not_configured") {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::string status = selector_repair_card_export_status(resolution);
  const bool external_export_configured = !external_export_path.empty();
  const bool external_export_written =
      selector_repair_card_external_export_written(
          external_export_write_status);
  std::vector<std::string> next_actions;
  if (resolution.repair_card_count == 0) {
    next_actions.push_back(
        "adjust --selector, --artifact, or --blocker if a focused repair card is expected");
  } else if (resolution.repair_card_truncated && external_export_written) {
    next_actions.push_back(
        "review the complete external selector repair-card export and retain the embedded cards as a bounded triage sample");
  } else if (resolution.repair_card_truncated) {
    next_actions.push_back(
        "narrow the selector to an exact artifact id or blocker category before reviewing every repair card");
    next_actions.push_back(
        "use selector_resolution.repair_cards as a bounded triage sample only");
  } else {
    next_actions.push_back(
        "review selector_resolution.repair_cards for the complete embedded selector repair-card set");
  }

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"export_status\": "
      << inferlab_plan::json_string(status) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << nested << "\"total_card_count\": "
      << resolution.repair_card_count << ",\n";
  out << nested << "\"embedded_card_count\": "
      << resolution.repair_card_emitted_count << ",\n";
  out << nested << "\"embedded_card_limit\": "
      << kMaxSelectorRepairCards << ",\n";
  out << nested << "\"full_card_count\": "
      << resolution.all_repair_cards.size() << ",\n";
  out << nested << "\"truncated\": "
      << (resolution.repair_card_truncated ? "true" : "false") << ",\n";
  out << nested << "\"full_export_available_in_current_artifact\": "
      << (!resolution.repair_card_truncated ? "true" : "false") << ",\n";
  out << nested << "\"external_export_configured\": "
      << (external_export_configured ? "true" : "false") << ",\n";
  out << nested << "\"external_export_written\": "
      << (external_export_written ? "true" : "false") << ",\n";
  out << nested << "\"external_export_write_status\": "
      << inferlab_plan::json_string(external_export_write_status) << ",\n";
  out << nested << "\"external_export_path\": "
      << inferlab_plan::redacted_optional_path_json(external_export_path)
      << ",\n";
  out << nested << "\"external_export_path_redacted\": "
      << inferlab_plan::optional_path_redacted_json_bool(external_export_path)
      << ",\n";
  out << nested << "\"export_review_focus\":\n"
      << selector_repair_card_export_review_focus_json(
             resolution, indent + 2, external_export_path,
             external_export_write_status)
      << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(resolution.first_action) << ",\n";
  out << nested << "\"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << ",\n";
  out << nested << "\"next_actions\": "
      << inferlab_plan::json_string_array(next_actions, indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"selector repair-card export metadata is a read-only export contract; optional external export files are review artifacts only and do not execute repairs, refresh upstream evidence, or produce benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string selector_resolution_json(const SelectorResolution& resolution,
                                     const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << nested << "\"resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"artifact_match_count\": "
      << resolution.artifact_match_count << ",\n";
  out << nested << "\"blocker_category_match_count\": "
      << resolution.blocker_category_match_count << ",\n";
  out << nested << "\"total_match_count\": "
      << resolution.total_match_count << ",\n";
  out << nested << "\"ambiguous\": "
      << (resolution.ambiguous ? "true" : "false") << ",\n";
  out << nested << "\"first_artifact\": "
      << inferlab_plan::json_string(resolution.first_artifact) << ",\n";
  out << nested << "\"first_category\": "
      << inferlab_plan::json_string(resolution.first_category) << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(resolution.first_action) << ",\n";
  out << nested << "\"repair_hint\": "
      << inferlab_plan::json_string(resolution.repair_hint) << ",\n";
  out << nested << "\"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << ",\n";
  out << nested << "\"repair_card_count\": "
      << resolution.repair_card_count << ",\n";
  out << nested << "\"repair_card_emitted_count\": "
      << resolution.repair_card_emitted_count << ",\n";
  out << nested << "\"repair_card_truncated\": "
      << (resolution.repair_card_truncated ? "true" : "false") << ",\n";
  out << nested << "\"repair_cards\": "
      << selector_repair_cards_json(resolution.repair_cards, indent + 2)
      << ",\n";
  out << nested << "\"matched_blocker_categories\": "
      << inferlab_plan::json_string_array(
             resolution.matched_blocker_categories, indent + 2)
      << ",\n";
  out << nested << "\"affected_claims\": "
      << inferlab_plan::json_string_array(resolution.affected_claims,
                                          indent + 2)
      << ",\n";
  out << nested << "\"affected_claim_count\": "
      << resolution.affected_claims.size() << ",\n";
  out << nested << "\"exact_wiring_locations\": "
      << inferlab_plan::json_string_array(
             resolution.exact_wiring_locations, indent + 2)
      << ",\n";
  out << nested << "\"exact_wiring_location_count\": "
      << resolution.exact_wiring_location_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << resolution.source_location_gap_count << ",\n";
  out << nested << "\"current_repo_actions\": "
      << inferlab_plan::json_string_array(resolution.current_repo_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << resolution.current_repo_actions.size() << ",\n";
  out << nested << "\"current_server_actions\": "
      << inferlab_plan::json_string_array(resolution.current_server_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"current_server_action_count\": "
      << resolution.current_server_actions.size() << ",\n";
  out << nested << "\"target_machine_actions\": "
      << inferlab_plan::json_string_array(resolution.target_machine_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_machine_action_count\": "
      << resolution.target_machine_actions.size() << ",\n";
  out << nested << "\"target_product_actions\": "
      << inferlab_plan::json_string_array(resolution.target_product_actions,
                                          indent + 2)
      << ",\n";
  out << nested << "\"target_product_action_count\": "
      << resolution.target_product_actions.size() << ",\n";
  out << nested
      << "\"claim_boundary\": \"selector resolution is read-only query routing metadata; it does not execute commands, refresh artifacts, mutate files, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string selector_resolution_text(
    const SelectorResolution& resolution) {
  std::ostringstream out;
  out << "selector_resolution: kind=" << resolution.selector_kind
      << " | status=" << resolution.resolution_status
      << " | selector="
      << (resolution.selector.empty() ? "<none>" : resolution.selector)
      << " | matches=" << resolution.total_match_count
      << " | artifact_matches=" << resolution.artifact_match_count
      << " | blocker_matches=" << resolution.blocker_category_match_count
      << " | ambiguous=" << (resolution.ambiguous ? "true" : "false")
      << " | repair_cards=" << resolution.repair_card_count
      << " | emitted_cards=" << resolution.repair_card_emitted_count
      << " | cards_truncated="
      << (resolution.repair_card_truncated ? "true" : "false")
      << " | source_locations="
      << resolution.exact_wiring_location_count
      << " | source_location_gaps="
      << resolution.source_location_gap_count
      << " | current_repo_actions="
      << resolution.current_repo_actions.size()
      << " | target_machine_actions="
      << resolution.target_machine_actions.size()
      << " | owner=" << resolution.repair_owner_hint
      << " | first_artifact="
      << (resolution.first_artifact.empty() ? "<none>"
                                            : resolution.first_artifact)
      << " | first_action=" << resolution.first_action
      << " | hint=" << resolution.repair_hint << "\n";
  return out.str();
}

inline std::string selector_focus_json(const SelectorResolution& resolution,
                                const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(resolution.selector) << ",\n";
  out << nested << "\"selector_kind\": "
      << inferlab_plan::json_string(resolution.selector_kind) << ",\n";
  out << nested << "\"resolution_status\": "
      << inferlab_plan::json_string(resolution.resolution_status) << ",\n";
  out << nested << "\"total_match_count\": "
      << resolution.total_match_count << ",\n";
  out << nested << "\"artifact_match_count\": "
      << resolution.artifact_match_count << ",\n";
  out << nested << "\"blocker_category_match_count\": "
      << resolution.blocker_category_match_count << ",\n";
  out << nested << "\"ambiguous\": "
      << (resolution.ambiguous ? "true" : "false") << ",\n";
  out << nested << "\"repair_owner_hint\": "
      << inferlab_plan::json_string(resolution.repair_owner_hint) << ",\n";
  out << nested << "\"first_artifact\": "
      << inferlab_plan::json_string(resolution.first_artifact) << ",\n";
  out << nested << "\"first_category\": "
      << inferlab_plan::json_string(resolution.first_category) << ",\n";
  out << nested << "\"first_action\": "
      << inferlab_plan::json_string(resolution.first_action) << ",\n";
  out << nested << "\"repair_card_count\": "
      << resolution.repair_card_count << ",\n";
  out << nested << "\"repair_card_emitted_count\": "
      << resolution.repair_card_emitted_count << ",\n";
  out << nested << "\"repair_card_truncated\": "
      << (resolution.repair_card_truncated ? "true" : "false") << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << resolution.source_location_gap_count << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << resolution.current_repo_actions.size() << ",\n";
  out << nested << "\"current_server_action_count\": "
      << resolution.current_server_actions.size() << ",\n";
  out << nested << "\"target_machine_action_count\": "
      << resolution.target_machine_actions.size() << ",\n";
  out << nested << "\"target_product_action_count\": "
      << resolution.target_product_actions.size() << ",\n";
  out << nested << "\"repair_hint\": "
      << inferlab_plan::json_string(resolution.repair_hint) << ",\n";
  out << nested
      << "\"claim_boundary\": \"selector_focus is a compact read-only mirror of selector_resolution for the focused operation payload; it does not execute repairs, refresh evidence, or create benchmark/replay/execute claims\"\n";
  out << pad << "}";
  return out.str();
}

inline std::string selector_focus_text_fragment(const SelectorResolution& resolution) {
  std::ostringstream out;
  out << " | selector_status=" << resolution.resolution_status
      << " | selector_kind=" << resolution.selector_kind
      << " | selector_matches=" << resolution.total_match_count
      << " | selector_ambiguous="
      << (resolution.ambiguous ? "true" : "false")
      << " | selector_owner=" << resolution.repair_owner_hint
      << " | selector_source_gaps="
      << resolution.source_location_gap_count
      << " | selector_first_artifact="
      << (resolution.first_artifact.empty() ? "<none>"
                                            : resolution.first_artifact);
  return out.str();
}

}  // namespace inferlab_evidence_query
