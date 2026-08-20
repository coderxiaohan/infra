#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_release_policy.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence {

inline constexpr const char* kRootCauseCurrentRepoRepairCountField =
    "current_repo_repair_card_count";
inline constexpr const char* kRootCauseTargetProductReplayRepairCountField =
    "target_product_replay_repair_card_count";
inline constexpr const char* kRootCauseTargetGpuExecuteRepairCountField =
    "target_gpu_execute_repair_card_count";
inline constexpr const char* kRootCauseLegacyTargetMachineRepairCountField =
    "target_machine_repair_card_count";
inline constexpr const char* kRootCauseRepairCardCountField =
    "root_cause_repair_card_count";
inline constexpr const char* kRootCauseRepairCardsField =
    "root_cause_repair_cards";
inline constexpr const char* kRootCauseRepairCardSourceLocationsField =
    "exact_wiring_locations";

struct RootCauseRepairCounts {
  std::size_t current_repo_repair_card_count = 0;
  std::size_t target_product_replay_repair_card_count = 0;
  std::size_t target_gpu_execute_repair_card_count = 0;
};

inline void increment_root_cause_repair_count(std::size_t& count,
                                              const char* name) {
  count = inferlab::internal::checked_add_size(count, 1U, name);
}

inline void add_root_cause_repair_category(
    RootCauseRepairCounts& counts,
    const std::string& category) {
  const auto environment = repair_environment_kind(category);
  if (repair_environment_blocks_report_ready(environment)) {
    increment_root_cause_repair_count(
        counts.current_repo_repair_card_count,
        "root-cause current-repo repair-card count");
  } else if (environment ==
             RepairEnvironment::kTargetProductOrReplayEvidence) {
    increment_root_cause_repair_count(
        counts.target_product_replay_repair_card_count,
        "root-cause target product/replay repair-card count");
  } else if (environment == RepairEnvironment::kTargetGpuOrExecuteMachine) {
    increment_root_cause_repair_count(
        counts.target_gpu_execute_repair_card_count,
        "root-cause target GPU/execute repair-card count");
  }
}

inline std::size_t root_cause_target_machine_repair_card_count(
    const RootCauseRepairCounts& counts) {
  return inferlab::internal::checked_add_size(
      counts.target_product_replay_repair_card_count,
      counts.target_gpu_execute_repair_card_count,
      "root-cause legacy target-machine repair-card count");
}

struct RootCauseRepairCountProjection {
  RootCauseRepairCounts declared;
  std::size_t legacy_target_machine_repair_card_count = 0;
  std::size_t effective_current_repo_repair_card_count = 0;
  std::size_t effective_target_product_replay_repair_card_count = 0;
  std::size_t effective_target_gpu_execute_repair_card_count = 0;
  bool source_present = false;
  bool current_repo_field_present = false;
  bool target_product_replay_field_present = false;
  bool target_gpu_execute_field_present = false;
  bool legacy_target_machine_field_present = false;
  bool typed_split_present = false;
  bool typed_split_partial = false;
  bool legacy_fallback_used = false;
  bool split_schema_gap = false;
  std::string split_status = "source_missing";
};

