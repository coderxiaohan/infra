#pragma once

#include "evidence_query_cross_artifact.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string cross_artifact_repair_path_json(
    const CrossArtifactBlockerFamily& family, const std::size_t rank,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const auto source_artifacts = cross_artifact_source_artifacts(family);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"rank\": " << rank << ",\n";
  out << nested << "\"family\": " << inferlab_plan::json_string(family.family)
      << ",\n";
  out << nested << "\"source_artifact\": "
      << inferlab_plan::json_string(family.source_artifact) << ",\n";
  out << nested << "\"source_artifacts\": "
      << inferlab_plan::json_string_array(source_artifacts, indent + 2)
      << ",\n";
  out << nested << "\"source_artifact_count\": " << source_artifacts.size()
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(cross_artifact_family_status(family))
      << ",\n";
  out << nested << "\"priority_score\": "
      << cross_artifact_repair_priority_score(family) << ",\n";
  out << nested << "\"repair_environment\": "
      << inferlab_plan::json_string(family.primary_environment) << ",\n";
  out << nested << "\"affected_claim\": "
      << inferlab_plan::json_string(cross_artifact_family_claim(family.family))
      << ",\n";
  out << nested << "\"blocker_reason\": "
      << inferlab_plan::json_string(family.blocker_reason) << ",\n";
  out << nested << "\"blocker_count\": " << family.blocker_count << ",\n";
  out << nested << "\"semantic_linkage_blocker_count\": "
      << family.semantic_linkage_blocker_count << ",\n";
  out << nested << "\"replay_or_execute_missing_count\": "
      << family.replay_or_execute_missing_count << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << family.current_repo_action_count << ",\n";
  out << nested << "\"target_or_validation_action_count\": "
      << family.target_or_validation_action_count << ",\n";
  out << nested << "\"target_product_replay_action_count\": "
      << family.target_product_replay_action_count << ",\n";
  out << nested << "\"target_gpu_execute_action_count\": "
      << family.target_gpu_execute_action_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << family.source_location_gap_count << ",\n";
  out << nested << "\"current_repo_actionable\": "
      << (family.current_repo_action_count > 0U ? "true" : "false")
      << ",\n";
  out << nested << "\"target_or_validation_required\": "
      << (family.target_or_validation_action_count > 0U ? "true" : "false")
      << ",\n";
  out << nested << "\"source_location_metadata_required\": "
      << (family.source_location_gap_count > 0U ? "true" : "false")
      << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(family.next_action) << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string cross_artifact_release_focus_json(
    const std::vector<CrossArtifactBlockerFamily>& families,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const auto* primary = primary_cross_artifact_family(families);
  const auto affected_claims = cross_artifact_affected_claims(families);
  const auto repair_paths = ranked_cross_artifact_repair_paths(families);
  const CrossArtifactBlockerFamily* first_path =
      repair_paths.empty() ? nullptr : &repair_paths.front();
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(
             cross_artifact_release_focus_decision(families))
      << ",\n";
  out << nested << "\"repair_lane\": "
      << inferlab_plan::json_string(
             cross_artifact_release_focus_repair_lane(families))
      << ",\n";
  out << nested << "\"primary_family\": "
      << inferlab_plan::json_string(primary == nullptr ? "" : primary->family)
      << ",\n";
  out << nested << "\"primary_environment\": "
      << inferlab_plan::json_string(
             primary == nullptr ? "" : primary->primary_environment)
      << ",\n";
  out << nested << "\"primary_action\": "
      << inferlab_plan::json_string(primary == nullptr ? ""
                                                       : primary->next_action)
      << ",\n";
  out << nested << "\"affected_claim_count\": " << affected_claims.size()
      << ",\n";
  out << nested << "\"affected_claims\": [";
  for (std::size_t i = 0; i < affected_claims.size(); ++i) {
    if (i > 0U) out << ", ";
    out << inferlab_plan::json_string(affected_claims[i]);
  }
  out << "],\n";
  out << nested << "\"repair_path_count\": " << repair_paths.size()
      << ",\n";
  out << nested << "\"first_repair_family\": "
      << inferlab_plan::json_string(first_path == nullptr ? ""
                                                            : first_path->family)
      << ",\n";
  out << nested << "\"first_repair_environment\": "
      << inferlab_plan::json_string(
             first_path == nullptr ? "" : first_path->primary_environment)
      << ",\n";
  out << nested << "\"first_repair_action\": "
      << inferlab_plan::json_string(first_path == nullptr
                                        ? ""
                                        : first_path->next_action)
      << ",\n";
  out << nested << "\"current_repo_actionable\": "
      << (cross_artifact_current_repo_action_total(families) > 0U ? "true"
                                                                  : "false")
      << ",\n";
  out << nested << "\"target_or_validation_required\": "
      << (cross_artifact_target_or_validation_action_total(families) > 0U
              ? "true"
              : "false")
      << ",\n";
  out << nested << "\"source_location_metadata_required\": "
      << (cross_artifact_source_location_gap_total(families) > 0U ? "true"
                                                                  : "false")
      << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? "no cross-artifact blocker"
                                        : primary->next_action)
      << ",\n";
  out << nested << "\"repair_paths\": [";
  for (std::size_t i = 0; i < repair_paths.size(); ++i) {
    if (i > 0U) out << ",";
    out << "\n"
        << cross_artifact_repair_path_json(repair_paths[i], i + 1U,
                                           indent + 4);
  }
  if (!repair_paths.empty()) out << "\n" << nested;
  out << "],\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "release focus is a read-only explanation derived from existing evidence; it does not execute repair commands, refresh artifacts, replay products, or create benchmark/execute claims")
      << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string cross_artifact_blocker_family_json(
    const CrossArtifactBlockerFamily& family, const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const auto source_artifacts = cross_artifact_source_artifacts(family);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"family\": " << inferlab_plan::json_string(family.family)
      << ",\n";
  out << nested << "\"source_artifact\": "
      << inferlab_plan::json_string(family.source_artifact) << ",\n";
  out << nested << "\"source_artifacts\": "
      << inferlab_plan::json_string_array(source_artifacts, indent + 2)
      << ",\n";
  out << nested << "\"source_artifact_count\": " << source_artifacts.size()
      << ",\n";
  out << nested << "\"present\": " << (family.present ? "true" : "false")
      << ",\n";
  out << nested << "\"readable\": " << (family.readable ? "true" : "false")
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(cross_artifact_family_status(family))
      << ",\n";
  out << nested << "\"blocker_reason\": "
      << inferlab_plan::json_string(family.blocker_reason) << ",\n";
  out << nested << "\"primary_environment\": "
      << inferlab_plan::json_string(family.primary_environment) << ",\n";
  out << nested << "\"blocker_count\": " << family.blocker_count << ",\n";
  out << nested << "\"semantic_linkage_blocker_count\": "
      << family.semantic_linkage_blocker_count << ",\n";
  out << nested << "\"replay_or_execute_missing_count\": "
      << family.replay_or_execute_missing_count << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << family.current_repo_action_count << ",\n";
  out << nested << "\"target_or_validation_action_count\": "
      << family.target_or_validation_action_count << ",\n";
  out << nested << "\"target_product_replay_action_count\": "
      << family.target_product_replay_action_count << ",\n";
  out << nested << "\"target_gpu_execute_action_count\": "
      << family.target_gpu_execute_action_count << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << family.source_location_gap_count << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(family.next_action) << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string cross_artifact_summary_json_fields(
    const std::vector<CrossArtifactBlockerFamily>& families,
    const int indent) {
  const std::string pad(indent, ' ');
  const auto active_family_count = cross_artifact_active_family_count(families);
  const auto blocker_total = cross_artifact_blocker_total(families);
  const auto current_repo_action_total =
      cross_artifact_current_repo_action_total(families);
  const auto target_or_validation_action_total =
      cross_artifact_target_or_validation_action_total(families);
  const auto source_location_gap_total =
      cross_artifact_source_location_gap_total(families);
  const auto* primary = primary_cross_artifact_family(families);
  const auto affected_claims = cross_artifact_affected_claims(families);
  const auto repair_paths = ranked_cross_artifact_repair_paths(families);
  const CrossArtifactBlockerFamily* first_path =
      repair_paths.empty() ? nullptr : &repair_paths.front();
  const auto* product_family =
      find_cross_artifact_family(families, "product_replay_or_execute");
  const auto product_family_sources =
      product_family == nullptr
          ? std::vector<std::string>{}
          : cross_artifact_source_artifacts(*product_family);

  std::ostringstream out;
  out << pad << "\"cross_artifact_blocker_explanation_present\": true,\n";
  out << pad << "\"cross_artifact_blocker_explanation_status\": "
      << inferlab_plan::json_string(
             cross_artifact_blocker_explanation_status(families))
      << ",\n";
  out << pad << "\"cross_artifact_blocker_family_count\": "
      << families.size() << ",\n";
  out << pad << "\"cross_artifact_active_family_count\": "
      << active_family_count << ",\n";
  out << pad << "\"cross_artifact_total_blocker_count\": "
      << blocker_total << ",\n";
  out << pad << "\"cross_artifact_current_repo_action_count\": "
      << current_repo_action_total << ",\n";
  out << pad << "\"cross_artifact_target_or_validation_action_count\": "
      << target_or_validation_action_total << ",\n";
  out << pad << "\"cross_artifact_source_location_gap_count\": "
      << source_location_gap_total << ",\n";
  out << pad << "\"cross_artifact_primary_family\": "
      << inferlab_plan::json_string(primary == nullptr ? "" : primary->family)
      << ",\n";
  out << pad << "\"cross_artifact_primary_blocker_reason\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->blocker_reason)
      << ",\n";
  out << pad << "\"cross_artifact_primary_environment\": "
      << inferlab_plan::json_string(
             primary == nullptr ? "" : primary->primary_environment)
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_present\": true,\n";
  out << pad << "\"cross_artifact_release_focus_decision\": "
      << inferlab_plan::json_string(
             cross_artifact_release_focus_decision(families))
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_repair_lane\": "
      << inferlab_plan::json_string(
             cross_artifact_release_focus_repair_lane(families))
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_affected_claim_count\": "
      << affected_claims.size() << ",\n";
  out << pad << "\"cross_artifact_release_focus_repair_path_count\": "
      << repair_paths.size() << ",\n";
  out << pad << "\"cross_artifact_release_focus_first_repair_family\": "
      << inferlab_plan::json_string(first_path == nullptr ? ""
                                                            : first_path->family)
      << ",\n";
  out << pad
      << "\"cross_artifact_release_focus_first_repair_environment\": "
      << inferlab_plan::json_string(
             first_path == nullptr ? "" : first_path->primary_environment)
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_first_repair_action\": "
      << inferlab_plan::json_string(first_path == nullptr
                                        ? ""
                                        : first_path->next_action)
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_primary_action\": "
      << inferlab_plan::json_string(primary == nullptr ? ""
                                                       : primary->next_action)
      << ",\n";
  out << pad << "\"cross_artifact_release_focus_current_repo_actionable\": "
      << (current_repo_action_total > 0U ? "true" : "false") << ",\n";
  out << pad
      << "\"cross_artifact_release_focus_target_or_validation_required\": "
      << (target_or_validation_action_total > 0U ? "true" : "false")
      << ",\n";
  out << pad
      << "\"cross_artifact_release_focus_source_location_metadata_required\": "
      << (source_location_gap_total > 0U ? "true" : "false") << ",\n";
  out << pad << "\"cross_artifact_product_replay_family_present\": "
      << (product_family != nullptr ? "true" : "false") << ",\n";
  out << pad << "\"cross_artifact_product_replay_family_status\": "
      << inferlab_plan::json_string(product_family == nullptr
                                        ? "not_reported"
                                        : cross_artifact_family_status(
                                              *product_family))
      << ",\n";
  out << pad << "\"cross_artifact_product_replay_family_blocker_count\": "
      << (product_family == nullptr ? 0U : product_family->blocker_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_semantic_linkage_blocker_count\": "
      << (product_family == nullptr
              ? 0U
              : product_family->semantic_linkage_blocker_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_replay_or_execute_missing_count\": "
      << (product_family == nullptr
              ? 0U
              : product_family->replay_or_execute_missing_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_target_or_validation_action_count\": "
      << (product_family == nullptr
              ? 0U
              : product_family->target_or_validation_action_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_target_product_replay_action_count\": "
      << (product_family == nullptr
              ? 0U
              : product_family->target_product_replay_action_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_target_gpu_execute_action_count\": "
      << (product_family == nullptr
              ? 0U
              : product_family->target_gpu_execute_action_count)
      << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_source_artifact_count\": "
      << product_family_sources.size() << ",\n";
  out << pad
      << "\"cross_artifact_product_replay_family_first_source_artifact\": "
      << inferlab_plan::json_string(product_family_sources.empty()
                                        ? ""
                                        : product_family_sources.front())
      << ",\n";
  out << pad << "\"cross_artifact_product_replay_family_next_action\": "
      << inferlab_plan::json_string(product_family == nullptr
                                        ? ""
                                        : product_family->next_action)
      << ",\n";
  return out.str();
}

inline std::string cross_artifact_blocker_explanation_json(
    const std::vector<CrossArtifactBlockerFamily>& families,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const auto* primary = primary_cross_artifact_family(families);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"present\": true,\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(
             cross_artifact_blocker_explanation_status(families))
      << ",\n";
  out << nested << "\"family_count\": " << families.size() << ",\n";
  out << nested << "\"active_family_count\": "
      << cross_artifact_active_family_count(families) << ",\n";
  out << nested << "\"total_blocker_count\": "
      << cross_artifact_blocker_total(families) << ",\n";
  out << nested << "\"current_repo_action_count\": "
      << cross_artifact_current_repo_action_total(families) << ",\n";
  out << nested << "\"target_or_validation_action_count\": "
      << cross_artifact_target_or_validation_action_total(families) << ",\n";
  out << nested << "\"source_location_gap_count\": "
      << cross_artifact_source_location_gap_total(families) << ",\n";
  out << nested << "\"primary_family\": "
      << inferlab_plan::json_string(primary == nullptr ? "" : primary->family)
      << ",\n";
  out << nested << "\"primary_blocker_reason\": "
      << inferlab_plan::json_string(primary == nullptr
                                        ? ""
                                        : primary->blocker_reason)
      << ",\n";
  out << nested << "\"primary_environment\": "
      << inferlab_plan::json_string(
             primary == nullptr ? "" : primary->primary_environment)
      << ",\n";
  out << nested << "\"primary_next_action\": "
      << inferlab_plan::json_string(primary == nullptr ? ""
                                                       : primary->next_action)
      << ",\n";
  out << nested << "\"release_focus\": "
      << cross_artifact_release_focus_json(families, indent + 2) << ",\n";
  out << nested << "\"families\": [";
  for (std::size_t i = 0; i < families.size(); ++i) {
    if (i > 0U) out << ",";
    out << "\n" << cross_artifact_blocker_family_json(families[i], indent + 4);
  }
  if (!families.empty()) out << "\n" << nested;
  out << "],\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "cross-artifact blocker explanation correlates existing evidence only; it does not execute repairs, refresh artifacts, replay products, or create benchmark/execute claims")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
