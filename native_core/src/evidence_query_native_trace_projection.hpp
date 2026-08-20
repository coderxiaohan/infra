#pragma once

#include "deep_dive_trace_token_join.hpp"
#include "evidence_release_policy.hpp"

#include <filesystem>

namespace inferlab_evidence_query {

inferlab_deep_dive_trace::TraceTokenJoinProjection
project_evidence_query_native_trace_correlation(
    const std::filesystem::path& results_path,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots);

}  // namespace inferlab_evidence_query
