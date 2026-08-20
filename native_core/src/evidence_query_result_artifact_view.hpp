#pragma once

#include "evidence_query_request.hpp"

#include <string>

namespace inferlab_evidence_query {

std::string render_evidence_query_result_artifact(
    const EvidenceQueryRequest& request);

}  // namespace inferlab_evidence_query
