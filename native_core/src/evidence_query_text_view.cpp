#include "evidence_query_text_view.hpp"

namespace inferlab_evidence_query {

std::string render_evidence_query_text(const EvidenceQueryRequest& request) {
  return detail::evidence_query_text_overview(request) +
         detail::evidence_query_text_filter_status(request) +
         detail::evidence_query_text_native_audit_and_snapshot(request) +
         detail::evidence_query_text_repair_and_results(request);
}

}  // namespace inferlab_evidence_query
