#include "release_evidence_pipeline_repair_projection.hpp"

#include "checked_index.hpp"
#include "release_evidence_pipeline_repair_group_projection.hpp"
#include "release_evidence_pipeline_repair_metrics.hpp"
#include "release_evidence_pipeline_repair_producer.hpp"
#include "root_cause_repair_counts.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

using inferlab_evidence::RepairEnvironment;

std::vector<NonGpuReviewRepairPath> collect_repair_paths(
    const ReleasePipelineSourceContext& sources) {
  RepairPathCollector paths;
  append_evidence_query_repair_paths(sources, paths);
  append_native_audit_repair_paths(sources, paths);
  return paths.take_paths();
}

void validate_repair_paths(
    const std::vector<NonGpuReviewRepairPath>& paths) {
  std::map<std::string, bool> path_ids;
  for (const auto& path : paths) {
    if (path.id.empty() || path.source.empty() || path.category.empty() ||
        path.severity.empty() || path.trigger.empty() ||
        path.recommended_action.empty() || path.command.empty()) {
      throw std::logic_error(
          "release pipeline repair path metadata must be nonempty");
    }
    if (!path_ids.emplace(path.id, true).second) {
      throw std::logic_error("duplicate release pipeline repair path id: " +
                             path.id);
    }
    if (path.repair_environment == RepairEnvironment::kUnknown) {
      throw std::logic_error(
          "release pipeline repair path environment must be known: " +
          path.id);
    }
    if (path.source_locations.empty()) {
      throw std::logic_error(
          "release pipeline repair path must have a source location: " +
          path.id);
    }
    std::map<std::string, bool> locations;
    for (const auto& location : path.source_locations) {
      if (location.empty() || !locations.emplace(location, true).second) {
        throw std::logic_error(
            "release pipeline repair path has invalid source locations: " +
            path.id);
      }
    }
    std::map<std::string, bool> counter_names;
    for (const auto& counter : path.evidence_counters) {
      if (counter.name.empty() || counter.value < 0 ||
          !counter_names.emplace(counter.name, true).second) {
        throw std::logic_error(
            "release pipeline repair path has invalid evidence counters: " +
            path.id);
      }
    }
  }
}

std::vector<NonGpuReviewRepairPath> project_repair_paths(
    const ReleasePipelineSourceContext& sources) {
  auto paths = collect_repair_paths(sources);
  validate_repair_paths(paths);
  return paths;
}

struct RootCauseRepairLinkCounts {
  int path_count = 0;
  int current_repo_path_count = 0;
  int target_or_validation_path_count = 0;
  int source_location_gap_count = 0;
  int linked_path_count = 0;
  int linked_card_count = 0;
};

RootCauseRepairLinkCounts root_cause_repair_link_counts(
    const inferlab_evidence::RootCauseRepairCardProjection& cards,
    const std::vector<NonGpuReviewRepairPath>& repair_paths,
    const std::string& root_cause_source) {
  RootCauseRepairLinkCounts counts;
  std::set<std::string> root_cause_path_categories;
  for (const auto& path : repair_paths) {
    if (path.source != root_cause_source) continue;
    counts.path_count = inferlab::internal::checked_add_int(
        counts.path_count, 1, "release pipeline Root Cause repair-path count");
    if (inferlab_evidence::repair_environment_is_current_repo(
            path.repair_environment)) {
      counts.current_repo_path_count = inferlab::internal::checked_add_int(
          counts.current_repo_path_count, 1,
          "release pipeline Root Cause current-repo repair-path count");
    }
    if (inferlab_evidence::repair_environment_is_target_or_validation(
            path.repair_environment)) {
      counts.target_or_validation_path_count =
          inferlab::internal::checked_add_int(
              counts.target_or_validation_path_count, 1,
              "release pipeline Root Cause target repair-path count");
    }
    if (path.source_locations.empty()) {
      counts.source_location_gap_count = inferlab::internal::checked_add_int(
          counts.source_location_gap_count, 1,
          "release pipeline Root Cause repair-path source-location gap count");
    }
    root_cause_path_categories.insert(path.category);
  }

  for (const auto& path : repair_paths) {
    if (path.source != root_cause_source) continue;
    const bool linked = std::any_of(
        cards.cards.begin(), cards.cards.end(), [&](const auto& card) {
          return card.category_present && card.category == path.category;
        });
    if (linked) {
      counts.linked_path_count = inferlab::internal::checked_add_int(
          counts.linked_path_count, 1,
          "release pipeline linked Root Cause repair-path count");
    }
  }
  for (const auto& card : cards.cards) {
    if (card.category_present &&
        root_cause_path_categories.count(card.category) != 0U) {
      counts.linked_card_count = inferlab::internal::checked_add_int(
          counts.linked_card_count, 1,
          "release pipeline linked Root Cause repair-card count");
    }
  }
  return counts;
}

