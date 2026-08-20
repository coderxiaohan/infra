#pragma once

#include "repository_evidence_dag.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace inferlab_repository_dag::internal {

class GraphDraft {
 public:
  void add_owner(const std::string& owner);
  void add_scope_exclusion(const std::string& exclusion);
  void add_composition_issue(const CompositionIssue& issue);
  void add_producer(const std::string& artifact_path,
                    const std::string& producer_target,
                    const std::string& identity_owner);
  void add_dependency(const std::string& artifact_path,
                      const std::string& consumer_target,
                      EdgeKind kind,
                      DependencyPhase phase,
                      ExemptionKind exemption,
                      const std::string& declared_by,
                      const std::string& role);
  std::vector<std::string> producer_targets_for(
      const std::string& artifact_path) const;
  RepositoryEvidenceDag finish();

 private:
  std::size_t ensure_node(const std::string& id,
                          const std::string& label,
                          NodeKind kind,
                          const std::string& identity_owner);
  void add_edge(const std::string& from_node,
                const std::string& to_node,
                EdgeKind kind,
                DependencyPhase phase,
                ExemptionKind exemption,
                const std::string& declared_by,
                const std::string& role);

  std::vector<Node> nodes_;
  std::vector<Edge> edges_;
  std::map<std::string, std::size_t> node_index_;
  std::map<std::string, std::set<std::string>> artifact_producers_;
  std::map<std::string, std::set<std::string>> artifact_producer_owners_;
  std::vector<std::string> composed_owners_;
  std::vector<std::string> scope_exclusions_;
  std::vector<CompositionIssue> composition_issues_;
};

RepositoryEvidenceDag finalize_repository_evidence_dag(
    RepositoryEvidenceDag graph);

}  // namespace inferlab_repository_dag::internal
