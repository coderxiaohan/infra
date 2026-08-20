#include "repository_evidence_dag_internal.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_repository_dag {
namespace {

constexpr std::size_t kMaximumNodeCount = 1024U;
constexpr std::size_t kMaximumEdgeCount = 8192U;
constexpr std::size_t kMaximumCycleWitnessCount = 128U;
constexpr std::size_t kMaximumCompositionIssueCount = 128U;
constexpr std::size_t kExpectedComposedOwnerCount = 12U;

const std::set<std::string>& expected_composed_owners() {
  static const std::set<std::string> owners = {
      "catalog_derived_report_surfaces",
      "claim_scope_benchmark_matrix_inventory",
      "deep_dive_contract_check_target_projection",
      "evidence_warehouse_inventory_policy",
      "deep_dive_artifact_wiring_registry",
      "high_value_artifact_catalog",
      "mature_product_artifact_dispatch",
      "mature_product_profile_registry",
      "product_replay_semantic_linkage_source_registry",
      "release_pipeline_stage_registry",
      "repository_direct_consumer_registry",
      "runtime_freshness_target_projection"};
  if (owners.size() != kExpectedComposedOwnerCount) {
    throw std::logic_error(
        "repository DAG expected owner registry count drift");
  }
  return owners;
}

bool composed_owner_registry_matches(
    const std::vector<std::string>& owners) {
  const std::set<std::string> actual(owners.begin(), owners.end());
  return owners.size() == actual.size() &&
         actual == expected_composed_owners();
}

bool safe_text(const std::string& value) {
  if (value.empty() || value.size() > 1024U) return false;
  for (const unsigned char ch : value) {
    if (ch < 0x20U || ch == 0x7fU) return false;
  }
  return true;
}

bool safe_repository_identity(const std::string& value) {
  return safe_text(value) && value.front() != '/' &&
         value.find("..") == std::string::npos &&
         value.find('\\') == std::string::npos;
}

void append_unique(std::vector<std::string>& values,
                   const std::string& value,
                   const char* label) {
  if (!safe_text(value)) {
    throw std::logic_error(std::string(label) + " must be nonempty and safe");
  }
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

std::size_t require_node_index(const RepositoryEvidenceDag& graph,
                               const std::string& id) {
  const Node* match = nullptr;
  std::size_t match_index = 0U;
  for (std::size_t index = 0U; index < graph.nodes.size(); ++index) {
    if (graph.nodes[index].id != id) continue;
    if (match != nullptr) {
      throw std::logic_error("repository DAG node identity is duplicated: " +
                             id);
    }
    match = &graph.nodes[index];
    match_index = index;
  }
  if (match == nullptr) {
    throw std::logic_error("repository DAG edge references unknown node: " +
                           id);
  }
  return match_index;
}

bool edge_is_self_loop(const Edge& edge) {
  return edge.from_node == edge.to_node;
}

std::vector<CycleWitness> project_cycle_witnesses(
    const RepositoryEvidenceDag& graph) {
  const std::size_t node_count = graph.nodes.size();
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> outgoing(
      node_count);
  for (std::size_t edge_index = 0U; edge_index < graph.edges.size();
       ++edge_index) {
    const auto& edge = graph.edges[edge_index];
    if (!edge.participates_in_cycle_check) continue;
    const std::size_t source = require_node_index(graph, edge.from_node);
    const std::size_t target = require_node_index(graph, edge.to_node);
    outgoing[source].push_back({target, edge_index});
  }

  std::vector<int> discovery(node_count, -1);
  std::vector<int> low_link(node_count, -1);
  std::vector<bool> on_stack(node_count, false);
  std::vector<std::size_t> stack;
  std::vector<std::vector<std::size_t>> components;
  int next_discovery = 0;

  std::function<void(std::size_t)> visit = [&](const std::size_t node) {
    discovery[node] = next_discovery;
    low_link[node] = next_discovery;
    ++next_discovery;
    stack.push_back(node);
    on_stack[node] = true;
    for (const auto& [target, edge_index] : outgoing[node]) {
      (void)edge_index;
      if (discovery[target] < 0) {
        visit(target);
        low_link[node] = std::min(low_link[node], low_link[target]);
      } else if (on_stack[target]) {
        low_link[node] = std::min(low_link[node], discovery[target]);
      }
    }
    if (low_link[node] != discovery[node]) return;
    std::vector<std::size_t> component;
    while (!stack.empty()) {
      const std::size_t member = stack.back();
      stack.pop_back();
      on_stack[member] = false;
      component.push_back(member);
      if (member == node) break;
    }
    components.push_back(std::move(component));
  };

  for (std::size_t node = 0U; node < node_count; ++node) {
    if (discovery[node] < 0) visit(node);
  }

  std::vector<CycleWitness> witnesses;
  for (const auto& component : components) {
    std::set<std::size_t> members(component.begin(), component.end());
    bool cyclic = component.size() > 1U;
    if (!cyclic && !component.empty()) {
      for (const auto& [target, edge_index] : outgoing[component.front()]) {
        (void)edge_index;
        if (target == component.front()) {
          cyclic = true;
          break;
        }
      }
    }
    if (!cyclic) continue;
    if (witnesses.size() >= kMaximumCycleWitnessCount) {
      throw std::logic_error(
          "repository DAG cycle witness policy limit exceeded");
    }

    CycleWitness witness;
    witness.witness_kind = "strongly_connected_component";
    for (const std::size_t node : component) {
      witness.node_ids.push_back(graph.nodes[node].id);
      if (graph.nodes[node].kind == NodeKind::Producer) {
        append_unique(witness.affected_targets, graph.nodes[node].label,
                      "repository DAG affected target");
      }
    }
    std::sort(witness.node_ids.begin(), witness.node_ids.end());
    std::sort(witness.affected_targets.begin(),
              witness.affected_targets.end());
    for (const auto& edge : graph.edges) {
      if (!edge.participates_in_cycle_check) continue;
      const std::size_t source = require_node_index(graph, edge.from_node);
      const std::size_t target = require_node_index(graph, edge.to_node);
      if (members.count(source) > 0U && members.count(target) > 0U) {
        witness.edge_ids.push_back(edge.id);
      }
    }
    std::sort(witness.edge_ids.begin(), witness.edge_ids.end());
    if (witness.edge_ids.empty()) {
      throw std::logic_error(
          "repository DAG cyclic component has no internal edge witness");
    }
    witness.id = "cycle_scc_" + std::to_string(witnesses.size() + 1U);
    witnesses.push_back(std::move(witness));
  }
  return witnesses;
}

Summary project_summary(const RepositoryEvidenceDag& graph) {
  Summary summary;
  summary.node_count = graph.nodes.size();
  for (const auto& node : graph.nodes) {
    if (node.kind == NodeKind::Producer) {
      summary.producer_node_count = inferlab::internal::checked_add_size(
          summary.producer_node_count, 1U,
          "repository DAG producer-node count");
    } else {
      summary.artifact_node_count = inferlab::internal::checked_add_size(
          summary.artifact_node_count, 1U,
          "repository DAG artifact-node count");
    }
  }
  summary.edge_count = graph.edges.size();
  for (const auto& edge : graph.edges) {
    if (edge.kind == EdgeKind::Produces) {
      summary.producer_edge_count = inferlab::internal::checked_add_size(
          summary.producer_edge_count, 1U,
          "repository DAG producer-edge count");
    } else if (edge.kind == EdgeKind::CurrentInput) {
      summary.current_dependency_edge_count =
          inferlab::internal::checked_add_size(
              summary.current_dependency_edge_count, 1U,
              "repository DAG current dependency count");
    } else if (edge.kind == EdgeKind::PostGenerationOrder) {
      summary.post_generation_edge_count =
          inferlab::internal::checked_add_size(
              summary.post_generation_edge_count, 1U,
              "repository DAG post-generation edge count");
    }
    if (edge.exemption != ExemptionKind::None) {
      summary.exempt_observation_edge_count =
          inferlab::internal::checked_add_size(
              summary.exempt_observation_edge_count, 1U,
              "repository DAG exempt edge count");
    }
    if (edge.participates_in_cycle_check) {
      summary.cycle_checked_edge_count =
          inferlab::internal::checked_add_size(
              summary.cycle_checked_edge_count, 1U,
              "repository DAG checked-edge count");
    }
  }
  summary.producer_conflict_count = graph.producer_conflicts.size();
  summary.unresolved_producer_count = graph.unresolved_artifact_nodes.size();
  summary.composition_issue_count = graph.composition_issues.size();
  summary.cycle_witness_count = graph.cycle_witnesses.size();
  std::set<std::string> cyclic_nodes;
  for (const auto& witness : graph.cycle_witnesses) {
    cyclic_nodes.insert(witness.node_ids.begin(), witness.node_ids.end());
  }
  summary.cyclic_node_count = cyclic_nodes.size();
  summary.repair_card_count = graph.repair_cards.size();
  summary.composed_owner_count = graph.composed_owners.size();
  summary.declared_evidence_surface_scope_complete =
      composed_owner_registry_matches(graph.composed_owners) &&
      graph.composition_issues.empty();
  summary.acyclic = graph.cycle_witnesses.empty();
  if (!summary.declared_evidence_surface_scope_complete ||
      !graph.producer_conflicts.empty() ||
      !graph.unresolved_artifact_nodes.empty()) {
    summary.decision = DagDecision::ProducerContractBlocked;
  } else if (!graph.cycle_witnesses.empty()) {
    summary.decision = DagDecision::CyclesDetected;
  } else {
    summary.decision = DagDecision::Acyclic;
  }
  return summary;
}

}  // namespace

const char* node_kind_name(const NodeKind kind) {
  switch (kind) {
    case NodeKind::Producer:
      return "producer";
    case NodeKind::Artifact:
      return "artifact";
  }
  throw std::invalid_argument("unknown repository DAG node kind");
}

const char* edge_kind_name(const EdgeKind kind) {
  switch (kind) {
    case EdgeKind::Produces:
      return "produces";
    case EdgeKind::CurrentInput:
      return "current_input";
    case EdgeKind::PostGenerationOrder:
      return "post_generation_order";
    case EdgeKind::Observation:
      return "observation";
  }
  throw std::invalid_argument("unknown repository DAG edge kind");
}

const char* dependency_phase_name(const DependencyPhase phase) {
  switch (phase) {
    case DependencyPhase::ProducerDeclaration:
      return "producer_declaration";
    case DependencyPhase::CurrentAuthoritative:
      return "current_authoritative";
    case DependencyPhase::CurrentRepositoryGovernance:
      return "current_repository_governance";
    case DependencyPhase::PostGeneration:
      return "post_generation";
    case DependencyPhase::HistoricalObservation:
      return "historical_observation";
  }
  throw std::invalid_argument("unknown repository DAG dependency phase");
}

const char* exemption_kind_name(const ExemptionKind exemption) {
  switch (exemption) {
    case ExemptionKind::None:
      return "none";
    case ExemptionKind::InventoryObservation:
      return "inventory_observation";
    case ExemptionKind::ExplanationOnly:
      return "explanation_only";
    case ExemptionKind::SelfOutputDeferred:
      return "self_output_deferred";
  }
  throw std::invalid_argument("unknown repository DAG exemption kind");
}

const char* dag_decision_name(const DagDecision decision) {
  switch (decision) {
    case DagDecision::Acyclic:
      return "acyclic";
    case DagDecision::ProducerContractBlocked:
      return "producer_contract_blocked";
    case DagDecision::CyclesDetected:
      return "cycles_detected";
  }
  throw std::invalid_argument("unknown repository DAG decision");
}

namespace internal {

void GraphDraft::add_owner(const std::string& owner) {
  append_unique(composed_owners_, owner, "repository DAG composed owner");
}

void GraphDraft::add_scope_exclusion(const std::string& exclusion) {
  append_unique(scope_exclusions_, exclusion,
                "repository DAG scope exclusion");
}

void GraphDraft::add_composition_issue(const CompositionIssue& issue) {
  if (!safe_repository_identity(issue.id) || !safe_text(issue.category) ||
      !safe_text(issue.source_label) || !safe_text(issue.detail) ||
      !safe_text(issue.current_repo_action)) {
    throw std::logic_error(
        "repository DAG composition issue metadata is invalid");
  }
  for (const auto& existing : composition_issues_) {
    if (existing.id == issue.id) {
      throw std::logic_error(
          "repository DAG composition issue identity is duplicated: " +
          issue.id);
    }
  }
  if (composition_issues_.size() >= kMaximumCompositionIssueCount) {
    throw std::logic_error(
        "repository DAG composition issue policy limit exceeded");
  }
  composition_issues_.push_back(issue);
}

std::size_t GraphDraft::ensure_node(const std::string& id,
                                    const std::string& label,
                                    const NodeKind kind,
                                    const std::string& identity_owner) {
  if (!safe_text(id) || !safe_repository_identity(label) ||
      !safe_text(identity_owner)) {
    throw std::logic_error("repository DAG node metadata is invalid");
  }
  const auto found = node_index_.find(id);
  if (found != node_index_.end()) {
    Node& existing = nodes_[found->second];
    if (existing.kind != kind || existing.label != label) {
      throw std::logic_error(
          "repository DAG node identity resolves to conflicting metadata: " +
          id);
    }
    append_unique(existing.identity_owners, identity_owner,
                  "repository DAG node identity owner");
    return found->second;
  }
  if (nodes_.size() >= kMaximumNodeCount) {
    throw std::logic_error("repository DAG node policy limit exceeded");
  }
  Node node;
  node.id = id;
  node.label = label;
  node.kind = kind;
  node.identity_owners.push_back(identity_owner);
  const std::size_t index = nodes_.size();
  nodes_.push_back(std::move(node));
  node_index_.emplace(id, index);
  return index;
}

void GraphDraft::add_edge(const std::string& from_node,
                          const std::string& to_node,
                          const EdgeKind kind,
                          const DependencyPhase phase,
                          const ExemptionKind exemption,
                          const std::string& declared_by,
                          const std::string& role) {
  const bool authoritative = exemption == ExemptionKind::None;
  const bool participates = kind == EdgeKind::Produces || authoritative;
  for (auto& edge : edges_) {
    if (edge.from_node == from_node && edge.to_node == to_node &&
        edge.kind == kind && edge.phase == phase &&
        edge.exemption == exemption) {
      append_unique(edge.declared_by, declared_by,
                    "repository DAG edge declaration owner");
      append_unique(edge.roles, role, "repository DAG edge role");
      return;
    }
  }
  if (edges_.size() >= kMaximumEdgeCount) {
    throw std::logic_error("repository DAG edge policy limit exceeded");
  }
  Edge edge;
  edge.from_node = from_node;
  edge.to_node = to_node;
  edge.kind = kind;
  edge.phase = phase;
  edge.exemption = exemption;
  edge.authoritative = authoritative;
  edge.participates_in_cycle_check = participates;
  edge.declared_by.push_back(declared_by);
  edge.roles.push_back(role);
  edge.id = std::string(edge_kind_name(kind)) + ":" + from_node + "->" +
            to_node + ":" + dependency_phase_name(phase) + ":" +
            exemption_kind_name(exemption);
  edges_.push_back(std::move(edge));
}

void GraphDraft::add_producer(const std::string& artifact_path,
                              const std::string& producer_target,
                              const std::string& identity_owner) {
  if (!safe_repository_identity(artifact_path) ||
      !safe_repository_identity(producer_target) ||
      !safe_text(identity_owner)) {
    throw std::logic_error(
        "repository DAG producer declaration is invalid");
  }
  const std::string target_node = "target:" + producer_target;
  const std::string artifact_node = "artifact:" + artifact_path;
  (void)ensure_node(target_node, producer_target, NodeKind::Producer,
                    identity_owner);
  (void)ensure_node(artifact_node, artifact_path, NodeKind::Artifact,
                    identity_owner);
  artifact_producers_[artifact_node].insert(target_node);
  artifact_producer_owners_[artifact_node].insert(identity_owner);
  add_edge(target_node, artifact_node, EdgeKind::Produces,
           DependencyPhase::ProducerDeclaration, ExemptionKind::None,
           identity_owner, "canonical_artifact_producer");
}

void GraphDraft::add_dependency(const std::string& artifact_path,
                                const std::string& consumer_target,
                                const EdgeKind kind,
                                const DependencyPhase phase,
                                const ExemptionKind exemption,
                                const std::string& declared_by,
                                const std::string& role) {
  if (!safe_repository_identity(artifact_path) ||
      !safe_repository_identity(consumer_target) ||
      !safe_text(declared_by) || !safe_text(role) ||
      kind == EdgeKind::Produces) {
    throw std::logic_error(
        "repository DAG consumer dependency declaration is invalid");
  }
  const std::string artifact_node = "artifact:" + artifact_path;
  const std::string consumer_node = "target:" + consumer_target;
  (void)ensure_node(artifact_node, artifact_path, NodeKind::Artifact,
                    declared_by);
  (void)ensure_node(consumer_node, consumer_target, NodeKind::Producer,
                    declared_by);
  add_edge(artifact_node, consumer_node, kind, phase, exemption,
           declared_by, role);
}

std::vector<std::string> GraphDraft::producer_targets_for(
    const std::string& artifact_path) const {
  const std::string artifact_node = "artifact:" + artifact_path;
  const auto found = artifact_producers_.find(artifact_node);
  if (found == artifact_producers_.end()) return {};
  std::vector<std::string> targets;
  targets.reserve(found->second.size());
  for (const auto& node_id : found->second) {
    constexpr char kPrefix[] = "target:";
    targets.push_back(node_id.substr(sizeof(kPrefix) - 1U));
  }
  return targets;
}

RepositoryEvidenceDag GraphDraft::finish() {
  if (!composed_owner_registry_matches(composed_owners_)) {
    add_composition_issue(
        {"canonical_owner_registry_drift", "dependency_owner_drift",
         "repository_evidence_dag_composition.cpp",
         "composed owner identities do not exactly match the canonical repository DAG owner registry",
         "restore the exact canonical owner set before claiming repository evidence-surface coverage"});
  }
  RepositoryEvidenceDag graph;
  graph.nodes = std::move(nodes_);
  graph.edges = std::move(edges_);
  graph.composed_owners = std::move(composed_owners_);
  graph.scope_exclusions = std::move(scope_exclusions_);
  graph.composition_issues = std::move(composition_issues_);
  for (const auto& [artifact_node, producers] : artifact_producers_) {
    if (producers.size() <= 1U) continue;
    ProducerConflict conflict;
    conflict.artifact_node = artifact_node;
    conflict.producer_nodes.assign(producers.begin(), producers.end());
    const auto owner = artifact_producer_owners_.find(artifact_node);
    if (owner != artifact_producer_owners_.end()) {
      conflict.identity_owners.assign(owner->second.begin(),
                                      owner->second.end());
    }
    conflict.current_repo_action =
        "select one canonical producer target, migrate callers, and retain any compatibility executable only as an explicitly owned thin alias";
    graph.producer_conflicts.push_back(std::move(conflict));
  }
  for (const auto& node : graph.nodes) {
    if (node.kind != NodeKind::Artifact) continue;
    bool consumed = false;
    for (const auto& edge : graph.edges) {
      if (edge.from_node == node.id && edge.kind != EdgeKind::Produces) {
        consumed = true;
        break;
      }
    }
    const auto producer = artifact_producers_.find(node.id);
    if (consumed &&
        (producer == artifact_producers_.end() || producer->second.empty())) {
      graph.unresolved_artifact_nodes.push_back(node.id);
    }
  }
  return finalize_repository_evidence_dag(std::move(graph));
}

RepositoryEvidenceDag finalize_repository_evidence_dag(
    RepositoryEvidenceDag graph) {
  std::sort(graph.composed_owners.begin(), graph.composed_owners.end());
  std::sort(graph.scope_exclusions.begin(), graph.scope_exclusions.end());
  std::sort(graph.composition_issues.begin(), graph.composition_issues.end(),
            [](const CompositionIssue& lhs, const CompositionIssue& rhs) {
              return lhs.id < rhs.id;
            });
  std::sort(graph.unresolved_artifact_nodes.begin(),
            graph.unresolved_artifact_nodes.end());
  graph.cycle_witnesses = project_cycle_witnesses(graph);

  for (const auto& conflict : graph.producer_conflicts) {
    RepairCard card;
    card.id = "producer_conflict_" +
              std::to_string(graph.repair_cards.size() + 1U);
    card.category = "producer_identity_conflict";
    for (const auto& producer : conflict.producer_nodes) {
      constexpr char kPrefix[] = "target:";
      card.affected_targets.push_back(
          producer.rfind(kPrefix, 0U) == 0U
              ? producer.substr(sizeof(kPrefix) - 1U)
              : producer);
    }
    card.repair_environment = "current_repo_fixable";
    card.current_repo_action = conflict.current_repo_action;
    card.target_machine_action =
        "none; producer identity ambiguity must be repaired in repository metadata before execution";
    graph.repair_cards.push_back(std::move(card));
  }
  for (const auto& artifact : graph.unresolved_artifact_nodes) {
    RepairCard card;
    card.id = "unresolved_producer_" +
              std::to_string(graph.repair_cards.size() + 1U);
    card.category = "unresolved_artifact_producer";
    card.repair_environment = "current_repo_fixable";
    card.current_repo_action =
        "register the artifact producer in its canonical catalog or lifecycle owner before treating it as a current dependency";
    card.target_machine_action =
        "none; running an unregistered producer cannot repair repository lineage";
    graph.repair_cards.push_back(std::move(card));
  }
  for (const auto& issue : graph.composition_issues) {
    RepairCard card;
    card.id = "composition_issue_" + issue.id;
    card.category = issue.category;
    card.repair_environment = "current_repo_fixable";
    card.current_repo_action = issue.current_repo_action;
    card.target_machine_action =
        "none; repository dependency inventory must be readable and valid before target-machine execution";
    graph.repair_cards.push_back(std::move(card));
  }
  for (const auto& witness : graph.cycle_witnesses) {
    RepairCard card;
    card.id = "repair_" + witness.id;
    card.category = "authoritative_dependency_cycle";
    card.affected_targets = witness.affected_targets;
    card.candidate_edge_ids = witness.edge_ids;
    card.repair_environment = "current_repo_fixable";
    card.current_repo_action =
        "remove or reclassify at least one authoritative reverse read in this strongly connected component; move packaging and report consumers downstream of query, root-cause, explain, and release decisions";
    card.target_machine_action =
        "none; artifact refresh order cannot make an authoritative source cycle valid";
    graph.repair_cards.push_back(std::move(card));
  }
  graph.claim_boundary =
      "Repository evidence DAG validation covers declared InferLab artifact producers and evidence/release-governance consumers only. Typed inventory and explanation observations are excluded from cycle authority. Acyclic status does not prove build, runtime execution, product behavior, freshness, performance, or release approval.";
  graph.summary = project_summary(graph);
  validate_repository_evidence_dag(graph);
  return graph;
}

}  // namespace internal

void validate_repository_evidence_dag(const RepositoryEvidenceDag& graph) {
  if (graph.nodes.empty() || graph.nodes.size() > kMaximumNodeCount ||
      graph.edges.empty() || graph.edges.size() > kMaximumEdgeCount ||
      graph.claim_boundary.empty() || graph.scope_exclusions.empty()) {
    throw std::logic_error(
        "repository DAG bounded graph or claim contract is invalid");
  }
  if (!composed_owner_registry_matches(graph.composed_owners)) {
    const auto issue = std::find_if(
        graph.composition_issues.begin(), graph.composition_issues.end(),
        [](const CompositionIssue& candidate) {
          return candidate.id == "canonical_owner_registry_drift";
        });
    if (issue == graph.composition_issues.end()) {
      throw std::logic_error(
          "repository DAG owner registry drift is not represented by a composition issue");
    }
  }
  std::set<std::string> node_ids;
  for (const auto& node : graph.nodes) {
    if (!safe_text(node.id) || !safe_repository_identity(node.label) ||
        node.identity_owners.empty() || !node_ids.insert(node.id).second) {
      throw std::logic_error("repository DAG node contract is invalid");
    }
    const std::string prefix =
        node.kind == NodeKind::Producer ? "target:" : "artifact:";
    if (node.id != prefix + node.label) {
      throw std::logic_error(
          "repository DAG node id does not match its typed label");
    }
  }
  if (graph.composition_issues.size() > kMaximumCompositionIssueCount) {
    throw std::logic_error(
        "repository DAG composition issue policy limit exceeded");
  }
  std::set<std::string> composition_issue_ids;
  for (const auto& issue : graph.composition_issues) {
    if (!safe_repository_identity(issue.id) || !safe_text(issue.category) ||
        !safe_text(issue.source_label) || !safe_text(issue.detail) ||
        !safe_text(issue.current_repo_action) ||
        !composition_issue_ids.insert(issue.id).second) {
      throw std::logic_error(
          "repository DAG composition issue contract is invalid");
    }
  }
  std::set<std::string> edge_ids;
  for (const auto& edge : graph.edges) {
    if (!safe_text(edge.id) || edge.declared_by.empty() ||
        edge.roles.empty() || !edge_ids.insert(edge.id).second) {
      throw std::logic_error("repository DAG edge contract is invalid");
    }
    const Node& from = graph.nodes[require_node_index(graph, edge.from_node)];
    const Node& to = graph.nodes[require_node_index(graph, edge.to_node)];
    if (edge.kind == EdgeKind::Produces) {
      if (from.kind != NodeKind::Producer || to.kind != NodeKind::Artifact ||
          edge.phase != DependencyPhase::ProducerDeclaration ||
          edge.exemption != ExemptionKind::None || !edge.authoritative ||
          !edge.participates_in_cycle_check) {
        throw std::logic_error(
            "repository DAG producer edge contract is invalid");
      }
    } else {
      if (from.kind != NodeKind::Artifact || to.kind != NodeKind::Producer) {
        throw std::logic_error(
            "repository DAG dependency edge must be artifact-to-consumer");
      }
      const bool exempt = edge.exemption != ExemptionKind::None;
      if (edge.authoritative == exempt ||
          edge.participates_in_cycle_check == exempt ||
          (edge.kind == EdgeKind::Observation) != exempt ||
          (edge.kind == EdgeKind::PostGenerationOrder) !=
              (edge.phase == DependencyPhase::PostGeneration)) {
        throw std::logic_error(
            "repository DAG dependency phase or exemption is inconsistent");
      }
    }
    if (edge_is_self_loop(edge)) {
      throw std::logic_error("repository DAG typed bipartite self-edge found");
    }
  }

  const Summary expected = project_summary(graph);
  if (graph.summary.node_count != expected.node_count ||
      graph.summary.producer_node_count != expected.producer_node_count ||
      graph.summary.artifact_node_count != expected.artifact_node_count ||
      graph.summary.edge_count != expected.edge_count ||
      graph.summary.producer_edge_count != expected.producer_edge_count ||
      graph.summary.current_dependency_edge_count !=
          expected.current_dependency_edge_count ||
      graph.summary.post_generation_edge_count !=
          expected.post_generation_edge_count ||
      graph.summary.exempt_observation_edge_count !=
          expected.exempt_observation_edge_count ||
      graph.summary.cycle_checked_edge_count !=
          expected.cycle_checked_edge_count ||
      graph.summary.producer_conflict_count !=
          expected.producer_conflict_count ||
      graph.summary.unresolved_producer_count !=
          expected.unresolved_producer_count ||
      graph.summary.composition_issue_count !=
          expected.composition_issue_count ||
      graph.summary.cycle_witness_count != expected.cycle_witness_count ||
      graph.summary.cyclic_node_count != expected.cyclic_node_count ||
      graph.summary.repair_card_count != expected.repair_card_count ||
      graph.summary.composed_owner_count != expected.composed_owner_count ||
      graph.summary.declared_evidence_surface_scope_complete !=
          expected.declared_evidence_surface_scope_complete ||
      graph.summary.acyclic != expected.acyclic ||
      graph.summary.decision != expected.decision) {
    throw std::logic_error(
        "repository DAG summary or decision reconciliation failed");
  }
  if (graph.repair_cards.size() !=
      inferlab::internal::checked_sum_size(
          {graph.producer_conflicts.size(),
           graph.unresolved_artifact_nodes.size(),
           graph.composition_issues.size(),
           graph.cycle_witnesses.size()},
          "repository DAG expected repair-card count")) {
    throw std::logic_error(
        "repository DAG repair-card cardinality reconciliation failed");
  }
}

}  // namespace inferlab_repository_dag
