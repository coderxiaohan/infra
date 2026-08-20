#pragma once

#include "deep_dive_core_artifact_registry.hpp"
#include "evidence_query_cli_contract.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_high_value {

struct ManagedArtifact {
  std::string id;
  std::string artifact;
  std::string target;
  std::string report;
  std::string evidence_level;
  std::string command;
  std::vector<std::string> required_fields;
  std::string claim_boundary;
  std::string freshness_policy;
  std::string summary_group;
  std::string final_report_group;
  std::string prometheus_category;
  std::string report_kind;
  bool requires_execute = false;
  bool requires_hardware = false;
  bool requires_product_replay = false;
  std::vector<std::string> required_metrics;
};

struct DependencySpec {
  std::string name;
  std::string pinned_version;
  std::string source;
  std::string license;
  std::string cmake_option;
  bool required = false;
  bool optional = true;
  std::vector<std::string> used_by;
  std::string engineering_value;
  std::string risk_level;
  std::string fallback;
  std::string claim_impact;
  std::string current_server_action;
};

inline void validate_dependency_spec_registry(
    const std::vector<DependencySpec>& dependencies) {
  if (dependencies.empty()) {
    throw std::logic_error("dependency registry must not be empty");
  }
  for (std::size_t index = 0; index < dependencies.size(); ++index) {
    const auto& dependency = dependencies[index];
    if (dependency.name.empty()) {
      throw std::logic_error("dependency registry name must not be empty");
    }
    if (dependency.required == dependency.optional) {
      throw std::logic_error(
          "dependency registry required/optional flags must be exclusive: " +
          dependency.name);
    }
    if (dependency.risk_level != "low" &&
        dependency.risk_level != "medium" &&
        dependency.risk_level != "high") {
      throw std::logic_error("unknown dependency risk level for " +
                             dependency.name + ": " +
                             dependency.risk_level);
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (dependencies[prior].name == dependency.name) {
        throw std::logic_error("duplicate dependency registry name: " +
                               dependency.name);
      }
    }
    for (std::size_t used_by_index = 0;
         used_by_index < dependency.used_by.size(); ++used_by_index) {
      const auto& target = dependency.used_by[used_by_index];
      for (std::size_t prior = 0; prior < used_by_index; ++prior) {
        if (!target.empty() && dependency.used_by[prior] == target) {
          throw std::logic_error("duplicate dependency used-by identity for " +
                                 dependency.name + ": " + target);
        }
      }
    }
  }
}

struct MicrobenchOperator {
  std::string name;
  std::string operator_family;
  std::string shape_signature;
  std::string dtype_layout;
  std::string backend_candidates;
  std::string correctness_oracle;
  std::string benchmark_case;
  std::string nvtx_range;
  std::string nsight_systems_command;
  std::string nsight_compute_command;
};

inline void validate_microbench_operator_registry(
    const std::vector<MicrobenchOperator>& operators) {
  if (operators.empty()) {
    throw std::invalid_argument(
        "microbenchmark operator registry must not be empty");
  }
  for (std::size_t index = 0; index < operators.size(); ++index) {
    const auto& current = operators[index];
    if (current.name.empty() || current.operator_family.empty() ||
        current.shape_signature.empty() ||
        current.dtype_layout.empty() ||
        current.backend_candidates.empty() ||
        current.correctness_oracle.empty() ||
        current.benchmark_case.empty() || current.nvtx_range.empty() ||
        current.nsight_systems_command.empty() ||
        current.nsight_compute_command.empty()) {
      throw std::invalid_argument(
          "microbenchmark operator registry fields must not be empty");
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      const auto& existing = operators[prior];
      if (existing.name == current.name) {
        throw std::invalid_argument(
            "microbenchmark operator names must be unique");
      }
      if (existing.benchmark_case == current.benchmark_case) {
        throw std::invalid_argument(
            "microbenchmark benchmark cases must be unique");
      }
      if (existing.nvtx_range == current.nvtx_range) {
        throw std::invalid_argument(
            "microbenchmark NVTX ranges must be unique");
      }
    }
  }
}

struct CorrelationRule {
  std::string product_event;
  std::string product_source;
  std::string native_artifact;
  std::string native_signal;
  std::string claim_boundary;
};

inline void validate_correlation_rule_registry(
    const std::vector<CorrelationRule>& rules) {
  if (rules.empty()) {
    throw std::logic_error(
        "product replay correlation rule registry must not be empty");
  }
  const auto valid_artifact_filename = [](const std::string& value) {
    return value.size() > 5U &&
           value.compare(value.size() - 5U, 5U, ".json") == 0 &&
           value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos &&
           value.find("..") == std::string::npos;
  };
  for (std::size_t index = 0; index < rules.size(); ++index) {
    const auto& rule = rules[index];
    if (rule.product_event.empty() ||
        !valid_artifact_filename(rule.product_source) ||
        !valid_artifact_filename(rule.native_artifact) ||
        rule.native_signal.empty() || rule.claim_boundary.empty()) {
      throw std::logic_error(
          "product replay correlation rule fields are invalid");
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (rules[prior].product_event == rule.product_event &&
          rules[prior].native_artifact == rule.native_artifact) {
        throw std::logic_error(
            "duplicate product replay event/native correlation rule: " +
            rule.product_event + " -> " + rule.native_artifact);
      }
    }
  }
}

