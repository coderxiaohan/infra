#pragma once

#include "evidence_query_request.hpp"

#include <string>

namespace inferlab_evidence_query {

namespace detail {

std::string evidence_query_text_overview(
    const EvidenceQueryRequest& request);
std::string evidence_query_text_filter_status(
    const EvidenceQueryRequest& request);
std::string evidence_query_text_native_audit_and_snapshot(
    const EvidenceQueryRequest& request);
std::string evidence_query_text_repair_and_results(
    const EvidenceQueryRequest& request);

}  // namespace detail

std::string render_evidence_query_text(const EvidenceQueryRequest& request);

}  // namespace inferlab_evidence_query
