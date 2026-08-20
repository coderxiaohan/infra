#pragma once

#include "artifact_metadata_reader.hpp"
#include "native_core_audit_projection.hpp"

#include <string>

namespace inferlab_evidence {

struct NativeCoreAuditRegisterProjection {
  NativeCoreAuditProjectionSignals signals;
  std::string filter_decision;
};

NativeCoreAuditRegisterProjection project_native_core_audit_register(
    const inferlab_artifact::Metadata& audit_meta,
    const inferlab_artifact::TextReadResult& audit_read);

}  // namespace inferlab_evidence