inline const std::vector<ManagedArtifact>& managed_artifacts() {
  static const std::vector<ManagedArtifact> artifacts = [] {
    std::vector<ManagedArtifact> values = {
      {"advanced_kv_router",
       "native_advanced_kv_router.json",
       "inferlab_advanced_kv_router",
       "reports/kv_aware_router.md",
       "advanced_serving_dry_run",
       "./build/inferlab_advanced_kv_router --repo-root . --output results/raw/native_advanced_kv_router.json",
       {"artifact_type", "schema", "generated_at", "claim_scope_detail",
        "environment", "official_sources", "refresh_commands",
        "migration_commands", "status", "mode", "executed", "replayed",
        "execution_kind", "claim_scope", "summary", "workload", "records",
        "decisions", "privacy_contract", "native_trace_context",
        "catalog_evidence_contract", "blockers", "not_run",
        "recommended_next_commands"},
       "Request-level KV-aware routing artifact is a compiled deterministic local simulation with an optional explicitly supplied trace projection. It anonymizes request and prefix identity and does not prove live routing, observed timestamps/cache hits/TTFT, production KV locality, control-plane correctness, mature-product support, execute, benchmark, or release evidence.",
       "refresh after the routing model, compiled workload, trace sidecar contract, or target product evidence changes",
       "advanced_serving",
       "advanced_serving",
       "advanced_serving",
       "native_mechanism_plan",
       false,
       false,
       false},
      {"evidence_warehouse_index",
       "native_evidence_warehouse_index.json",
       "inferlab_evidence_warehouse_index",
       "reports/96_evidence_warehouse_lineage.md",
       "evidence_warehouse_lineage",
       "./build/inferlab_evidence_warehouse_index --repo-root . --results results/raw --output results/raw/native_evidence_warehouse_index.json --format json",
       {"schema_version", "schema", "artifact_type", "mode", "executed",
        "replayed", "status", "generated_at", "claim_scope",
        "claim_scope_detail", "environment", "official_sources",
        "refresh_commands", "migration_commands", "not_run",
        "cli_context", "dependency_runtime", "summary", "source_files",
        "raw_scan", "artifacts", "scanned_raw_artifacts",
        "lineage_edges", "blockers", "pending_artifact_refresh_commands",
        "recommended_next_commands"},
       "Evidence warehouse output is a read-only catalog/repository lineage index. It does not execute artifact producers, prove current artifact semantics or freshness, run SQLite queries, import product replay, establish release readiness, or create benchmark/performance evidence."},
      {"kernel_microbench_harness",
       "native_kernel_microbench_harness.json",
       "inferlab_kernel_microbench_harness",
       "reports/98_kernel_microbench_harness.md",
       "kernel_microbenchmark_harness",
       "./build/inferlab_kernel_microbench_harness --repo-root . --output results/raw/native_kernel_microbench_harness.json --format json",
       {"schema_version", "schema", "artifact_type", "mode", "executed",
        "replayed", "status", "target_gpu_execute_verified", "generated_at",
        "claim_scope",
        "claim_scope_detail", "environment", "official_sources",
        "refresh_commands", "migration_commands", "cli_context",
        "dependency_runtime", "summary", "target_execution_contract", "operators",
        "benchmark_cases", "profiling_commands", "correctness_contracts",
        "input_generators", "reference_outputs", "tolerance_policies",
        "failure_classification", "shape_matrix",
        "target_machine_execution_matrix", "correctness_results",
        "timing_results", "profile_metadata", "blockers", "not_run",
        "recommended_next_commands"},
       "Kernel microbenchmark dry-run output is a typed execution manifest only. Execute evidence is limited to explicitly selected InferLab CPU-reference or CUDA-runtime cases and cannot establish cross-product speedup, production SLO, release readiness, or unselected device/workload behavior."},
      {"third_party_dependency_gate",
       "native_third_party_dependency_gate.json",
       "inferlab_third_party_dependency_gate",
       "reports/99_third_party_dependency_governance.md",
       "dependency_governance_gate",
       "./build/inferlab_third_party_dependency_gate --repo-root . --output results/raw/native_third_party_dependency_gate.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
        "summary", "dependencies", "dependency_to_target_map",
        "dependency_to_claim_map", "license_review", "fallback_matrix",
        "blockers",
        "recommended_next_commands"},
       "Dependency governance policy artifact only; it does not prove dependency installation, license approval, build success, runtime correctness, product readiness, or performance."},
      {"release_evidence_pipeline",
       "native_release_evidence_pipeline.json",
       "inferlab_release_evidence_pipeline",
       "reports/100_release_evidence_pipeline.md",
       "release_evidence_pipeline",
       "./build/inferlab_release_evidence_pipeline --repo-root . --results results/raw --output results/raw/native_release_evidence_pipeline.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
        "cli_context", "summary", "input_artifacts", "pipeline_stages",
        "release_dimensions", "blockers", "repair_plan", "repair_order",
        "blocked_by_environment", "blocked_by_missing_wiring",
        "blocked_by_missing_replay", "blocked_by_missing_execute",
        "shortest_path_to_report_ready", "shortest_path_to_replay_ready",
        "shortest_path_to_execute_ready", "non_gpu_review_closure",
        "evidence_query_cross_artifact_product_replay_family_present",
        "evidence_query_cross_artifact_product_replay_family_status",
        "evidence_query_cross_artifact_product_replay_family_blocker_count",
        "evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count",
        "evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count",
        "evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count",
        "evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count",
        "evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count",
        "evidence_query_cross_artifact_product_replay_family_source_artifact_count",
        "evidence_query_cross_artifact_product_replay_family_first_source_artifact",
        "evidence_query_cross_artifact_product_replay_family_next_action",
        "product_replay_semantic_linkage_repair_path_count",
        "product_replay_semantic_linkage_repair_path_source_location_gap_count",
        "evidence_query_product_replay_semantic_linkage_repair_path_count",
        "evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count",
        "recommended_next_commands"}},
      {"evidence_query_result",
       "native_evidence_query_result.json",
       "inferlab_evidence_query",
       "reports/101_evidence_query.md",
       "evidence_query",
       "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query confidence_summary --output results/raw/native_evidence_query_result.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
       "summary", "query", "query_plan", "warehouse_query", "results",
        "blockers",
        "operation_descriptor", "operation_catalog", "selector_resolution",
        "selector_repair_card_export",
        "cross_artifact_blocker_explanation",
        "product_replay_semantic_linkage",
        "native_trace_correlation",
        "selected_operation_result", "top_blockers", "repair_plan",
        "repair_path_query",
        "catalog_flow_filter_query",
        "native_core_audit_filter_query",
        "environment_split", "query_ux", "explain_tree", "repair_cards",
        "root_cause_repair_cards",
        "repair_card_group_plan", "repair_group_summary",
        "repair_group_navigation", "source_location_repair_worklist",
        "repair_card_path_ranking", "repair_card_rank_readiness_linkage",
        "repair_card_source_exactness",
        "source_location_category_coverage",
        "affected_claims",
        "exact_wiring_locations",
        "current_repo_actions", "current_server_actions",
        "target_machine_actions", "target_product_actions",
        "shortest_path_to_report_ready", "shortest_path_to_replay_ready",
        "shortest_path_to_execute_ready", "snapshot_diff", "confidence_level",
        "explain_repair_linkage", "human_readable_summary",
        "recommended_next_commands"}},
      {"evidence_query_selector_repair_card_export",
       "native_evidence_query_selector_repair_card_export.json",
       inferlab_evidence_query_cli::kCanonicalTarget,
       "reports/101_evidence_query.md",
       "evidence_query_selector_repair_card_export",
       inferlab_evidence_query_cli::selector_repair_card_export_command(),
       {"schema_version", "artifact_type", "mode", "executed", "replayed",
        "claim_scope", "query", "selector_resolution_summary",
        "export_contract", "export_review_focus", "repair_cards", "summary",
        "claim_boundary"}},
      {"dependency_sbom_gate",
       "native_dependency_sbom_gate.json",
       "inferlab_dependency_sbom_gate",
       "reports/102_dependency_sbom_gate.md",
       "dependency_sbom_gate",
       "./build/inferlab_dependency_sbom_gate --repo-root . --output results/raw/native_dependency_sbom_gate.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
        "summary", "dependencies", "dependency_to_target_map",
        "dependency_to_claim_map", "license_review", "fallback_matrix",
        "blockers", "recommended_next_commands"},
       "SBOM-style dependency inventory only; it does not prove dependency installation, production license approval, CUDA availability, product readiness, runtime correctness, or performance."},
      {"artifact_schema_registry",
       "native_artifact_schema_registry.json",
       "inferlab_artifact_schema_registry",
       "reports/104_artifact_schema_registry.md",
       "artifact_schema_registry",
       "./build/inferlab_artifact_schema_registry --repo-root . --output results/raw/native_artifact_schema_registry.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
        "summary", "registry_files", "artifact_schemas",
        "confidence_levels", "claim_boundary_rules",
        "release_decision_rules", "recommended_next_commands"}},
      {"artifact_catalog_single_source",
       "native_artifact_catalog_single_source.json",
       "inferlab_artifact_catalog_single_source",
       "reports/105_artifact_catalog_single_source.md",
       "artifact_catalog_single_source",
       "./build/inferlab_artifact_catalog_single_source --repo-root . --output results/raw/native_artifact_catalog_single_source.json --format json",
       {"schema_version", "status", "artifact_type", "mode", "executed",
        "replayed", "claim_scope", "cli_context", "summary", "artifacts",
        "artifact_entry_required_fields", "registry_sources",
        "derived_contract_entries", "derived_freshness_entries",
        "derived_summary_sources", "derived_prometheus_sources",
        "derived_final_report_sources",
        "derived_evidence_bundle_components",
        "hardcoded_duplicate_locations", "consumer_statuses",
        "surface_migration_readiness", "surface_consumer_migration_plan",
        "fallback_removal_queue", "fallback_removal_shortest_path",
        "fallback_removal_parity_gate",
        "consumer_field_coverage_summary", "consumer_field_coverage",
        "downstream_wiring_matrix", "catalog_drift_review",
        "single_source_parity_review", "single_source_repair_cards",
        "metadata_completeness_review", "metadata_policy_cards",
        "migration_readiness",
        "wiring_policies",
        "claim_boundary_rules", "freshness_policies", "report_kind_rules",
        "generated_surfaces", "migration_plan", "blockers",
        "recommended_next_commands"}},
      {"gateway_policy_matrix_plan",
       "native_gateway_policy_matrix_plan.json",
       "inferlab_gateway_policy_matrix_plan",
       "reports/46_gateway_policy_matrix.md",
       "control_plane_dry_run",
       "./build/inferlab_gateway_policy_matrix_plan --repo-root . --output results/raw/native_gateway_policy_matrix_plan.json",
       {"schema_version", "status", "artifact_type", "mode", "executed",
        "replayed", "claim_scope", "summary", "records", "schema",
        "generated_at", "environment", "official_sources",
        "claim_scope_detail", "refresh_commands", "migration_commands",
        "not_run", "recommended_next_commands", "research_basis",
        "catalog_evidence_contract"},
       "Compiled six-policy gateway routing analytical fixture only; not live gateway enforcement, request-trace simulation, imported product replay, observed TTFT/backend support, measured routing quality/performance, benchmark, or release readiness.",
       "refresh after the compiled gateway scenario or policy registry changes",
       "advanced_serving",
       "advanced_serving",
       "advanced_serving",
       "native_mechanism_plan"},
      {"replica_placement_plan",
       "native_replica_placement_plan.json",
       "inferlab_replica_placement_plan",
       "reports/47_replica_placement.md",
       "control_plane_dry_run",
       "./build/inferlab_replica_placement_plan --repo-root . --output results/raw/native_replica_placement_plan.json",
       {"schema_version", "status", "artifact_type", "mode", "executed",
        "replayed", "claim_scope", "summary", "records", "schema",
        "generated_at", "environment", "official_sources",
        "claim_scope_detail", "refresh_commands", "migration_commands",
        "not_run", "recommended_next_commands", "research_basis",
        "catalog_evidence_contract"},
       "Compiled analytical replica-placement fixture only; not live scheduler placement, cache residency, gateway routing, Kubernetes mutation, observed P99, product replay/execute, benchmark performance, release readiness, or mature-product core implementation.",
       "refresh after the compiled replica-placement fixture or policy changes",
       "advanced_serving",
       "advanced_serving",
       "advanced_serving",
       "native_mechanism_plan"},
      {"native_core_audit_register",
       "native_core_audit_register.json",
       "inferlab_native_core_audit_register",
       "reports/111_native_core_audit_register.md",
       "native_core_audit_register",
       "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json",
       {"artifact_type", "mode", "executed", "replayed", "claim_scope",
        "summary", "audit_scope", "source_roots",
        "review_manifest", "files",
        "recommended_next_commands", "not_run"}},
      {"edge_runtime_matrix",
       "native_edge_runtime_matrix.json",
       "inferlab_edge_runtime_matrix",
       "reports/112_edge_runtime_matrix.md",
       "edge_runtime_matrix",
       "./build/inferlab_edge_runtime_matrix --repo-root . --config configs/edge_runtime_matrix.example.json --mode plan --output results/raw/native_edge_runtime_matrix.json --format json",
       {"schema_version", "artifact_type", "status", "mode", "executed",
        "replayed", "claim_scope", "generated_at", "target_platform",
        "build_environment",
        "config", "isa_capabilities", "kernel_capabilities",
        "dependency_status", "runtime_rows", "comparison_identity",
        "deployment_recommendation", "delivery_contract", "summary", "blockers",
        "recommended_next_commands"},
       "C++ edge runtime lifecycle contract, scalar/ARM-NEON capability, checked resource planning, execution-window model/tokenizer metadata stability, bounded generated-text comparison, optional whole-system Linux powercap lifecycle energy delta and execute-gated performance candidate only; plan, blocked, input-drifted, partial, empty-text or null rows cannot form a quality comparison or select a runtime, metadata stability is not a content hash, mismatch offset exposes position but not generated text, a single-request text match does not prove model quality or token/logit/quantization equivalence, and powercap energy is not process/model-exclusive energy, instantaneous power, battery, production fitness or release readiness.",
       "refresh after edge kernel, runtime adapter, model identity, dependency or target-machine evidence changes",
       "edge_inference",
       "edge_inference",
       "edge_runtime",
       "deep_dive_implementation_report",
       false,
       false,
       false,
       {"runtime_row_count", "dependency_linked_count",
        "adapter_implemented_count", "model_present_count",
        "executed_row_count", "blocker_count", "evidence_level",
        "decision"}},
      {"edge_cloud_routing",
       "native_edge_cloud_routing.json",
       "inferlab_edge_cloud_routing",
       "reports/113_edge_cloud_routing.md",
       "edge_cloud_routing",
       "./build/inferlab_edge_cloud_routing --repo-root . --config configs/edge_cloud_routing.example.json --mode plan --output results/raw/native_edge_cloud_routing.json --format json",
       {"schema_version", "artifact_type", "status", "mode", "executed",
        "replayed", "claim_scope", "generated_at", "config",
        "dependency_status", "policy_input", "placement_decision",
        "local_load", "execution", "stream_accounting",
        "security_controls", "summary", "blockers",
        "recommended_next_commands", "not_run"},
       "C++17 request-before-execution edge/cloud placement, caller-observed network-quality gating, caller-estimated battery/local-energy/cloud-cost gating, caller-reconstructed durable-transcript pre-generation session recovery admission and optional OpenAI-compatible streaming transport only. Device-only data can never leave the device and local-to-cloud fallback is legal only before any output has been delivered, for retryable local failures, with remaining deadline and fresh policy-eligible network, budget and recovery evidence. Session recovery only continues after a completed turn or restarts an incomplete zero-output request; it never resumes partial output or restores KV/hidden state. Configured observations and estimates are not active probes, device-exclusive energy measurements or cloud billing records. Plan/config readiness and dependency linkage are not execute, product performance, model equivalence, cost accounting, mid-generation migration, KV transfer or release evidence.",
       "refresh after edge runtime, cloud transport, privacy/deadline/budget/session policy, endpoint capability or target-machine evidence changes",
       "edge_inference",
       "edge_inference",
       "edge_cloud_routing",
       "deep_dive_implementation_report",
       false,
       false,
       false,
       {"local_decision_count", "cloud_decision_count",
        "local_then_cloud_decision_count", "reject_decision_count",
        "attempt_count", "local_attempt_count", "cloud_attempt_count",
        "successful_attempt_count", "failed_attempt_count",
        "cancelled_attempt_count", "delivered_chunk_count",
        "pre_output_fallback_count", "privacy_blocker_count",
        "network_blocker_count", "resource_blocker_count",
        "dependency_blocker_count", "network_quality_issue_count",
        "budget_issue_count", "local_budget_ineligible_count",
        "cloud_budget_ineligible_count",
        "session_recovery_issue_count", "session_recovery_ready_count",
        "session_recovery_blocked_count",
        "fallback_attempted",
        "fallback_performed", "blocker_count", "evidence_level",
        "decision"}},
    };
    for (const auto& spec : inferlab_deep_dive_core::artifact_specs()) {
      values.push_back(
          {spec.id,
           spec.artifact,
           spec.target,
           spec.report_path,
           spec.expected_kind,
           spec.command,
           spec.required_fields,
           spec.claim_boundary,
           "refresh after the compiled mechanism slice, trace contract, or target evidence changes",
           "native_deep_dive",
           "native_deep_dive",
           "native_deep_dive",
           "deep_dive_implementation_report",
           false,
           false,
           false});
    }
    return values;
  }();
  return artifacts;
}

