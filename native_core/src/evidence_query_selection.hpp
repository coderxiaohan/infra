#pragma once

#include "artifact_metadata_reader.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_repair_policy.hpp"
#include "evidence_query_snapshot_diff.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline constexpr std::size_t kMaxSelectorRepairCards = 8;
static_assert(kMaxSelectorRepairCards > 0,
              "selector repair-card sample must not be empty");

struct QueryOperation {
  std::string raw_query;
  std::string operation;
  std::string target;
  std::string selector;
  std::string normalized_query;
  std::string human_label;
};

using SelectorRepairCard = inferlab_evidence::RepairCard;

struct SelectorResolution {
  std::string selector;
  std::string selector_kind;
  std::string resolution_status;
  std::string first_artifact;
  std::string first_category;
  std::string first_action;
  std::string repair_hint;
  std::string repair_owner_hint;
  std::vector<std::string> matched_blocker_categories;
  std::vector<std::string> affected_claims;
  std::vector<std::string> exact_wiring_locations;
  std::vector<std::string> current_repo_actions;
  std::vector<std::string> current_server_actions;
  std::vector<std::string> target_machine_actions;
  std::vector<std::string> target_product_actions;
  std::vector<SelectorRepairCard> repair_cards;
  std::vector<SelectorRepairCard> all_repair_cards;
  std::size_t artifact_match_count = 0;
  std::size_t blocker_category_match_count = 0;
  std::size_t total_match_count = 0;
  std::size_t repair_card_count = 0;
  std::size_t repair_card_emitted_count = 0;
  std::size_t exact_wiring_location_count = 0;
  std::size_t source_location_gap_count = 0;
  bool ambiguous = false;
  bool repair_card_truncated = false;
};

struct ShowArtifactSelection {
  std::string selector;
  const inferlab_artifact::WarehouseArtifactRecord* selected = nullptr;
  std::vector<std::string> candidate_artifacts;
  std::size_t candidate_count = 0;
  bool candidate_list_truncated = false;
  bool exact_match = false;
  bool selector_ambiguous = false;
  std::string selection_strategy = "no_match";
};

inline bool operation_selector_recommended(
    const QueryOperation& operation) {
  return operation.operation == "show_artifact" ||
         operation.operation == "explain_blocker" ||
         operation.operation == "suggest_repair" ||
         operation.operation == "repair_group_summary";
}

