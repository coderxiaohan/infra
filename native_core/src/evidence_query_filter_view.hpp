#pragma once

#include "evidence_query_consumer_filter_signals.hpp"

#include <string>

namespace inferlab_evidence_query {

std::string catalog_flow_filter_query_json(
    const CatalogFlowFilterSignals& signals,
    int indent);

std::string native_core_audit_filter_query_json(
    const NativeCoreAuditFilterSignals& signals,
    int indent);

}  // namespace inferlab_evidence_query