inline void validate_managed_artifact_registry();

inline const ManagedArtifact& require_managed_artifact(
    const std::string& id) {
  validate_managed_artifact_registry();
  for (const auto& artifact : managed_artifacts()) {
    if (artifact.id == id) return artifact;
  }
  throw std::invalid_argument("unknown managed artifact id: " + id);
}

inline void validate_managed_artifact_registry() {
  static const bool validated = [] {
    const auto& artifacts = managed_artifacts();
    for (std::size_t i = 0; i < artifacts.size(); ++i) {
      const auto& artifact = artifacts[i];
      if (artifact.id.empty() || artifact.artifact.empty() ||
          artifact.target.empty() || artifact.report.empty() ||
          artifact.command.empty()) {
        throw std::logic_error(
            "managed artifact identity and wiring fields must be nonempty");
      }
      for (std::size_t j = i + 1U; j < artifacts.size(); ++j) {
        if (artifact.id == artifacts[j].id) {
          throw std::logic_error("duplicate managed artifact id: " +
                                 artifact.id);
        }
        if (artifact.artifact == artifacts[j].artifact) {
          throw std::logic_error("duplicate managed artifact filename: " +
                                 artifact.artifact);
        }
      }
      for (std::size_t metric_index = 0U;
           metric_index < artifact.required_metrics.size(); ++metric_index) {
        if (artifact.required_metrics[metric_index].empty()) {
          throw std::logic_error(
              "managed artifact required metric must not be empty: " +
              artifact.id);
        }
        for (std::size_t prior = 0U; prior < metric_index; ++prior) {
          if (artifact.required_metrics[prior] ==
              artifact.required_metrics[metric_index]) {
            throw std::logic_error(
                "duplicate managed artifact required metric: " +
                artifact.id + ":" + artifact.required_metrics[metric_index]);
          }
        }
      }
    }
    return true;
  }();
  (void)validated;
}

