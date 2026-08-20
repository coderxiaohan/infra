#include "evidence_query_result_artifact_projection.hpp"

namespace inferlab_evidence_query {

EvidenceQueryResultArtifactProjection::EvidenceQueryResultArtifactProjection(
    const EvidenceQueryRequest& request_value)
    : request(request_value),
      operation_catalog_summary(evidence_query_operation_catalog_summary()),
      source_location_focus(project_source_location_worklist_focus(
          request_value.repair_groups)),
      operation_view{request_value.query_artifacts,
                     request_value.query_operation,
                     request_value.query,
                     request_value.shortest_paths,
                     request_value.repair_groups,
                     request_value.operation_guidance,
                     request_value.selector_resolution,
                     request_value.blocker_drilldown,
                     request_value.snapshot_diff},
      query_view{operation_view,
                 request_value.repair_cards,
                 request_value.repair_collection,
                 request_value.source_location_coverage,
                 request_value.source_exactness,
                 source_location_focus},
      selected_operation_view{query_view, request_value.selected_operation},
      payload_selector_focus_present(
          request_value.query_operation.operation == "explain_blocker" ||
          request_value.query_operation.operation == "suggest_repair" ||
          request_value.query_operation.operation == "repair_group_summary" ||
          request_value.query_operation.operation == "path") {}

}  // namespace inferlab_evidence_query