inline RootCauseRepairCountProjection project_root_cause_repair_counts(
    const std::string& artifact_text) {
  RootCauseRepairCountProjection projection;
  projection.source_present = !artifact_text.empty();
  const std::string summary_text =
      inferlab_artifact::json_object_section_or_empty(artifact_text,
                                                      "summary");
  const std::string& count_source =
      summary_text.empty() ? artifact_text : summary_text;
  projection.current_repo_field_present =
      inferlab_artifact::extract_unsigned_key_fallback(
          count_source, kRootCauseCurrentRepoRepairCountField,
          projection.declared.current_repo_repair_card_count);
  projection.target_product_replay_field_present =
      inferlab_artifact::extract_unsigned_key_fallback(
          count_source, kRootCauseTargetProductReplayRepairCountField,
          projection.declared.target_product_replay_repair_card_count);
  projection.target_gpu_execute_field_present =
      inferlab_artifact::extract_unsigned_key_fallback(
          count_source, kRootCauseTargetGpuExecuteRepairCountField,
          projection.declared.target_gpu_execute_repair_card_count);
  projection.legacy_target_machine_field_present =
      inferlab_artifact::extract_unsigned_key_fallback(
          count_source, kRootCauseLegacyTargetMachineRepairCountField,
          projection.legacy_target_machine_repair_card_count);

  projection.typed_split_present =
      projection.target_product_replay_field_present &&
      projection.target_gpu_execute_field_present;
  projection.typed_split_partial =
      projection.target_product_replay_field_present !=
      projection.target_gpu_execute_field_present;
  projection.legacy_fallback_used =
      !projection.typed_split_present &&
      projection.legacy_target_machine_field_present;
  projection.split_schema_gap =
      projection.source_present &&
      (!projection.current_repo_field_present ||
       projection.typed_split_partial ||
       (!projection.typed_split_present &&
        !projection.legacy_target_machine_field_present));

  projection.effective_current_repo_repair_card_count =
      projection.current_repo_field_present
          ? projection.declared.current_repo_repair_card_count
          : (projection.source_present ? 1U : 0U);
  if (projection.typed_split_present) {
    projection.effective_target_product_replay_repair_card_count =
        projection.declared.target_product_replay_repair_card_count;
    projection.effective_target_gpu_execute_repair_card_count =
        projection.declared.target_gpu_execute_repair_card_count;
    projection.split_status = "typed_split";
  } else if (projection.legacy_target_machine_field_present) {
    projection.effective_target_product_replay_repair_card_count =
        projection.legacy_target_machine_repair_card_count;
    projection.effective_target_gpu_execute_repair_card_count =
        projection.legacy_target_machine_repair_card_count;
    projection.split_status = "legacy_aggregate_fallback";
  } else if (projection.source_present) {
    projection.effective_target_product_replay_repair_card_count =
        projection.target_product_replay_field_present
            ? projection.declared.target_product_replay_repair_card_count
            : 1U;
    projection.effective_target_gpu_execute_repair_card_count =
        projection.target_gpu_execute_field_present
            ? projection.declared.target_gpu_execute_repair_card_count
            : 1U;
    projection.split_status = projection.typed_split_partial
                                  ? "typed_split_incomplete"
                                  : "repair_count_fields_missing";
  }
  return projection;
}

inline std::size_t effective_root_cause_target_machine_repair_card_count(
    const RootCauseRepairCountProjection& projection) {
  if (projection.typed_split_present) {
    return inferlab::internal::checked_add_size(
        projection.effective_target_product_replay_repair_card_count,
        projection.effective_target_gpu_execute_repair_card_count,
        "effective root-cause target-machine repair-card count");
  }
  if (projection.legacy_target_machine_field_present) {
    return projection.legacy_target_machine_repair_card_count;
  }
  return std::max(
      projection.effective_target_product_replay_repair_card_count,
      projection.effective_target_gpu_execute_repair_card_count);
}

struct RootCauseRepairCardEntryProjection {
  std::string category;
  std::vector<std::string> source_locations;
  bool category_present = false;
  bool source_locations_present = false;
  bool source_locations_valid = true;
};

struct RootCauseRepairCardProjection {
  RootCauseRepairCountProjection environment_counts;
  std::vector<RootCauseRepairCardEntryProjection> cards;
  std::size_t declared_repair_card_count = 0;
  std::size_t repair_card_count = 0;
  std::size_t source_location_card_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t first_card_source_location_count = 0;
  std::string first_source_location;
  bool source_present = false;
  bool declared_repair_card_count_present = false;
  bool declared_repair_card_count_invalid = false;
  bool repair_cards_field_present = false;
  bool repair_cards_array_valid = false;
  bool repair_cards_all_objects = true;
  bool declared_count_mismatch = false;
  bool card_contract_issue = false;
};

inline std::string root_cause_repair_card_category(
    const std::string& card, bool& present) {
  std::string category;
  if (inferlab_artifact::contains_top_level_key_fallback(
          card, "blocker_category")) {
    present = inferlab_artifact::extract_top_level_string_key_fallback(
        card, "blocker_category", category);
  } else {
    present = inferlab_artifact::extract_top_level_string_key_fallback(
        card, "category", category);
  }
  present = present && !category.empty();
  return category;
}

