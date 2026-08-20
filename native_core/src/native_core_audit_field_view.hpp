#pragma once

#include "native_core_audit_field_catalog.hpp"
#include "plan_json_utils.hpp"

#include <initializer_list>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>

namespace inferlab_evidence {

inline void validate_native_core_audit_output_keys(
    const std::initializer_list<const char*> extra_keys,
    const std::string& context) {
  std::set<std::string> keys;
  const auto require_unique = [&keys, &context](const std::string& key) {
    if (key.empty() || !keys.insert(key).second) {
      throw std::logic_error(
          "duplicate or empty Native Audit output key in " + context +
          ": " + key);
    }
  };
  for (const char* key : extra_keys) {
    require_unique(key == nullptr ? std::string() : std::string(key));
  }
  for (const auto& field : kNativeCoreAuditSizeFields) {
    require_unique(native_core_audit_query_key(field.direct_key));
  }
  for (const auto& field : kNativeCoreAuditBoolFields) {
    require_unique(native_core_audit_query_key(field.direct_key));
  }
  for (const auto& field : kNativeCoreAuditStringFields) {
    require_unique(native_core_audit_query_key(field.direct_key));
  }
}

inline void write_native_core_audit_catalog_fields(
    std::ostream& out, const std::string& pad,
    const NativeCoreAuditSignals& signals) {
  for (const auto& field : kNativeCoreAuditSizeFields) {
    out << pad
        << inferlab_plan::json_string(
               native_core_audit_query_key(field.direct_key))
        << ": " << signals.*(field.member) << ",\n";
  }
  for (const auto& field : kNativeCoreAuditBoolFields) {
    out << pad
        << inferlab_plan::json_string(
               native_core_audit_query_key(field.direct_key))
        << ": " << inferlab_plan::json_bool(signals.*(field.member))
        << ",\n";
  }
  for (const auto& field : kNativeCoreAuditStringFields) {
    out << pad
        << inferlab_plan::json_string(
               native_core_audit_query_key(field.direct_key))
        << ": " << inferlab_plan::json_string(signals.*(field.member))
        << ",\n";
  }
}

inline void write_native_core_audit_direct_catalog_fields(
    std::ostream& out, const std::string& pad,
    const NativeCoreAuditSignals& signals) {
  for (const auto& field : kNativeCoreAuditSizeFields) {
    out << pad << inferlab_plan::json_string(field.direct_key) << ": "
        << signals.*(field.member) << ",\n";
  }
  for (const auto& field : kNativeCoreAuditBoolFields) {
    out << pad << inferlab_plan::json_string(field.direct_key) << ": "
        << inferlab_plan::json_bool(signals.*(field.member)) << ",\n";
  }
  for (const auto& field : kNativeCoreAuditStringFields) {
    out << pad << inferlab_plan::json_string(field.direct_key) << ": "
        << inferlab_plan::json_string(signals.*(field.member)) << ",\n";
  }
}

inline void write_native_core_audit_catalog_field(
    std::ostream& out, const std::string& pad,
    const NativeCoreAuditSignals& signals, const std::string& output_key) {
  if (output_key.empty()) {
    throw std::invalid_argument(
        "Native Audit catalog output key must not be empty");
  }
  for (const auto& field : kNativeCoreAuditSizeFields) {
    if (native_core_audit_query_key(field.direct_key) == output_key) {
      out << pad << inferlab_plan::json_string(output_key) << ": "
          << signals.*(field.member) << ",\n";
      return;
    }
  }
  for (const auto& field : kNativeCoreAuditBoolFields) {
    if (native_core_audit_query_key(field.direct_key) == output_key) {
      out << pad << inferlab_plan::json_string(output_key) << ": "
          << inferlab_plan::json_bool(signals.*(field.member)) << ",\n";
      return;
    }
  }
  for (const auto& field : kNativeCoreAuditStringFields) {
    if (native_core_audit_query_key(field.direct_key) == output_key) {
      out << pad << inferlab_plan::json_string(output_key) << ": "
          << inferlab_plan::json_string(signals.*(field.member)) << ",\n";
      return;
    }
  }
  throw std::logic_error("unknown Native Audit catalog output key: " +
                         output_key);
}

}  // namespace inferlab_evidence
