#pragma once

#include "high_value_artifact_catalog.hpp"
#include "native_core_audit_merge.hpp"
#include "release_evidence_pipeline_source_context.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

struct ReleasePipelineNativeAuditClosureProjection {
  explicit ReleasePipelineNativeAuditClosureProjection(
      const ReleasePipelineSourceContext& sources);

  const inferlab_high_value::ManagedArtifact& evidence_query_artifact;
  const inferlab_high_value::ManagedArtifact& audit_register_artifact;
  const inferlab_evidence::NativeCoreAuditProjectionSignals& signals;
  bool evidence_query_direct_present = false;
  bool audit_register_direct_present = false;
  bool audit_register_readable = false;
  std::string audit_register_read_status;
  std::size_t blocker_count = 0;
  std::size_t source_contract_issue_count = 0;
  std::string filter_decision;
};

}  // namespace inferlab_release_pipeline
