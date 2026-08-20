#include "native_core_audit_query_projection.hpp"

#include "evidence_release_policy.hpp"
#include "native_core_audit_field_catalog.hpp"

#include <algorithm>
#include <string>

namespace {

using inferlab_evidence::NativeCoreAuditProjectionSignals;

void read_query_fields(const std::string& summary,
                       NativeCoreAuditProjectionSignals& signals) {
  for (const auto& field : inferlab_evidence::kNativeCoreAuditSizeFields) {
    signals.*(field.member) = inferlab_artifact::json_number_value_or_zero(
        summary,
        inferlab_evidence::native_core_audit_query_key(field.direct_key));
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditBoolFields) {
    signals.*(field.member) = inferlab_artifact::json_bool_value_or_false(
        summary,
        inferlab_evidence::native_core_audit_query_key(field.direct_key));
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditStringFields) {
    signals.*(field.member) = inferlab_artifact::extract_string_key_fallback(
        summary,
        inferlab_evidence::native_core_audit_query_key(field.direct_key));
  }
}

}  // namespace

namespace inferlab_evidence {

NativeCoreAuditQueryProjection project_native_core_audit_query(
    const inferlab_artifact::Metadata& evidence_query_meta,
    const inferlab_artifact::TextReadResult& evidence_query_read) {
  const std::string document =
      evidence_query_read.readable && evidence_query_meta.parse_ok
          ? evidence_query_read.text
          : "";
  const std::string summary =
      inferlab_artifact::json_object_section_or_empty(document, "summary");
  const std::string filter = inferlab_artifact::json_object_section_or_empty(
      document, "native_core_audit_filter_query");

  NativeCoreAuditQueryProjection result;
  auto& signals = result.signals;
  signals.evidence_query_present = evidence_query_read.present;
  signals.evidence_query_readable = evidence_query_read.readable;
  signals.evidence_query_read_status = evidence_query_read.status;
  signals.read_issue_count = has_read_issue(evidence_query_read) ? 1U : 0U;
  signals.filter_query_present = evidence_query_meta.parse_ok && !filter.empty();
  signals.audit_register_present =
      signals.filter_query_present && inferlab_artifact::json_bool_value_or_false(
                                          filter, "audit_register_present");
  signals.audit_register_readable =
      signals.filter_query_present && inferlab_artifact::json_bool_value_or_false(
                                          filter, "audit_register_readable");
  signals.audit_register_read_status =
      signals.filter_query_present
          ? inferlab_artifact::extract_string_key_fallback(
                filter, "audit_register_read_status")
          : "missing";
  signals.audit_summary_present =
      signals.filter_query_present && inferlab_artifact::json_bool_value_or_false(
                                          filter, "audit_summary_present");
  if (!summary.empty()) {
    read_query_fields(summary, signals);
  }
  normalize_native_core_audit_unknown_strings(signals);
  result.blocker_count = inferlab_artifact::json_number_value_or_zero(
      summary, "native_core_audit_blocker_count");
  result.filter_decision = inferlab_artifact::extract_string_key_fallback(
      summary, "native_core_audit_filter_decision");
  return result;
}

std::size_t native_core_audit_query_consumer_blocker_count(
    const NativeCoreAuditQueryProjection& projection) {
  const auto& signals = projection.signals;
  const std::size_t query_contract_blocker =
      signals.read_issue_count > 0U || !signals.evidence_query_present ||
              !signals.filter_query_present
          ? 1U
          : 0U;
  return std::max(
      {projection.blocker_count, query_contract_blocker,
       native_core_audit_core_blocker_count(signals)});
}

std::string native_core_audit_query_consumer_filter_decision(
    const NativeCoreAuditQueryProjection& projection) {
  const auto& signals = projection.signals;
  if (signals.read_issue_count > 0U) {
    return "native_core_audit_evidence_query_unreadable";
  }
  if (!signals.evidence_query_present) {
    return "native_core_audit_evidence_query_missing";
  }
  if (!signals.filter_query_present) {
    return "native_core_audit_filter_missing";
  }
  if (!signals.audit_register_present) {
    return "native_core_audit_register_missing";
  }
  if (!signals.audit_summary_present) {
    return "native_core_audit_summary_missing";
  }
  if (!projection.filter_decision.empty()) {
    return projection.filter_decision;
  }
  return native_core_audit_query_consumer_blocker_count(projection) > 0U
             ? "native_core_audit_pending"
             : "native_core_audit_ready";
}

}  // namespace inferlab_evidence
