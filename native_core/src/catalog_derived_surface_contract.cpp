#include "catalog_derived_views.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_catalog {
namespace {

struct SurfaceConsumerSpec {
  SurfaceConsumerStatus status;
  std::string consumer_kind;
  std::string catalog_view;
  std::vector<std::string> required_fields;
  bool consumes_catalog = false;
  bool fallback_retained = false;
};

std::string expected_consumer_status(const SurfaceConsumerSpec& spec) {
  if (spec.consumes_catalog) {
    return spec.fallback_retained ? "registry_consumed_with_fallback"
                                  : "registry_consumed";
  }
  if (spec.fallback_retained) {
    throw std::logic_error(
        "catalog consumer surface retains fallback without catalog consumption");
  }
  return "registry_not_consumed";
}

void validate_surface_specs(const std::vector<SurfaceConsumerSpec>& specs) {
  if (specs.empty()) {
    throw std::logic_error("catalog consumer surface registry is empty");
  }
  for (std::size_t i = 0; i < specs.size(); ++i) {
    const auto& spec = specs[i];
    if (spec.status.surface.empty() || spec.status.consumer_status.empty() ||
        spec.status.source_of_truth.empty() ||
        spec.status.migration_status.empty() ||
        spec.status.next_action.empty() || spec.consumer_kind.empty() ||
        spec.catalog_view.empty() || spec.required_fields.empty()) {
      throw std::logic_error(
          "catalog consumer surface contract contains an empty field");
    }
    if (spec.fallback_retained && spec.status.fallback_location.empty()) {
      throw std::logic_error(
          "catalog fallback surface must declare its source location");
    }
    if (spec.status.consumer_status != expected_consumer_status(spec)) {
      throw std::logic_error(
          "catalog consumer surface state disagrees with its migration contract");
    }
    for (std::size_t j = i + 1U; j < specs.size(); ++j) {
      if (spec.status.surface == specs[j].status.surface) {
        throw std::logic_error("duplicate catalog consumer surface: " +
                               spec.status.surface);
      }
    }
    for (std::size_t field_index = 0;
         field_index < spec.required_fields.size(); ++field_index) {
      if (spec.required_fields[field_index].empty()) {
        throw std::logic_error("catalog consumer surface has an empty field: " +
                               spec.status.surface);
      }
      for (std::size_t other = field_index + 1U;
           other < spec.required_fields.size(); ++other) {
        if (spec.required_fields[field_index] == spec.required_fields[other]) {
          throw std::logic_error(
              "duplicate required catalog field for surface: " +
              spec.status.surface);
        }
      }
    }
  }
}

const std::vector<SurfaceConsumerSpec>& surface_specs() {
  static const std::vector<SurfaceConsumerSpec> specs = [] {
    std::vector<SurfaceConsumerSpec> values = {
        {{"artifact_contract", "registry_consumed",
          "high_value_artifact_catalog.hpp",
          "",
          "registry entries are composed with canonical non-catalog contract extensions owned by artifact_contract_fixture.cpp",
          "keep non-catalog contracts in the validated extension registry and reject identity overlap with catalog entries"},
         "native_core",
         "contract_views",
         {"artifact", "producer_target", "required_fields",
          "summary_metrics", "claim_boundary", "requires_execute",
          "requires_hardware", "requires_product_replay"},
         true,
         false},
        {{"freshness_gate", "registry_consumed",
          "high_value_artifact_catalog.hpp",
          "",
          "catalog owns managed freshness targets; distinct runtime and product targets remain an unmanaged extension inventory",
          "register future high-value targets in catalog and reject catalog/unmanaged overlap"},
         "native_core",
         "freshness_views",
         {"artifact", "producer_target", "refresh_command",
          "freshness_policy", "requires_execute", "requires_hardware"},
         true,
         false},
        {{"evidence_bundle", "registry_consumed",
          "high_value_artifact_catalog.hpp",
          "",
          "registry entries are composed with canonical non-catalog bundle components owned by their domain registries",
          "keep domain-only components as typed extensions and reject identity overlap with catalog entries"},
         "native_core",
         "bundle_component_views",
         {"artifact", "producer_target", "benchmark_matrix_entry",
          "refresh_command", "report", "claim_boundary"},
         true,
         false},
        {{"summary", "registry_consumed",
          "high_value_artifact_catalog.hpp", "",
          "summary resolves catalog-managed artifacts and composes the canonical non-catalog evidence source extension inventory",
          "keep the shared extension inventory canonical and reject overlap or consumer-local copies"},
         "python_glue",
         "summary_source_views",
         {"artifact", "producer_target", "report", "summary_group",
          "evidence_level", "claim_boundary", "requires_execute",
          "requires_product_replay"},
         true,
         false},
        {{"prometheus", "registry_consumed",
          "high_value_artifact_catalog.hpp",
          "",
          "exporter resolves catalog-managed artifacts and composes the canonical non-catalog evidence source extension inventory",
          "keep the shared extension inventory canonical and reject overlap or exporter-local copies"},
         "python_glue",
         "prometheus_source_views",
         {"artifact", "prometheus_category", "evidence_level",
          "claim_boundary", "summary_metrics"},
         true,
         false},
        {{"final_report", "registry_consumed",
          "high_value_artifact_catalog.hpp", "",
          "final report resolves catalog-managed governance sources and composes its configured canonical module-result extensions",
          "keep configured module sources canonical and reject duplicate catalog ownership"},
         "python_glue",
         "final_report_source_views",
         {"artifact", "report", "final_report_group", "report_kind",
          "evidence_level", "claim_boundary"},
         true,
         false},
    };
    validate_surface_specs(values);
    return values;
  }();
  return specs;
}

const SurfaceConsumerSpec& require_surface_spec(const std::string& surface) {
  for (const auto& spec : surface_specs()) {
    if (spec.status.surface == surface) return spec;
  }
  throw std::logic_error("unknown catalog consumer surface: " + surface);
}

}  // namespace

