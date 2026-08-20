#pragma once

#include "evidence_query_operation_catalog.hpp"
#include "evidence_query_request.hpp"
#include "evidence_query_selected_operation_view.hpp"
#include "evidence_query_source_location_worklist_projection.hpp"
#include "evidence_query_view_context.hpp"

namespace inferlab_evidence_query {

struct EvidenceQueryResultArtifactProjection {
  explicit EvidenceQueryResultArtifactProjection(
      const EvidenceQueryRequest& request_value);

  EvidenceQueryResultArtifactProjection(
      const EvidenceQueryResultArtifactProjection&) = delete;
  EvidenceQueryResultArtifactProjection& operator=(
      const EvidenceQueryResultArtifactProjection&) = delete;
  EvidenceQueryResultArtifactProjection(
      EvidenceQueryResultArtifactProjection&&) = delete;
  EvidenceQueryResultArtifactProjection& operator=(
      EvidenceQueryResultArtifactProjection&&) = delete;

  const EvidenceQueryRequest& request;
  EvidenceQueryOperationCatalogSummary operation_catalog_summary;
  SourceLocationWorklistFocusProjection source_location_focus;
  EvidenceQueryOperationViewContext operation_view;
  EvidenceQueryViewContext query_view;
  SelectedOperationJsonContext selected_operation_view;
  bool payload_selector_focus_present = false;
};

}  // namespace inferlab_evidence_query