std::string blocker_link_status(const bool root_cause_present,
                                const int root_cause_card_count,
                                const int release_repair_path_count,
                                const int linked_repair_path_count,
                                const int unmatched_root_cause_card_count) {
  if (!root_cause_present && release_repair_path_count == 0) {
    return "missing_inputs";
  }
  if (!root_cause_present) return "missing_root_cause_artifact";
  if (root_cause_card_count == 0 && release_repair_path_count == 0) {
    return "no_active_blocker_repair_link";
  }
  if (root_cause_card_count == 0) return "missing_root_cause_cards";
  if (release_repair_path_count == 0) return "missing_release_repair_paths";
  if (linked_repair_path_count != release_repair_path_count) {
    return "partial_link";
  }
  return unmatched_root_cause_card_count > 0
             ? "linked_with_unaggregated_root_causes"
             : "linked";
}

std::size_t blocker_link_contract_issue_count(
    const bool root_cause_present,
    const int release_repair_path_count,
    const int linked_repair_path_count) {
  if (!root_cause_present) return 0U;
  return linked_repair_path_count < release_repair_path_count ? 1U : 0U;
}

}  // namespace

ReleasePipelineBlockerRepairCardLinkProjection::
    ReleasePipelineBlockerRepairCardLinkProjection(
        const ReleasePipelineRootCauseSourceProjection& root_cause_value,
        const std::vector<NonGpuReviewRepairPath>& repair_paths)
    : root_cause(root_cause_value) {
  const auto& cards = root_cause.repair_cards;
  const auto& environment = cards.environment_counts;
  root_cause_repair_cards_present = cards.repair_cards_field_present;
  root_cause_repair_card_count = repair_counter_value(
      cards.repair_card_count, "release pipeline Root Cause repair-card count");
  root_cause_current_repo_repair_card_count = repair_counter_value(
      environment.effective_current_repo_repair_card_count,
      "release pipeline Root Cause current-repo repair-card count");
  root_cause_target_product_replay_repair_card_count = repair_counter_value(
      environment.effective_target_product_replay_repair_card_count,
      "release pipeline Root Cause product/replay repair-card count");
  root_cause_target_gpu_execute_repair_card_count = repair_counter_value(
      environment.effective_target_gpu_execute_repair_card_count,
      "release pipeline Root Cause GPU/execute repair-card count");
  root_cause_target_machine_repair_card_count = repair_counter_value(
      inferlab_evidence::effective_root_cause_target_machine_repair_card_count(
          environment),
      "release pipeline Root Cause target-machine repair-card count");
  root_cause_repair_environment_split_status = environment.split_status;
  root_cause_repair_environment_split_schema_gap =
      environment.split_schema_gap;
  root_cause_repair_environment_legacy_fallback_used =
      environment.legacy_fallback_used;
  root_cause_flow_check_repair_card_count = repair_counter_value(
      root_cause.flow_check_repair_card_count,
      "release pipeline Root Cause flow-check repair-card count");
  total_release_repair_path_count = repair_path_count_as_int(repair_paths);
  const auto link_counts = root_cause_repair_link_counts(
      cards, repair_paths, root_cause.artifact.artifact);
  release_repair_path_count = link_counts.path_count;
  release_current_repo_repair_path_count =
      link_counts.current_repo_path_count;
  release_target_or_validation_repair_path_count =
      link_counts.target_or_validation_path_count;
  repair_path_source_location_gap_count =
      link_counts.source_location_gap_count;
  linked_root_cause_to_release_path_count = link_counts.linked_path_count;
  linked_root_cause_repair_card_count = link_counts.linked_card_count;
  unmatched_release_repair_path_count =
      positive_delta(release_repair_path_count,
                     linked_root_cause_to_release_path_count);
  unmatched_root_cause_repair_card_count =
      positive_delta(root_cause_repair_card_count,
                     linked_root_cause_repair_card_count);
  link_present = root_cause.source_present || release_repair_path_count > 0;
  link_status = blocker_link_status(root_cause.source_present,
                                    root_cause_repair_card_count,
                                    release_repair_path_count,
                                    linked_root_cause_to_release_path_count,
                                    unmatched_root_cause_repair_card_count);
  upstream_source_contract_issue_count =
      root_cause.repair_card_contract_issue_count;
  link_contract_issue_count =
      blocker_link_contract_issue_count(root_cause.source_present,
                                        release_repair_path_count,
                                        linked_root_cause_to_release_path_count);
  source_contract_issue_count = inferlab::internal::checked_add_size(
      upstream_source_contract_issue_count, link_contract_issue_count,
      "release pipeline blocker-link source contract issue count");

  const auto first_path_it = std::find_if(
      repair_paths.begin(), repair_paths.end(), [&](const auto& path) {
        return path.source == root_cause.artifact.artifact;
      });
  if (first_path_it != repair_paths.end()) {
    const auto& first_path = *first_path_it;
    first_release_repair_path_present = true;
    first_release_repair_path_id = first_path.id;
    first_release_repair_path_category = first_path.category;
    first_release_repair_environment =
        inferlab_evidence::repair_environment_name(
            first_path.repair_environment);
    if (!first_path.source_locations.empty()) {
      first_release_repair_source_location =
          first_path.source_locations.front();
    }
    recommended_action = first_path.recommended_action;
  } else {
    recommended_action =
        "refresh Evidence Query and release pipeline artifacts when validation is enabled";
  }
  current_repo_action_required =
      release_current_repo_repair_path_count > 0;
  target_or_validation_action_required =
      release_target_or_validation_repair_path_count > 0;
}

ReleasePipelineRepairProjection::ReleasePipelineRepairProjection(
    const ReleasePipelineSourceContext& sources)
    : paths(project_repair_paths(sources)),
      groups(active_repair_path_groups(paths)) {
  for (const auto& group : groups) {
    if (group.spec.grouping == "environment") {
      environment_group_count = inferlab::internal::checked_add_size(
          environment_group_count, 1U,
          "release pipeline environment repair-group count");
    } else if (group.spec.grouping == "category" ||
               group.spec.grouping == "category_family") {
      category_group_count = inferlab::internal::checked_add_size(
          category_group_count, 1U,
          "release pipeline category repair-group count");
    }
    if (group.current_repo_action_required) {
      current_repo_group_count = inferlab::internal::checked_add_size(
          current_repo_group_count, 1U,
          "release pipeline current-repo repair-group count");
    }
    if (group.target_or_validation_action_required) {
      target_or_validation_group_count =
          inferlab::internal::checked_add_size(
              target_or_validation_group_count, 1U,
              "release pipeline target-or-validation repair-group count");
    }
  }
}

}  // namespace inferlab_release_pipeline