inline const std::vector<DependencySpec>& dependency_specs() {
  static const std::vector<DependencySpec> dependencies = {
      {"nlohmann/json",
       "v3.11.3",
       "https://github.com/nlohmann/json",
       "MIT",
       "INFERLAB_ENABLE_THIRD_PARTY_JSON",
       false,
       true,
       {"inferlab_native_core"},
       "Process-consistent structured JSON parsing for shared inline artifact "
       "and runtime readers",
       "low",
       "shared bounded fallback parser with strict artifact contracts",
       "Improves parser coverage and diagnostics; does not create performance "
       "or execute claims",
       "resolved only during configure; current slimming worktree is not validated"},
      {"libcurl",
       ">=8.5 system package; record curl_version() at target configure/runtime",
       "https://curl.se/libcurl/",
       "curl license (MIT/X inspired)",
       "INFERLAB_ENABLE_EDGE_CLOUD_CURL",
       false,
       true,
       {"edge_cloud_routing", "OpenAI-compatible SSE transport"},
       "Bounded HTTPS streaming transport with TLS peer verification, redirect denial, cooperative cancellation and explicit loopback-only HTTP development policy",
       "medium",
       "capability-only edge/cloud placement artifact without cloud execute",
       "Transport linkage enables cloud execution only; it does not create performance, product availability, cost or release claims",
       "disabled by default; use an existing libcurl 8.5+ system package and never fetch it implicitly"},
      {"SQLite3",
       "system package; record sqlite3_libversion() at configure/runtime",
       "https://sqlite.org/",
       "Public Domain",
       "INFERLAB_ENABLE_EVIDENCE_SQLITE",
       false,
       true,
       {"evidence_warehouse_index"},
       "Optional local evidence warehouse materialization backend",
       "low",
       "JSON-only warehouse artifact",
       "Adds query convenience only; missing SQLite must not block dry-run claims",
       "disabled by default on this server"},
      {"google/benchmark",
       "v1.8.5",
       "https://github.com/google/benchmark",
       "Apache-2.0",
       "INFERLAB_ENABLE_GOOGLE_BENCHMARK",
       false,
       true,
       {"kernel_microbench_harness"},
       "Repeatable CPU/GPU microkernel benchmark cases on target machines",
       "medium",
       "microbenchmark manifest without timing execution",
       "Timing claims remain blocked until target-machine results/raw artifacts exist",
       "disabled by default on this server"},
      {"CUDA NVTX",
       "CUDA Toolkit-provided",
       "https://docs.nvidia.com/nsight-systems/UserGuide/index.html",
       "NVIDIA CUDA Toolkit EULA",
       "INFERLAB_ENABLE_NVTX",
       false,
       true,
       {"kernel_microbench_harness", "CUDA Nsight profiling"},
       "Nsight Systems/Compute profiling ranges for CUDA microkernels",
       "medium",
       "plain manifest and commands without NVTX ranges",
       "Profiling ranges help diagnosis but do not prove performance gains",
       "disabled unless CUDA and NVTX are explicitly enabled on target machine"},
      {"CUDA Toolkit",
       "target-machine CUDA Toolkit version",
       "https://developer.nvidia.com/cuda-toolkit",
       "NVIDIA CUDA Toolkit EULA",
       "INFERLAB_ENABLE_CUDA",
       false,
       true,
       {"kernel_microbench_harness", "CUDA correctness", "Nsight profiling"},
       "Builds and runs CUDA correctness/timing targets on GPU machines",
       "medium",
       "CPU/dry-run manifests and migration commands",
       "Required before CUDA timing or GPU performance claims",
       "not available on this server per user request"},
      {"vLLM",
       "target product environment version",
       "https://docs.vllm.ai/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"product_replay_semantic_events", "product_vllm_profile",
        "vllm_replay_sample"},
       "Default mature-product replay sample and OpenAI-compatible serving profile",
       "medium",
       "sample replay JSONL and dry-run adapter artifact",
       "Replay evidence can explain product phenomena but cannot claim execute performance",
       "not installed or executed on this server"},
      {"SGLang",
       "target product environment version",
       "https://github.com/sgl-project/sglang",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"product profile", "scheduler comparison"},
       "Mature serving runtime for radix/prefix and scheduler comparison",
       "medium",
       "dry-run product profile artifact",
       "External product owns runtime behavior until replay/execute evidence exists",
       "not installed or executed on this server"},
      {"TensorRT-LLM",
       "target product environment version",
       "https://nvidia.github.io/TensorRT-LLM/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"runtime backend profile", "engine/quant alignment"},
       "Mature NVIDIA runtime for engine, batching, quantization, and serving alignment",
       "high",
       "dry-run product profile and native alignment artifacts",
       "Engine/timing claims require target CUDA/TensorRT-LLM artifacts",
       "not installed or executed on this server"},
      {"FlashInfer",
       "target product environment version",
       "https://docs.flashinfer.ai/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"kernel library profile", "kernel compatibility matrix"},
       "Mature kernel library alignment for paged/ragged attention and microbench baselines",
       "medium",
       "native compatibility matrix and dry-run product profile",
       "Kernel performance remains external until target profiling evidence exists",
       "not installed or executed on this server"},
      {"Android Gradle Plugin",
       "8.7.3",
       "https://developer.android.com/build/releases/agp-8-7-0-release-notes",
       "Apache-2.0",
       "android/edge_runtime_jni:com.android.library",
       false,
       true,
       {"Android edge runtime AAR packaging"},
       "Pinned Android library, external CMake, consumer-rule and AAR packaging orchestration",
       "medium",
       "direct Android NDK CMake shared-library build without an AAR",
       "Builds packaging only; plugin resolution, AAR assembly, runtime linkage and device execution remain separate evidence",
       "declared but not resolved or run on this server per user request"},
      {"llama.cpp",
       "b10173 (e9fa078)",
       "https://github.com/ggml-org/llama.cpp",
       "MIT",
       "INFERLAB_ENABLE_EDGE_LLAMA_CPP",
       false,
       true,
       {"edge_runtime_matrix", "external llama.cpp matrix runner"},
       "GGUF edge inference runtime and CPU/NEON reference product adapter",
       "medium",
       "scalar InferLab kernels and capability-only rows remain available when the optional adapter is not linked",
       "llama.cpp behavior and timing are evidence only when the pinned in-process adapter is linked and explicitly run in execute mode",
       "disabled by default; source must be an existing Git checkout pinned to b10173/e9fa078 and is never downloaded implicitly"},
      {"ExecuTorch",
       "v1.2.0",
       "https://github.com/pytorch/executorch",
       "BSD-3-Clause",
       "INFERLAB_ENABLE_EDGE_EXECUTORCH",
       false,
       true,
       {"edge_runtime_matrix", "ExecuTorch C++ Runtime", "XNNPACK backend"},
       "PTE edge inference runtime with a mobile-oriented C++ lifecycle",
       "medium",
       "scalar InferLab kernels and capability-only rows remain available when the optional adapter is not linked",
       "PTE/XNNPACK behavior and timing are evidence only when the v1.2.0 SDK, matching source checkout and in-process adapter are linked and explicitly run",
       "disabled by default; requires an installed SDK with extension_llm_runner, XNNPACK and static registration link support"},
      {"LMCache",
       "target product environment version",
       "https://docs.lmcache.ai/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"KV offload profile", "KV lifecycle evidence"},
       "KV reuse/offload product evidence for replay correlation",
       "medium",
       "dry-run product profile and native KV lifecycle artifacts",
       "KV runtime behavior remains product-owned until replay/execute evidence exists",
       "not installed or executed on this server"},
      {"Mooncake",
       "target product environment version",
       "https://kvcache-ai.github.io/Mooncake/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"KV transfer profile", "PD split crossover evidence"},
       "KV-centric disaggregation and transfer replay alignment",
       "medium",
       "dry-run product profile and native PD crossover artifacts",
       "Transfer performance requires target replay/execute evidence",
       "not installed or executed on this server"},
      {"Ray Serve",
       "target product environment version",
       "https://docs.ray.io/en/latest/serve/",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"cloud control-plane profile"},
       "Serving control-plane and autoscaling comparison",
       "medium",
       "dry-run product profile artifact",
       "Deployment behavior remains external until product environment evidence exists",
       "not installed or executed on this server"},
      {"KServe/llm-d",
       "target product environment version",
       "https://github.com/llm-d/llm-d",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"cloud control-plane profile", "deployment readiness"},
       "Kubernetes/llm-d control-plane alignment and deployment templates",
       "medium",
       "dry-run templates and product profile artifacts",
       "Kubernetes readiness requires target cluster validation",
       "not installed or executed on this server"},
      {"Hugging Face TGI",
       "target product environment version",
       "https://github.com/huggingface/text-generation-inference",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"runtime backend profile", "decode/streaming replay"},
       "Mature serving runtime for decode/streaming event comparison",
       "medium",
       "dry-run product profile artifact",
       "Runtime behavior remains external until replay/execute evidence exists",
       "not installed or executed on this server"},
      {"KTransformers",
       "target product environment version",
       "https://github.com/kvcache-ai/ktransformers",
       "Apache-2.0",
       "product profile config",
       false,
       true,
       {"hybrid runtime profile"},
       "Hybrid runtime comparison for offload and local execution strategies",
       "medium",
       "dry-run product profile artifact",
       "Hybrid runtime claims require target environment evidence",
       "not installed or executed on this server"},
  };
  static const bool validated = [] {
    validate_dependency_spec_registry(dependencies);
    return true;
  }();
  (void)validated;
  return dependencies;
}