std::vector<SurfaceConsumerStatus> surface_consumer_statuses() {
  std::vector<SurfaceConsumerStatus> statuses;
  statuses.reserve(surface_specs().size());
  for (const auto& spec : surface_specs()) statuses.push_back(spec.status);
  return statuses;
}

std::vector<std::string> consumer_required_fields(
    const std::string& surface) {
  return require_surface_spec(surface).required_fields;
}

std::string consumer_catalog_view(const std::string& surface) {
  return require_surface_spec(surface).catalog_view;
}

std::string consumer_kind_for_surface(const std::string& surface) {
  return require_surface_spec(surface).consumer_kind;
}

std::vector<ConsumerFieldCoverageView> consumer_field_coverage_views() {
  std::vector<ConsumerFieldCoverageView> views;
  views.reserve(surface_specs().size());
  for (const auto& spec : surface_specs()) {
    std::vector<std::string> covered;
    std::vector<std::string> missing;
    if (spec.consumes_catalog) {
      covered = spec.required_fields;
    } else {
      missing = spec.required_fields;
    }
    views.push_back(
        {spec.status.surface,
         spec.consumer_kind,
         spec.catalog_view,
         spec.status.source_of_truth,
         spec.status.fallback_location,
         spec.consumes_catalog,
         spec.fallback_retained,
         spec.consumes_catalog && spec.fallback_retained
             ? "catalog_fields_covered_with_fallback_pending_parity_refresh"
         : spec.consumes_catalog ? "catalog_fields_covered"
                                 : "catalog_field_consumption_missing",
         spec.required_fields,
         covered,
         missing,
         spec.status.next_action,
         spec.fallback_retained
             ? "refresh this surface and compare registry-derived fields against retained fallback output before fallback removal"
             : "refresh this surface when validation is re-enabled",
         "consumer field coverage is registry metadata only; it does not execute downstream refresh commands or prove artifact parity"});
  }
  return views;
}

}  // namespace inferlab_catalog
