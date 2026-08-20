#pragma once

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct CrossArtifactBlockerFamily {
  std::string family;
  std::string source_artifact;
  std::vector<std::string> source_artifacts;
  std::string blocker_reason;
  std::string primary_environment;
  std::string next_action;
  std::size_t blocker_count = 0;
  std::size_t semantic_linkage_blocker_count = 0;
  std::size_t replay_or_execute_missing_count = 0;
  std::size_t current_repo_action_count = 0;
  std::size_t target_or_validation_action_count = 0;
  std::size_t target_product_replay_action_count = 0;
  std::size_t target_gpu_execute_action_count = 0;
  std::size_t source_location_gap_count = 0;
  bool present = false;
  bool readable = false;
};

inline std::vector<std::string> cross_artifact_source_artifacts(
    const CrossArtifactBlockerFamily& family) {
  if (!family.source_artifacts.empty()) return family.source_artifacts;
  if (!family.source_artifact.empty()) return {family.source_artifact};
  return {};
}

inline std::string cross_artifact_family_status(
    const CrossArtifactBlockerFamily& family) {
  if (!family.present) return "source_missing";
  if (!family.readable) return "source_unreadable";
  const bool clear = family.blocker_count == 0U &&
                     family.semantic_linkage_blocker_count == 0U &&
                     family.replay_or_execute_missing_count == 0U &&
                     family.current_repo_action_count == 0U &&
                     family.target_or_validation_action_count == 0U &&
                     family.target_product_replay_action_count == 0U &&
                     family.target_gpu_execute_action_count == 0U &&
                     family.source_location_gap_count == 0U;
  if (clear) return "clear";
  if (family.source_location_gap_count > 0U) return "source_location_gap";
  if (family.current_repo_action_count > 0U) {
    return "current_repo_action_required";
  }
  if (family.target_or_validation_action_count > 0U ||
      family.target_product_replay_action_count > 0U ||
      family.target_gpu_execute_action_count > 0U) {
    return "target_or_validation_required";
  }
  return "blocked";
}

inline std::size_t cross_artifact_active_family_count(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::size_t count = 0U;
  for (const auto& family : families) {
    if (cross_artifact_family_status(family) != "clear") {
      count = inferlab::internal::checked_add_size(
          count, 1U, "cross-artifact active family count");
    }
  }
  return count;
}

inline std::size_t cross_artifact_blocker_total(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::size_t total = 0U;
  for (const auto& family : families) {
    total = inferlab::internal::checked_add_size(
        total, family.blocker_count, "cross-artifact blocker total");
  }
  return total;
}

inline std::size_t cross_artifact_current_repo_action_total(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::size_t total = 0U;
  for (const auto& family : families) {
    total = inferlab::internal::checked_add_size(
        total, family.current_repo_action_count,
        "cross-artifact current-repo action total");
  }
  return total;
}

inline std::size_t cross_artifact_target_or_validation_action_total(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::size_t total = 0U;
  for (const auto& family : families) {
    total = inferlab::internal::checked_add_size(
        total, family.target_or_validation_action_count,
        "cross-artifact target-or-validation action total");
  }
  return total;
}

inline std::size_t cross_artifact_source_location_gap_total(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::size_t total = 0U;
  for (const auto& family : families) {
    total = inferlab::internal::checked_add_size(
        total, family.source_location_gap_count,
        "cross-artifact source-location gap total");
  }
  return total;
}

inline const CrossArtifactBlockerFamily* find_cross_artifact_family(
    const std::vector<CrossArtifactBlockerFamily>& families,
    const std::string& family_name) {
  for (const auto& family : families) {
    if (family.family == family_name) return &family;
  }
  return nullptr;
}

inline std::string cross_artifact_blocker_explanation_status(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  if (cross_artifact_active_family_count(families) == 0U) return "clear";
  if (cross_artifact_current_repo_action_total(families) > 0U) {
    return "current_repo_action_required";
  }
  if (cross_artifact_source_location_gap_total(families) > 0U) {
    return "source_location_metadata_required";
  }
  if (cross_artifact_target_or_validation_action_total(families) > 0U) {
    return "target_or_validation_required";
  }
  return "blocked";
}

