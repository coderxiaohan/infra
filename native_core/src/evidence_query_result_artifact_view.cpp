#include "evidence_query_result_artifact_view.hpp"

#include "evidence_query_result_artifact_projection.hpp"
#include "evidence_query_result_artifact_sections.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

std::string render_evidence_query_result_artifact(
    const EvidenceQueryRequest& request) {
  const EvidenceQueryResultArtifactProjection projection(request);
  std::ostringstream out;
  detail::append_result_artifact_header_and_summary_core(out, projection);
  detail::append_result_artifact_summary_query_ux(out, projection);
  detail::append_result_artifact_summary_catalog_review(out, projection);
  detail::append_result_artifact_summary_native_audit_core(out, projection);
  detail::append_result_artifact_summary_snapshot_and_close(out, projection);
  detail::append_result_artifact_body(out, projection);
  return out.str();
}

}  // namespace inferlab_evidence_query
