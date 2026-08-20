#include "repository_evidence_dag_internal.hpp"

#include "artifact_contract_model.hpp"
#include "artifact_metadata_reader.hpp"
#include "catalog_derived_views.hpp"
#include "claim_scope_contract_matrix.hpp"
#include "deep_dive_evidence_bundle_fixture.hpp"
#include "deep_dive_contract_check_model.hpp"
#include "evidence_query_cli_contract.hpp"
#include "high_value_artifact_catalog.hpp"
#include "mature_product_artifact_dispatch.hpp"
#include "mature_product_profile_registry.hpp"
#include "product_replay_semantic_linkage_projection.hpp"
#include "release_evidence_pipeline_stage.hpp"
#include "repository_evidence_consumer_registry.hpp"
#include "runtime_evidence_freshness_gate_model.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_repository_dag {
namespace {

constexpr char kResultsPrefix[] = "results/raw/";
constexpr std::size_t kMaximumBenchmarkMatrixBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumClaimScopeDagArtifactCount = 256U;

std::string results_artifact(const std::string& artifact) {
  if (artifact.rfind("results/", 0U) == 0U) return artifact;
  if (artifact.empty() || artifact.front() == '/' ||
      artifact.find("..") != std::string::npos ||
      artifact.find('\\') != std::string::npos) {
    throw std::logic_error(
        "repository DAG artifact identity is not repository-relative");
  }
  return std::string(kResultsPrefix) + artifact;
}

std::string stage_artifact_path(
    const inferlab_release_pipeline::PipelineStage& stage) {
  return results_artifact(stage.artifact());
}

void add_canonical_producers(internal::GraphDraft& draft) {
  draft.add_owner("high_value_artifact_catalog");
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    draft.add_producer(results_artifact(artifact.artifact), artifact.target,
                       "high_value_artifact_catalog");
  }

  draft.add_owner("deep_dive_artifact_wiring_registry");
  for (const auto& wiring :
       inferlab_deep_dive_bundle::artifact_wiring_specs()) {
    const auto& contract =
        inferlab_artifact_contract::require_contract_extension(
            wiring.contract_name);
    draft.add_producer(contract.artifact, wiring.target,
                       "deep_dive_artifact_wiring_registry");
  }

  draft.add_owner("mature_product_artifact_dispatch");
  for (const auto& spec : inferlab_mature_product::kArtifactDispatchSpecs) {
    draft.add_producer(
        results_artifact(inferlab_mature_product::artifact_filename(spec)),
        std::string(inferlab_mature_product::canonical_target(spec.family)),
        "mature_product_artifact_dispatch");
  }

  draft.add_owner("mature_product_profile_registry");
  for (const auto& profile :
       inferlab::mature_product_profiles::product_profiles()) {
    draft.add_producer(
        results_artifact(profile.artifact),
        inferlab::mature_product_profiles::profile_module(profile),
        "mature_product_profile_registry");
  }

  draft.add_owner("release_pipeline_stage_registry");
  for (const auto& stage :
       inferlab_release_pipeline::default_pipeline_stages()) {
    draft.add_producer(
        stage_artifact_path(stage),
        stage.catalog_managed() ? stage.managed_artifact->target
                                : stage.fallback_owner,
        "release_pipeline_stage_registry");
  }

}

void add_post_generation_order(
    internal::GraphDraft& draft, const std::string& current_output,
    const std::string& downstream_artifact, const std::string& declared_by,
    const std::string& role) {
  const auto targets = draft.producer_targets_for(downstream_artifact);
  if (targets.empty()) {
    draft.add_dependency(
        downstream_artifact, "unresolved_producer_registry",
        EdgeKind::CurrentInput,
        DependencyPhase::CurrentRepositoryGovernance,
        ExemptionKind::None, declared_by,
        "unresolved downstream producer for " + role);
    return;
  }
  for (const auto& target : targets) {
    draft.add_dependency(current_output, target,
                         EdgeKind::PostGenerationOrder,
                         DependencyPhase::PostGeneration,
                         ExemptionKind::None, declared_by, role);
  }
}

