#pragma once

#include "artifact_metadata_reader.hpp"
#include "native_core_audit_projection.hpp"

#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct NativeCoreAuditQueryProjection {
  NativeCoreAuditProjectionSignals signals;
  std::size_t blocker_count = 0;
  std::string filter_decision;
};

NativeCoreAuditQueryProjection project_native_core_audit_query(
    const inferlab_artifact::Metadata& evidence_query_meta,
    const inferlab_artifact::TextReadResult& evidence_query_read);

std::size_t native_core_audit_query_consumer_blocker_count(
    const NativeCoreAuditQueryProjection& projection);

std::string native_core_audit_query_consumer_filter_decision(
    const NativeCoreAuditQueryProjection& projection);

}  // namespace inferlab_evidence
