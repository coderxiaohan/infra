#pragma once

#include "evidence_query_record_policy.hpp"
#include "plan_json_utils.hpp"

#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline const std::vector<std::string>& known_blocker_categories() {
  static const std::vector<std::string> categories = {
      "artifact_missing",      "schema_missing",
      "claim_scope_missing",   "summary_missing",
      "artifact_blocked",      "upstream_blocker",
      "report_wiring_missing", "freshness_stale",
      "replay_missing",        "execute_missing",
  };
  return categories;
}

inline bool is_known_blocker_category(const std::string& value) {
  for (const auto& category : known_blocker_categories()) {
    if (category == value) return true;
  }
  return false;
}

inline void append_affected_claims(
    std::vector<std::string>& values,
    const inferlab_artifact::WarehouseArtifactRecord& record) {
  for (const auto& claim : affected_claims(record)) {
    inferlab_plan::append_unique_nonempty_string(values, claim);
  }
}

inline std::vector<std::string> wiring_locations_for_category(
    const std::string& category) {
  if (category == "schema_missing") {
    return {"configs/schemas/",
            "native_core/src/artifact_schema_registry_main.cpp",
            "native_core/src/high_value_artifact_catalog.hpp"};
  }
  if (category == "claim_scope_missing") {
    return {"native_core/src/high_value_artifact_catalog.hpp",
            "native_core/src/claim_scope_contract_main.cpp",
            "results/raw/native_claim_scope_contract.json"};
  }
  if (category == "summary_missing") {
    return {"scripts/summarize_benchmarks.py",
            "native_core/src/high_value_artifact_catalog.hpp",
            "README.md"};
  }
  if (category == "report_wiring_missing") {
    return {"scripts/summarize_benchmarks.py",
            "scripts/export_prometheus_metrics.py",
            "scripts/summarize_benchmarks.py", "README.md"};
  }
  if (category == "freshness_stale") {
    return {"native_core/src/runtime_evidence_freshness_gate_main.cpp",
            "configs/benchmark_matrix.yaml"};
  }
  if (category == "artifact_missing") {
    return {"configs/benchmark_matrix.yaml",
            "native_core/src/high_value_artifact_catalog.hpp"};
  }
  if (category == "replay_missing") {
    return {"native_core/src/product_replay_semantic_events_main.cpp",
            "native_core/src/product_replay_semantic_linkage_projection.cpp",
            "results/raw/product_replay_semantic_events.json"};
  }
  if (category == "execute_missing") {
    return {"native_core/src/kernel_microbench_harness_model.cpp",
            "native_core/src/kernel_microbench_harness_runner.cpp",
            "cuda_kernels/", "configs/benchmark_matrix.yaml"};
  }
  if (category == "artifact_blocked") {
    return {"results/raw/",
            "native_core/src/evidence_query_record_policy.hpp",
            "native_core/src/evidence_query_main.cpp"};
  }
  if (category == "upstream_blocker") {
    return {"results/raw/",
            "native_core/src/release_evidence_pipeline_main.cpp",
            "native_core/src/evidence_query_main.cpp"};
  }
  return {"native_core/src/evidence_query_repair_policy.hpp",
          "native_core/src/release_evidence_pipeline_main.cpp"};
}

inline bool category_has_exact_wiring_locations(
    const std::string& category) {
  return is_known_blocker_category(category);
}

}  // namespace inferlab_evidence_query