void add_release_pipeline_lifecycle(internal::GraphDraft& draft) {
  const auto& pipeline =
      inferlab_high_value::require_managed_artifact(
          "release_evidence_pipeline");
  const std::string pipeline_output = results_artifact(pipeline.artifact);
  for (const auto& stage :
       inferlab_release_pipeline::default_pipeline_stages()) {
    if (stage.current_decision_input()) {
      draft.add_dependency(
          stage_artifact_path(stage), pipeline.target,
          EdgeKind::CurrentInput,
          DependencyPhase::CurrentAuthoritative, ExemptionKind::None,
          "release_pipeline_stage_registry", stage.id);
      continue;
    }
    add_post_generation_order(
        draft, pipeline_output, stage_artifact_path(stage),
        "release_pipeline_stage_registry", stage.id);
    draft.add_dependency(
        stage_artifact_path(stage), pipeline.target, EdgeKind::Observation,
        DependencyPhase::HistoricalObservation,
        ExemptionKind::ExplanationOnly,
        "release_pipeline_stage_registry",
        stage.id + "/previous_snapshot_diagnostics_only");
  }
}

void add_contract_check_lifecycle(internal::GraphDraft& draft) {
  draft.add_owner("deep_dive_contract_check_target_projection");
  for (const auto& target :
       inferlab_deep_dive_contract::default_targets()) {
    draft.add_dependency(
        results_artifact(target.artifact),
        "inferlab_deep_dive_contract_check", EdgeKind::CurrentInput,
        DependencyPhase::CurrentRepositoryGovernance, ExemptionKind::None,
        "deep_dive_contract_check_target_projection", target.name);
  }
}

void add_freshness_gate_lifecycle(internal::GraphDraft& draft) {
  draft.add_owner("runtime_freshness_target_projection");
  const auto projection =
      inferlab_freshness_gate::project_freshness_targets();
  for (const auto& target : projection.targets) {
    draft.add_dependency(
        results_artifact(target.artifact),
        "inferlab_runtime_evidence_freshness_gate",
        EdgeKind::CurrentInput,
        DependencyPhase::CurrentRepositoryGovernance, ExemptionKind::None,
        "runtime_freshness_target_projection", target.name);
  }
}

