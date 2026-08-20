#pragma once

#include "evidence_query_operation_catalog.hpp"

#include <string>

namespace inferlab_evidence_query {

struct QueryOperation;
struct SnapshotDiffSignals;

std::string operation_descriptor_json(
    const QueryOperation& operation,
    const SnapshotDiffSignals& snapshot_diff,
    int indent);

std::string operation_catalog_json(int indent);

}  // namespace inferlab_evidence_query