inline const std::vector<MicrobenchOperator>& microbench_operators() {
  static const std::vector<MicrobenchOperator> operators = {
      {"rmsnorm",
       "normalization",
       "batch={1,8,32}, hidden={1024,4096,8192}",
       "fp16/fp32 contiguous row-major",
       "CPU reference, CUDA naive, CUDA optimized future",
       "native_core normalization RMSNorm reference",
       "BM_RMSNorm_hidden_batch",
       "inferlab.rmsnorm",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator rmsnorm",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator rmsnorm"},
      {"rope",
       "position_encoding",
       "seq={128,2048,8192}, head_dim={64,128}",
       "fp16/fp32 interleaved Q/K",
       "CPU RoPE reference, CUDA naive, CUDA vectorized future",
       "native_core RoPE reference",
       "BM_RoPE_seq_head_dim",
       "inferlab.rope",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator rope",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator rope"},
      {"row_softmax",
       "attention_softmax",
       "rows={1,32,128}, cols={128,2048,8192}",
       "fp32 logits row-major",
       "CPU softmax reference, CUDA block softmax",
       "native_core row softmax reference",
       "BM_RowSoftmax_rows_cols",
       "inferlab.row_softmax",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator row_softmax",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator row_softmax"},
      {"int4_dequant_matvec",
       "quantized_linear",
       "out_features={1024,4096}, in_features={4096,8192}, group={32,128}",
       "INT4 packed weights + fp16/fp32 scales",
       "CPU fused dequant+matvec, CUDA dequant matvec skeleton",
       "native_core quantization fused dequant matvec reference",
       "BM_INT4DequantMatvec_shape_group",
       "inferlab.int4_dequant_matvec",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator int4_dequant_matvec",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator int4_dequant_matvec"},
      {"paged_kv_gather",
       "paged_attention_kv",
       "tokens={128,2048,8192}, block={16,32}, head_dim={64,128}",
       "paged KV block table + contiguous block payload",
       "CPU Paged KV gather, CUDA Paged KV gather skeleton",
       "native_core PagedAttention block-table gather reference",
       "BM_PagedKVGather_tokens_block",
       "inferlab.paged_kv_gather",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator paged_kv_gather",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator paged_kv_gather"},
      {"decode_attention",
       "decode_attention",
       "num_heads={1,16,64}, context={128,2048,8192}, head_dim={64,128}",
       "fp32 Q + contiguous K/V + fp32 softmax accumulator",
       "CPU scaled dot-product attention, CUDA naive/shared attention runtime",
       "native_core scaled dot-product attention reference",
       "BM_DecodeAttention_heads_context",
       "inferlab.decode_attention",
       "nsys profile --trace=cuda,nvtx ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator decode_attention",
       "ncu --set full ./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator decode_attention"},
  };
  static const bool validated = [] {
    validate_microbench_operator_registry(operators);
    return true;
  }();
  (void)validated;
  return operators;
}

