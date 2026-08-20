#pragma once

#include "checked_index.hpp"
#include "native_core_audit_query_projection.hpp"
#include "native_core_audit_register_projection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct NativeCoreAuditMergedProjection {
  NativeCoreAuditProjectionSignals signals;
  std::size_t blocker_count = 0U;
  std::size_t deferred_file_count = 0U;
  std::string filter_decision;
};

inline NativeCoreAuditMergedProjection
project_native_core_audit_register_authority(
    const NativeCoreAuditRegisterProjection& direct) {
  NativeCoreAuditMergedProjection result;
  result.signals = direct.signals;
  result.deferred_file_count =
      direct.signals.deferred_with_reason_file_count;
  result.blocker_count = native_core_audit_core_blocker_count(result.signals);
  result.filter_decision =
      direct.filter_decision.empty()
          ? (result.blocker_count > 0U ? "native_core_audit_pending"
                                       : "native_core_audit_ready")
          : direct.filter_decision;
  return result;
}

inline NativeCoreAuditMergedProjection merge_native_core_audit_projections(
    const NativeCoreAuditQueryProjection& query,
    const NativeCoreAuditRegisterProjection& direct) {
  NativeCoreAuditMergedProjection result =
      project_native_core_audit_register_authority(direct);

  // The current register snapshot owns audit facts. Evidence Query contributes
  // only its own source/filter contract so a stale query cannot overwrite a
  // newer direct audit summary.
  result.signals.evidence_query_present = query.signals.evidence_query_present;
  result.signals.evidence_query_readable =
      query.signals.evidence_query_readable;
  result.signals.evidence_query_read_status =
      query.signals.evidence_query_read_status;
  result.signals.filter_query_present = query.signals.filter_query_present;
  result.signals.read_issue_count = inferlab::internal::checked_sum_size(
      {query.signals.read_issue_count, direct.signals.read_issue_count},
      "native audit merged source read-issue count");

  result.blocker_count = std::max(
      result.blocker_count,
      native_core_audit_query_consumer_blocker_count(query));
  if (query.signals.read_issue_count > 0U) {
    result.filter_decision = "native_core_audit_evidence_query_unreadable";
  } else if (!query.signals.evidence_query_present) {
    result.filter_decision = "native_core_audit_evidence_query_missing";
  } else if (!query.signals.filter_query_present) {
    result.filter_decision = "native_core_audit_filter_missing";
  } else if (!query.filter_decision.empty() &&
             query.filter_decision != "native_core_audit_ready") {
    result.filter_decision = query.filter_decision;
  } else if (result.blocker_count > 0U) {
    result.filter_decision = "native_core_audit_pending";
  } else {
    result.filter_decision = "native_core_audit_ready";
  }
  return result;
}

}  // namespace inferlab_evidence
