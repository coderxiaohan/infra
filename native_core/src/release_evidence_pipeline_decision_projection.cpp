#include "release_evidence_pipeline_decision_projection.hpp"

#include "checked_index.hpp"

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

void require_aligned_stage_snapshots(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots) {
  if (stages.size() != snapshots.size()) {
    throw std::logic_error(
        "release pipeline decision stage and snapshot counts must match");
  }
  for (const auto* snapshot : snapshots) {
    if (snapshot == nullptr) {
      throw std::logic_error(
          "release pipeline decision snapshot must not be null");
    }
  }
}

ReleasePipelineContractSourceProjection contract_source(
    const std::string& id,
    const std::string& canonical_owner,
    const std::size_t issue_count) {
  if (id.empty() || canonical_owner.empty()) {
    throw std::logic_error(
        "release pipeline contract source identity must be nonempty");
  }
  ReleasePipelineContractSourceProjection source;
  source.id = id;
  source.canonical_owner = canonical_owner;
  source.issue_count = issue_count;
  source.blocks_release = issue_count > 0U;
  return source;
}

void validate_contract_sources(
    const std::vector<ReleasePipelineContractSourceProjection>& sources,
    const bool issues_block_release) {
  if (sources.empty()) {
    throw std::logic_error(
        "release pipeline contract source registry must not be empty");
  }
  std::map<std::string, bool> ids;
  for (const auto& source : sources) {
    if (source.id.empty() || source.canonical_owner.empty() ||
        source.blocks_release !=
            (issues_block_release && source.issue_count > 0U) ||
        !ids.emplace(source.id, true).second) {
      throw std::logic_error(
          "release pipeline contract source registry is invalid");
    }
  }
}

inferlab_evidence::ReleaseDecision release_decision(
    const std::size_t blocking_stage_count,
    const std::size_t source_contract_blocking_source_count,
    const std::size_t replayed_input_count,
    const std::size_t execute_verified_input_count,
    const bool replay_evidence_ready) {
  if (blocking_stage_count > 0U ||
      source_contract_blocking_source_count > 0U) {
    return inferlab_evidence::ReleaseDecision::kBlocked;
  }
  if (execute_verified_input_count > 0U) {
    return inferlab_evidence::ReleaseDecision::kExecuteReady;
  }
  if (replayed_input_count > 0U && replay_evidence_ready) {
    return inferlab_evidence::ReleaseDecision::kReplayReady;
  }
  return inferlab_evidence::ReleaseDecision::kReportReady;
}

std::string release_decision_reason(
    const std::size_t blocking_stage_count,
    const std::size_t source_contract_blocking_source_count,
    const std::size_t replayed_input_count,
    const bool replay_evidence_ready,
    const inferlab_evidence::ReleaseDecision decision) {
  if (blocking_stage_count > 0U &&
      source_contract_blocking_source_count > 0U) {
    return "stage_metadata_and_source_contract_blocked";
  }
  if (blocking_stage_count > 0U) return "stage_metadata_blocked";
  if (source_contract_blocking_source_count > 0U) {
    return "source_contract_blocked";
  }
  if (decision == inferlab_evidence::ReleaseDecision::kExecuteReady) {
    return "execute_evidence_available";
  }
  if (decision == inferlab_evidence::ReleaseDecision::kReplayReady) {
    return "replay_evidence_available";
  }
  if (replayed_input_count > 0U && !replay_evidence_ready) {
    return "replay_evidence_incomplete_report_only";
  }
  return "report_contract_ready";
}

std::size_t count_repository_dag_issues(
    const inferlab_repository_dag::RepositoryEvidenceDag& graph) {
  std::size_t count = graph.summary.composition_issue_count;
  count = inferlab::internal::checked_add_size(
      count, graph.summary.producer_conflict_count,
      "release pipeline repository DAG issue count");
  count = inferlab::internal::checked_add_size(
      count, graph.summary.unresolved_producer_count,
      "release pipeline repository DAG issue count");
  count = inferlab::internal::checked_add_size(
      count, graph.summary.cycle_witness_count,
      "release pipeline repository DAG issue count");
  if (!graph.summary.declared_evidence_surface_scope_complete && count == 0U) {
    count = 1U;
  }
  return count;
}

}  // namespace