namespace detail {

inline bool selector_context_record_matches(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const SelectorResolution& resolution,
    const std::string& query,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  const std::string category = blocker_category(record);
  if (!blocker_filter.empty() && category != blocker_filter) return false;
  if (resolution.selector_kind == "blocker_category") {
    if (!artifact_filter.empty() && record.artifact != artifact_filter &&
        !inferlab_plan::contains(record.artifact, artifact_filter)) {
      return false;
    }
    return category == resolution.selector;
  }
  if (resolution.selector_kind == "artifact_exact") {
    return include_record(record, query, artifact_filter, blocker_filter) &&
           record.artifact == resolution.selector;
  }
  if (resolution.selector_kind == "artifact_fragment") {
    return include_record(record, query, artifact_filter, blocker_filter) &&
           inferlab_plan::contains(record.artifact, resolution.selector);
  }
  return include_record(record, query, artifact_filter, blocker_filter);
}

inline void append_repair_action(
    SelectorResolution& resolution,
    const inferlab_evidence::RepairEnvironment environment,
    const std::string& action) {
  if (environment == inferlab_evidence::RepairEnvironment::kCurrentRepoFixable) {
    inferlab_plan::append_unique_nonempty_string(
        resolution.current_repo_actions, action);
  } else if (environment == inferlab_evidence::RepairEnvironment::
                                kCurrentServerWhenValidationEnabled) {
    inferlab_plan::append_unique_nonempty_string(
        resolution.current_server_actions, action);
  } else if (environment ==
             inferlab_evidence::RepairEnvironment::kTargetGpuOrExecuteMachine) {
    inferlab_plan::append_unique_nonempty_string(
        resolution.target_machine_actions, action);
  } else {
    inferlab_plan::append_unique_nonempty_string(
        resolution.target_product_actions, action);
  }
}

inline void enrich_selector_resolution(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const std::string& query,
    const std::string& artifact_filter,
    const std::string& blocker_filter,
    SelectorResolution& resolution) {
  for (const auto& record : records) {
    if (!selector_context_record_matches(record, resolution, query,
                                         artifact_filter, blocker_filter)) {
      continue;
    }
    const std::string category = blocker_category(record);
    if (category == "none") continue;
    ++resolution.repair_card_count;
    inferlab_plan::append_unique_nonempty_string(
        resolution.matched_blocker_categories, category);
    append_affected_claims(resolution.affected_claims, record);
    const auto category_wiring_locations =
        wiring_locations_for_category(category);
    const bool category_source_exact =
        category_has_exact_wiring_locations(category) &&
        !category_wiring_locations.empty();
    if (category_source_exact) {
      for (const auto& location : category_wiring_locations) {
        inferlab_plan::append_unique_nonempty_string(
            resolution.exact_wiring_locations, location);
      }
    }

    const std::string action = repair_action(category);
    const auto environment =
        inferlab_evidence::repair_environment_kind(category);
    SelectorRepairCard card;
    card.id =
        "selector_repair_card_" +
        std::to_string(resolution.repair_card_count);
    card.artifact = record.artifact;
    card.category = category;
    card.repair_environment = environment;
    card.repair_action = action;
    card.affected_claims = affected_claims(record);
    card.exact_wiring_locations =
        category_source_exact ? category_wiring_locations
                              : std::vector<std::string>{};
    card.exact_wiring_location_count = card.exact_wiring_locations.size();
    card.source_location_gap_count =
        card.exact_wiring_location_count == 0 ? 1U : 0U;
    resolution.all_repair_cards.push_back(card);
    if (resolution.repair_cards.size() < kMaxSelectorRepairCards) {
      resolution.repair_cards.push_back(card);
    } else {
      resolution.repair_card_truncated = true;
    }
    if (resolution.first_action.empty()) resolution.first_action = action;
    append_repair_action(resolution, environment, action);
  }

  resolution.exact_wiring_location_count =
      resolution.exact_wiring_locations.size();
  resolution.repair_card_emitted_count = resolution.repair_cards.size();
  resolution.source_location_gap_count = 0;
  for (const auto& card : resolution.all_repair_cards) {
    if (card.source_location_gap_count > 0U) {
      ++resolution.source_location_gap_count;
    }
  }
  if (resolution.repair_card_count == 0) {
    resolution.repair_owner_hint =
        "no repair card matched this selector in the current evidence view";
  } else if (!resolution.current_repo_actions.empty()) {
    resolution.repair_owner_hint = "current_repo_fixable";
  } else if (!resolution.current_server_actions.empty()) {
    resolution.repair_owner_hint = "current_server_when_validation_enabled";
  } else if (!resolution.target_machine_actions.empty()) {
    resolution.repair_owner_hint = "target_gpu_or_execute_machine";
  } else {
    resolution.repair_owner_hint = "target_product_or_replay_evidence";
  }
  if (resolution.affected_claims.empty()) {
    resolution.affected_claims.push_back(
        "no blocking claim impact detected for selector");
  }
  if (resolution.first_action.empty()) {
    resolution.first_action = resolution.repair_hint;
  }
}

}  // namespace detail

