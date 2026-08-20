#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_repository_dag {

enum class NodeKind {
  Producer,
  Artifact,
};

enum class EdgeKind {
  Produces,
  CurrentInput,
  PostGenerationOrder,
  Observation,
};

enum class DependencyPhase {
  ProducerDeclaration,
  CurrentAuthoritative,
  CurrentRepositoryGovernance,
  PostGeneration,
  HistoricalObservation,
};

enum class ExemptionKind {
  None,
  InventoryObservation,
  ExplanationOnly,
  SelfOutputDeferred,
};

enum class DagDecision {
  Acyclic,
  ProducerContractBlocked,
  CyclesDetected,
};

struct Node {
  std::string id;
  std::string label;
  NodeKind kind = NodeKind::Artifact;
  std::vector<std::string> identity_owners;
};

struct Edge {
  std::string id;
  std::string from_node;
  std::string to_node;
  EdgeKind kind = EdgeKind::CurrentInput;
  DependencyPhase phase = DependencyPhase::CurrentAuthoritative;
  ExemptionKind exemption = ExemptionKind::None;
  bool authoritative = true;
  bool participates_in_cycle_check = true;
  std::vector<std::string> declared_by;
  std::vector<std::string> roles;
};

struct ProducerConflict {
  std::string artifact_node;
  std::vector<std::string> producer_nodes;
  std::vector<std::string> identity_owners;
  std::string current_repo_action;
};

struct CompositionIssue {
  std::string id;
  std::string category;
  std::string source_label;
  std::string detail;
  std::string current_repo_action;
};

struct CycleWitness {
  std::string id;
  std::string witness_kind;
  std::vector<std::string> node_ids;
  std::vector<std::string> edge_ids;
  std::vector<std::string> affected_targets;
};

struct RepairCard {
  std::string id;
  std::string category;
  std::vector<std::string> affected_targets;
  std::vector<std::string> candidate_edge_ids;
  std::string repair_environment;
  std::string current_repo_action;
  std::string target_machine_action;
};

struct Summary {
  std::size_t node_count = 0U;
  std::size_t producer_node_count = 0U;
  std::size_t artifact_node_count = 0U;
  std::size_t edge_count = 0U;
  std::size_t producer_edge_count = 0U;
  std::size_t current_dependency_edge_count = 0U;
  std::size_t post_generation_edge_count = 0U;
  std::size_t exempt_observation_edge_count = 0U;
  std::size_t cycle_checked_edge_count = 0U;
  std::size_t producer_conflict_count = 0U;
  std::size_t unresolved_producer_count = 0U;
  std::size_t composition_issue_count = 0U;
  std::size_t cycle_witness_count = 0U;
  std::size_t cyclic_node_count = 0U;
  std::size_t repair_card_count = 0U;
  std::size_t composed_owner_count = 0U;
  bool declared_evidence_surface_scope_complete = false;
  // Graph-theoretic property only. Producer resolution and composition
  // completeness are reported independently and still block the decision.
  bool acyclic = false;
  DagDecision decision = DagDecision::ProducerContractBlocked;
};

struct RepositoryEvidenceDag {
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::vector<ProducerConflict> producer_conflicts;
  std::vector<CompositionIssue> composition_issues;
  std::vector<std::string> unresolved_artifact_nodes;
  std::vector<CycleWitness> cycle_witnesses;
  std::vector<RepairCard> repair_cards;
  std::vector<std::string> composed_owners;
  std::vector<std::string> scope_exclusions;
  Summary summary;
  std::string claim_boundary;
};

const char* node_kind_name(NodeKind kind);
const char* edge_kind_name(EdgeKind kind);
const char* dependency_phase_name(DependencyPhase phase);
const char* exemption_kind_name(ExemptionKind exemption);
const char* dag_decision_name(DagDecision decision);

RepositoryEvidenceDag build_repository_evidence_dag(
    const std::filesystem::path& repo_root);
void validate_repository_evidence_dag(const RepositoryEvidenceDag& graph);

}  // namespace inferlab_repository_dag
