#pragma once

#include "artifact_text_source.hpp"
#include "evidence_query_request.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

std::string product_replay_semantic_linkage_json(
    const ProductReplaySemanticLinkageSignals& signals,
    int indent);

std::string source_files_json(
    const std::vector<inferlab_artifact::TextSource>& sources,
    int indent);

std::string query_plan_json(const EvidenceQueryRequest& request, int indent);

std::string human_summary_json(
    std::size_t matched_count,
    std::size_t blocked_count,
    const std::string& query,
    const std::string& blocker_filter,
    int indent);

}  // namespace inferlab_evidence_query