inline SelectorResolution selector_resolution(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& query,
    const std::string& artifact_filter,
    const std::string& blocker_filter,
    const SnapshotDiffSignals& snapshot_diff) {
  SelectorResolution resolution;
  resolution.selector = operation.selector;
  if (resolution.selector.empty() && !artifact_filter.empty()) {
    resolution.selector = artifact_filter;
  }
  if (resolution.selector.empty() && !blocker_filter.empty()) {
    resolution.selector = blocker_filter;
  }

  if (operation.operation == "path") {
    const bool target_valid =
        inferlab_evidence::is_readiness_target(operation.target);
    resolution.selector_kind = "readiness_target";
    resolution.selector = operation.target;
    resolution.total_match_count = target_valid ? 1U : 0U;
    resolution.resolution_status =
        target_valid ? "target_resolved" : "target_invalid";
    resolution.repair_hint =
        target_valid
            ? "inspect the selected readiness path and repair blockers in order"
            : "pass --target report_ready, replay_ready, or execute_ready";
    detail::enrich_selector_resolution(records, query, artifact_filter,
                                       blocker_filter, resolution);
    return resolution;
  }

  if (operation.operation == "diff_snapshots") {
    resolution.selector_kind = "snapshot_pair";
    resolution.total_match_count =
        snapshot_diff.before_configured && snapshot_diff.after_configured ? 1U
                                                                          : 0U;
    resolution.resolution_status =
        resolution.total_match_count > 0 ? "snapshot_inputs_configured"
                                         : "snapshot_inputs_missing";
    resolution.repair_hint =
        resolution.total_match_count > 0
            ? "inspect the snapshot diff decision before changing evidence"
            : "provide both --before and --after snapshot paths";
    detail::enrich_selector_resolution(records, query, artifact_filter,
                                       blocker_filter, resolution);
    return resolution;
  }

  if (resolution.selector.empty()) {
    const bool selector_recommended = operation_selector_recommended(operation);
    resolution.selector_kind =
        selector_recommended ? "selector_missing" : "none";
    resolution.resolution_status =
        selector_recommended ? "selector_recommended" : "no_selector_required";
    resolution.repair_hint =
        selector_recommended
            ? "pass --selector <artifact-or-blocker-category> for a focused result"
            : "optionally pass --artifact or --blocker to narrow the query";
    for (const auto& record : records) {
      if (!include_record(record, query, artifact_filter, blocker_filter)) {
        continue;
      }
      ++resolution.total_match_count;
      if (resolution.first_artifact.empty()) {
        resolution.first_artifact = record.artifact;
        resolution.first_category = blocker_category(record);
      }
    }
    resolution.ambiguous =
        resolution.total_match_count > 1U && selector_recommended;
    detail::enrich_selector_resolution(records, query, artifact_filter,
                                       blocker_filter, resolution);
    return resolution;
  }

  if (is_known_blocker_category(resolution.selector)) {
    resolution.selector_kind = "blocker_category";
    for (const auto& record : records) {
      if (!include_record(record, query, artifact_filter, "")) continue;
      const std::string category = blocker_category(record);
      if (category != resolution.selector) continue;
      ++resolution.blocker_category_match_count;
      if (resolution.first_artifact.empty()) {
        resolution.first_artifact = record.artifact;
        resolution.first_category = category;
      }
    }
    resolution.total_match_count = resolution.blocker_category_match_count;
    resolution.resolution_status =
        resolution.total_match_count > 0 ? "selector_resolved"
                                         : "selector_not_found";
    resolution.repair_hint =
        resolution.total_match_count > 0
            ? "inspect explain_tree or repair_cards for this blocker category"
            : "check --blocker value or refresh upstream evidence before querying";
    detail::enrich_selector_resolution(records, query, artifact_filter,
                                       blocker_filter, resolution);
    return resolution;
  }

  const inferlab_artifact::WarehouseArtifactRecord* first_fragment = nullptr;
  const inferlab_artifact::WarehouseArtifactRecord* first_exact = nullptr;
  std::size_t exact_match_count = 0;
  for (const auto& record : records) {
    if (!include_record(record, query, artifact_filter, blocker_filter)) {
      continue;
    }
    if (!inferlab_plan::contains(record.artifact, resolution.selector)) {
      continue;
    }
    ++resolution.artifact_match_count;
    if (first_fragment == nullptr) first_fragment = &record;
    if (record.artifact == resolution.selector) {
      ++exact_match_count;
      if (first_exact == nullptr) first_exact = &record;
    }
  }

  if (exact_match_count > 0U) {
    resolution.selector_kind = "artifact_exact";
    resolution.total_match_count = exact_match_count;
    resolution.ambiguous = exact_match_count > 1U;
    resolution.first_artifact = first_exact->artifact;
    resolution.first_category = blocker_category(*first_exact);
  } else {
    resolution.selector_kind = "artifact_fragment";
    resolution.total_match_count = resolution.artifact_match_count;
    resolution.ambiguous = resolution.total_match_count > 1U;
    if (first_fragment != nullptr) {
      resolution.first_artifact = first_fragment->artifact;
      resolution.first_category = blocker_category(*first_fragment);
    }
  }

  if (resolution.total_match_count == 0U) {
    resolution.resolution_status = "selector_not_found";
    resolution.repair_hint =
        "use list_artifacts or remove the selector to inspect available artifact ids";
  } else if (resolution.ambiguous) {
    resolution.resolution_status = "selector_ambiguous";
    resolution.repair_hint =
        resolution.selector_kind == "artifact_exact"
            ? "remove duplicate exact artifact ids before trusting focused evidence"
            : "use the exact artifact id or add --artifact to narrow the selector";
  } else {
    resolution.resolution_status = "selector_resolved";
    resolution.repair_hint =
        "inspect the selected artifact detail and exact wiring locations";
  }
  detail::enrich_selector_resolution(records, query, artifact_filter,
                                     blocker_filter, resolution);
  return resolution;
}

}  // namespace inferlab_evidence_query
