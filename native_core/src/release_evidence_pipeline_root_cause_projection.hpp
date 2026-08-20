#pragma once

#include "high_value_artifact_catalog.hpp"
#include "release_evidence_pipeline_source_context.hpp"
#include "root_cause_repair_counts.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

// Compatibility name retained while downstream views migrate to the
// Evidence Query canonical repair-card projection.
struct ReleasePipelineRootCauseSourceProjection {
  explicit ReleasePipelineRootCauseSourceProjection(
      const ReleasePipelineSourceContext& sources);

  const inferlab_high_value::ManagedArtifact& artifact;
  bool source_present = false;
  bool source_readable = false;
  bool summary_usable = false;
  std::string source_read_status = "missing";
  inferlab_evidence::RootCauseRepairCardProjection repair_cards;
  std::size_t flow_check_repair_card_count = 0;
  std::size_t repair_card_contract_issue_count = 0;
  std::size_t contract_issue_count = 0;
};

}  // namespace inferlab_release_pipeline