ReleasePipelineDecisionProjection::ReleasePipelineDecisionProjection(
    const std::vector<PipelineStage>& stages,
    const EvidenceSnapshotRefs& snapshots,
    const inferlab_evidence::ProductReplayReadinessSignals&
        product_replay_readiness,
    const ReleasePipelineAlignmentProjection& alignment,
    const inferlab_repository_dag::RepositoryEvidenceDag& repository_dag,
    const ReleasePipelineEvidenceQueryClosureProjection& evidence_query,
    const ReleasePipelineBlockerRepairCardLinkProjection& blocker_link,
    const ReleasePipelineNativeAuditClosureProjection& native_audit)
    : stage_count(stages.size()),
      evidence_query_source_contract_issue_count(
          evidence_query.source_contract_issue_count),
      blocker_link_contract_issue_count(blocker_link.link_contract_issue_count),
      native_audit_source_contract_issue_count(
          native_audit.source_contract_issue_count) {
  require_aligned_stage_snapshots(stages, snapshots);
  for (std::size_t index = 0; index < stages.size(); ++index) {
    const auto& stage = stages[index];
    const auto& snapshot = *snapshots[index];
    if (stage.post_generation_consumer()) {
      post_generation_stage_count = inferlab::internal::checked_add_size(
          post_generation_stage_count, 1U,
          "release pipeline post-generation stage count");
    } else {
      current_decision_stage_count = inferlab::internal::checked_add_size(
          current_decision_stage_count, 1U,
          "release pipeline current-decision stage count");
    }
    const bool ready = inferlab_evidence::evidence_snapshot_ready(snapshot);
    if (stage.current_decision_input() && stage.required) {
      required_stage_count = inferlab::internal::checked_add_size(
          required_stage_count, 1U,
          "release pipeline decision required stage count");
      if (ready) {
        ready_required_stage_count = inferlab::internal::checked_add_size(
            ready_required_stage_count, 1U,
            "release pipeline decision ready required stage count");
      } else {
        blocking_stage_count = inferlab::internal::checked_add_size(
            blocking_stage_count, 1U,
            "release pipeline decision blocking stage count");
      }
    }
    if (stage.catalog_managed()) {
      catalog_managed_stage_count = inferlab::internal::checked_add_size(
          catalog_managed_stage_count, 1U,
          "release pipeline decision catalog-managed stage count");
    } else {
      fallback_retained_stage_count = inferlab::internal::checked_add_size(
          fallback_retained_stage_count, 1U,
          "release pipeline decision fallback-retained stage count");
    }
    if (stage.current_decision_input() && snapshot.metadata.replayed) {
      replayed_input_count = inferlab::internal::checked_add_size(
          replayed_input_count, 1U,
          "release pipeline decision replayed input count");
    }
    if (stage.current_decision_input() && snapshot.metadata.executed) {
      executed_input_count = inferlab::internal::checked_add_size(
          executed_input_count, 1U,
          "release pipeline decision executed input count");
    }
    if (stage.current_decision_input() && snapshot.metadata.executed &&
        inferlab_artifact::json_mode_matches(snapshot.metadata.mode,
                                             "execute")) {
      execute_verified_input_count = inferlab::internal::checked_add_size(
          execute_verified_input_count, 1U,
          "release pipeline decision execute-verified input count");
    }
  }
  if (inferlab::internal::checked_add_size(
          current_decision_stage_count, post_generation_stage_count,
          "release pipeline stage phase reconciliation") != stage_count) {
    throw std::logic_error(
        "release pipeline stage phases do not reconcile");
  }

  replay_semantic_events_present =
      product_replay_readiness.semantic_events_present;
  replay_alignment_present = product_replay_readiness.alignment_present;
  replay_sample_present = product_replay_readiness.sample_replay_present;
  replay_evidence_read_issue_count =
      product_replay_readiness.read_issue_count;
  replay_evidence_available_event_count =
      product_replay_readiness.available_event_count;
  replay_evidence_replay_supported_event_count =
      product_replay_readiness.replay_supported_event_count;
  replay_evidence_alignment_rule_count =
      product_replay_readiness.alignment_rule_count;
  replay_evidence_fully_aligned_rule_count =
      product_replay_readiness.fully_aligned_rule_count;
  replay_evidence_ready = product_replay_readiness.replay_ready;
  replay_dimension_blocks_replay_ready = !replay_evidence_ready;
  replay_evidence_blocker_count = product_replay_readiness.blocker_count;
  replay_evidence_status = product_replay_readiness.status;
  replay_evidence_alignment_decision =
      product_replay_readiness.alignment_decision;
  replay_evidence_next_action = product_replay_readiness.next_action;
  release_alignment_dimension_count = alignment.dimensions.size();
  release_alignment_ready_dimension_count = alignment.ready_dimension_count;
  release_alignment_blocking_dimension_count =
      alignment.blocking_dimension_count;
  release_alignment_advisory_dimension_count =
      alignment.advisory_dimension_count;
  release_alignment_issue_count = alignment.issue_count;
  repository_dag_acyclic = repository_dag.summary.acyclic;
  repository_dag_scope_complete =
      repository_dag.summary.declared_evidence_surface_scope_complete;
  repository_dag_issue_count =
      count_repository_dag_issues(repository_dag);

  stage_identity_drift_status =
      fallback_retained_stage_count == 0U
          ? "catalog_single_source_complete"
          : "legacy_fallback_retained";
  contract_sources = {
      contract_source(
          "release_alignment",
          "release_evidence_pipeline_alignment_projection",
          release_alignment_blocking_dimension_count),
      contract_source(
          "repository_dependency_graph",
          "repository_evidence_dag_contract",
          repository_dag_issue_count),
      contract_source(
          "native_core_audit",
          "release_evidence_pipeline_native_audit_closure_projection",
          native_audit_source_contract_issue_count),
  };
  post_generation_observations = {
      contract_source(
          "evidence_query",
          "release_evidence_pipeline_evidence_query_closure_projection",
          evidence_query_source_contract_issue_count),
      contract_source(
          "blocker_repair_link",
          "release_evidence_pipeline_repair_projection",
          blocker_link_contract_issue_count),
  };
  for (auto& source : post_generation_observations) {
    source.blocks_release = false;
  }
  validate_contract_sources(contract_sources, true);
  validate_contract_sources(post_generation_observations, false);
  for (const auto& source : contract_sources) {
    source_contract_issue_count = inferlab::internal::checked_add_size(
        source_contract_issue_count, source.issue_count,
        "release pipeline source contract issue count");
    if (source.blocks_release) {
      source_contract_blocking_source_count =
          inferlab::internal::checked_add_size(
              source_contract_blocking_source_count, 1U,
              "release pipeline source contract blocking-source count");
    }
  }
  for (const auto& source : post_generation_observations) {
    post_generation_observation_issue_count =
        inferlab::internal::checked_add_size(
            post_generation_observation_issue_count, source.issue_count,
            "release pipeline post-generation observation issue count");
  }
  source_contract_gate_blocks_release =
      source_contract_blocking_source_count > 0U;
  release_blocking_signal_count = inferlab::internal::checked_add_size(
      blocking_stage_count, source_contract_blocking_source_count,
      "release pipeline blocking signal count");
  decision = release_decision(blocking_stage_count,
                              source_contract_blocking_source_count,
                              replayed_input_count,
                              execute_verified_input_count,
                              replay_evidence_ready);
  decision_name = inferlab_evidence::release_decision_name(decision);
  decision_reason = release_decision_reason(
      blocking_stage_count, source_contract_blocking_source_count,
      replayed_input_count, replay_evidence_ready, decision);
  evidence_level =
      inferlab_evidence::release_decision_evidence_level(decision);
  evidence_confidence = inferlab_evidence::evidence_level_name(evidence_level);
}

}  // namespace inferlab_release_pipeline
