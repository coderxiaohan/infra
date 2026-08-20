#include "repository_evidence_dag_view.hpp"

#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_repository_dag {
namespace {

using inferlab_plan::json_bool;

template <typename Value, typename Renderer>
std::string array_json(const std::vector<Value>& values, const int indent,
                       Renderer renderer) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[";
  if (!values.empty()) out << "\n";
  for (std::size_t index = 0U; index < values.size(); ++index) {
    out << renderer(values[index], indent + 2)
        << (index + 1U == values.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string node_json(const Node& node, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(node.id)
      << ",\n";
  out << nested << "\"identity_owners\": "
      << inferlab_plan::json_string_array(node.identity_owners, indent + 2)
      << ",\n";
  out << nested << "\"kind\": "
      << inferlab_plan::json_string(node_kind_name(node.kind)) << ",\n";
  out << nested << "\"label\": "
      << inferlab_plan::json_string(node.label) << "\n";
  out << pad << "}";
  return out.str();
}

std::string edge_json(const Edge& edge, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"authoritative\": " << json_bool(edge.authoritative)
      << ",\n";
  out << nested << "\"declared_by\": "
      << inferlab_plan::json_string_array(edge.declared_by, indent + 2)
      << ",\n";
  out << nested << "\"exemption\": "
      << inferlab_plan::json_string(exemption_kind_name(edge.exemption))
      << ",\n";
  out << nested << "\"from_node\": "
      << inferlab_plan::json_string(edge.from_node) << ",\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(edge.id)
      << ",\n";
  out << nested << "\"kind\": "
      << inferlab_plan::json_string(edge_kind_name(edge.kind)) << ",\n";
  out << nested << "\"participates_in_cycle_check\": "
      << json_bool(edge.participates_in_cycle_check) << ",\n";
  out << nested << "\"phase\": "
      << inferlab_plan::json_string(dependency_phase_name(edge.phase))
      << ",\n";
  out << nested << "\"roles\": "
      << inferlab_plan::json_string_array(edge.roles, indent + 2) << ",\n";
  out << nested << "\"to_node\": "
      << inferlab_plan::json_string(edge.to_node) << "\n";
  out << pad << "}";
  return out.str();
}

std::string conflict_json(const ProducerConflict& conflict,
                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact_node\": "
      << inferlab_plan::json_string(conflict.artifact_node) << ",\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(conflict.current_repo_action) << ",\n";
  out << nested << "\"identity_owners\": "
      << inferlab_plan::json_string_array(conflict.identity_owners,
                                           indent + 2)
      << ",\n";
  out << nested << "\"producer_nodes\": "
      << inferlab_plan::json_string_array(conflict.producer_nodes,
                                           indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string composition_issue_json(const CompositionIssue& issue,
                                   const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"category\": "
      << inferlab_plan::json_string(issue.category) << ",\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(issue.current_repo_action) << ",\n";
  out << nested << "\"detail\": "
      << inferlab_plan::json_string(issue.detail) << ",\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(issue.id)
      << ",\n";
  out << nested << "\"source_label\": "
      << inferlab_plan::json_string(issue.source_label) << "\n";
  out << pad << "}";
  return out.str();
}

std::string cycle_json(const CycleWitness& witness, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"affected_targets\": "
      << inferlab_plan::json_string_array(witness.affected_targets,
                                           indent + 2)
      << ",\n";
  out << nested << "\"edge_ids\": "
      << inferlab_plan::json_string_array(witness.edge_ids, indent + 2)
      << ",\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(witness.id)
      << ",\n";
  out << nested << "\"node_ids\": "
      << inferlab_plan::json_string_array(witness.node_ids, indent + 2)
      << ",\n";
  out << nested << "\"witness_kind\": "
      << inferlab_plan::json_string(witness.witness_kind) << "\n";
  out << pad << "}";
  return out.str();
}

std::string repair_json(const RepairCard& card, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"affected_targets\": "
      << inferlab_plan::json_string_array(card.affected_targets, indent + 2)
      << ",\n";
  out << nested << "\"candidate_edge_ids\": "
      << inferlab_plan::json_string_array(card.candidate_edge_ids,
                                           indent + 2)
      << ",\n";
  out << nested << "\"category\": "
      << inferlab_plan::json_string(card.category) << ",\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(card.current_repo_action) << ",\n";
  out << nested << "\"id\": " << inferlab_plan::json_string(card.id)
      << ",\n";
  out << nested << "\"repair_environment\": "
      << inferlab_plan::json_string(card.repair_environment) << ",\n";
  out << nested << "\"target_machine_action\": "
      << inferlab_plan::json_string(card.target_machine_action) << "\n";
  out << pad << "}";
  return out.str();
}

std::string summary_json(const Summary& summary, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"acyclic\": " << json_bool(summary.acyclic) << ",\n";
  out << nested << "\"artifact_node_count\": "
      << summary.artifact_node_count << ",\n";
  out << nested << "\"composed_owner_count\": "
      << summary.composed_owner_count << ",\n";
  out << nested << "\"composition_issue_count\": "
      << summary.composition_issue_count << ",\n";
  out << nested << "\"current_dependency_edge_count\": "
      << summary.current_dependency_edge_count << ",\n";
  out << nested << "\"cycle_checked_edge_count\": "
      << summary.cycle_checked_edge_count << ",\n";
  out << nested << "\"cycle_witness_count\": "
      << summary.cycle_witness_count << ",\n";
  out << nested << "\"cyclic_node_count\": "
      << summary.cyclic_node_count << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(dag_decision_name(summary.decision))
      << ",\n";
  out << nested << "\"declared_evidence_surface_scope_complete\": "
      << json_bool(summary.declared_evidence_surface_scope_complete)
      << ",\n";
  out << nested << "\"edge_count\": " << summary.edge_count << ",\n";
  out << nested << "\"exempt_observation_edge_count\": "
      << summary.exempt_observation_edge_count << ",\n";
  out << nested << "\"node_count\": " << summary.node_count << ",\n";
  out << nested << "\"post_generation_edge_count\": "
      << summary.post_generation_edge_count << ",\n";
  out << nested << "\"producer_conflict_count\": "
      << summary.producer_conflict_count << ",\n";
  out << nested << "\"producer_edge_count\": "
      << summary.producer_edge_count << ",\n";
  out << nested << "\"producer_node_count\": "
      << summary.producer_node_count << ",\n";
  out << nested << "\"repair_card_count\": "
      << summary.repair_card_count << ",\n";
  out << nested << "\"unresolved_producer_count\": "
      << summary.unresolved_producer_count << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string repository_evidence_dag_json(
    const RepositoryEvidenceDag& graph, const int indent) {
  validate_repository_evidence_dag(graph);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(graph.claim_boundary) << ",\n";
  out << nested << "\"composed_owners\": "
      << inferlab_plan::json_string_array(graph.composed_owners, indent + 2)
      << ",\n";
  out << nested << "\"composition_issues\": "
      << array_json(graph.composition_issues, indent + 2,
                    composition_issue_json)
      << ",\n";
  out << nested << "\"cycle_witnesses\": "
      << array_json(graph.cycle_witnesses, indent + 2, cycle_json) << ",\n";
  out << nested << "\"edges\": "
      << array_json(graph.edges, indent + 2, edge_json) << ",\n";
  out << nested << "\"nodes\": "
      << array_json(graph.nodes, indent + 2, node_json) << ",\n";
  out << nested << "\"producer_conflicts\": "
      << array_json(graph.producer_conflicts, indent + 2, conflict_json)
      << ",\n";
  out << nested << "\"repair_cards\": "
      << array_json(graph.repair_cards, indent + 2, repair_json) << ",\n";
  out << nested << "\"scope_exclusions\": "
      << inferlab_plan::json_string_array(graph.scope_exclusions, indent + 2)
      << ",\n";
  out << nested << "\"summary\": "
      << summary_json(graph.summary, indent + 2) << ",\n";
  out << nested << "\"unresolved_artifact_nodes\": "
      << inferlab_plan::json_string_array(graph.unresolved_artifact_nodes,
                                           indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_repository_dag
