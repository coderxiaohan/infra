#pragma once

#include "repository_evidence_dag.hpp"

#include <string>

namespace inferlab_repository_dag {

std::string repository_evidence_dag_json(
    const RepositoryEvidenceDag& graph, int indent);

}  // namespace inferlab_repository_dag
