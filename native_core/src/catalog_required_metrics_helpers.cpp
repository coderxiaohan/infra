#include "catalog_required_metrics_internal.hpp"

#include "native_core_audit_field_catalog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inferlab_catalog::required_metrics_internal {
namespace {

void append_metric_name(std::vector<std::string>& metrics,
                        const std::string& name) {
  if (name.empty()) {
    throw std::logic_error("catalog required metric name must be nonempty");
  }
  if (std::find(metrics.begin(), metrics.end(), name) != metrics.end()) {
    throw std::logic_error("duplicate catalog required metric name: " + name);
  }
  metrics.push_back(name);
}

}  // namespace

void append_metric_names(std::vector<std::string>& metrics,
                         const std::initializer_list<const char*> names) {
  for (const char* name : names) {
    if (name == nullptr || name[0] == '\0') {
      throw std::logic_error("catalog required metric name must be nonempty");
    }
    append_metric_name(metrics, name);
  }
}

void append_native_core_audit_catalog_metrics(
    std::vector<std::string>& metrics, const std::string& prefix) {
  for (const auto& field : inferlab_evidence::kNativeCoreAuditSizeFields) {
    append_metric_name(metrics, prefix + field.direct_key);
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditBoolFields) {
    append_metric_name(metrics, prefix + field.direct_key);
  }
  for (const auto& field : inferlab_evidence::kNativeCoreAuditStringFields) {
    append_metric_name(metrics, prefix + field.direct_key);
  }
}

}  // namespace inferlab_catalog::required_metrics_internal
