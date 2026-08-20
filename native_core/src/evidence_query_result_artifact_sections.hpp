#pragma once

#include "evidence_query_result_artifact_projection.hpp"

#include <ostream>

namespace inferlab_evidence_query::detail {

void append_result_artifact_header_and_summary_core(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

void append_result_artifact_summary_query_ux(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

void append_result_artifact_summary_catalog_review(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

void append_result_artifact_summary_native_audit_core(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

void append_result_artifact_summary_snapshot_and_close(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

void append_result_artifact_body(
    std::ostream& out,
    const EvidenceQueryResultArtifactProjection& projection);

}  // namespace inferlab_evidence_query::detail
