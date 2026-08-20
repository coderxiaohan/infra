#include "release_evidence_pipeline_stage.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

constexpr std::size_t kExpectedPipelineStageCount = 10U;
constexpr std::size_t kExpectedCurrentDecisionStageCount = 5U;
constexpr std::size_t kExpectedPostGenerationStageCount = 5U;

PipelineStage managed_stage(const std::string& id,
                            const std::string& catalog_id,
                            const std::string& stage,
                            const std::string& evidence_role,
                            const bool required,
                            const PipelineStagePhase phase) {
  PipelineStage result;
  result.id = id;
  result.stage = stage;
  result.evidence_role = evidence_role;
  result.required = required;
  result.phase = phase;
  result.managed_artifact =
      &inferlab_high_value::require_managed_artifact(catalog_id);
  return result;
}

PipelineStage fallback_stage(const std::string& id,
                             const std::string& stage,
                             const std::string& artifact,
                             const std::string& evidence_role,
                             const std::string& refresh_command,
                             const bool required,
                             const PipelineStagePhase phase,
                             const std::string& owner,
                             const std::string& removal_condition) {
  if (artifact.empty() || refresh_command.empty() || owner.empty() ||
      removal_condition.empty()) {
    throw std::logic_error(
        "release pipeline fallback stage metadata must be nonempty");
  }
  PipelineStage result;
  result.id = id;
  result.stage = stage;
  result.evidence_role = evidence_role;
  result.required = required;
  result.phase = phase;
  result.fallback_artifact = artifact;
  result.fallback_refresh_command = refresh_command;
  result.fallback_owner = owner;
  result.fallback_removal_condition = removal_condition;
  return result;
}

void validate_stage_registry(const std::vector<PipelineStage>& stages) {
  if (stages.size() != kExpectedPipelineStageCount) {
    throw std::logic_error("release pipeline stage registry count drift");
  }
  std::size_t current_decision_count = 0U;
  std::size_t post_generation_count = 0U;
  for (std::size_t i = 0; i < stages.size(); ++i) {
    const auto& stage = stages[i];
    const bool fallback_identity_present =
        !stage.fallback_artifact.empty() ||
        !stage.fallback_refresh_command.empty() ||
        !stage.fallback_owner.empty() ||
        !stage.fallback_removal_condition.empty();
    if (stage.id.empty() || stage.stage.empty() || stage.evidence_role.empty() ||
        stage.artifact().empty() || stage.refresh_command().empty()) {
      throw std::logic_error(
          "release pipeline stage identity and wiring must be nonempty");
    }
    if (std::string(pipeline_stage_phase_name(stage.phase)) == "unknown") {
      throw std::logic_error("release pipeline stage phase is invalid");
    }
    if (stage.current_decision_input()) {
      ++current_decision_count;
    } else if (stage.post_generation_consumer()) {
      ++post_generation_count;
    }
    if ((stage.catalog_managed() && fallback_identity_present) ||
        (!stage.catalog_managed() && !stage.fallback_retained())) {
      throw std::logic_error(
          "release pipeline stage must have exactly one identity owner");
    }
    for (std::size_t j = i + 1U; j < stages.size(); ++j) {
      if (stage.id == stages[j].id) {
        throw std::logic_error("duplicate release pipeline stage id: " +
                               stage.id);
      }
      if (stage.artifact() == stages[j].artifact()) {
        throw std::logic_error(
            "duplicate release pipeline stage artifact: " + stage.artifact());
      }
    }
  }
  if (current_decision_count != kExpectedCurrentDecisionStageCount ||
      post_generation_count != kExpectedPostGenerationStageCount ||
      current_decision_count + post_generation_count != stages.size()) {
    throw std::logic_error(
        "release pipeline current/post-generation stage count drift");
  }
}

}  // namespace

const char* pipeline_stage_phase_name(
    const PipelineStagePhase phase) noexcept {
  switch (phase) {
    case PipelineStagePhase::CurrentDecisionInput:
      return "current_decision_input";
    case PipelineStagePhase::PostGenerationConsumer:
      return "post_generation_consumer";
  }
  return "unknown";
}

const std::string& PipelineStage::artifact() const {
  return managed_artifact == nullptr ? fallback_artifact
                                     : managed_artifact->artifact;
}

const std::string& PipelineStage::refresh_command() const {
  return managed_artifact == nullptr ? fallback_refresh_command
                                     : managed_artifact->command;
}

bool PipelineStage::catalog_managed() const noexcept {
  return managed_artifact != nullptr;
}

