#include "release_evidence_pipeline_repair_source_policy.hpp"

#include "high_value_artifact_catalog.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

struct RepairSourceLocationRule {
  std::vector<std::string> categories;
  std::vector<std::string> locations;
};

void validate_repair_source_location_rules(
    const std::vector<RepairSourceLocationRule>& rules) {
  std::map<std::string, bool> category_owners;
  for (const auto& rule : rules) {
    if (rule.categories.empty() || rule.locations.empty()) {
      throw std::invalid_argument(
          "repair source-location rule must have categories and locations");
    }
    std::map<std::string, bool> location_ids;
    for (const auto& location : rule.locations) {
      if (location.empty()) {
        throw std::invalid_argument(
            "repair source-location path must not be empty");
      }
      if (!location_ids.emplace(location, true).second) {
        throw std::invalid_argument(
            "duplicate repair source-location path: " + location);
      }
    }
    for (const auto& category : rule.categories) {
      if (category.empty()) {
        throw std::invalid_argument(
            "repair source-location category must not be empty");
      }
      if (!category_owners.emplace(category, true).second) {
        throw std::invalid_argument(
            "duplicate repair source-location category owner: " + category);
      }
    }
  }
}

const std::vector<RepairSourceLocationRule>& repair_source_location_rules() {
  static const std::vector<RepairSourceLocationRule> rules = [] {
    std::vector<RepairSourceLocationRule> result = {
        {{"catalog_flow_discrepancy"},
         {"native_core/src/artifact_catalog_single_source_main.cpp",
          "native_core/src/evidence_query_source_projection.cpp",
          "native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp",
          "results/raw/native_evidence_query_result.json"}},
        {{"catalog_flow_field_coverage_gap"},
         {"native_core/src/high_value_artifact_catalog.hpp",
          "native_core/src/catalog_derived_views.hpp",
          "native_core/src/evidence_query_catalog_filter_view.cpp",
          "native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp"}},
        {{"catalog_flow_fallback_removal_shortest_path",
          "catalog_flow_fallback_removal_parity_gate"},
         {"native_core/src/artifact_catalog_single_source_model.cpp",
          "native_core/src/evidence_query_catalog_filter_view.cpp",
          "native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp",
          "results/raw/native_release_evidence_pipeline.json"}},
        {{"product_replay_semantic_linkage", "product_replay_family_gate"},
         {"native_core/src/product_replay_semantic_events_model.cpp",
          "native_core/src/product_replay_alignment_model.cpp",
          "native_core/src/evidence_query_cross_artifact.hpp",
          "native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp"}},
        {{"evidence_query_snapshot_diff_review"},
         {"native_core/src/evidence_query_snapshot_diff.hpp",
          "native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp",
          "results/raw/native_evidence_query_result.json"}},
        {{"native_core_audit_backlog", "native_core_audit_review_coverage_gate",
          "native_core_audit_review_manifest", "native_core_audit_scope_blocked"},
         {"configs/native_core_audit_review_manifest.json",
          "native_core/src/native_core_audit_register_projection.cpp",
          "native_core/src/evidence_query_source_projection.cpp",
          "native_core/src/release_evidence_pipeline_repair_native_audit_producer.cpp"}},
    };
    validate_repair_source_location_rules(result);
    return result;
  }();
  return rules;
}

}  // namespace

std::vector<std::string> repair_path_source_locations_for(
    const std::string& category, const std::string& source) {
  for (const auto& rule : repair_source_location_rules()) {
    for (const auto& owned_category : rule.categories) {
      if (owned_category == category) return rule.locations;
    }
  }
  if (source == inferlab_high_value::require_managed_artifact(
                    "release_evidence_pipeline")
                    .artifact) {
    return {"native_core/src/release_evidence_pipeline_main.cpp",
            "results/raw/native_release_evidence_pipeline.json"};
  }
  return {"native_core/src/release_evidence_pipeline_main.cpp"};
}

}  // namespace inferlab_release_pipeline
