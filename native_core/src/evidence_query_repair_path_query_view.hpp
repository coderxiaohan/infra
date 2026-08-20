#pragma once

#include "evidence_query_request.hpp"

#include <string>

namespace inferlab_evidence_query {

std::string repair_path_query_json(
    const EvidenceQueryRequest& request,
    int indent);

std::string root_cause_repair_cards_json(
    const EvidenceQueryReleaseSourceProjection& projection,
    int indent);

}  // namespace inferlab_evidence_query
