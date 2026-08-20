#pragma once

#include "evidence_domain_utils.hpp"
#include "product_replay_semantic_linkage_projection.hpp"
#include "release_evidence_pipeline_alignment_projection.hpp"
#include "release_evidence_pipeline_evidence_query_closure_projection.hpp"
#include "release_evidence_pipeline_native_audit_closure_projection.hpp"
#include "release_evidence_pipeline_repair_projection.hpp"
#include "release_evidence_pipeline_root_cause_projection.hpp"
#include "release_evidence_pipeline_stage.hpp"
#include "repository_evidence_dag.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {

struct ReleasePipelineContractSourceProjection {
  std::string id;
  std::string canonical_owner;
  std::size_t issue_count = 0;
  bool blocks_release = false;
};

struct ReleasePipelineDecisionProjection {
  ReleasePipelineDecisionProjection(
      const std::vector<PipelineStage>& stages,
      const EvidenceSnapshotRefs& snapshots,
      const inferlab_evidence::ProductReplayReadinessSignals&
          product_replay_readiness,
      const ReleasePipelineAlignmentProjection& alignment,
      const inferlab_repository_dag::RepositoryEvidenceDag& repository_dag,
      const ReleasePipelineEvidenceQueryClosureProjection& evidence_query,
      const ReleasePipelineBlockerRepairCardLinkProjection& blocker_link,
      const ReleasePipelineNativeAuditClosureProjection& native_audit);

  inferlab_evidence::ReleaseDecision decision =
      inferlab_evidence::ReleaseDecision::kBlocked;
  inferlab_evidence::EvidenceLevel evidence_level =
      inferlab_evidence::EvidenceLevel::kStaleOrBlocked;
  std::string decision_name = "blocked";
  std::string decision_reason = "stage_metadata_blocked";
  std::string evidence_confidence = "stale_or_blocked";
  std::string stage_identity_drift_status = "legacy_fallback_retained";
  std::size_t stage_count = 0;
  std::size_t current_decision_stage_count = 0;
  std::size_t post_generation_stage_count = 0;
  std::size_t required_stage_count = 0;
  std::size_t ready_required_stage_count = 0;
  std::size_t blocking_stage_count = 0;
  std::size_t catalog_managed_stage_count = 0;
  std::size_t fallback_retained_stage_count = 0;
  std::size_t replayed_input_count = 0;
  std::size_t executed_input_count = 0;
  std::size_t execute_verified_input_count = 0;
  std::size_t replay_evidence_blocker_count = 0;
  std::size_t replay_evidence_read_issue_count = 0;
  std::size_t replay_evidence_available_event_count = 0;
  std::size_t replay_evidence_replay_supported_event_count = 0;
  std::size_t replay_evidence_alignment_rule_count = 0;
  std::size_t replay_evidence_fully_aligned_rule_count = 0;
  std::size_t release_alignment_dimension_count = 0;
  std::size_t release_alignment_ready_dimension_count = 0;
  std::size_t release_alignment_blocking_dimension_count = 0;
  std::size_t release_alignment_advisory_dimension_count = 0;
  std::size_t release_alignment_issue_count = 0;
  std::size_t repository_dag_issue_count = 0;
  bool replay_semantic_events_present = false;
  bool replay_alignment_present = false;
  bool replay_sample_present = false;
  bool replay_evidence_ready = false;
  bool replay_dimension_blocks_replay_ready = true;
  bool repository_dag_acyclic = false;
  bool repository_dag_scope_complete = false;
  std::string replay_evidence_status = "not_reported";
  std::string replay_evidence_alignment_decision = "not_reported";
  std::string replay_evidence_next_action;
  std::size_t evidence_query_source_contract_issue_count = 0;
  std::size_t blocker_link_contract_issue_count = 0;
  std::size_t native_audit_source_contract_issue_count = 0;
  std::size_t post_generation_observation_issue_count = 0;
  std::size_t source_contract_issue_count = 0;
  std::size_t source_contract_blocking_source_count = 0;
  std::size_t release_blocking_signal_count = 0;
  bool source_contract_gate_applied = true;
  bool source_contract_gate_blocks_release = false;
  std::vector<ReleasePipelineContractSourceProjection> contract_sources;
  std::vector<ReleasePipelineContractSourceProjection>
      post_generation_observations;
};

}  // namespace inferlab_release_pipeline
