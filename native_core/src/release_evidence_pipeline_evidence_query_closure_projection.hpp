#pragma once

#include "artifact_field_projection.hpp"
#include "evidence_query_projection.hpp"
#include "high_value_artifact_catalog.hpp"
#include "release_evidence_pipeline_evidence_query_catalog_flow_projection.hpp"
#include "release_evidence_pipeline_evidence_query_cross_artifact_projection.hpp"
#include "release_evidence_pipeline_evidence_query_query_ux_projection.hpp"
#include "release_evidence_pipeline_source_context.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

struct EvidenceQueryOperationCatalogClosureProjection {
  bool object_present = false;
  inferlab_artifact::UnsignedFieldProjection supported_count;
  inferlab_artifact::UnsignedFieldProjection read_only_count;
  inferlab_artifact::UnsignedFieldProjection snapshot_required_count;
  inferlab_artifact::UnsignedFieldProjection selector_recommended_count;
  inferlab_artifact::UnsignedFieldProjection target_required_count;
  bool read_only_complete = false;
  std::size_t contract_issue_count = 0;
};

struct EvidenceQuerySelectedOperationClosureProjection {
  bool object_present = false;
  inferlab_artifact::BoolFieldProjection result_present;
  inferlab_artifact::BoolFieldProjection input_ready;
  inferlab_artifact::UnsignedFieldProjection payload_count;
  inferlab_artifact::StringFieldProjection result_kind;
  inferlab_artifact::UnsignedFieldProjection show_artifact_candidate_count;
  inferlab_artifact::BoolFieldProjection show_artifact_candidate_truncated;
  inferlab_artifact::BoolFieldProjection show_artifact_exact_match;
  inferlab_artifact::BoolFieldProjection show_artifact_selector_ambiguous;
  inferlab_artifact::StringFieldProjection show_artifact_selection_strategy;
  inferlab_artifact::BoolFieldProjection payload_selector_focus_present;
  inferlab_artifact::StringFieldProjection payload_selector_focus_status;
  inferlab_artifact::UnsignedFieldProjection payload_selector_focus_match_count;
  inferlab_artifact::BoolFieldProjection payload_selector_focus_ambiguous;
  inferlab_artifact::UnsignedFieldProjection
      payload_selector_focus_source_location_gap_count;
  std::size_t contract_issue_count = 0;
};

struct ReleasePipelineEvidenceQueryClosureProjection {
  explicit ReleasePipelineEvidenceQueryClosureProjection(
      const ReleasePipelineSourceContext& sources);

  const inferlab_high_value::ManagedArtifact& artifact;
  const inferlab_evidence::EvidenceQuerySnapshotDiffSignals& snapshot_diff;
  const inferlab_evidence::EvidenceQuerySelectorResolutionSignals&
      selector_resolution;
  const inferlab_evidence::EvidenceQuerySelectorRepairCardExportSignals&
      selector_repair_card_export;
  const inferlab_evidence::EvidenceQuerySelectorActionPlanSignals&
      selector_action_plan;
  bool source_present = false;
  bool source_readable = false;
  std::string source_read_status = "missing";
  bool summary_usable = false;
  EvidenceQueryQueryUxClosureProjection query_ux;
  EvidenceQueryCatalogFlowClosureProjection catalog_flow;
  EvidenceQueryCrossArtifactClosureProjection cross_artifact;
  EvidenceQueryProductReplayClosureProjection product_replay;
  EvidenceQueryOperationCatalogClosureProjection operation_catalog;
  EvidenceQuerySelectedOperationClosureProjection selected_operation;
  std::size_t source_contract_issue_count = 0;
};

}  // namespace inferlab_release_pipeline
