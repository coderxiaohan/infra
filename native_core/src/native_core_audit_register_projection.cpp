#include "native_core_audit_register_projection.hpp"

#include "evidence_release_policy.hpp"
#include "native_core_audit_field_catalog.hpp"

#include <string>

namespace {

using NativeAuditSignals = inferlab_evidence::NativeCoreAuditSignals;

void read_native_audit_register_fields(const std::string& summary,
                                       NativeAuditSignals& signals) {
  for (const auto& field : inferlab_evidence::kNativeCoreAuditSizeFields) {
    signals.*(field.member) = inferlab_artifact::json_number_value_or_zero(
        summary, field.direct_key);
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditBoolFields) {
    signals.*(field.member) = inferlab_artifact::json_bool_value_or_false(
        summary, field.direct_key);
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditStringFields) {
    signals.*(field.member) = inferlab_artifact::extract_string_key_fallback(
        summary, field.direct_key);
  }
}

bool has_version_two_summary_contract(const std::string& summary) {
  return !summary.empty() &&
         inferlab_artifact::contains_key(summary, "audit_decision") &&
         inferlab_artifact::contains_key(summary, "in_scope_file_count") &&
         inferlab_artifact::contains_key(summary,
                                         "audit_review_coverage_state");
}

}  // namespace

namespace inferlab_evidence {

NativeCoreAuditRegisterProjection project_native_core_audit_register(
    const inferlab_artifact::Metadata& audit_meta,
    const inferlab_artifact::TextReadResult& audit_read) {
  const std::string document =
      audit_read.readable && audit_meta.parse_ok ? audit_read.text : "";
  const std::string summary =
      inferlab_artifact::json_object_section_or_empty(document, "summary");

  NativeCoreAuditRegisterProjection result;
  auto& signals = result.signals;
  signals.audit_register_present = audit_meta.present;
  signals.audit_register_readable = audit_read.readable;
  signals.audit_register_read_status = audit_read.status;
  signals.read_issue_count = has_read_issue(audit_read) ? 1U : 0U;
  signals.audit_summary_present =
      audit_meta.present && audit_meta.parse_ok &&
      has_version_two_summary_contract(summary);
  if (signals.audit_summary_present) {
    read_native_audit_register_fields(summary, signals);
  }
  normalize_native_core_audit_unknown_strings(signals);

  if (!signals.audit_register_present) {
    result.filter_decision = "native_core_audit_register_missing";
  } else if (signals.read_issue_count > 0U) {
    result.filter_decision = "native_core_audit_register_unreadable";
  } else if (!signals.audit_summary_present) {
    result.filter_decision = "native_core_audit_summary_missing";
  } else if (native_core_audit_scope_blocker_count(signals) > 0U) {
    result.filter_decision = "native_core_audit_scope_blocked";
  } else if (native_core_audit_core_blocker_count(signals) > 0U) {
    result.filter_decision = "native_core_audit_pending";
  } else {
    result.filter_decision = "native_core_audit_ready";
  }
  return result;
}

}  // namespace inferlab_evidence
