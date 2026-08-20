#include "catalog_derived_views.hpp"
#include "catalog_required_metrics_internal.hpp"
#include "deep_dive_core_artifact_registry.hpp"
#include "plan_json_utils.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_catalog {

using inferlab_plan::contains;
using required_metrics_internal::append_artifact_catalog_single_source_metrics;
using required_metrics_internal::append_artifact_schema_registry_metrics;
using required_metrics_internal::append_evidence_query_result_metrics;
using required_metrics_internal::append_native_core_audit_register_metrics;
using required_metrics_internal::append_query_family_metrics;
using required_metrics_internal::append_release_evidence_pipeline_metrics;

std::vector<std::string> registry_required_metrics(
    const inferlab_high_value::ManagedArtifact& artifact) {
  if (!artifact.required_metrics.empty()) {
    return artifact.required_metrics;
  }
  for (const auto& spec : inferlab_deep_dive_core::artifact_specs()) {
    if (artifact.id == spec.id) return spec.required_metrics;
  }
  if (artifact.id == "evidence_query_selector_repair_card_export") {
    return {"selector_repair_card_full_export_count",
            "selector_repair_card_export_review_status"};
  }
  std::vector<std::string> metrics;
  const auto append_metrics = [&](const std::initializer_list<const char*> names) {
    for (const char* name : names) {
      inferlab_plan::append_unique_string(metrics, name);
    }
  };
  if (artifact.id == "evidence_warehouse_index") {
    append_metrics({"artifact_count", "lineage_score",
                    "warehouse_decision"});
  }
  if (artifact.id == "kernel_microbench_harness") {
    append_metrics({"operator_count", "microbench_decision",
                    "evidence_confidence"});
  }
  if (artifact.id == "third_party_dependency_gate") {
    append_metrics({"dependency_count", "dependency_gate_decision",
                    "evidence_confidence"});
  }
  if (artifact.id == "dependency_sbom_gate") {
    append_metrics({"dependency_count", "sbom_gate_decision",
                    "evidence_confidence"});
  }
  if (contains(artifact.id, "release")) {
    metrics.push_back("release_decision");
  }
  if (artifact.id == "artifact_catalog_single_source") {
    append_artifact_catalog_single_source_metrics(metrics);
  }
  if (artifact.id == "release_evidence_pipeline") {
    append_release_evidence_pipeline_metrics(metrics);
  }
  if (artifact.id == "evidence_query_result") {
    append_query_family_metrics(metrics);
  }
  if (artifact.id == "artifact_schema_registry") {
    append_artifact_schema_registry_metrics(metrics);
  }
  if (artifact.id == "evidence_query_result") {
    append_evidence_query_result_metrics(metrics);
  }
  if (artifact.id == "native_core_audit_register") {
    append_native_core_audit_register_metrics(metrics);
  }
  if (artifact.id == "gateway_policy_matrix_plan") {
    for (const auto* metric : {"lowest_p99_risk_score", "scenario_count",
                               "active_scenario_count"}) {
      inferlab_plan::append_unique_string(metrics, metric);
    }
  }
  if (artifact.id == "replica_placement_plan") {
    for (const auto* metric : {"best_placement_score", "riskiest_p99_score",
                               "scenario_count"}) {
      inferlab_plan::append_unique_string(metrics, metric);
    }
  }
  if (artifact.id == "advanced_kv_router") {
    const auto routing_metrics =
        inferlab_deep_dive_core::required_metrics_with_native_trace(
            {"policy", "cache_hit", "prefix_hit_rate", "reused_tokens",
             "recomputed_tokens", "worker_load_after",
             "trace_record_key"});
    for (const auto& metric : routing_metrics) {
      inferlab_plan::append_unique_string(metrics, metric);
    }
  }
  if (metrics.empty()) {
    throw std::logic_error("managed artifact has no required metric contract: " +
                           artifact.id);
  }
  return metrics;
}

}  // namespace inferlab_catalog