void add_claim_scope_lifecycle(internal::GraphDraft& draft,
                               const std::filesystem::path& repo_root) {
  draft.add_owner("claim_scope_benchmark_matrix_inventory");
  const auto read = inferlab_artifact::read_text_with_status(
      repo_root / "configs/benchmark_matrix.yaml",
      kMaximumBenchmarkMatrixBytes);
  if (!read.readable || read.empty) {
    draft.add_composition_issue(
        {"claim_scope_matrix_read", "dependency_inventory_unavailable",
         "configs/benchmark_matrix.yaml",
         "benchmark matrix read status is " + read.status,
         "restore a readable nonempty benchmark matrix before claiming complete Claim Scope dependency coverage"});
    return;
  }

  inferlab_claim_scope::ClaimScopeMatrixInventory inventory;
  try {
    inventory = inferlab_claim_scope::parse_claim_scope_matrix(read.text);
  } catch (const std::exception&) {
    draft.add_composition_issue(
        {"claim_scope_matrix_parse", "dependency_inventory_malformed",
         "configs/benchmark_matrix.yaml",
         "benchmark matrix parser rejected the bounded source",
         "repair benchmark matrix output declarations before regenerating repository dependency evidence"});
    return;
  }
  if (!inventory.issues.empty()) {
    draft.add_composition_issue(
        {"claim_scope_matrix_contract", "dependency_inventory_malformed",
         "configs/benchmark_matrix.yaml",
         "benchmark matrix issue count is " +
             std::to_string(inventory.issues.size()),
         "repair duplicate, unsafe, or malformed benchmark output declarations before claiming complete dependency coverage"});
  }
  if (inventory.artifacts.empty()) {
    draft.add_composition_issue(
        {"claim_scope_matrix_empty", "dependency_inventory_empty",
         "configs/benchmark_matrix.yaml",
         "benchmark matrix contains no valid results/raw JSON outputs",
         "declare at least one valid results/raw JSON output for Claim Scope dependency discovery"});
    return;
  }
  if (inventory.artifacts.size() > kMaximumClaimScopeDagArtifactCount) {
    draft.add_composition_issue(
        {"claim_scope_matrix_policy_limit", "dependency_inventory_oversized",
         "configs/benchmark_matrix.yaml",
         "benchmark matrix valid artifact count exceeds the repository DAG bounded Claim Scope inventory",
         "split or reduce the Claim Scope dependency inventory, or deliberately raise the bounded DAG policy with node-budget evidence"});
  }
  const std::size_t artifact_count =
      inventory.artifacts.size() < kMaximumClaimScopeDagArtifactCount
          ? inventory.artifacts.size()
          : kMaximumClaimScopeDagArtifactCount;
  for (const auto& producer : inventory.producers) {
    const std::string artifact = results_artifact(producer.artifact);
    if (draft.producer_targets_for(artifact).empty()) {
      draft.add_producer(artifact, producer.producer_target,
                         "claim_scope_benchmark_matrix_inventory");
    }
  }
  for (std::size_t index = 0U; index < artifact_count; ++index) {
    const auto& artifact = inventory.artifacts[index];
    if (artifact == inferlab_claim_scope::kClaimScopeContractArtifact) {
      draft.add_dependency(
          results_artifact(artifact), "inferlab_claim_scope_contract",
          EdgeKind::Observation, DependencyPhase::HistoricalObservation,
          ExemptionKind::SelfOutputDeferred,
          "claim_scope_benchmark_matrix_inventory",
          "current output excluded from current claim-scope audit");
      continue;
    }
    draft.add_dependency(
        results_artifact(artifact), "inferlab_claim_scope_contract",
        EdgeKind::CurrentInput,
        DependencyPhase::CurrentRepositoryGovernance, ExemptionKind::None,
        "claim_scope_benchmark_matrix_inventory", artifact);
  }
}

std::string direct_consumer_target(const std::string& consumer_id) {
  if (consumer_id == "evidence_query") {
    return inferlab_high_value::require_managed_artifact(
               "evidence_query_result")
        .target;
  }
  throw std::logic_error(
      "repository DAG direct consumer has no target mapping: " +
      consumer_id);
}

void add_direct_consumer_lifecycle(internal::GraphDraft& draft) {
  draft.add_owner("repository_direct_consumer_registry");
  for (const auto& source : direct_consumer_sources()) {
    const auto& artifact =
        inferlab_high_value::require_managed_artifact(source.catalog_id);
    const DependencyPhase phase =
        source.phase == DirectDependencyPhase::CurrentRepositoryGovernance
            ? DependencyPhase::CurrentRepositoryGovernance
            : DependencyPhase::CurrentAuthoritative;
    draft.add_dependency(results_artifact(artifact.artifact),
                         direct_consumer_target(source.consumer_id),
                         EdgeKind::CurrentInput, phase,
                         ExemptionKind::None,
                         "repository_direct_consumer_registry",
                         source.consumer_id + "/" + source.role);
  }
  const auto& warehouse =
      inferlab_high_value::require_managed_artifact(
          "evidence_warehouse_index");
  if (std::string(inferlab_evidence_query_cli::kDefaultWarehouseArtifact) !=
      results_artifact(warehouse.artifact)) {
    throw std::logic_error(
        "Evidence Query CLI warehouse default drifted from catalog identity");
  }
  draft.add_dependency(
      inferlab_evidence_query_cli::kDefaultWarehouseArtifact,
      inferlab_evidence_query_cli::kCanonicalTarget,
      EdgeKind::CurrentInput,
      DependencyPhase::CurrentAuthoritative, ExemptionKind::None,
      "evidence_query_cli_contract", "default_warehouse_input");
}

