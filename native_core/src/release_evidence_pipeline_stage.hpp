#pragma once

#include "evidence_domain_types.hpp"
#include "high_value_artifact_catalog.hpp"

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

using EvidenceSnapshotRefs =
    std::vector<const inferlab_evidence::EvidenceSnapshot*>;

enum class PipelineStagePhase {
  CurrentDecisionInput,
  PostGenerationConsumer,
};

const char* pipeline_stage_phase_name(PipelineStagePhase phase) noexcept;

struct PipelineStage {
  std::string id;
  std::string stage;
  std::string evidence_role;
  bool required = true;
  PipelineStagePhase phase = PipelineStagePhase::CurrentDecisionInput;
  const inferlab_high_value::ManagedArtifact* managed_artifact = nullptr;
  std::string fallback_artifact;
  std::string fallback_refresh_command;
  std::string fallback_owner;
  std::string fallback_removal_condition;

  const std::string& artifact() const;
  const std::string& refresh_command() const;
  bool catalog_managed() const noexcept;
  bool fallback_retained() const noexcept;
  bool current_decision_input() const noexcept;
  bool post_generation_consumer() const noexcept;
  std::string identity_source() const;
  std::string drift_status() const;
  std::string next_action() const;
};

std::vector<PipelineStage> default_pipeline_stages();

}  // namespace inferlab_release_pipeline
