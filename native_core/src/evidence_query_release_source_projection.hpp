#pragma once

#include "checked_index.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_repair_policy.hpp"
#include "root_cause_repair_counts.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence_query {

struct EvidenceQueryReleaseSourceProjection {
  inferlab_evidence::RootCauseRepairCountProjection repair_counts;
  inferlab_evidence::RootCauseRepairCardProjection root_cause_repair_cards;
  std::size_t root_cause_repair_card_count = 0;
  std::size_t current_repo_repair_card_count = 0;
  std::size_t target_product_replay_repair_card_count = 0;
  std::size_t target_gpu_execute_repair_card_count = 0;
  std::size_t target_machine_repair_card_count = 0;
  std::size_t flow_check_repair_card_count = 0;
  std::size_t root_cause_source_location_card_count = 0;
  std::size_t effective_source_location_card_count = 0;
  std::size_t repair_path_source_location_count = 0;
  std::size_t repair_path_source_location_gap_count = 0;
  std::size_t source_gap_count = 0;
  std::size_t source_read_issue_count = 0;
  std::size_t blocking_root_cause_count = 0;
  std::size_t top_root_cause_count = 0;
  std::string top_root_cause = "none";
  std::string release_decision = "blocked";
  bool root_cause_repair_cards_present = false;
};

inline EvidenceQueryReleaseSourceProjection
project_evidence_query_release_sources(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records) {
  EvidenceQueryReleaseSourceProjection projection;
  auto& cards = projection.root_cause_repair_cards;
  cards.source_present = true;
  cards.declared_repair_card_count_present = true;
  cards.repair_cards_field_present = true;
  cards.repair_cards_array_valid = true;
  auto& counts = cards.environment_counts;
  std::map<std::string, std::size_t> category_counts;
  counts.source_present = true;
  counts.current_repo_field_present = true;
  counts.target_product_replay_field_present = true;
  counts.target_gpu_execute_field_present = true;
  counts.legacy_target_machine_field_present = true;
  counts.typed_split_present = true;
  counts.split_status = "typed_split";

  for (const auto& record : records) {
    const std::string category = blocker_category(record);
    if (category == "none") continue;
    projection.blocking_root_cause_count =
        inferlab::internal::checked_add_size(
            projection.blocking_root_cause_count, 1U,
            "Evidence Query blocking root-cause count");
    auto& category_count = category_counts[category];
    category_count = inferlab::internal::checked_add_size(
        category_count, 1U, "Evidence Query root-cause category count");
    inferlab_evidence::add_root_cause_repair_category(counts.declared,
                                                       category);
    inferlab_evidence::RootCauseRepairCardEntryProjection entry;
    entry.category = category;
    entry.category_present = true;
    const auto locations = wiring_locations_for_category(category);
    const bool source_exact =
        category_has_exact_wiring_locations(category) && !locations.empty();
    entry.source_locations_present = source_exact;
    entry.source_locations_valid = true;
    if (source_exact) {
      entry.source_locations = locations;
      cards.source_location_card_count =
          inferlab::internal::checked_add_size(
              cards.source_location_card_count, 1U,
              "Evidence Query repair-card source-location count");
      if (cards.first_source_location.empty()) {
        cards.first_card_source_location_count = locations.size();
        cards.first_source_location = locations.front();
      }
    } else {
      cards.source_location_gap_count =
          inferlab::internal::checked_add_size(
              cards.source_location_gap_count, 1U,
              "Evidence Query repair-card source-location gap count");
    }
    cards.cards.push_back(std::move(entry));
  }

  cards.declared_repair_card_count = cards.cards.size();
  cards.repair_card_count = cards.cards.size();
  counts.effective_current_repo_repair_card_count =
      counts.declared.current_repo_repair_card_count;
  counts.effective_target_product_replay_repair_card_count =
      counts.declared.target_product_replay_repair_card_count;
  counts.effective_target_gpu_execute_repair_card_count =
      counts.declared.target_gpu_execute_repair_card_count;
  counts.legacy_target_machine_repair_card_count =
      inferlab_evidence::root_cause_target_machine_repair_card_count(
          counts.declared);

  projection.root_cause_repair_cards_present = true;
  projection.root_cause_repair_card_count =
      cards.repair_card_count;
  projection.repair_counts = counts;
  projection.current_repo_repair_card_count =
      projection.repair_counts.effective_current_repo_repair_card_count;
  projection.target_product_replay_repair_card_count =
      projection.repair_counts
          .effective_target_product_replay_repair_card_count;
  projection.target_gpu_execute_repair_card_count =
      projection.repair_counts.effective_target_gpu_execute_repair_card_count;
  projection.target_machine_repair_card_count =
      inferlab_evidence::effective_root_cause_target_machine_repair_card_count(
          projection.repair_counts);
  projection.root_cause_source_location_card_count =
      cards.source_location_card_count;
  const std::size_t source_location_card_count =
      projection.root_cause_source_location_card_count;
  projection.effective_source_location_card_count =
      source_location_card_count;
  projection.repair_path_source_location_count =
      std::min(projection.root_cause_repair_card_count,
               source_location_card_count);
  projection.repair_path_source_location_gap_count =
      projection.root_cause_repair_card_count > source_location_card_count
          ? projection.root_cause_repair_card_count -
                source_location_card_count
          : 0U;
  projection.source_gap_count = 0U;
  projection.source_read_issue_count = 0U;
  for (const auto& entry : category_counts) {
    if (entry.second > projection.top_root_cause_count) {
      projection.top_root_cause = entry.first;
      projection.top_root_cause_count = entry.second;
    }
  }
  projection.release_decision =
      projection.blocking_root_cause_count == 0U ? "report_ready" : "blocked";
  return projection;
}

}  // namespace inferlab_evidence_query