void add_product_replay_linkage_lifecycle(
    internal::GraphDraft& draft) {
  draft.add_owner("product_replay_semantic_linkage_source_registry");
  const auto& evidence_query =
      inferlab_high_value::require_managed_artifact(
          "evidence_query_result");
  for (const auto& source :
       inferlab_evidence::product_replay_linkage_sources()) {
    const std::string role =
        inferlab_evidence::product_replay_linkage_source_role_name(
            source.role);
    draft.add_dependency(
        results_artifact(source.artifact), evidence_query.target,
        EdgeKind::Observation, DependencyPhase::HistoricalObservation,
        ExemptionKind::ExplanationOnly,
        "product_replay_semantic_linkage_source_registry",
        "evidence_query/" + role);
  }
}

void add_warehouse_observations(internal::GraphDraft& draft) {
  draft.add_owner("evidence_warehouse_inventory_policy");
  const auto& warehouse =
      inferlab_high_value::require_managed_artifact(
          "evidence_warehouse_index");
  for (const auto& artifact : inferlab_high_value::managed_artifacts()) {
    const bool self = artifact.id == warehouse.id;
    draft.add_dependency(
        results_artifact(artifact.artifact), warehouse.target,
        EdgeKind::Observation,
        DependencyPhase::HistoricalObservation,
        self ? ExemptionKind::SelfOutputDeferred
             : ExemptionKind::InventoryObservation,
        "evidence_warehouse_inventory_policy",
        self ? "current output excluded from inventory scan"
             : "read-only managed artifact inventory");
  }
}

void add_catalog_report_consumers(internal::GraphDraft& draft) {
  draft.add_owner("catalog_derived_report_surfaces");
  for (const auto& source : inferlab_catalog::summary_source_views()) {
    draft.add_dependency(
        results_artifact(source.artifact),
        "scripts/summarize_benchmarks.py",
        EdgeKind::PostGenerationOrder,
        DependencyPhase::PostGeneration, ExemptionKind::None,
        "catalog_derived_report_surfaces", "summary/" + source.name);
  }
  for (const auto& source : inferlab_catalog::prometheus_source_views()) {
    draft.add_dependency(
        results_artifact(source.artifact),
        "scripts/export_prometheus_metrics.py",
        EdgeKind::PostGenerationOrder,
        DependencyPhase::PostGeneration, ExemptionKind::None,
        "catalog_derived_report_surfaces", "prometheus/" + source.name);
  }
  for (const auto& source : inferlab_catalog::final_report_source_views()) {
    draft.add_dependency(
        results_artifact(source.artifact),
        "scripts/render_final_report.py",
        EdgeKind::PostGenerationOrder,
        DependencyPhase::PostGeneration, ExemptionKind::None,
        "catalog_derived_report_surfaces", "final_report/" + source.name);
  }
}

}  // namespace

RepositoryEvidenceDag build_repository_evidence_dag(
    const std::filesystem::path& repo_root) {
  internal::GraphDraft draft;
  add_canonical_producers(draft);
  add_release_pipeline_lifecycle(draft);
  add_contract_check_lifecycle(draft);
  add_freshness_gate_lifecycle(draft);
  add_claim_scope_lifecycle(draft, repo_root);
  add_direct_consumer_lifecycle(draft);
  add_product_replay_linkage_lifecycle(draft);
  add_warehouse_observations(draft);
  add_catalog_report_consumers(draft);
  draft.add_scope_exclusion(
      "unregistered utility or benchmark executables that do not declare an evidence artifact");
  draft.add_scope_exclusion(
      "runtime-discovered external product internals and target-machine process graphs");
  draft.add_scope_exclusion(
      "network, CUDA stream, database, and Kubernetes runtime dependencies that require execution evidence");
  return draft.finish();
}

}  // namespace inferlab_repository_dag
