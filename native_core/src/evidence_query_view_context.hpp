#pragma once

#include "evidence_query_artifact_projection.hpp"
#include "evidence_query_blocker_drilldown.hpp"
#include "evidence_query_operation_guidance_projection.hpp"
#include "evidence_query_repair_card_projection.hpp"
#include "evidence_query_repair_collection_projection.hpp"
#include "evidence_query_repair_source_projection.hpp"
#include "evidence_query_selection.hpp"
#include "evidence_query_snapshot_diff.hpp"
#include "evidence_query_source_location_worklist_projection.hpp"

#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct EvidenceQueryOperationViewContext {
  const QueryArtifactProjection& artifact_projection;
  const QueryOperation& operation;
  const std::string& query;
  const std::vector<RepairPathSummary>& shortest_paths;
  const std::vector<RepairCardGroup>& repair_groups;
  const OperationGuidanceProjection& guidance;
  const SelectorResolution& resolution;
  const BlockerDrilldown& blocker_drilldown;
  const SnapshotDiffSignals& snapshot_diff;
};

struct EvidenceQueryViewContext {
  const EvidenceQueryOperationViewContext& operation_view;
  const RepairCardRenderProjection& repair_cards;
  const RepairCollectionProjection& repair_collection;
  const SourceLocationCategoryCoverage& source_location_coverage;
  const RepairCardSourceExactnessProjection& source_exactness;
  const SourceLocationWorklistFocusProjection& source_location_focus;
};

}  // namespace inferlab_evidence_query