inline const std::vector<CorrelationRule>& correlation_rules() {
  static const std::vector<CorrelationRule> rules = {
      {"request_prefill_started",
       "product_replay_semantic_events.json",
       "native_scheduler_batching_deep_dive.json",
       "prefill chunk timeline",
       "requires replay event timestamps before product scheduler claims"},
      {"decode_step_stalled",
       "product_replay_semantic_events.json",
       "native_scheduler_root_cause_deep_dive.json",
       "decode stall and root cause",
       "cannot claim product TPOT/P99 behavior without replay or execute evidence"},
      {"kv_cache_hit",
       "product_replay_semantic_events.json",
       "native_kv_lifecycle_deep_dive.json",
       "prefix sharing and KV reuse",
       "external product owns KV runtime; InferLab explains reuse evidence only"},
      {"kv_transfer_started",
       "product_replay_semantic_events.json",
       "native_kv_transport_timeline_deep_dive.json",
       "KV transfer and overlap timeline",
       "transfer-vs-prefill claims require product replay or execute evidence"},
      {"kernel_fallback",
       "product_replay_semantic_events.json",
       "native_kernel_compatibility_matrix.json",
       "selected backend and fallback reason",
       "kernel library performance remains external until target GPU profiling exists"},
      {"admission_rejected",
       "product_replay_semantic_events.json",
       "native_tenant_quota_admission_deep_dive.json",
       "tenant quota, admission and SLO risk",
       "tenant admission behavior must be replayed or executed before product claims"},
      {"stream_cancelled",
       "product_replay_semantic_events.json",
       "native_streaming_backpressure_deep_dive.json",
       "slot release and cancellation cleanup risk",
       "streaming runtime behavior remains product-owned"},
  };
  static const bool validated = [] {
    validate_correlation_rule_registry(rules);
    return true;
  }();
  (void)validated;
  return rules;
}

}  // namespace inferlab_high_value
