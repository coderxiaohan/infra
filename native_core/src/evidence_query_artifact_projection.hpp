#pragma once

#include "checked_index.hpp"
#include "evidence_query_record_policy.hpp"
#include "evidence_query_repair_policy.hpp"
#include "evidence_query_selection.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline constexpr std::size_t kMaxShowArtifactCandidates = 8U;
static_assert(kMaxShowArtifactCandidates > 0U,
              "show-artifact candidate sample must not be empty");

struct ShowArtifactDetailProjection {
  ShowArtifactSelection selection;
  std::string status;
  std::string category;
  std::string repair_environment;
  std::string confidence;
  std::vector<std::string> affected_claims;
  std::vector<std::string> wiring_locations;
  inferlab_evidence::RepairEnvironment repair_environment_kind =
      inferlab_evidence::RepairEnvironment::kNone;
  bool source_exact = false;
  std::size_t source_location_gap_count = 0;
};

struct QueryArtifactProjection {
  // Borrowed records remain valid while the source record vector is unchanged.
  std::vector<const inferlab_artifact::WarehouseArtifactRecord*>
      matched_records;
  ShowArtifactDetailProjection show_artifact;
  std::map<std::string, std::size_t> blocker_category_counts;
  std::size_t blocked_result_count = 0;
  std::size_t blocker_record_count = 0;
  std::size_t execute_verified_count = 0;
  std::size_t replay_supported_count = 0;
  std::size_t dry_run_ready_count = 0;
  std::size_t schema_only_count = 0;
};

namespace artifact_projection_detail {

inline void increment(std::size_t& value, const char* name) {
  value = inferlab::internal::checked_add_size(value, 1U, name);
}

inline void append_unique_candidate(std::vector<std::string>& candidates,
                                    const std::string& artifact) {
  for (const auto& candidate : candidates) {
    if (candidate == artifact) return;
  }
  if (candidates.size() < kMaxShowArtifactCandidates) {
    candidates.push_back(artifact);
  }
}

inline void resolve_show_artifact_detail(
    const std::string& query,
    ShowArtifactDetailProjection& detail) {
  const auto* selected = detail.selection.selected;
  if (selected == nullptr) return;

  detail.status = result_status(*selected, query);
  detail.category = blocker_category(*selected);
  detail.repair_environment = repair_environment(*selected);
  detail.repair_environment_kind =
      inferlab_evidence::parse_repair_environment(
          detail.repair_environment);
  detail.confidence = evidence_confidence(*selected);
  detail.affected_claims = affected_claims(*selected);
  if (detail.category != "none") {
    detail.wiring_locations =
        wiring_locations_for_category(detail.category);
    detail.source_exact =
        category_has_exact_wiring_locations(detail.category) &&
        !detail.wiring_locations.empty();
    detail.source_location_gap_count = detail.source_exact ? 0U : 1U;
  }
}

}  // namespace artifact_projection_detail

inline QueryArtifactProjection query_artifact_projection(
    const std::vector<inferlab_artifact::WarehouseArtifactRecord>& records,
    const QueryOperation& operation,
    const std::string& query,
    const std::string& artifact_filter,
    const std::string& blocker_filter) {
  QueryArtifactProjection projection;
  auto& selection = projection.show_artifact.selection;
  selection.selector =
      operation.selector.empty() ? artifact_filter : operation.selector;
  const inferlab_artifact::WarehouseArtifactRecord* first_fragment_match =
      nullptr;
  const inferlab_artifact::WarehouseArtifactRecord* exact_match_record =
      nullptr;
  std::size_t exact_match_count = 0;
  std::vector<std::string> fragment_candidates;

  for (const auto& record : records) {
    if (!include_record(record, query, artifact_filter, blocker_filter)) {
      continue;
    }
    projection.matched_records.push_back(&record);
    const std::string& confidence = evidence_confidence(record);
    if (confidence == "stale_or_blocked") {
      artifact_projection_detail::increment(
          projection.blocked_result_count,
          "query artifact blocked-result count");
    } else if (confidence == "execute_verified") {
      artifact_projection_detail::increment(
          projection.execute_verified_count,
          "query artifact execute-verified count");
    } else if (confidence == "replay_supported") {
      artifact_projection_detail::increment(
          projection.replay_supported_count,
          "query artifact replay-supported count");
    } else if (confidence == "dry_run_ready") {
      artifact_projection_detail::increment(
          projection.dry_run_ready_count,
          "query artifact dry-run-ready count");
    } else if (confidence == "schema_only") {
      artifact_projection_detail::increment(
          projection.schema_only_count,
          "query artifact schema-only count");
    }

    const std::string& category = blocker_category(record);
    if (category != "none") {
      artifact_projection_detail::increment(
          projection.blocker_record_count,
          "query artifact blocker-record count");
      artifact_projection_detail::increment(
          projection.blocker_category_counts[category],
          "query artifact blocker-category count");
    }

    const bool selector_match =
        selection.selector.empty() || record.artifact == selection.selector ||
        inferlab_plan::contains(record.artifact, selection.selector);
    if (!selector_match) continue;
    if (first_fragment_match == nullptr) first_fragment_match = &record;
    artifact_projection_detail::increment(
        selection.candidate_count,
        "show-artifact candidate count");
    const bool exact = !selection.selector.empty() &&
                       record.artifact == selection.selector;
    if (exact) {
      artifact_projection_detail::increment(exact_match_count,
                                             "show-artifact exact count");
      if (exact_match_record == nullptr) exact_match_record = &record;
    } else {
      artifact_projection_detail::append_unique_candidate(
          fragment_candidates, record.artifact);
    }
  }

  selection.exact_match = exact_match_record != nullptr;
  selection.selected =
      selection.exact_match ? exact_match_record : first_fragment_match;
  if (selection.exact_match) {
    selection.candidate_artifacts.push_back(exact_match_record->artifact);
  }
  const std::size_t remaining_slots =
      kMaxShowArtifactCandidates - selection.candidate_artifacts.size();
  for (std::size_t index = 0;
       index < fragment_candidates.size() && index < remaining_slots;
       ++index) {
    selection.candidate_artifacts.push_back(fragment_candidates[index]);
  }
  selection.candidate_list_truncated =
      selection.candidate_count > selection.candidate_artifacts.size();
  selection.selector_ambiguous =
      exact_match_count > 1U ||
      (!selection.exact_match && selection.candidate_count > 1U);

  if (selection.selected == nullptr) {
    selection.selection_strategy = "no_match";
  } else if (selection.exact_match && exact_match_count > 1U) {
    selection.selection_strategy = "exact_artifact_match_ambiguous";
  } else if (selection.exact_match) {
    selection.selection_strategy = "exact_artifact_match";
  } else if (selection.selector.empty()) {
    selection.selection_strategy = "unscoped_first_match";
  } else if (selection.selector_ambiguous) {
    selection.selection_strategy = "fragment_first_match_ambiguous";
  } else {
    selection.selection_strategy = "fragment_match";
  }
  artifact_projection_detail::resolve_show_artifact_detail(
      query, projection.show_artifact);
  return projection;
}

}  // namespace inferlab_evidence_query
