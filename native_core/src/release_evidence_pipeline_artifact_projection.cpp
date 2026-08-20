#include "release_evidence_pipeline_artifact_projection.hpp"

#include "checked_index.hpp"
#include "evidence_domain_types.hpp"
#include "release_evidence_pipeline_repair_group_projection.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

using inferlab_evidence::repair_environment_is_current_repo;
using inferlab_evidence::repair_environment_is_target_or_validation;

RepairPathSummaryMetrics group_metrics(
    const std::vector<ActiveRepairPathGroup>& groups,
    const std::string& group_id) {
  const auto& spec = require_repair_path_group_spec(group_id);
  const auto* match = active_repair_path_group_for_id(groups, group_id);
  if (match == nullptr) return {};
  if (match->spec.grouping != spec.grouping ||
      match->spec.key != spec.key) {
    throw std::logic_error(
        "release pipeline repair summary group contract drift: " +
        group_id);
  }
  if (match->source_location_gap_count > match->path_count) {
    throw std::logic_error(
        "release pipeline repair group source-location gaps exceed paths: " +
        match->spec.id);
  }
  RepairPathSummaryMetrics result;
  result.path_count = match->path_count;
  result.source_location_gap_count = match->source_location_gap_count;
  result.source_location_count =
      match->path_count - match->source_location_gap_count;
  result.evidence_counter_count = match->evidence_counter_count;
  result.group_plan_step = match->step;
  result.current_repo_action_required =
      match->current_repo_action_required;
  result.target_or_validation_action_required =
      match->target_or_validation_action_required;
  return result;
}

std::vector<std::string> recommended_commands(
    const ReleaseEvidencePipelineRequest& request,
    const inferlab_high_value::ManagedArtifact& pipeline_artifact) {
  std::vector<std::string> commands;
  std::map<std::string, bool> seen;
  const auto append_stage_phase = [&](const PipelineStagePhase phase) {
    for (const auto& stage : request.stages) {
      if (stage.phase != phase) continue;
      const std::string& command = stage.refresh_command();
      if (command.empty() || !seen.emplace(command, true).second) {
        throw std::logic_error(
            "release pipeline recommended stage command must be nonempty and unique: " +
            stage.id);
      }
      commands.push_back(command);
    }
  };
  append_stage_phase(PipelineStagePhase::CurrentDecisionInput);
  if (pipeline_artifact.command.empty() ||
      !seen.emplace(pipeline_artifact.command, true).second) {
    throw std::logic_error(
        "release pipeline self refresh command must be nonempty and unique");
  }
  commands.push_back(pipeline_artifact.command);
  append_stage_phase(PipelineStagePhase::PostGenerationConsumer);
  return commands;
}

}  // namespace

ReleasePipelineRepairSummaryProjection::
    ReleasePipelineRepairSummaryProjection(
        const ReleasePipelineRepairProjection& repair,
        const ReleasePipelineSnapshotDiffReleaseLinkProjection&
            snapshot_link) {
  total.path_count = repair.paths.size();
  for (const auto& path : repair.paths) {
    if (path.source_locations.empty()) {
      total.source_location_gap_count =
          inferlab::internal::checked_add_size(
              total.source_location_gap_count, 1U,
              "release pipeline repair summary source-location gap count");
    } else {
      total.source_location_count = inferlab::internal::checked_add_size(
          total.source_location_count, 1U,
          "release pipeline repair summary source-location count");
    }
    total.evidence_counter_count = inferlab::internal::checked_add_size(
        total.evidence_counter_count, path.evidence_counters.size(),
        "release pipeline repair summary evidence-counter count");
    if (repair_environment_is_current_repo(path.repair_environment)) {
      current_repo_path_count = inferlab::internal::checked_add_size(
          current_repo_path_count, 1U,
          "release pipeline current-repo repair summary count");
    }
    if (repair_environment_is_target_or_validation(
            path.repair_environment)) {
      target_or_validation_path_count =
          inferlab::internal::checked_add_size(
              target_or_validation_path_count, 1U,
              "release pipeline target-or-validation repair summary count");
    }
  }
  if (inferlab::internal::checked_add_size(
          total.source_location_count, total.source_location_gap_count,
          "release pipeline repair summary source-location reconciliation") !=
      total.path_count) {
    throw std::logic_error(
        "release pipeline repair summary source-location counts do not reconcile");
  }
  total.current_repo_action_required = current_repo_path_count > 0U;
  total.target_or_validation_action_required =
      target_or_validation_path_count > 0U;

  catalog_flow = group_metrics(repair.groups,
                               "category_catalog_flow_discrepancy");
  catalog_field_coverage = group_metrics(
      repair.groups, "category_catalog_flow_field_coverage_gap");
  catalog_fallback_shortest_path = group_metrics(
      repair.groups,
      "category_catalog_flow_fallback_removal_shortest_path");
  catalog_fallback_parity_gate = group_metrics(
      repair.groups,
      "category_catalog_flow_fallback_removal_parity_gate");
  evidence_query_snapshot_diff.path_count = snapshot_link.repair_path_count;
  evidence_query_snapshot_diff.source_location_gap_count =
      snapshot_link.source_location_gap_count;
  evidence_query_snapshot_diff.source_location_count =
      snapshot_link.repair_path_count - snapshot_link.source_location_gap_count;
  evidence_query_snapshot_diff.evidence_counter_count =
      snapshot_link.evidence_counter_count;
  evidence_query_snapshot_diff.group_plan_step = snapshot_link.group_plan_step;
  evidence_query_snapshot_diff.current_repo_action_required =
      snapshot_link.current_repo_action_required;
  evidence_query_snapshot_diff.target_or_validation_action_required =
      snapshot_link.target_or_validation_action_required;
  native_core_audit = group_metrics(
      repair.groups, "category_family_native_core_audit");
  native_core_audit_scope = group_metrics(
      repair.groups, "category_native_core_audit_scope_blocked");
  native_core_audit_backlog = group_metrics(
      repair.groups, "category_native_core_audit_backlog");
}

ReleasePipelineArtifactProjection::ReleasePipelineArtifactProjection(
    const ReleaseEvidencePipelineRequest& request_value)
    : request(request_value),
      artifact(inferlab_high_value::require_managed_artifact(
          "release_evidence_pipeline")),
      repair_summary(request_value.repair,
                     request_value.snapshot_diff_release_link),
      recommended_next_commands(
          recommended_commands(request_value, artifact)) {}

const char* release_pipeline_claim_scope() noexcept {
  return "release evidence pipeline only; aggregates existing artifacts and does not execute product benchmarks";
}

}  // namespace inferlab_release_pipeline
