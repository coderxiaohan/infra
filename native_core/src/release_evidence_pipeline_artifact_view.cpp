#include "release_evidence_pipeline_artifact_view.hpp"

#include "release_evidence_pipeline_artifact_projection.hpp"
#include "release_evidence_pipeline_artifact_sections.hpp"

#include <sstream>

namespace inferlab_release_pipeline {

std::string render_release_evidence_pipeline_artifact(
    const ReleaseEvidencePipelineRequest& request) {
  const ReleasePipelineArtifactProjection projection(request);
  std::ostringstream out;
  detail::append_release_pipeline_header_and_summary_core(out, projection);
  detail::append_release_pipeline_summary_query_and_catalog(out, projection);
  detail::append_release_pipeline_summary_native_audit_and_close(out,
                                                                 projection);
  detail::append_release_pipeline_artifact_body(out, projection);
  return out.str();
}

}  // namespace inferlab_release_pipeline