inline std::string cross_artifact_release_focus_decision(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  if (cross_artifact_active_family_count(families) == 0U) {
    return "no_cross_artifact_blockers";
  }
  if (cross_artifact_source_location_gap_total(families) > 0U) {
    return "blocked_by_source_location_metadata";
  }
  if (cross_artifact_current_repo_action_total(families) > 0U) {
    return "blocked_by_current_repo_actions";
  }
  if (cross_artifact_target_or_validation_action_total(families) > 0U) {
    return "blocked_by_target_or_validation_evidence";
  }
  return "blocked_by_unclassified_evidence";
}

inline std::string cross_artifact_release_focus_repair_lane(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  if (cross_artifact_active_family_count(families) == 0U) return "none";
  if (cross_artifact_source_location_gap_total(families) > 0U ||
      cross_artifact_current_repo_action_total(families) > 0U) {
    return "current_repo_fixable";
  }
  if (cross_artifact_target_or_validation_action_total(families) > 0U) {
    return "target_or_validation_required";
  }
  return "manual_review_required";
}

inline std::string cross_artifact_family_claim(const std::string& family) {
  if (family == "warehouse_metadata") return "artifact_contract_confidence";
  if (family == "root_cause_repair_cards") {
    return "release_root_cause_confidence";
  }
  if (family == "catalog_flow") return "catalog_single_source_confidence";
  if (family == "native_core_audit") return "native_core_audit_confidence";
  if (family == "product_replay_or_execute") {
    return "product_replay_execute_confidence";
  }
  return "evidence_governance_confidence";
}

inline std::vector<std::string> cross_artifact_affected_claims(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::vector<std::string> claims;
  for (const auto& family : families) {
    if (cross_artifact_family_status(family) == "clear") continue;
    inferlab_plan::append_unique_nonempty_string(
        claims, cross_artifact_family_claim(family.family));
  }
  return claims;
}

inline std::size_t cross_artifact_repair_priority_score(
    const CrossArtifactBlockerFamily& family) {
  if (cross_artifact_family_status(family) == "clear") return 0U;
  return inferlab::internal::checked_sum_size(
      {!family.present ? 500U : 0U,
       !family.readable ? 400U : 0U,
       inferlab::internal::checked_product_size(
           family.blocker_count, std::size_t{100},
           "cross-artifact blocker priority score"),
       inferlab::internal::checked_product_size(
           family.source_location_gap_count, std::size_t{30},
           "cross-artifact source-location priority score"),
       inferlab::internal::checked_product_size(
           family.current_repo_action_count, std::size_t{20},
           "cross-artifact current-repo priority score"),
       inferlab::internal::checked_product_size(
           family.target_or_validation_action_count, std::size_t{10},
           "cross-artifact target priority score"),
       family.primary_environment == "current_repo_fixable" ? 15U : 0U,
       family.primary_environment == "target_gpu_or_execute_machine" ||
               family.primary_environment ==
                   "target_product_or_replay_evidence"
           ? 8U
           : 0U},
      "cross-artifact repair priority score");
}

inline bool cross_artifact_repair_path_precedes(
    const CrossArtifactBlockerFamily& lhs,
    const CrossArtifactBlockerFamily& rhs) {
  const auto lhs_score = cross_artifact_repair_priority_score(lhs);
  const auto rhs_score = cross_artifact_repair_priority_score(rhs);
  if (lhs_score != rhs_score) return lhs_score > rhs_score;
  if (lhs.blocker_count != rhs.blocker_count) {
    return lhs.blocker_count > rhs.blocker_count;
  }
  return lhs.family < rhs.family;
}

inline const CrossArtifactBlockerFamily* primary_cross_artifact_family(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  const CrossArtifactBlockerFamily* selected = nullptr;
  for (const auto& family : families) {
    if (cross_artifact_family_status(family) == "clear") continue;
    if (selected == nullptr ||
        cross_artifact_repair_path_precedes(family, *selected)) {
      selected = &family;
    }
  }
  return selected;
}

inline std::vector<CrossArtifactBlockerFamily>
ranked_cross_artifact_repair_paths(
    const std::vector<CrossArtifactBlockerFamily>& families) {
  std::vector<CrossArtifactBlockerFamily> paths;
  for (const auto& family : families) {
    if (cross_artifact_family_status(family) != "clear") {
      paths.push_back(family);
    }
  }
  std::sort(paths.begin(), paths.end(), cross_artifact_repair_path_precedes);
  return paths;
}

}  // namespace inferlab_evidence_query