inline RootCauseRepairCardProjection project_root_cause_repair_cards(
    const std::string& artifact_text) {
  RootCauseRepairCardProjection projection;
  projection.source_present = !artifact_text.empty();
  projection.environment_counts =
      project_root_cause_repair_counts(artifact_text);
  const std::string summary_section =
      inferlab_artifact::json_object_section_or_empty(artifact_text,
                                                      "summary");
  const std::string& count_source =
      summary_section.empty() ? artifact_text : summary_section;
  projection.declared_repair_card_count_present =
      inferlab_artifact::contains_top_level_key_fallback(
          count_source, kRootCauseRepairCardCountField);
  if (projection.declared_repair_card_count_present) {
    projection.declared_repair_card_count_invalid =
        !inferlab_artifact::extract_unsigned_key_fallback(
            count_source, kRootCauseRepairCardCountField,
            projection.declared_repair_card_count);
  }

  projection.repair_cards_field_present =
      inferlab_artifact::contains_top_level_key_fallback(
          artifact_text, kRootCauseRepairCardsField);
  const std::string cards_section =
      inferlab_artifact::json_array_section_or_empty(
          artifact_text, kRootCauseRepairCardsField);
  projection.repair_cards_array_valid =
      projection.repair_cards_field_present && !cards_section.empty();
  const auto card_values =
      inferlab_artifact::top_level_json_values(cards_section);
  for (const auto& card : card_values) {
    const std::size_t card_start =
        inferlab_artifact::skip_json_space(card, 0);
    if (card_start >= card.size() || card[card_start] != '{') {
      projection.repair_cards_all_objects = false;
      continue;
    }
    RootCauseRepairCardEntryProjection entry;
    entry.category = root_cause_repair_card_category(
        card, entry.category_present);
    entry.source_locations_present =
        inferlab_artifact::contains_top_level_key_fallback(
            card, kRootCauseRepairCardSourceLocationsField);
    if (entry.source_locations_present) {
      const std::string locations_section =
          inferlab_artifact::json_array_section_or_empty(
              card, kRootCauseRepairCardSourceLocationsField);
      entry.source_locations_valid =
          !locations_section.empty() &&
          inferlab_artifact::parse_string_array_items_fallback(
              locations_section, entry.source_locations);
      if (entry.source_locations_valid &&
          std::any_of(entry.source_locations.begin(),
                      entry.source_locations.end(),
                      [](const std::string& location) {
                        return location.empty();
                      })) {
        entry.source_locations_valid = false;
        entry.source_locations.clear();
      }
    }
    if (entry.source_locations_valid && !entry.source_locations.empty()) {
      projection.source_location_card_count =
          inferlab::internal::checked_add_size(
              projection.source_location_card_count, 1U,
              "root-cause repair-card source-location count");
      if (projection.first_source_location.empty()) {
        projection.first_card_source_location_count =
            entry.source_locations.size();
        projection.first_source_location = entry.source_locations.front();
      }
    }
    projection.card_contract_issue =
        projection.card_contract_issue || !entry.category_present ||
        !entry.source_locations_valid;
    projection.cards.push_back(std::move(entry));
  }

  if (projection.repair_cards_array_valid) {
    projection.repair_card_count = card_values.size();
  } else if (projection.declared_repair_card_count_present &&
             !projection.declared_repair_card_count_invalid) {
    projection.repair_card_count =
        projection.declared_repair_card_count;
  }
  projection.declared_count_mismatch =
      projection.repair_cards_array_valid &&
      projection.declared_repair_card_count_present &&
      !projection.declared_repair_card_count_invalid &&
      projection.declared_repair_card_count != projection.repair_card_count;
  projection.source_location_gap_count =
      projection.repair_card_count > projection.source_location_card_count
          ? projection.repair_card_count -
                projection.source_location_card_count
          : 0U;
  projection.card_contract_issue =
      projection.card_contract_issue ||
      projection.declared_repair_card_count_invalid ||
      (projection.repair_cards_field_present &&
       !projection.repair_cards_array_valid) ||
      !projection.repair_cards_all_objects ||
      projection.declared_count_mismatch;
  return projection;
}

struct RootCauseRepairCategoryProjection {
  std::size_t repair_card_count = 0;
  std::size_t source_location_card_count = 0;
  std::size_t source_location_gap_count = 0;
  std::string first_source_location;
};

inline RootCauseRepairCategoryProjection project_root_cause_repair_category(
    const RootCauseRepairCardProjection& cards,
    const std::string& category) {
  RootCauseRepairCategoryProjection projection;
  for (const auto& card : cards.cards) {
    if (card.category != category) continue;
    projection.repair_card_count = inferlab::internal::checked_add_size(
        projection.repair_card_count, 1U,
        "root-cause category repair-card count");
    if (!card.source_locations.empty()) {
      projection.source_location_card_count =
          inferlab::internal::checked_add_size(
              projection.source_location_card_count, 1U,
              "root-cause category source-location card count");
      if (projection.first_source_location.empty()) {
        projection.first_source_location = card.source_locations.front();
      }
    }
  }
  projection.source_location_gap_count =
      projection.repair_card_count > projection.source_location_card_count
          ? projection.repair_card_count -
                projection.source_location_card_count
          : 0U;
  return projection;
}

}  // namespace inferlab_evidence
