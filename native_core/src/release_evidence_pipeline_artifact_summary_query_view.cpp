#include "release_evidence_pipeline_artifact_sections.hpp"

#include "release_evidence_pipeline_evidence_query_closure_view.hpp"
#include "release_evidence_pipeline_repair_link_view.hpp"

#include <ostream>

namespace inferlab_release_pipeline::detail {

void append_release_pipeline_summary_query_and_catalog(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection) {
  const auto& request = projection.request;
  out << blocker_repair_card_link_summary_fields_json(
      request.blocker_repair_link, 4);
  out << evidence_query_query_ux_summary_head_fields_json(
      request.evidence_query_closure, 4);
  out << evidence_query_cross_artifact_summary_fields_json(
      request.evidence_query_closure, 4);
  out << evidence_query_product_replay_summary_fields_json(
      request.evidence_query_closure, 4);
  out << evidence_query_query_ux_repair_summary_fields_json(
      request.evidence_query_closure, 4);
  out << evidence_query_catalog_flow_summary_fields_json(
      request.evidence_query_closure, 4);
  append_repair_metric_fields(
      out, "catalog_flow_fallback_removal_parity_gate",
      projection.repair_summary.catalog_fallback_parity_gate);
  out << evidence_query_query_ux_navigation_summary_fields_json(
      request.evidence_query_closure, 4);
}

}  // namespace inferlab_release_pipeline::detail