bool PipelineStage::fallback_retained() const noexcept {
  return managed_artifact == nullptr && !fallback_artifact.empty() &&
         !fallback_refresh_command.empty() && !fallback_owner.empty() &&
         !fallback_removal_condition.empty();
}

bool PipelineStage::current_decision_input() const noexcept {
  return phase == PipelineStagePhase::CurrentDecisionInput;
}

bool PipelineStage::post_generation_consumer() const noexcept {
  return phase == PipelineStagePhase::PostGenerationConsumer;
}

std::string PipelineStage::identity_source() const {
  return catalog_managed() ? "high_value_artifact_catalog"
                           : "release_pipeline_legacy_fallback";
}

std::string PipelineStage::drift_status() const {
  return catalog_managed() ? "catalog_identity_borrowed"
                           : "fallback_retained_pending_catalog_migration";
}

std::string PipelineStage::next_action() const {
  return catalog_managed()
             ? "keep artifact identity and refresh command in the central catalog"
             : fallback_removal_condition;
}

std::vector<PipelineStage> default_pipeline_stages() {
  std::vector<PipelineStage> stages = {
      fallback_stage(
          "freshness_gate", "freshness",
          "native_runtime_evidence_freshness_gate.json", "freshness_gate",
          "./build/inferlab_runtime_evidence_freshness_gate --repo-root . --results results/raw --output results/raw/native_runtime_evidence_freshness_gate.json --format json",
          true, PipelineStagePhase::PostGenerationConsumer,
          "inferlab_runtime_evidence_freshness_gate",
          "register the freshness artifact in the central catalog and verify freshness/report parity before removing this fallback"),
      fallback_stage(
          "claim_scope", "claim", "native_claim_scope_contract.json",
          "claim_boundary",
          "./build/inferlab_claim_scope_contract --repo-root . --matrix configs/benchmark_matrix.yaml --results results/raw --output results/raw/native_claim_scope_contract.json --format json",
          true, PipelineStagePhase::PostGenerationConsumer,
          "inferlab_claim_scope_contract",
          "register the claim-scope artifact in the central catalog and verify claim/report parity before removing this fallback"),
      managed_stage("evidence_warehouse", "evidence_warehouse_index",
                    "warehouse", "lineage_query", true,
                    PipelineStagePhase::CurrentDecisionInput),
      managed_stage("artifact_catalog_single_source",
                    "artifact_catalog_single_source", "catalog",
                    "single_source_registry", true,
                    PipelineStagePhase::CurrentDecisionInput),
      managed_stage("evidence_query", "evidence_query_result",
                    "evidence_query", "release_evidence_query", true,
                    PipelineStagePhase::PostGenerationConsumer),
      managed_stage("native_core_audit_register", "native_core_audit_register",
                    "native_core_audit", "static_native_core_audit_ledger",
                    true, PipelineStagePhase::CurrentDecisionInput),
      fallback_stage(
          "deep_dive_contract", "deep_dive_contract",
          "native_deep_dive_contract_check.json", "deep_dive_schema",
          "./build/inferlab_deep_dive_contract_check --repo-root . --results results/raw --output results/raw/native_deep_dive_contract_check.json --format json",
          true, PipelineStagePhase::PostGenerationConsumer,
          "inferlab_deep_dive_contract_check",
          "register the deep-dive contract artifact in the central catalog and verify bundle/report parity before removing this fallback"),
      fallback_stage(
          "product_replay_semantics", "product_replay",
          "product_replay_semantic_events.json", "product_replay_linkage",
          "./build/inferlab_product_replay_semantic_events --repo-root . --results results/raw --output results/raw/product_replay_semantic_events.json --format json",
          false, PipelineStagePhase::CurrentDecisionInput,
          "inferlab_product_replay_semantic_events",
          "register the product replay semantic artifact or formalize it as an external-product source before removing this fallback"),
      fallback_stage(
          "benchmark_summary", "summary", "benchmark_summary.json",
          "summary_report_input",
          "python3 scripts/summarize_benchmarks.py --repo-root . --results results/raw --output results/tables/benchmark_summary.md --json-output results/raw/benchmark_summary.json",
          false, PipelineStagePhase::PostGenerationConsumer,
          "scripts/summarize_benchmarks.py",
          "extend the central catalog to describe Python report-glue producers and verify summary parity before removing this fallback"),
  };
  validate_stage_registry(stages);
  return stages;
}

}  // namespace inferlab_release_pipeline
