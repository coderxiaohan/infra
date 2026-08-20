#!/usr/bin/env python3
"""Render the InferLab final report from available artifacts.

The renderer is intentionally environment-independent. It can run before GPU,
vLLM, TensorRT-LLM, llama.cpp, Android, Prometheus, or real-model fixtures are
available. Missing evidence is reported explicitly so the final report remains
honest and migration-ready.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

from artifact_catalog_glue import resolve_catalog_sources
from artifact_json import load_json_dict as load_json
from benchmark_matrix_commands import reproducible_commands_from_matrix_with_status
from command_redaction import artifact_path_payload as path_payload, redact_path_error
from product_profiles.common import default_product_profile_keys
from evidence_projection import (
    CATALOG_FLOW_FALLBACK_REMOVAL_QUEUE_COUNT_FIELDS,
    artifact_summary,
    default_repo_root,
    dict_value,
    first_present,
    list_value,
    path_label,
    project_artifact_catalog_governance,
    project_catalog_flow_fallback_removal_queue,
    project_evidence_query_catalog_flow_filter,
    project_evidence_query_native_core_audit_filter,
    project_evidence_query_ux_summary,
    project_final_report_evidence_query_blocker_drilldown,
    project_final_report_evidence_query_cross_artifact,
    project_final_report_evidence_query_repair_navigation,
    project_final_report_evidence_query_repair_guidance,
    project_final_report_evidence_query_repair_path_review,
    project_final_report_evidence_query_readiness_path,
    project_final_report_evidence_query_selector_action_plan,
    project_final_report_evidence_query_source_location,
    project_final_report_evidence_query_source_exactness,
    project_final_report_catalog_flow_fallback_scorecard,
    project_final_report_catalog_flow_fallback_review,
    project_final_report_evidence_query_catalog_flow_scorecard,
    project_final_report_evidence_query_native_audit_scorecard,
    project_final_report_evidence_query_ux_closure,
    project_final_report_evidence_query_ux_operation_snapshot_scorecard,
    project_final_report_evidence_query_ux_review_scorecard,
    project_final_report_evidence_query_ux_selector_scorecard,
    project_final_report_review_source_location,
    project_final_report_root_cause_catalog_flow_scorecard,
    project_final_report_root_cause_native_audit_residual,
    project_final_report_release_linkage,
    project_final_report_release_pipeline_headline,
    project_final_report_release_pipeline_product_replay_scorecard,
    project_final_report_release_pipeline_repair_path_scorecard,
    project_final_report_root_cause_repair_review,
    project_release_evidence_query_catalog_flow_scorecard,
    project_release_pipeline_product_replay,
    project_root_cause_product_replay,
    resolve_repo_path,
    safe_path_probe,
)
from json_projection import get_path
from release_audit_projection import (
    project_final_report_repair_path_diagnostics,
    project_final_report_repair_path_groups,
    project_root_cause_catalog_flow_governance,
    project_root_cause_catalog_flow_parity_gate,
    project_root_cause_catalog_flow_shortest_path,
    project_root_cause_native_audit_coverage,
    project_root_cause_native_audit_manifest,
)
from text_output import write_text_file
from time_utils import utc_now


MODULES = (
    {
        "name": "Mini Transformer and KV Cache",
        "why": "Explains prefill/decode shape flow, cached decode, and recompute avoidance.",
        "code": ["mini_llm/transformer.py", "mini_llm/kv_cache.py", "mini_llm/shape_probe.py"],
        "reports": [],
        "source_notes": [],
        "results": [
            "shape_probe.json",
            "mini_transformer_cache_compare.json",
            "mini_transformer_workload_profile.json",
            "mini_transformer_bottleneck_advisor.json",
            "mini_transformer_scenario_sweep.json",
        ],
    },
    {
        "name": "Native KV, PagedAttention, and Scheduler",
        "why": "Connects KV allocation, block tables, queueing, TTFT-style latency, and serving throughput.",
        "code": ["native_core/include/inferlab/paged_kv.hpp", "native_core/include/inferlab/scheduler.hpp"],
        "reports": ["reports/03_kv_cache_paged_attention.md"],
        "source_notes": [],
        "results": [
            "scheduler_trace.json",
            "scheduler_pressure_sweep.json",
            "scheduler_workload_sweep.json",
            "kv_allocation_compare.json",
            "prefix_cache_compare.json",
        ],
    },
    {
        "name": "CUDA Kernels",
        "why": "Shows bottom-up operator implementation and where GPU profiling should be attached.",
        "code": ["cuda_kernels", "native_core/include/inferlab/matrix_ops.hpp"],
        "reports": ["reports/02_cuda_kernel_profile.md"],
        "source_notes": [],
        "results": ["cuda_environment.json", "matrix_ops_profile.json", "attention_probe.json", "cuda_profile_matrix.json"],
    },
    {
        "name": "Quantization and GGUF",
        "why": "Explains storage layout, block dequantization, metadata overhead, and low-bit trade-offs.",
        "code": ["native_core/include/inferlab/gguf.hpp", "native_core/src/gguf.cpp"],
        "reports": ["reports/06_quantization_tradeoff.md", "reports/08_gguf_parser.md"],
        "source_notes": [],
        "results": ["quantization_probe.json", "quantization_sweep.json", "gguf_probe.json"],
    },
    {
        "name": "Cloud Serving",
        "why": "Compares online serving, OpenAI-compatible endpoints, scheduler behavior, and monitoring.",
        "code": ["scripts/run_vllm_benchmark.py", "scripts/run_trtllm_benchmark.py"],
        "reports": [],
        "source_notes": ["serving/vllm_server.md", "serving/trtllm_build.md"],
        "results": ["vllm_serving_benchmark.json", "trtllm_benchmark.json"],
    },
    {
        "name": "Edge llama.cpp and Android",
        "why": "Covers GGUF runtime, CPU/ARM deployment, simpleperf/perf hooks, and device constraints.",
        "code": ["scripts/run_llama_cpp_benchmark.py", "android/native_hello", "scripts/plan_android_deploy.py"],
        "reports": ["reports/09_android_edge_deployment.md"],
        "source_notes": ["source_notes/android_ndk_llama_trace.md", "android/llama_runner/README.md"],
        "results": ["llama_cpp_benchmark.json", "llama_cpp_matrix.json", "android_deploy_plan.json", "android_edge_benchmark.json"],
    },
    {
        "name": "Runtime, Observability, and Reproducibility",
        "why": "Makes results reproducible and explainable across CPU, GPU, and monitoring machines.",
        "code": ["scripts/collect_system_info.py", "scripts/export_prometheus_metrics.py", "scripts/plan_runtime_deploy.py", "scripts/render_evidence_checklist.py", "scripts/verify_source_traces.py", "native_core/src/runtime_evidence_freshness_gate_main.cpp"],
        "reports": ["reports/11_runtime_profiling_plan.md", "reports/13_profiling_artifacts.md", "reports/15_runtime_evidence_closure.md"],
        "source_notes": ["deploy/README.md", "monitoring/README.md", "profiling/README.md"],
        "results": ["system_info.json", "benchmark_summary.json", "runtime_deploy_plan.json", "profiling_artifacts.json", "source_trace_verification.json", "evidence_checklist.json", "native_runtime_evidence_freshness_gate.json"],
    },
    {
        "name": "Backend Quality Evaluation",
        "why": "Keeps optimization tied to fixed-prompt correctness checks and cross-backend output comparison.",
        "code": ["scripts/evaluate_backend_quality.py"],
        "reports": ["reports/12_quality_evaluation.md"],
        "source_notes": [],
        "results": ["backend_quality_eval.json"],
    },
    {
        "name": "Backend Comparison",
        "why": "Turns cloud, compiled GPU, edge GGUF, native, profiling, quality, and cost artifacts into one trade-off table.",
        "code": ["scripts/compare_backends.py"],
        "reports": ["reports/14_backend_comparison.md"],
        "source_notes": [],
        "results": ["backend_comparison.json"],
    },
    {
        "name": "Cloud Serving and Native Mechanisms",
        "why": "Keeps the project-facing KV, scheduler, CUDA/quant, product replay, and deployment evidence paths without indexing every historical dry-run plan.",
        "code": [
            "advanced_serving",
            "native_core/include/inferlab/advanced_serving.hpp",
            "native_core/src/advanced_serving_backend_integration.cpp",
            "native_core/src/kv_lifecycle_deep_dive_model.cpp",
            "native_core/src/product_replay_alignment_model.cpp",
        ],
        "reports": [
            "reports/16_advanced_serving_research.md",
            "reports/61_kv_paged_attention_deep_dive.md",
            "reports/62_scheduler_batching_deep_dive.md",
            "reports/63_cuda_quant_microkernel_deep_dive.md",
            "reports/64_product_replay_alignment.md",
            "reports/69_kv_tier_decision_deep_dive.md",
            "reports/70_pd_split_crossover_deep_dive.md",
            "reports/71_scheduler_root_cause_deep_dive.md",
            "reports/75_streaming_backpressure_deep_dive.md",
            "reports/77_autoscaler_cost_slo_deep_dive.md",
            "reports/78_lora_adapter_cache_deep_dive.md",
            "reports/80_edge_compiler_artifact_matrix.md",
            "reports/89_flashinfer_replay_alignment.md",
            "reports/90_trtllm_engine_quant_alignment.md",
            "reports/93_openai_api_conformance_matrix.md",
            "reports/94_kv_lifecycle_deep_dive.md",
        ],
        "source_notes": ["../docs/ai/project-positioning.md"],
        "results": [
            "native_kv_paged_attention_deep_dive.json",
            "native_scheduler_batching_deep_dive.json",
            "native_cuda_quant_microkernel_deep_dive.json",
            "native_product_replay_alignment.json",
            "native_kv_lifecycle_deep_dive.json",
            "native_kv_tier_decision_deep_dive.json",
            "native_pd_split_crossover_deep_dive.json",
            "native_scheduler_root_cause_deep_dive.json",
            "native_streaming_backpressure_deep_dive.json",
            "native_autoscaler_cost_slo_deep_dive.json",
            "native_lora_adapter_cache_deep_dive.json",
            "native_edge_compiler_artifact_matrix.json",
            "native_flashinfer_replay_alignment.json",
            "native_trtllm_engine_quant_alignment.json",
            "native_openai_api_conformance_matrix.json",
        ],
    },
    {
        "name": "Mature Product Profiles",
        "why": "Uses vLLM, SGLang, and TensorRT-LLM as the default cloud profile scope; other mature products remain available through on-demand adapters without expanding the default release report.",
        "code": ["scripts/product_profiles", "configs/product_profiles", "native_core/src/mature_product_profile_contract.cpp", "native_core/src/product_profile_artifact_contract.cpp", "native_core/src/product_profile_contract_check_model.cpp", "native_core/src/product_profile_contract_check_view.cpp", "native_core/src/product_profile_contract_check_main.cpp", "native_core/src/python_source_contract.cpp", "native_core/src/product_integration_inventory_model.cpp", "native_core/src/product_integration_inventory_view.cpp", "native_core/src/product_integration_inventory_main.cpp", "native_core/src/claim_scope_contract_main.cpp"],
        "reports": ["reports/49_product_vllm_profile.md", "reports/50_product_sglang_profile.md", "reports/52_product_runtime_backend_profile.md", "reports/57_product_profile_contract_check.md", "reports/58_product_integration_inventory.md", "reports/59_claim_scope_contract.md"],
        "source_notes": ["../相关产品深度调研与接入计划.md"],
        "results": ["native_product_profile_contract_check.json", "native_product_integration_inventory.json", "native_claim_scope_contract.json", "product_vllm_capability_profile.json", "product_sglang_capability_profile.json", "product_tensorrtllm_runtime_profile.json"],
    },
    {
        "name": "Evidence Governance And High-Value Engineering",
        "why": "Keeps one catalog and contract source, one query surface, and one release decision while retaining dependency and audit evidence.",
        "code": [
            "native_core/src/high_value_artifact_catalog.hpp",
            "native_core/src/artifact_schema_registry_main.cpp",
            "native_core/src/artifact_catalog_single_source_main.cpp",
            "native_core/src/evidence_warehouse_index_model.cpp",
            "native_core/src/evidence_query_main.cpp",
            "native_core/src/release_evidence_pipeline_main.cpp",
            "native_core/src/native_core_audit_register_main.cpp",
            "native_core/src/kernel_microbench_harness_model.cpp",
            "native_core/src/dependency_governance_model.cpp",
        ],
        "reports": [
            "reports/96_evidence_warehouse_lineage.md",
            "reports/98_kernel_microbench_harness.md",
            "reports/99_third_party_dependency_governance.md",
            "reports/100_release_evidence_pipeline.md",
            "reports/101_evidence_query.md",
            "reports/102_dependency_sbom_gate.md",
            "reports/104_artifact_schema_registry.md",
            "reports/105_artifact_catalog_single_source.md",
            "reports/111_native_core_audit_register.md",
        ],
        "source_notes": ["../docs/ai/project-positioning.md"],
        "results": [],
    },
)

CATALOG_CONSUMING_MODULE = "Evidence Governance And High-Value Engineering"


def module_result_sources(module: Dict[str, Any], results_root: Path) -> Tuple[Tuple[str, ...], Dict[str, Any]]:
    configured_results = tuple(module["results"])
    if module["name"] != CATALOG_CONSUMING_MODULE:
        return configured_results, {}
    resolution = resolve_catalog_sources(results_root, ())
    return resolution.names, resolution.to_dict()


def evidence_governance_sources(results_root: Path) -> Tuple[List[str], Dict[str, Any]]:
    for module in MODULES:
        if module["name"] == CATALOG_CONSUMING_MODULE:
            sources, resolution = module_result_sources(module, results_root)
            return list(sources), resolution
    return [], {}


NON_GPU_REVIEW_ARTIFACTS = (
    "native_release_evidence_pipeline.json",
    "native_evidence_query_result.json",
    "native_evidence_query_selector_repair_card_export.json",
    "native_core_audit_register.json",
)

CONFIDENCE_TIERS = (
    "execute_verified",
    "replay_supported",
    "dry_run_ready",
    "schema_only",
    "stale_or_blocked",
    "unknown",
)

CONFIDENCE_TIER_DESCRIPTIONS = {
    "execute_verified": "Target environment produced execute/correctness/timing evidence.",
    "replay_supported": "Replay evidence exists, but it is not an execute-mode benchmark claim.",
    "dry_run_ready": "Schema, summary, and claim boundary are present for dry-run/report wiring.",
    "schema_only": "Schema-level artifact exists; runtime, replay, or benchmark claims are not supported.",
    "stale_or_blocked": "Evidence is missing, stale, blocked, or reports blockers.",
    "unknown": "Confidence tier was not reported by the source artifact.",
}


def int_value(value: Any, default: int = 0) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    if isinstance(value, str):
        try:
            return int(value)
        except ValueError:
            return default
    return default


def truthy_value(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "present", "ready"}
    return bool(value)


def markdown_cell(value: Any, limit: int = 120) -> str:
    if value is None:
        text = "unknown"
    elif isinstance(value, bool):
        text = "true" if value else "false"
    else:
        text = str(value)
    text = " ".join(text.split())
    text = text.replace("|", "\\|")
    if limit > 3 and len(text) > limit:
        return text[: limit - 3] + "..."
    return text


def markdown_inline_code(value: Any, limit: int = 240) -> str:
    text = markdown_cell(value, limit=limit)
    longest_run = 0
    current_run = 0
    for char in text:
        if char == "`":
            current_run += 1
            longest_run = max(longest_run, current_run)
        else:
            current_run = 0
    fence = "`" * (longest_run + 1)
    if longest_run > 0:
        return f"{fence} {text} {fence}"
    return f"{fence}{text}{fence}"


def sorted_nonzero_blocker_rows(
    blockers: Dict[str, Any], limit: int = 12
) -> Tuple[List[Tuple[str, int]], int]:
    rows: List[Tuple[str, int]] = []
    for key, value in blockers.items():
        count = int_value(value)
        if count <= 0:
            continue
        rows.append((str(key), count))
    rows.sort(key=lambda item: (-item[1], item[0]))
    return rows[:limit], max(0, len(rows) - limit)


def selected_metric_value(metrics: Any, name: str) -> Any:
    if not isinstance(metrics, list):
        return None
    for item in metrics:
        if isinstance(item, dict) and item.get("name") == name:
            return item.get("value")
    return None


def root_cause_repair_card_summaries(cards: List[Any], limit: int = 5) -> List[Dict[str, Any]]:
    summaries: List[Dict[str, Any]] = []
    for item in cards:
        if not isinstance(item, dict):
            continue
        source_locations = [str(value) for value in list_value(item.get("exact_wiring_locations"))]
        summaries.append(
            {
                "source": "root_cause_repair_card",
                "card_id": item.get("card_id"),
                "artifact": item.get("artifact"),
                "category": item.get("blocker_category"),
                "severity": item.get("severity"),
                "priority": item.get("priority"),
                "repair_environment": item.get("repair_environment"),
                "action": first_present(item.get("suggested_repair"), item.get("recommended_action")),
                "flow_check_recommended": item.get("flow_check_recommended"),
                "source_location_count": len(source_locations),
                "first_source_location": source_locations[0] if source_locations else None,
            }
        )
        if len(summaries) >= limit:
            break
    return summaries


def root_cause_category_card_stats(cards: List[Any], category: str) -> Dict[str, Any]:
    card_count = 0
    source_location_card_count = 0
    source_location_count = 0
    first_source_location: Optional[str] = None
    for item in cards:
        if not isinstance(item, dict):
            continue
        card_category = first_present(item.get("blocker_category"), item.get("category"))
        if card_category != category:
            continue
        card_count += 1
        source_locations = [str(value) for value in list_value(item.get("exact_wiring_locations"))]
        if source_locations:
            source_location_card_count += 1
            source_location_count += len(source_locations)
            if first_source_location is None:
                first_source_location = source_locations[0]
    return {
        "card_count": card_count,
        "source_location_card_count": source_location_card_count,
        "source_location_count": source_location_count,
        "first_source_location": first_source_location,
    }


@dataclass(frozen=True)
class TopRepairActionsProjection:
    actions: Tuple[Dict[str, Any], ...]
    selected_trace_sources: Tuple[str, ...]


def top_non_gpu_repair_actions(
    closure: Dict[str, Any],
    action_source_trace: List[Dict[str, Any]],
    limit: int = 5,
) -> TopRepairActionsProjection:
    if isinstance(limit, bool) or not isinstance(limit, int) or not 1 <= limit <= 100:
        raise ValueError("top repair action limit must be in [1, 100]")
    trace_sources = [
        str(item.get("source"))
        for item in action_source_trace
        if isinstance(item, dict) and item.get("source")
    ]
    if len(trace_sources) != len(action_source_trace):
        raise ValueError("action-source trace contains an invalid source row")
    if len(trace_sources) != len(set(trace_sources)):
        raise ValueError("action-source trace contains duplicate sources")
    priority_by_source = {
        str(item.get("source")): int_value(item.get("priority_order"), 1000)
        for item in action_source_trace
    }
    priorities = tuple(priority_by_source.values())
    if any(priority <= 0 or priority >= 1000 for priority in priorities):
        raise ValueError("action-source trace contains an invalid priority")
    if len(priorities) != len(set(priorities)):
        raise ValueError("action-source trace contains duplicate priorities")
    ranked_actions: List[Tuple[int, int, str, Dict[str, Any]]] = []

    def append_action(action: Dict[str, Any], trace_source: str) -> None:
        if trace_source not in priority_by_source:
            raise ValueError(
                f"top repair action references unknown trace source: {trace_source}"
            )
        ranked_actions.append(
            (
                priority_by_source.get(trace_source, 1000),
                len(ranked_actions),
                trace_source,
                action,
            )
        )

    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    root_cause_catalog_flow_shortest_path = dict_value(
        closure.get(
            "root_cause_catalog_flow_fallback_removal_shortest_path"
        )
    )
    root_cause_catalog_flow_parity_gate = dict_value(
        closure.get("root_cause_catalog_flow_fallback_removal_parity_gate")
    )
    root_cause_catalog_flow_governance = dict_value(
        closure.get("root_cause_catalog_flow_governance")
    )
    for card in list_value(root_cause_review.get("repair_cards")):
        if not isinstance(card, dict):
            continue
        projected_card = dict(card)
        projected_card.setdefault("source", "root_cause_repair_card")
        append_action(projected_card, "root_cause_repair_cards")

    for path in list_value(closure.get("repair_paths")):
        if not isinstance(path, dict):
            continue
        path_category = path.get("category")
        trace_source = {
            "catalog_flow_discrepancy": (
                "release_pipeline_catalog_flow_repair_paths"
            ),
            "catalog_flow_fallback_removal_shortest_path": (
                "release_pipeline_catalog_flow_repair_paths"
            ),
            "catalog_flow_fallback_removal_parity_gate": (
                "release_pipeline_catalog_flow_repair_paths"
            ),
            "evidence_query_snapshot_diff_review": (
                "release_pipeline_evidence_query_snapshot_diff_repair_paths"
            ),
        }.get(path_category, "release_pipeline_repair_paths")
        append_action(
            {
                "source": "release_pipeline_repair_path",
                "artifact": path.get("source"),
                "category": path_category,
                "severity": path.get("severity"),
                "priority": path.get("priority"),
                "repair_environment": path.get("repair_environment"),
                "action": path.get("recommended_action"),
                "command": path.get("command"),
            },
            trace_source,
        )

    evidence_query_review = dict_value(closure.get("evidence_query_repair_path_review"))
    first_action = evidence_query_review.get("first_action")
    if first_action:
        append_action(
            {
                "source": "evidence_query_first_action",
                "artifact": evidence_query_review.get("upstream_artifact", "native_evidence_query_result.json"),
                "category": "repair_path_first_action",
                "severity": "advisory",
                "priority": "P0",
                "repair_environment": "current_repo_or_target_machine_from_repair_path",
                "action": first_action,
                "target": evidence_query_review.get("first_target"),
                "state": evidence_query_review.get("first_state"),
            },
            "evidence_query_first_action",
        )

    evidence_query_catalog_filter = dict_value(
        closure.get("evidence_query_catalog_flow_filter")
    )
    catalog_flow_discrepancy_count = int_value(
        evidence_query_catalog_filter.get("catalog_flow_discrepancy_count")
    )
    catalog_flow_input_gap_count = int_value(
        evidence_query_catalog_filter.get("catalog_flow_input_gap_count")
    )
    fallback_review_projection = project_final_report_catalog_flow_fallback_review(
        dict_value(closure.get("catalog_flow_fallback_removal_queue")),
        root_cause_catalog_flow_shortest_path,
        root_cause_catalog_flow_parity_gate,
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    fallback_shortest_path = fallback_review_projection.shortest_path
    if bool(fallback_shortest_path.get("repair_action_required")):
        append_action(
            {
                "source": "catalog_flow_fallback_removal_shortest_path",
                "artifact": "native_release_evidence_pipeline.json",
                "category": "catalog_flow_fallback_removal_shortest_path",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": fallback_shortest_path.get(
                    "repair_environment"
                ),
                "action": "Follow the catalog fallback-removal shortest path before deleting legacy fallback surfaces; refresh catalog, evidence query, release pipeline, summary, Prometheus, and final report after each step.",
                "state": fallback_shortest_path.get("state"),
                "first_surface": fallback_shortest_path.get(
                    "repair_first_surface"
                ),
                "step_count": fallback_shortest_path.get("step_count"),
                "current_repo_step_count": fallback_shortest_path.get(
                    "current_repo_step_count"
                ),
                "target_machine_step_count": fallback_shortest_path.get(
                    "target_machine_step_count"
                ),
                "source_location_gap_count": fallback_shortest_path.get(
                    "source_location_gap_count"
                ),
                "canonical_gap_count": fallback_shortest_path.get(
                    "canonical_gap_count"
                ),
                "source_drift_count": fallback_shortest_path.get(
                    "source_drift_count"
                ),
                "source_drift_fields": fallback_shortest_path.get(
                    "source_drift_fields"
                ),
                "source_drift_sources": fallback_shortest_path.get(
                    "source_drift_sources"
                ),
            },
            "root_cause_catalog_flow_blockers",
        )
    fallback_parity_gate = fallback_review_projection.parity_gate
    if bool(fallback_parity_gate.get("repair_action_required")):
        append_action(
            {
                "source": "catalog_flow_fallback_removal_parity_gate",
                "artifact": "native_evidence_query_result.json",
                "category": "catalog_flow_fallback_removal_parity_gate",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": fallback_parity_gate.get(
                    "repair_environment"
                ),
                "action": "Keep catalog legacy fallback surfaces until the fallback-removal parity gate reports removal_allowed=true; refresh catalog, evidence query, root-cause query, release pipeline, summary, Prometheus, and final report after each parity repair.",
                "decision": fallback_parity_gate.get("repair_action_decision"),
                "removal_allowed": fallback_parity_gate.get(
                    "repair_action_removal_allowed"
                ),
                "blocker_count": fallback_parity_gate.get("blocker_count"),
                "parity_refresh_required_count": fallback_parity_gate.get(
                    "parity_refresh_required_count"
                ),
                "source_location_gap_count": fallback_parity_gate.get(
                    "source_location_gap_count"
                ),
                "canonical_gap_count": fallback_parity_gate.get(
                    "canonical_gap_count"
                ),
                "source_drift_count": fallback_parity_gate.get(
                    "source_drift_count"
                ),
            },
            "root_cause_catalog_flow_blockers",
        )
    fallback_queue = fallback_review_projection.queue
    if bool(fallback_queue.get("repair_action_required")):
        append_action(
            {
                "source": "catalog_flow_fallback_removal_queue",
                "artifact": fallback_queue.get("action_artifact"),
                "category": "catalog_flow_fallback_removal_queue",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": fallback_queue.get("repair_environment"),
                "action": "Inspect catalog fallback-removal queue, keep legacy fallback surfaces until parity refresh evidence exists, then remove fallback surfaces one by one.",
                "blocked_count": fallback_queue.get("blocked_count"),
                "parity_refresh_required_count": fallback_queue.get(
                    "parity_refresh_required_count"
                ),
                "current_repo_action_count": fallback_queue.get(
                    "current_repo_action_count"
                ),
                "target_machine_action_count": fallback_queue.get(
                    "target_machine_action_count"
                ),
                "canonical_gap_count": fallback_queue.get("canonical_gap_count"),
                "source_drift_count": fallback_queue.get("source_drift_count"),
                "source_drift_fields": list_value(
                    fallback_queue.get("source_drift_fields")
                ),
                "source_drift_sources": list_value(
                    fallback_queue.get("source_drift_sources")
                ),
            },
            "catalog_flow_fallback_removal_queue",
        )
    if catalog_flow_discrepancy_count > 0 or catalog_flow_input_gap_count > 0:
        append_action(
            {
                "source": "catalog_flow_discrepancy_review",
                "artifact": "native_evidence_query_result.json",
                "category": "catalog_flow_discrepancy",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": "current_repo_then_validation_machine",
                "action": "Repair catalog discrepancies, then refresh Catalog, Evidence Query, Release Pipeline, summary, Prometheus, and final report.",
                "discrepancy_count": catalog_flow_discrepancy_count,
                "input_gap_count": catalog_flow_input_gap_count,
            },
            "root_cause_catalog_flow_blockers",
        )

    root_cause_catalog_flow_governance_canonical_gap_count = int_value(
        root_cause_catalog_flow_governance.get("canonical_gap_count")
    )
    root_cause_catalog_flow_governance_source_drift_count = int_value(
        root_cause_catalog_flow_governance.get("source_drift_count")
    )
    if (
        root_cause_catalog_flow_governance_canonical_gap_count > 0
        or root_cause_catalog_flow_governance_source_drift_count > 0
    ):
        append_action(
            {
                "source": "root_cause_catalog_flow_governance",
                "artifact": first_present(
                    root_cause_catalog_flow_governance.get("canonical_source"),
                    "native_evidence_query_result.json.summary",
                ),
                "category": "catalog_flow_discrepancy",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": "current_repo_fixable",
                "action": "Refresh the canonical Catalog and Evidence Query snapshot, then repair any remaining Catalog Flow source gap.",
                "canonical_gap_count": (
                    root_cause_catalog_flow_governance_canonical_gap_count
                ),
                "source_drift_count": (
                    root_cause_catalog_flow_governance_source_drift_count
                ),
                "source_drift_fields": list_value(
                    root_cause_catalog_flow_governance.get(
                        "source_drift_fields"
                    )
                ),
                "source_drift_sources": list_value(
                    root_cause_catalog_flow_governance.get(
                        "source_drift_sources"
                    )
                ),
            },
            "root_cause_catalog_flow_blockers",
        )

    native_audit_filter = dict_value(closure.get("evidence_query_native_core_audit_filter"))
    native_audit_blockers = int_value(native_audit_filter.get("audit_blocker_count"))
    native_audit_deferred = int_value(
        native_audit_filter.get("deferred_with_reason_file_count")
    )
    if native_audit_blockers > 0 or native_audit_deferred > 0:
        append_action(
            {
                "source": "native_core_audit_filter",
                "artifact": native_audit_filter.get("source_artifact", "native_core_audit_register.json"),
                "category": "native_core_audit_backlog",
                "severity": "review_blocking",
                "priority": "P0",
                "repair_environment": "current_repo_then_validation_machine",
                "action": "Close deferred native-core audit items or record explicit deferral reasons, then refresh audit register, evidence query, summary, Prometheus, and final report.",
                "audit_blocker_count": native_audit_blockers,
                "deferred_with_reason_file_count": native_audit_deferred,
            },
            "native_core_audit_filter",
        )

    for missing in list_value(closure.get("missing_artifacts")):
        append_action(
            {
                "source": "missing_review_artifact",
                "artifact": missing,
                "category": "artifact_missing",
                "severity": "blocking",
                "priority": "P0",
                "repair_environment": "current_server_when_validation_enabled",
                "action": "Refresh or generate this non-GPU review artifact before release claims.",
            },
            "missing_review_artifacts",
        )
    ranked_actions.sort(key=lambda item: (item[0], item[1]))
    selected_actions = ranked_actions[:limit]
    return TopRepairActionsProjection(
        actions=tuple(action for _, _, _, action in selected_actions),
        selected_trace_sources=tuple(
            dict.fromkeys(trace_source for _, _, trace_source, _ in selected_actions)
        ),
    )


def project_action_source_trace_selection(
    action_source_trace: List[Dict[str, Any]],
    selected_trace_sources: Tuple[str, ...],
) -> List[Dict[str, Any]]:
    selected = set(selected_trace_sources)
    return [
        {
            **item,
            "selected_for_top_actions": item.get("source") in selected,
        }
        for item in action_source_trace
        if isinstance(item, dict)
    ]


def preferred_top_repair_action(
    actions: List[Dict[str, Any]],
) -> Optional[Dict[str, Any]]:
    for item in actions:
        if not isinstance(item, dict):
            continue
        action = first_present(
            item.get("suggested_repair"),
            item.get("action"),
        )
        if not action:
            continue
        return {
            "action": action,
            "source": first_present(item.get("source"), "top_repair_action"),
        }
    return None


def action_source_status(present: bool, action_count: int, source_gap_count: int) -> str:
    if source_gap_count > 0:
        return "source_gap"
    if action_count > 0:
        return "action_available"
    if present:
        return "no_action"
    return "missing_input"


def scorecard_action_source_trace(closure: Dict[str, Any]) -> List[Dict[str, Any]]:
    release_pipeline = dict_value(closure.get("release_pipeline"))
    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    root_cause_catalog_flow_governance = dict_value(
        closure.get("root_cause_catalog_flow_governance")
    )
    catalog_flow_fallback_review_projection = (
        project_final_report_catalog_flow_fallback_review(
            dict_value(closure.get("catalog_flow_fallback_removal_queue")),
            dict_value(
                closure.get(
                    "root_cause_catalog_flow_fallback_removal_shortest_path"
                )
            ),
            dict_value(
                closure.get(
                    "root_cause_catalog_flow_fallback_removal_parity_gate"
                )
            ),
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    evidence_query_review = dict_value(closure.get("evidence_query_repair_path_review"))
    evidence_query_native_core_audit_filter = dict_value(
        closure.get("evidence_query_native_core_audit_filter")
    )
    evidence_query_catalog_filter = dict_value(
        closure.get("evidence_query_catalog_flow_filter")
    )
    evidence_query_ux = dict_value(closure.get("evidence_query_ux"))
    repair_path_diagnostics = dict_value(
        closure.get("repair_path_diagnostics")
    )
    repair_path_stats = dict_value(repair_path_diagnostics.get("all"))
    catalog_flow_repair_path_stats = dict_value(
        repair_path_diagnostics.get("catalog_flow")
    )
    snapshot_diff_repair_path_stats = dict_value(
        repair_path_diagnostics.get("evidence_query_snapshot_diff")
    )
    missing_artifacts = list_value(closure.get("missing_artifacts"))

    root_cause_count = int_value(root_cause_review.get("repair_card_count"))
    root_cause_catalog_flow_action_count = int_value(
        first_present(
            root_cause_review.get("catalog_flow_repair_card_count"),
            root_cause_review.get("catalog_flow_root_cause_count"),
        )
    )
    root_cause_catalog_flow_governance_canonical_gap_count = int_value(
        root_cause_catalog_flow_governance.get("canonical_gap_count")
    )
    root_cause_catalog_flow_governance_source_drift_count = int_value(
        root_cause_catalog_flow_governance.get("source_drift_count")
    )
    root_cause_catalog_flow_governance_issue_count = max(
        root_cause_catalog_flow_governance_canonical_gap_count,
        root_cause_catalog_flow_governance_source_drift_count,
    )
    repair_path_count = int_value(release_pipeline.get("repair_path_count"))
    repair_path_group_count = int_value(
        release_pipeline.get("repair_path_group_count")
    )
    repair_path_environment_group_count = int_value(
        release_pipeline.get("repair_path_environment_group_count")
    )
    repair_path_category_group_count = int_value(
        release_pipeline.get("repair_path_category_group_count")
    )
    repair_path_current_repo_action_group_count = int_value(
        release_pipeline.get("repair_path_current_repo_action_group_count")
    )
    repair_path_target_or_validation_action_group_count = int_value(
        release_pipeline.get(
            "repair_path_target_or_validation_action_group_count"
        )
    )
    release_pipeline_source_location_count = int_value(
        first_present(
            release_pipeline.get("repair_path_source_location_count"),
            repair_path_stats.get("source_location_path_count"),
        )
    )
    release_pipeline_source_location_gap_count = int_value(
        first_present(
            release_pipeline.get("repair_path_source_location_gap_count"),
            repair_path_stats.get("source_location_gap_count"),
        )
    )
    release_pipeline_catalog_flow_repair_path_count = int_value(
        release_pipeline.get("catalog_flow_repair_path_count")
    )
    release_pipeline_catalog_flow_field_coverage_repair_path_count = int_value(
        release_pipeline.get("catalog_flow_field_coverage_repair_path_count")
    )
    release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_count = int_value(
        release_pipeline.get(
            "catalog_flow_fallback_removal_shortest_path_repair_path_count"
        )
    )
    release_pipeline_catalog_flow_fallback_removal_shortest_path_source_location_count = int_value(
        release_pipeline.get(
            "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count"
        )
    )
    release_pipeline_catalog_flow_fallback_removal_shortest_path_source_location_gap_count = int_value(
        release_pipeline.get(
            "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count"
        )
    )
    release_pipeline_catalog_flow_fallback_removal_shortest_path_evidence_counter_count = int_value(
        release_pipeline.get(
            "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count"
        )
    )
    release_pipeline_catalog_flow_source_location_count = int_value(
        first_present(
            release_pipeline.get("catalog_flow_repair_path_source_location_count"),
            catalog_flow_repair_path_stats.get("source_location_path_count"),
        )
    )
    release_pipeline_catalog_flow_source_location_gap_count = int_value(
        first_present(
            release_pipeline.get("catalog_flow_repair_path_source_location_gap_count"),
            catalog_flow_repair_path_stats.get("source_location_gap_count"),
        )
    )
    release_pipeline_snapshot_diff_repair_path_count = int_value(
        release_pipeline.get("evidence_query_snapshot_diff_repair_path_count")
    )
    release_pipeline_snapshot_diff_source_location_count = int_value(
        first_present(
            release_pipeline.get("evidence_query_snapshot_diff_repair_path_source_location_count"),
            snapshot_diff_repair_path_stats.get("source_location_path_count"),
        )
    )
    release_pipeline_snapshot_diff_source_location_gap_count = int_value(
        first_present(
            release_pipeline.get("evidence_query_snapshot_diff_repair_path_source_location_gap_count"),
            snapshot_diff_repair_path_stats.get("source_location_gap_count"),
        )
    )
    evidence_query_source_gap_count = int_value(evidence_query_review.get("repair_path_source_gap_count"))
    evidence_query_source_location_count = int_value(
        evidence_query_review.get("repair_path_source_location_count")
    )
    evidence_query_source_location_gap_count = int_value(
        evidence_query_review.get("repair_path_source_location_gap_count")
    )
    evidence_query_action_count = max(
        int_value(evidence_query_review.get("repair_path_count")),
        evidence_query_source_location_count + evidence_query_source_location_gap_count,
        1 if evidence_query_review.get("first_action") else 0,
    )
    missing_artifact_count = len(missing_artifacts)
    native_core_audit_action_count = max(
        int_value(evidence_query_native_core_audit_filter.get("audit_blocker_count")),
        int_value(evidence_query_native_core_audit_filter.get("deferred_with_reason_file_count")),
        int_value(evidence_query_native_core_audit_filter.get("issue_count")),
        int_value(evidence_query_native_core_audit_filter.get("source_unreadable_count")),
        int_value(evidence_query_native_core_audit_filter.get("issue_group_count")),
        int_value(evidence_query_native_core_audit_filter.get("repair_card_count")),
        int_value(evidence_query_native_core_audit_filter.get("repair_card_source_location_gap_count")),
        int_value(evidence_query_native_core_audit_filter.get("current_repo_repair_card_count")),
        int_value(evidence_query_native_core_audit_filter.get("target_machine_action_count")),
        int_value(evidence_query_native_core_audit_filter.get("review_worklist_item_count")),
        int_value(evidence_query_native_core_audit_filter.get("open_review_item_count")),
        int_value(evidence_query_native_core_audit_filter.get("review_worklist_source_location_gap_count")),
        int_value(evidence_query_native_core_audit_filter.get("high_priority_review_item_count")),
        int_value(evidence_query_native_core_audit_filter.get("subsystem_review_backlog_count")),
        int_value(evidence_query_native_core_audit_filter.get("audit_closure_shortest_path_step_count")),
        int_value(evidence_query_native_core_audit_filter.get("audit_closure_shortest_path_source_location_gap_count")),
        int_value(evidence_query_native_core_audit_filter.get("read_issue_count")),
    )
    native_core_audit_source_gap_count = (
        0
        if truthy_value(evidence_query_native_core_audit_filter.get("filter_query_present"))
        else 1
    )
    root_cause_source_location_count = int_value(root_cause_review.get("source_location_card_count"))
    root_cause_source_location_gap_count = max(root_cause_count - root_cause_source_location_count, 0)
    root_cause_catalog_flow_source_location_count = int_value(
        root_cause_review.get("catalog_flow_source_location_card_count")
    )
    root_cause_catalog_flow_source_location_gap_count = max(
        root_cause_catalog_flow_action_count -
        root_cause_catalog_flow_source_location_count,
        0,
    )
    fallback_queue = catalog_flow_fallback_review_projection.queue

    root_cause_present = truthy_value(root_cause_review.get("direct_artifact_present"))
    release_pipeline_present = truthy_value(release_pipeline.get("present"))
    evidence_query_present = truthy_value(evidence_query_review.get("evidence_query_artifact_present"))
    closure_present = truthy_value(release_pipeline.get("has_non_gpu_review_closure"))
    fallback_queue_present = bool(fallback_queue.get("trace_present"))

    root_cause_gap_count = 0 if root_cause_present else 1
    release_pipeline_gap_count = 0 if release_pipeline_present and closure_present else 1
    evidence_query_gap_count = evidence_query_source_gap_count
    if not evidence_query_present:
        evidence_query_gap_count += 1

    fallback_queue_action_count = int_value(
        fallback_queue.get("trace_action_count")
    )
    sources = [
        {
            "source": "root_cause_repair_cards",
            "upstream_artifact": "native_evidence_query_result.json",
            "present": root_cause_present,
            "action_count": root_cause_count,
            "source_gap_count": root_cause_gap_count,
            "source_location_count": root_cause_source_location_count,
            "source_location_gap_count": root_cause_source_location_gap_count,
            "first_source_location": root_cause_review.get("first_source_location"),
            "priority_order": 1,
            "checklist_status": action_source_status(root_cause_present, root_cause_count, root_cause_gap_count),
        },
        {
            "source": "root_cause_catalog_flow_blockers",
            "upstream_artifact": "native_evidence_query_result.json",
            "present": root_cause_present,
            "action_count": max(
                root_cause_catalog_flow_action_count,
                root_cause_catalog_flow_governance_issue_count,
            ),
            "source_gap_count": (
                root_cause_gap_count
                + root_cause_catalog_flow_governance_canonical_gap_count
                + root_cause_catalog_flow_governance_source_drift_count
            ),
            "source_location_count": root_cause_catalog_flow_source_location_count,
            "source_location_gap_count": root_cause_catalog_flow_source_location_gap_count,
            "canonical_gap_count": (
                root_cause_catalog_flow_governance_canonical_gap_count
            ),
            "source_drift_count": (
                root_cause_catalog_flow_governance_source_drift_count
            ),
            "source_drift_fields": root_cause_catalog_flow_governance.get(
                "source_drift_fields", []
            ),
            "source_drift_sources": root_cause_catalog_flow_governance.get(
                "source_drift_sources", []
            ),
            "first_source_location": root_cause_review.get("catalog_flow_first_source_location"),
            "priority_order": 2,
            "checklist_status": action_source_status(
                root_cause_present,
                max(
                    root_cause_catalog_flow_action_count,
                    root_cause_catalog_flow_governance_issue_count,
                ),
                (
                    root_cause_gap_count
                    + root_cause_catalog_flow_governance_canonical_gap_count
                    + root_cause_catalog_flow_governance_source_drift_count
                ),
            ),
        },
        {
            "source": "catalog_flow_fallback_removal_queue",
            "upstream_artifact": fallback_queue.get("canonical_source"),
            "present": fallback_queue_present,
            "action_count": fallback_queue_action_count,
            "source_gap_count": fallback_queue.get("canonical_gap_count"),
            "source_location_count": fallback_queue.get("item_count"),
            "source_location_gap_count": fallback_queue.get(
                "canonical_gap_count"
            ),
            "source_drift_count": fallback_queue.get("source_drift_count"),
            "source_drift_fields": fallback_queue.get("source_drift_fields", []),
            "source_drift_sources": fallback_queue.get("source_drift_sources", []),
            "first_source_location": "native_core/src/release_evidence_pipeline_main.cpp",
            "priority_order": 3,
            "checklist_status": action_source_status(
                fallback_queue_present,
                fallback_queue_action_count,
                int_value(fallback_queue.get("canonical_gap_count")),
            ),
            "current_repo_action_count": fallback_queue.get(
                "current_repo_action_count"
            ),
            "target_machine_action_count": fallback_queue.get(
                "target_machine_action_count"
            ),
        },
        {
            "source": "release_pipeline_repair_paths",
            "upstream_artifact": "native_release_evidence_pipeline.json",
            "present": release_pipeline_present and closure_present,
            "action_count": repair_path_count,
            "source_gap_count": release_pipeline_gap_count,
            "source_location_count": release_pipeline_source_location_count,
            "source_location_gap_count": release_pipeline_source_location_gap_count,
            "first_source_location": repair_path_stats.get("first_source_location"),
            "priority_order": 4,
            "checklist_status": action_source_status(
                release_pipeline_present and closure_present,
                repair_path_count,
                release_pipeline_gap_count,
            ),
        },
        {
            "source": "release_pipeline_catalog_flow_repair_paths",
            "upstream_artifact": "native_release_evidence_pipeline.json",
            "present": release_pipeline_present and closure_present,
            "action_count": release_pipeline_catalog_flow_repair_path_count,
            "source_gap_count": release_pipeline_gap_count,
            "source_location_count": release_pipeline_catalog_flow_source_location_count,
            "source_location_gap_count": release_pipeline_catalog_flow_source_location_gap_count,
            "first_source_location": catalog_flow_repair_path_stats.get("first_source_location"),
            "priority_order": 5,
            "checklist_status": action_source_status(
                release_pipeline_present and closure_present,
                release_pipeline_catalog_flow_repair_path_count,
                release_pipeline_gap_count,
            ),
        },
        {
            "source": "release_pipeline_evidence_query_snapshot_diff_repair_paths",
            "upstream_artifact": "native_release_evidence_pipeline.json",
            "present": release_pipeline_present and closure_present,
            "action_count": release_pipeline_snapshot_diff_repair_path_count,
            "source_gap_count": release_pipeline_gap_count,
            "source_location_count": release_pipeline_snapshot_diff_source_location_count,
            "source_location_gap_count": release_pipeline_snapshot_diff_source_location_gap_count,
            "first_source_location": snapshot_diff_repair_path_stats.get("first_source_location"),
            "priority_order": 7,
            "checklist_status": action_source_status(
                release_pipeline_present and closure_present,
                release_pipeline_snapshot_diff_repair_path_count,
                release_pipeline_gap_count,
            ),
        },
        {
            "source": "evidence_query_first_action",
            "upstream_artifact": evidence_query_review.get("upstream_artifact", "native_evidence_query_result.json"),
            "present": evidence_query_present,
            "action_count": evidence_query_action_count,
            "source_gap_count": evidence_query_gap_count,
            "source_location_count": evidence_query_source_location_count,
            "source_location_gap_count": evidence_query_source_location_gap_count,
            "priority_order": 8,
            "checklist_status": action_source_status(
                evidence_query_present,
                evidence_query_action_count,
                evidence_query_gap_count,
            ),
        },
        {
            "source": "missing_review_artifacts",
            "upstream_artifact": "final_report_index.json.non_gpu_review_closure.missing_artifacts",
            "present": True,
            "action_count": missing_artifact_count,
            "source_gap_count": missing_artifact_count,
            "source_location_count": 0,
            "source_location_gap_count": 0,
            "priority_order": 9,
            "checklist_status": action_source_status(True, missing_artifact_count, missing_artifact_count),
        },
        {
            "source": "native_core_audit_filter",
            "upstream_artifact": "native_evidence_query_result.json",
            "present": truthy_value(evidence_query_native_core_audit_filter.get("filter_query_present")),
            "action_count": native_core_audit_action_count,
            "source_gap_count": native_core_audit_source_gap_count,
            "source_location_count": int_value(
                evidence_query_native_core_audit_filter.get("repair_card_source_location_count")
            ),
            "source_location_gap_count": int_value(
                evidence_query_native_core_audit_filter.get("repair_card_source_location_gap_count")
            ),
            "priority_order": 12,
            "checklist_status": action_source_status(
                truthy_value(evidence_query_native_core_audit_filter.get("filter_query_present")),
                native_core_audit_action_count,
                native_core_audit_source_gap_count,
            ),
        },
    ]
    return sources


def deduped_non_gpu_root_causes(blocker_counts: Dict[str, int]) -> List[Dict[str, Any]]:
    specs = (
        (
            "missing_review_artifacts",
            ("missing_review_artifacts", "missing_release_pipeline_closure"),
            "current_repo_or_validation_machine",
            "Refresh missing required review artifacts before release claims.",
        ),
        (
            "catalog_flow_discrepancy",
            (
                "catalog_flow_discrepancies",
                "catalog_flow_input_gaps",
                "release_pipeline_catalog_flow_repair_paths",
                "release_pipeline_catalog_flow_source_location_gaps",
                "root_cause_catalog_flow_root_causes",
                "root_cause_catalog_flow_source_location_gaps",
                "explain_catalog_flow_discrepancy_cards",
            ),
            "current_repo_then_validation_machine",
            "Repair catalog/new-artifact-flow discrepancies, then refresh catalog, flow check, summary, Prometheus, and final report.",
        ),
        (
            "catalog_flow_fallback_removal_queue",
            (
                "catalog_flow_fallback_removal_queue_blocked",
                "catalog_flow_fallback_removal_queue_parity_refresh_required",
                "catalog_flow_fallback_removal_queue_current_repo_actions",
                "catalog_flow_fallback_removal_queue_target_machine_actions",
            ),
            "current_repo_then_validation_machine",
            "Inspect catalog fallback-removal queue, keep legacy fallback surfaces until parity refresh evidence exists, then remove fallback surfaces one by one.",
        ),
        (
            "evidence_query_snapshot_diff_review",
            (
                "release_pipeline_evidence_query_snapshot_diff_repair_paths",
                "release_pipeline_evidence_query_snapshot_diff_source_location_gaps",
            ),
            "current_repo_fixable",
            "Review bounded Evidence Query snapshot JSON path differences, decide whether the evidence drift is expected, then refresh release/report surfaces.",
        ),
        (
            "native_core_audit_backlog",
            (
                "release_pipeline_native_core_audit_repair_paths",
                "release_pipeline_native_core_audit_source_location_gaps",
                "evidence_query_native_core_audit_blockers",
                "evidence_query_native_core_audit_deferred_files",
                "evidence_query_native_core_audit_issues",
                "evidence_query_native_core_audit_read_issues",
                "evidence_query_native_core_audit_manifest_invalid_overrides",
                "evidence_query_native_core_audit_manifest_unmatched_overrides",
                "evidence_query_native_core_audit_open_review_items",
                "evidence_query_native_core_audit_high_priority_review_items",
                "evidence_query_native_core_audit_coverage_source_location_gaps",
                "explain_native_core_audit_filter_cards",
                "explain_native_core_audit_deferred_files",
                "explain_native_core_audit_issues",
                "explain_native_core_audit_open_review_items",
                "explain_native_core_audit_high_priority_review_items",
                "explain_native_core_audit_manifest_invalid_overrides",
                "explain_native_core_audit_manifest_unmatched_overrides",
                "explain_native_core_audit_coverage_source_location_gaps",
            ),
            "current_repo_then_validation_machine",
            "Close deferred native-core audit items or record explicit deferral reasons, then refresh audit register, evidence query, summary, Prometheus, and final report.",
        ),
        (
            "source_location_or_action_source_gaps",
            (
                "release_pipeline_repair_path_source_location_gaps",
                "root_cause_source_location_gaps",
                "evidence_query_repair_path_source_gaps",
                "evidence_query_repair_path_source_location_gaps",
                "evidence_query_action_source_gaps",
                "evidence_query_action_source_location_gaps",
                "explain_path_source_location_blocked",
                "explain_action_source_filter_cards",
                "explain_action_source_filter_source_gaps",
                "explain_action_source_filter_source_location_gaps",
            ),
            "current_repo",
            "Attach exact source locations and action sources to repair paths before review handoff.",
        ),
        (
            "product_replay_semantic_linkage",
            (
                "root_cause_product_replay_semantic_linkage_root_cause_count",
                "root_cause_product_replay_semantic_linkage_blocker_count",
                "root_cause_product_replay_semantic_linkage_pending_event_count",
                "root_cause_product_replay_semantic_linkage_pending_rule_count",
                "root_cause_product_replay_semantic_linkage_blocked_correlation_count",
                "root_cause_product_replay_semantic_linkage_target_or_validation_action_count",
                "root_cause_product_replay_semantic_linkage_blocks_release",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_blockers",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_read_issues",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_pending_events",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_blocked_correlations",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_target_or_validation_actions",
                "release_pipeline_product_replay_semantic_linkage_repair_path_count",
                "release_pipeline_product_replay_semantic_linkage_repair_path_source_location_gap_count",
                "release_pipeline_evidence_query_product_replay_semantic_linkage_repair_path_count",
                "release_pipeline_root_cause_product_replay_semantic_linkage_repair_path_count",
                "root_cause_product_replay_semantic_linkage_repair_path_count",
                "root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count",
            ),
            "target_product_or_replay_evidence",
            "Refresh product replay semantic events, alignment, referenced native artifacts, Evidence Query, and release gate artifacts before using replay evidence in release claims.",
        ),
        (
            "target_machine_validation_not_run",
            ("not_run_items", "not_run_handoff_items"),
            "target_machine",
            "Run native refresh, summary, Prometheus, final report, CUDA/product replay, or execute steps only after validation is re-enabled.",
        ),
        (
            "remaining_handoff_required",
            ("required_handoff_items",),
            "current_repo_or_target_machine",
            "Complete remaining non-export handoff actions before claiming report-ready state.",
        ),
    )
    causes: List[Dict[str, Any]] = []
    for root_cause, keys, owner, action in specs:
        nonzero = [
            {"signal": key, "count": int_value(blocker_counts.get(key))}
            for key in keys
            if int_value(blocker_counts.get(key)) > 0
        ]
        if not nonzero:
            continue
        causes.append(
            {
                "root_cause": root_cause,
                "representative_count": max(item["count"] for item in nonzero),
                "raw_signal_count": len(nonzero),
                "signals": nonzero,
                "owner": owner,
                "recommended_action": action,
            }
        )
    return causes


@dataclass(frozen=True)
class NonGpuReviewOutcomeProjection:
    scorecard_fields: Dict[str, Any]

    def to_scorecard_dict(self) -> Dict[str, Any]:
        return dict(self.scorecard_fields)


@dataclass(frozen=True)
class NonGpuReviewFragments:
    scorecard_fields: Dict[str, Any]
    blocker_counts: Dict[str, int]


def project_non_gpu_release_audit_fragments(
    release_pipeline: Dict[str, Any],
    root_cause_review: Dict[str, Any],
    root_cause_native_audit_coverage_review: Dict[str, Any],
    root_cause_native_audit_manifest_review: Dict[str, Any],
) -> NonGpuReviewFragments:
    """Compose existing Native Audit report views."""
    native_coverage = project_root_cause_native_audit_coverage(
        release_pipeline,
        root_cause_native_audit_coverage_review,
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    native_manifest = project_root_cause_native_audit_manifest(
        release_pipeline,
        root_cause_native_audit_manifest_review,
        count_coercer=int_value,
    )
    native_residual = project_final_report_root_cause_native_audit_residual(
        root_cause_review,
        count_coercer=int_value,
    )
    return NonGpuReviewFragments(
        scorecard_fields={
            **native_coverage.to_final_report_scorecard_dict(),
            **native_manifest.to_final_report_scorecard_dict(),
            **native_residual.to_scorecard_dict(),
        },
        blocker_counts={
            **native_coverage.to_final_report_blocker_counts_dict(),
            **native_manifest.to_final_report_blocker_counts_dict(),
        },
    )


def project_non_gpu_evidence_query_fragments(
    evidence_query_ux: Dict[str, Any],
    evidence_query_native_core_audit_filter: Dict[str, Any],
    root_cause_review: Dict[str, Any],
    evidence_query_review: Dict[str, Any],
) -> NonGpuReviewFragments:
    """Compose the canonical Evidence Query report views."""
    ux_review = project_final_report_evidence_query_ux_review_scorecard(
        evidence_query_ux,
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    query_native_audit = project_final_report_evidence_query_native_audit_scorecard(
        evidence_query_native_core_audit_filter,
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    source_location = project_final_report_review_source_location(
        root_cause_review,
        evidence_query_review,
        count_coercer=int_value,
    )
    return NonGpuReviewFragments(
        scorecard_fields={
            **project_final_report_evidence_query_ux_selector_scorecard(
                evidence_query_ux,
                count_coercer=int_value,
            ),
            **project_final_report_evidence_query_ux_operation_snapshot_scorecard(
                evidence_query_ux,
                count_coercer=int_value,
            ),
            **ux_review.to_scorecard_dict(),
            **query_native_audit.to_scorecard_dict(),
            **source_location.to_scorecard_dict(),
        },
        blocker_counts={
            **ux_review.to_blocker_counts_dict(),
            **query_native_audit.to_blocker_counts_dict(),
            **source_location.to_blocker_counts_dict(),
        },
    )


def project_non_gpu_review_outcome(
    closure: Dict[str, Any],
    blocker_counts: Dict[str, int],
    *,
    missing_artifact_count: int,
    closure_present: bool,
) -> NonGpuReviewOutcomeProjection:
    """Project review status and operator guidance from existing evidence."""
    release_pipeline = dict_value(closure.get("release_pipeline"))
    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    blocking_signal_count = sum(
        1 for value in blocker_counts.values() if value > 0
    )
    deduped_root_causes = deduped_non_gpu_root_causes(blocker_counts)

    if missing_artifact_count > 0 or not closure_present:
        review_decision = "review_incomplete"
    elif blocking_signal_count > 0:
        review_decision = "repair_required"
    else:
        review_decision = "review_ready"

    base_action_source_trace = scorecard_action_source_trace(closure)
    top_repair_actions_projection = top_non_gpu_repair_actions(
        closure,
        base_action_source_trace,
    )
    action_source_trace = project_action_source_trace_selection(
        base_action_source_trace,
        top_repair_actions_projection.selected_trace_sources,
    )
    top_repair_actions = list(top_repair_actions_projection.actions)
    preferred_repair_action = preferred_top_repair_action(top_repair_actions)
    if preferred_repair_action is not None:
        next_action = preferred_repair_action["action"]
        next_action_source = preferred_repair_action["source"]
    elif deduped_root_causes:
        first_root_cause = dict_value(deduped_root_causes[0])
        next_action = first_present(
            first_root_cause.get("recommended_action"),
            "Inspect the first deduplicated root cause and refresh the affected evidence surfaces.",
        )
        next_action_source = "deduped_root_cause:{root_cause}".format(
            root_cause=first_present(first_root_cause.get("root_cause"), "unknown")
        )
    elif blocking_signal_count > 0:
        next_action = "Inspect the non-zero blocker_counts entries, classify the unmapped blocker source, then refresh the affected evidence and report surfaces."
        next_action_source = "blocker_counts"
    else:
        next_action = "Refresh benchmark summary and final report after evidence artifacts are current."
        next_action_source = "final_report_refresh"

    return NonGpuReviewOutcomeProjection(
        scorecard_fields={
            "review_decision": review_decision,
            "blocking_signal_count": blocking_signal_count,
            "deduped_root_cause_count": len(deduped_root_causes),
            "deduped_root_causes": deduped_root_causes,
            "repair_ownership": {
                "current_repo": int_value(
                    first_present(
                        release_pipeline.get("current_repo_repair_count"),
                        root_cause_review.get("current_repo_repair_card_count"),
                    )
                ),
                "target_machine": int_value(
                    first_present(
                        release_pipeline.get("target_machine_repair_count"),
                        root_cause_review.get("target_machine_repair_card_count"),
                    )
                ),
                "flow_check": int_value(
                    root_cause_review.get("flow_check_repair_card_count")
                ),
            },
            "next_action": next_action,
            "next_action_source": next_action_source,
            "top_repair_action_count": len(top_repair_actions),
            "top_repair_actions": top_repair_actions,
            "action_source_trace_count": len(action_source_trace),
            "action_source_trace_gap_count": sum(
                int_value(dict_value(item).get("source_gap_count"))
                for item in action_source_trace
            ),
            "action_source_trace_source_location_gap_count": sum(
                int_value(dict_value(item).get("source_location_gap_count"))
                for item in action_source_trace
            ),
            "action_source_trace_available_count": sum(
                1
                for item in action_source_trace
                if int_value(dict_value(item).get("action_count")) > 0
            ),
            "action_source_trace": action_source_trace,
        }
    )


def non_gpu_review_scorecard(
    closure: Dict[str, Any],
) -> Dict[str, Any]:
    if not isinstance(closure, dict) or not closure:
        return {}

    required = list_value(closure.get("required_artifacts"))
    present = list_value(closure.get("present_artifacts"))
    missing = list_value(closure.get("missing_artifacts"))
    release_pipeline = dict_value(closure.get("release_pipeline"))
    evidence_query_catalog_filter = dict_value(closure.get("evidence_query_catalog_flow_filter"))
    evidence_query_native_core_audit_filter = dict_value(
        closure.get("evidence_query_native_core_audit_filter")
    )
    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    root_cause_catalog_flow_shortest_path = dict_value(
        closure.get(
            "root_cause_catalog_flow_fallback_removal_shortest_path"
        )
    )
    root_cause_catalog_flow_parity_gate = dict_value(
        closure.get("root_cause_catalog_flow_fallback_removal_parity_gate")
    )
    evidence_query_review = dict_value(closure.get("evidence_query_repair_path_review"))
    evidence_query_selector_resolution = dict_value(
        closure.get("evidence_query_selector_resolution")
    )
    evidence_query_explain_tree_review = dict_value(
        closure.get("evidence_query_explain_tree_review")
    )
    evidence_query_blocker_drilldown_review = dict_value(
        closure.get("evidence_query_blocker_drilldown_review")
    )
    evidence_query_readiness_path_drilldown_review = dict_value(
        closure.get("evidence_query_readiness_path_drilldown_review")
    )
    evidence_query_cross_artifact_review = dict_value(
        closure.get("evidence_query_cross_artifact_blocker_explanation")
    )
    evidence_query_product_replay_linkage_review = dict_value(
        closure.get("evidence_query_product_replay_semantic_linkage")
    )
    root_cause_native_audit_coverage_review = dict_value(
        closure.get("root_cause_native_core_audit_review_coverage_gate")
    )
    root_cause_native_audit_manifest_review = dict_value(
        closure.get("root_cause_native_core_audit_review_manifest")
    )
    evidence_query_repair_group_plan = dict_value(
        closure.get("evidence_query_repair_group_plan")
    )
    evidence_query_ux = dict_value(closure.get("evidence_query_ux"))
    release_audit_fragments = project_non_gpu_release_audit_fragments(
        release_pipeline,
        root_cause_review,
        root_cause_native_audit_coverage_review,
        root_cause_native_audit_manifest_review,
    )
    evidence_query_summary = project_evidence_query_ux_summary(evidence_query_ux)
    evidence_query_fragments = project_non_gpu_evidence_query_fragments(
        evidence_query_ux,
        evidence_query_native_core_audit_filter,
        root_cause_review,
        evidence_query_review,
    )

    release_pipeline_headline = project_final_report_release_pipeline_headline(
        release_pipeline,
        count_coercer=int_value,
    )
    release_pipeline_headline_scorecard = (
        release_pipeline_headline.to_scorecard_dict()
    )
    release_pipeline_linkage_projection = project_final_report_release_linkage(
        release_pipeline,
        {},
        {},
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    release_pipeline_linkage_scorecard = (
        release_pipeline_linkage_projection.to_release_pipeline_scorecard_dict(
            count_coercer=int_value,
        )
    )
    release_pipeline_linkage_blocker_counts = (
        release_pipeline_linkage_projection.to_final_report_blocker_counts_dict(
            count_coercer=int_value,
        )
    )
    release_pipeline_selector_action_plan_projection = (
        project_final_report_evidence_query_selector_action_plan(
            release_pipeline,
            evidence_query_selector_resolution,
            count_coercer=int_value,
        )
    )
    release_pipeline_repair_card_navigation_projection = (
        project_final_report_evidence_query_repair_navigation(
            release_pipeline,
            evidence_query_repair_group_plan,
            value_coercer=int_value,
        )
    )
    release_pipeline_repair_card_navigation_scorecard = (
        release_pipeline_repair_card_navigation_projection.to_release_pipeline_scorecard_dict()
    )
    release_pipeline_repair_guidance_projection = (
        project_final_report_evidence_query_repair_guidance(
            release_pipeline,
            evidence_query_repair_group_plan,
            count_coercer=int_value,
            presence_coercer=lambda value: 1 if truthy_value(value) else 0,
        )
    )
    release_pipeline_repair_guidance_scorecard = (
        release_pipeline_repair_guidance_projection.to_release_pipeline_scorecard_dict()
    )
    release_pipeline_source_location_projection = (
        project_final_report_evidence_query_source_location(
            release_pipeline,
            evidence_query_repair_group_plan,
            {},
            count_coercer=int_value,
        )
    )
    release_pipeline_source_location_scorecard = (
        release_pipeline_source_location_projection.to_release_pipeline_scorecard_dict()
    )
    release_pipeline_catalog_flow_fallback_projection = (
        project_final_report_catalog_flow_fallback_scorecard(
            release_pipeline,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_catalog_flow_fallback_scorecard = (
        release_pipeline_catalog_flow_fallback_projection.to_scorecard_dict()
    )
    release_pipeline_catalog_flow_fallback_blocker_counts = (
        release_pipeline_catalog_flow_fallback_projection.to_blocker_counts_dict()
    )
    release_pipeline_source_exactness_projection = (
        project_final_report_evidence_query_source_exactness(
            release_pipeline,
            evidence_query_repair_group_plan,
            evidence_query_explain_tree_review,
            evidence_query_summary,
            count_coercer=int_value,
        )
    )
    release_pipeline_source_exactness_scorecard = (
        release_pipeline_source_exactness_projection.to_release_pipeline_scorecard_dict()
    )
    release_pipeline_blocker_drilldown_projection = (
        project_final_report_evidence_query_blocker_drilldown(
            release_pipeline,
            evidence_query_blocker_drilldown_review,
            evidence_query_summary,
            count_coercer=int_value,
        )
    )
    release_pipeline_blocker_drilldown_scorecard = (
        release_pipeline_blocker_drilldown_projection.to_release_pipeline_scorecard_dict()
    )
    release_pipeline_readiness_path_projection = (
        project_final_report_evidence_query_readiness_path(
            release_pipeline,
            evidence_query_readiness_path_drilldown_review,
            evidence_query_summary,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_readiness_path_scorecard = (
        release_pipeline_readiness_path_projection.to_release_pipeline_scorecard_dict(
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_cross_artifact_projection = (
        project_final_report_evidence_query_cross_artifact(
            release_pipeline,
            evidence_query_cross_artifact_review,
            evidence_query_summary,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_cross_artifact_scorecard = (
        release_pipeline_cross_artifact_projection.to_release_pipeline_scorecard_dict(
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_product_replay_projection = (
        project_release_pipeline_product_replay(
            release_pipeline,
            evidence_query_cross_artifact_review,
            evidence_query_product_replay_linkage_review,
            evidence_query_summary,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_product_replay_scorecard_projection = (
        project_final_report_release_pipeline_product_replay_scorecard(
            release_pipeline_product_replay_projection,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    release_pipeline_product_replay_scorecard = (
        release_pipeline_product_replay_scorecard_projection.to_scorecard_dict()
    )
    release_pipeline_product_replay_blocker_counts = (
        release_pipeline_product_replay_scorecard_projection.to_blocker_counts_dict()
    )
    root_cause_catalog_flow_scorecard_projection = (
        project_final_report_root_cause_catalog_flow_scorecard(
            root_cause_review,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    root_cause_catalog_flow_scorecard = (
        root_cause_catalog_flow_scorecard_projection.to_scorecard_dict()
    )
    root_cause_catalog_flow_blocker_counts = (
        root_cause_catalog_flow_scorecard_projection.to_blocker_counts_dict()
    )
    root_cause_product_replay_projection = project_root_cause_product_replay(
        release_pipeline,
        root_cause_review,
        {},
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    root_cause_product_replay_scorecard = (
        root_cause_product_replay_projection.to_final_report_scorecard_dict()
    )
    root_cause_product_replay_blocker_counts = (
        root_cause_product_replay_projection.to_final_report_blocker_counts_dict()
    )
    release_pipeline_repair_path_scorecard_projection = (
        project_final_report_release_pipeline_repair_path_scorecard(
            release_pipeline,
            release_pipeline_cross_artifact_projection,
            release_pipeline_product_replay_projection,
            root_cause_product_replay_projection,
            count_coercer=int_value,
        )
    )
    release_pipeline_repair_path_scorecard = (
        release_pipeline_repair_path_scorecard_projection.to_scorecard_dict()
    )
    release_pipeline_repair_path_blocker_counts = (
        release_pipeline_repair_path_scorecard_projection.to_blocker_counts_dict()
    )
    evidence_query_catalog_flow_scorecard_projection = (
        project_final_report_evidence_query_catalog_flow_scorecard(
            evidence_query_catalog_filter,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    evidence_query_catalog_flow_scorecard = (
        evidence_query_catalog_flow_scorecard_projection.to_scorecard_dict()
    )
    evidence_query_catalog_flow_blocker_counts = (
        evidence_query_catalog_flow_scorecard_projection.to_blocker_counts_dict()
    )
    closure_present = truthy_value(release_pipeline.get("has_non_gpu_review_closure"))
    catalog_flow_fallback_review_projection = (
        project_final_report_catalog_flow_fallback_review(
            dict_value(closure.get("catalog_flow_fallback_removal_queue")),
            root_cause_catalog_flow_shortest_path,
            root_cause_catalog_flow_parity_gate,
            count_coercer=int_value,
            truthy_coercer=truthy_value,
        )
    )
    catalog_flow_fallback_review_scorecard = (
        catalog_flow_fallback_review_projection.to_scorecard_dict()
    )
    catalog_flow_fallback_review_blocker_counts = (
        catalog_flow_fallback_review_projection.to_blocker_counts_dict()
    )

    blocker_counts = {
        "missing_review_artifacts": len(missing),
        **release_audit_fragments.blocker_counts,
        **release_pipeline_repair_path_blocker_counts,
        **release_pipeline_linkage_blocker_counts,
        **root_cause_catalog_flow_blocker_counts,
        **release_pipeline_catalog_flow_fallback_blocker_counts,
        **release_pipeline_product_replay_blocker_counts,
        **root_cause_product_replay_blocker_counts,
        **evidence_query_fragments.blocker_counts,
        **evidence_query_catalog_flow_blocker_counts,
        **catalog_flow_fallback_review_blocker_counts,
        "missing_release_pipeline_closure": 0 if closure_present else 1,
    }
    review_outcome = project_non_gpu_review_outcome(
        closure,
        blocker_counts,
        missing_artifact_count=len(missing),
        closure_present=closure_present,
    )
    review_outcome_scorecard = review_outcome.to_scorecard_dict()
    return {
        **review_outcome_scorecard,
        **release_pipeline_headline_scorecard,
        **release_audit_fragments.scorecard_fields,
        "artifact_coverage": {
            "required_count": len(required),
            "present_count": len(present),
            "missing_count": len(missing),
            "missing_artifacts": missing,
        },
        "blocker_counts": blocker_counts,
        "root_cause_repair_card_table_count": len(
            list_value(root_cause_review.get("repair_cards"))
        ),
        **root_cause_catalog_flow_scorecard,
        **root_cause_product_replay_scorecard,
        **release_pipeline_catalog_flow_fallback_scorecard,
        **catalog_flow_fallback_review_scorecard,
        **release_pipeline_repair_path_scorecard,
        **release_pipeline_linkage_scorecard,
        **release_pipeline_selector_action_plan_projection.to_scorecard_dict(),
        **release_pipeline_repair_card_navigation_scorecard,
        **release_pipeline_repair_guidance_scorecard,
        **release_pipeline_source_location_scorecard,
        **release_pipeline_source_exactness_scorecard,
        **release_pipeline_blocker_drilldown_scorecard,
        **release_pipeline_readiness_path_scorecard,
        **release_pipeline_cross_artifact_scorecard,
        **release_pipeline_product_replay_scorecard,
        **evidence_query_catalog_flow_scorecard,
        **evidence_query_fragments.scorecard_fields,
        "claim_boundary": (
            "Scorecard summarizes existing non-GPU evidence readiness only; it does not execute repairs, "
            "refresh artifacts, or create benchmark, CUDA timing, replay, execute, or mature-product performance claims."
        ),
    }


@dataclass(frozen=True)
class NonGpuReviewDocuments:
    artifacts: Mapping[str, Optional[Dict[str, Any]]]
    present_artifacts: Sequence[str]
    missing_artifacts: Sequence[str]
    release: Optional[Dict[str, Any]]
    root_cause: Optional[Dict[str, Any]]
    evidence_query: Optional[Dict[str, Any]]
    native_audit: Optional[Dict[str, Any]]
    release_summary: Mapping[str, Any]
    root_cause_summary: Mapping[str, Any]
    evidence_query_summary: Mapping[str, Any]
    native_audit_summary: Mapping[str, Any]
    evidence_query_operation_descriptor: Mapping[str, Any]


@dataclass(frozen=True)
class NonGpuReviewClosureSources:
    document: Mapping[str, Any]
    source_contract_gate: Mapping[str, Any]
    repair_paths: Sequence[Any]
    repair_path_groups: Sequence[Any]
    repair_path_group_plan: Sequence[Any]
    snapshot_diff_review: Mapping[str, Any]
    snapshot_to_release_link: Mapping[str, Any]
    blocker_repair_card_link: Mapping[str, Any]
    evidence_query_explain_tree_review: Mapping[str, Any]
    evidence_query_blocker_drilldown_review: Mapping[str, Any]
    evidence_query_readiness_path_drilldown_review: Mapping[str, Any]
    evidence_query_cross_artifact_review: Mapping[str, Any]
    evidence_query_product_replay_linkage: Mapping[str, Any]
    root_cause_native_audit_coverage: Mapping[str, Any]
    evidence_query_source_location_category_coverage: Mapping[str, Any]
    evidence_query_catalog_flow_filter: Mapping[str, Any]
    root_cause_catalog_flow_fallback_shortest_path: Mapping[str, Any]
    root_cause_catalog_flow_fallback_parity_gate: Mapping[str, Any]
    evidence_query_repair_group_plan: Mapping[str, Any]


@dataclass(frozen=True)
class NonGpuReviewDerivedSources:
    root_cause_cards: Sequence[Any]
    root_cause_card_summaries: Sequence[Mapping[str, Any]]
    root_cause_catalog_flow_card_stats: Mapping[str, Any]
    first_root_cause_card: Mapping[str, Any]
    root_cause_review: Mapping[str, Any]
    evidence_query_review: Mapping[str, Any]


@dataclass(frozen=True)
class NonGpuReviewQuerySources:
    catalog_filter: Mapping[str, Any]
    native_core_audit_filter: Mapping[str, Any]


@dataclass(frozen=True)
class NonGpuReviewSourceContext:
    documents: NonGpuReviewDocuments
    closure: NonGpuReviewClosureSources
    derived: NonGpuReviewDerivedSources
    query: NonGpuReviewQuerySources


def load_non_gpu_review_source_context(
    results_root: Path,
) -> NonGpuReviewSourceContext:
    """Read each non-GPU review artifact once and bind request-owned views."""
    artifacts = {
        name: load_json(results_root / name) for name in NON_GPU_REVIEW_ARTIFACTS
    }
    present_artifacts = [
        name for name, data in artifacts.items() if data is not None
    ]
    missing_artifacts = [name for name, data in artifacts.items() if data is None]

    release = artifacts["native_release_evidence_pipeline.json"]
    evidence_query = artifacts["native_evidence_query_result.json"]
    root_cause = evidence_query
    native_audit = artifacts["native_core_audit_register.json"]
    release_summary = artifact_summary(release)
    evidence_query_summary = artifact_summary(evidence_query)
    root_cause_summary = evidence_query_summary
    native_audit_summary = artifact_summary(native_audit)
    operation_descriptor = (
        dict_value(evidence_query.get("operation_descriptor"))
        if isinstance(evidence_query, dict)
        else {}
    )
    closure = (
        dict_value(release.get("non_gpu_review_closure"))
        if isinstance(release, dict)
        else {}
    )

    root_cause_cards = (
        list_value(root_cause.get("root_cause_repair_cards"))
        if isinstance(root_cause, dict)
        else []
    )
    documents = NonGpuReviewDocuments(
        artifacts=artifacts,
        present_artifacts=present_artifacts,
        missing_artifacts=missing_artifacts,
        release=release,
        root_cause=root_cause,
        evidence_query=evidence_query,
        native_audit=native_audit,
        release_summary=release_summary,
        root_cause_summary=root_cause_summary,
        evidence_query_summary=evidence_query_summary,
        native_audit_summary=native_audit_summary,
        evidence_query_operation_descriptor=operation_descriptor,
    )
    closure_sources = NonGpuReviewClosureSources(
        document=closure,
        source_contract_gate=(
            dict_value(release.get("source_contract_gate"))
            if isinstance(release, dict)
            else {}
        ),
        repair_paths=list_value(closure.get("repair_paths")),
        repair_path_groups=list_value(closure.get("repair_path_groups")),
        repair_path_group_plan=list_value(closure.get("repair_path_group_plan")),
        snapshot_diff_review=dict_value(
            closure.get("evidence_query_snapshot_diff_review")
        ),
        snapshot_to_release_link=dict_value(
            closure.get("evidence_query_snapshot_to_release_link")
        ),
        blocker_repair_card_link=dict_value(
            closure.get("blocker_repair_card_link")
        ),
        evidence_query_explain_tree_review=dict_value(
            closure.get("evidence_query_explain_tree_review")
        ),
        evidence_query_blocker_drilldown_review=dict_value(
            closure.get("evidence_query_blocker_drilldown_review")
        ),
        evidence_query_readiness_path_drilldown_review=dict_value(
            closure.get("evidence_query_readiness_path_drilldown_review")
        ),
        evidence_query_cross_artifact_review=dict_value(
            closure.get("evidence_query_cross_artifact_blocker_explanation")
        ),
        evidence_query_product_replay_linkage=dict_value(
            closure.get("evidence_query_product_replay_semantic_linkage")
        ),
        root_cause_native_audit_coverage=dict_value(
            closure.get("root_cause_native_core_audit_review_coverage_gate")
        ),
        evidence_query_source_location_category_coverage=dict_value(
            closure.get("evidence_query_source_location_category_coverage")
        ),
        evidence_query_catalog_flow_filter=dict_value(
            closure.get("evidence_query_catalog_flow_filter")
        ),
        root_cause_catalog_flow_fallback_shortest_path=dict_value(
            closure.get("root_cause_catalog_flow_fallback_removal_shortest_path")
        ),
        root_cause_catalog_flow_fallback_parity_gate=dict_value(
            closure.get("root_cause_catalog_flow_fallback_removal_parity_gate")
        ),
        evidence_query_repair_group_plan=dict_value(
            closure.get("evidence_query_repair_group_plan")
        ),
    )
    derived_sources = NonGpuReviewDerivedSources(
        root_cause_cards=root_cause_cards,
        root_cause_card_summaries=root_cause_repair_card_summaries(
            root_cause_cards
        ),
        root_cause_catalog_flow_card_stats=root_cause_category_card_stats(
            root_cause_cards,
            "catalog_flow_discrepancy",
        ),
        first_root_cause_card=(
            root_cause_cards[0]
            if root_cause_cards and isinstance(root_cause_cards[0], dict)
            else {}
        ),
        root_cause_review=(
            dict_value(closure.get("root_cause_repair_review"))
        ),
        evidence_query_review=(
            dict_value(evidence_query.get("repair_path_query"))
            if isinstance(evidence_query, dict)
            else {}
        ),
    )
    query_sources = NonGpuReviewQuerySources(
        catalog_filter=(
            dict_value(evidence_query.get("catalog_flow_filter_query"))
            if isinstance(evidence_query, dict)
            else {}
        ),
        native_core_audit_filter=(
            dict_value(evidence_query.get("native_core_audit_filter_query"))
            if isinstance(evidence_query, dict)
            else {}
        ),
    )
    return NonGpuReviewSourceContext(
        documents=documents,
        closure=closure_sources,
        derived=derived_sources,
        query=query_sources,
    )


@dataclass(frozen=True)
class NonGpuReviewClosureViews:
    repair_path_diagnostics: Any
    repair_path_groups: Any
    release_linkage: Any
    repair_navigation: Any
    repair_guidance: Any
    source_location: Any
    catalog_flow_shortest_path: Any
    catalog_flow_parity_gate: Any
    source_exactness: Any
    blocker_drilldown: Any
    readiness_path: Any
    cross_artifact: Any
    product_replay: Any
    root_cause_native_audit_coverage: Any
    root_cause_native_audit_manifest: Any
    root_cause_catalog_flow_governance: Any
    catalog_flow_fallback_removal_queue: Mapping[str, Any]
    root_cause_repair_review: Mapping[str, Any]
    evidence_query_repair_path_review: Mapping[str, Any]
    evidence_query_ux: Mapping[str, Any]


def compose_non_gpu_review_closure(
    source_context: NonGpuReviewSourceContext,
    views: NonGpuReviewClosureViews,
) -> Dict[str, Any]:
    """Serialize one request's canonical non-GPU review projections."""
    documents = source_context.documents
    closure_sources = source_context.closure
    query_sources = source_context.query
    present_artifacts = documents.present_artifacts
    missing_artifacts = documents.missing_artifacts
    release = documents.release
    evidence_query = documents.evidence_query
    native_audit = documents.native_audit
    release_summary = documents.release_summary
    evidence_query_summary = documents.evidence_query_summary
    native_audit_summary = documents.native_audit_summary
    closure = closure_sources.document
    source_contract_gate = closure_sources.source_contract_gate
    closure_repair_paths = closure_sources.repair_paths
    closure_repair_path_groups = closure_sources.repair_path_groups
    closure_repair_path_group_plan = closure_sources.repair_path_group_plan
    closure_snapshot_diff_review = closure_sources.snapshot_diff_review
    closure_snapshot_to_release_link = closure_sources.snapshot_to_release_link
    closure_evidence_query_explain_tree_review = (
        closure_sources.evidence_query_explain_tree_review
    )
    closure_evidence_query_blocker_drilldown_review = (
        closure_sources.evidence_query_blocker_drilldown_review
    )
    closure_evidence_query_readiness_path_drilldown_review = (
        closure_sources.evidence_query_readiness_path_drilldown_review
    )
    closure_evidence_query_cross_artifact_review = (
        closure_sources.evidence_query_cross_artifact_review
    )
    closure_evidence_query_product_replay_linkage = (
        closure_sources.evidence_query_product_replay_linkage
    )
    closure_evidence_query_catalog_flow_filter = (
        closure_sources.evidence_query_catalog_flow_filter
    )
    evidence_query_catalog_filter = query_sources.catalog_filter
    evidence_query_native_core_audit_filter = query_sources.native_core_audit_filter
    closure_repair_path_diagnostics_projection = views.repair_path_diagnostics
    closure_repair_path_group_projection = views.repair_path_groups
    closure_release_linkage_projection = views.release_linkage
    closure_repair_navigation_projection = views.repair_navigation
    closure_repair_guidance_projection = views.repair_guidance
    closure_source_location_projection = views.source_location
    closure_root_cause_catalog_flow_shortest_path_projection = (
        views.catalog_flow_shortest_path
    )
    closure_root_cause_catalog_flow_parity_gate_projection = (
        views.catalog_flow_parity_gate
    )
    closure_source_exactness_projection = views.source_exactness
    closure_blocker_drilldown_projection = views.blocker_drilldown
    closure_readiness_path_projection = views.readiness_path
    closure_cross_artifact_projection = views.cross_artifact
    closure_product_replay_projection = views.product_replay
    closure_root_cause_native_audit_coverage_projection = (
        views.root_cause_native_audit_coverage
    )
    closure_root_cause_native_audit_manifest_projection = (
        views.root_cause_native_audit_manifest
    )
    closure_root_cause_catalog_flow_governance_projection = (
        views.root_cause_catalog_flow_governance
    )
    closure_catalog_flow_fallback_removal_queue = (
        views.catalog_flow_fallback_removal_queue
    )
    closure_root_cause_repair_review = views.root_cause_repair_review
    closure_evidence_query_repair_path_review = (
        views.evidence_query_repair_path_review
    )
    closure_evidence_query_ux = views.evidence_query_ux

    return {
        "required_artifacts": list(NON_GPU_REVIEW_ARTIFACTS),
        "present_artifacts": present_artifacts,
        "missing_artifacts": missing_artifacts,
        "release_pipeline": {
            "source": "native_release_evidence_pipeline.json",
            "present": release is not None,
            "has_non_gpu_review_closure": bool(closure),
            "release_decision": first_present(release_summary.get("release_decision"), release_summary.get("decision")),
            "release_decision_reason": release_summary.get("release_decision_reason"),
            "blocking_stage_count": release_summary.get("blocking_stage_count"),
            "source_contract_gate_applied": release_summary.get(
                "source_contract_gate_applied"
            ),
            "source_contract_gate_blocks_release": release_summary.get(
                "source_contract_gate_blocks_release"
            ),
            "source_contract_issue_count": release_summary.get(
                "source_contract_issue_count"
            ),
            "source_contract_blocking_source_count": release_summary.get(
                "source_contract_blocking_source_count"
            ),
            "release_blocking_signal_count": release_summary.get(
                "release_blocking_signal_count"
            ),
            "source_contract_gate": source_contract_gate,
            "executed_input_count": release_summary.get("executed_input_count"),
            "execute_verified_input_count": release_summary.get("execute_verified_input_count"),
            "non_gpu_review_signal_count": release_summary.get("non_gpu_review_signal_count"),
            **closure_repair_path_diagnostics_projection.to_release_pipeline_dict(
                release_summary,
                closure,
            ),
            **closure_repair_path_group_projection.to_release_pipeline_dict(),
            "evidence_query_snapshot_diff_added_deep_path_count": first_present(
                release_summary.get("evidence_query_snapshot_diff_added_deep_path_count"),
                closure_snapshot_diff_review.get("added_deep_path_count"),
            ),
            "evidence_query_snapshot_diff_removed_deep_path_count": first_present(
                release_summary.get("evidence_query_snapshot_diff_removed_deep_path_count"),
                closure_snapshot_diff_review.get("removed_deep_path_count"),
            ),
            "evidence_query_snapshot_diff_changed_deep_value_count": first_present(
                release_summary.get("evidence_query_snapshot_diff_changed_deep_value_count"),
                closure_snapshot_diff_review.get("changed_deep_value_count"),
            ),
            "evidence_query_snapshot_diff_deep_semantic_truncated": first_present(
                release_summary.get("evidence_query_snapshot_diff_deep_semantic_truncated"),
                closure_snapshot_diff_review.get("deep_semantic_truncated"),
            ),
            "evidence_query_snapshot_diff_deep_semantic_decision": first_present(
                release_summary.get("evidence_query_snapshot_diff_deep_semantic_decision"),
                closure_snapshot_diff_review.get("deep_semantic_decision"),
            ),
            **closure_release_linkage_projection.to_release_pipeline_dict(),
            **vars(closure_repair_navigation_projection),
            **closure_repair_guidance_projection.to_release_pipeline_dict(),
            **closure_source_location_projection.to_release_pipeline_dict(),
            **project_release_evidence_query_catalog_flow_scorecard(
                release_summary,
                closure_evidence_query_catalog_flow_filter,
                evidence_query_catalog_filter,
            ),
            **closure_root_cause_catalog_flow_shortest_path_projection.to_release_pipeline_dict(),
            **closure_root_cause_catalog_flow_parity_gate_projection.to_release_pipeline_dict(),
            **closure_source_exactness_projection.to_release_pipeline_dict(),
            "current_repo_repair_count": release_summary.get("non_gpu_current_repo_repair_count"),
            "target_machine_repair_count": release_summary.get("non_gpu_target_machine_repair_count"),
        },
        "evidence_query_explain_tree_review": {
            "source": "native_evidence_query_result.json",
            "direct_artifact_present": evidence_query is not None,
            "release_pipeline_signal_present": bool(
                closure_evidence_query_explain_tree_review
            ),
            **closure_source_exactness_projection.to_explain_tree_closure_dict(),
            "claim_boundary": (
                "Evidence-query explain tree source coverage is read-only "
                "blocker explanation metadata; it does not execute repairs or "
                "create benchmark/replay/execute evidence."
            ),
        },
        "evidence_query_blocker_drilldown_review": {
            "source": "native_evidence_query_result.json",
            "direct_artifact_present": evidence_query is not None,
            "release_pipeline_signal_present": bool(
                closure_evidence_query_blocker_drilldown_review
            ),
            **closure_blocker_drilldown_projection.to_closure_dict(),
            "claim_boundary": (
                "Evidence-query blocker drilldown is read-only operator triage "
                "metadata; it does not execute repairs or create "
                "benchmark/replay/execute evidence."
            ),
        },
        "evidence_query_readiness_path_drilldown_review": {
            "source": "native_evidence_query_result.json",
            "direct_artifact_present": evidence_query is not None,
            "release_pipeline_signal_present": bool(
                closure_evidence_query_readiness_path_drilldown_review
            ),
            **closure_readiness_path_projection.to_closure_dict(),
            "claim_boundary": (
                "Evidence-query readiness path drilldown is read-only operator "
                "triage metadata; it does not execute path commands, refresh "
                "artifacts, repair blockers, or prove readiness."
            ),
        },
        "evidence_query_cross_artifact_blocker_explanation": {
            "source": "native_evidence_query_result.json",
            "direct_artifact_present": evidence_query is not None,
            "release_pipeline_signal_present": bool(
                closure_evidence_query_cross_artifact_review
            ),
            **closure_cross_artifact_projection.to_closure_dict(),
            **closure_product_replay_projection.to_cross_artifact_closure_dict(),
            "claim_boundary": (
                "Evidence-query cross-artifact blocker explanation correlates "
                "existing evidence only; it does not execute repairs, refresh "
                "artifacts, replay products, or create benchmark/execute evidence."
            ),
        },
        "evidence_query_product_replay_semantic_linkage": {
            "source": "native_evidence_query_result.json",
            "direct_artifact_present": evidence_query is not None,
            "release_pipeline_signal_present": bool(
                closure_evidence_query_product_replay_linkage
            ),
            **closure_product_replay_projection.to_semantic_linkage_closure_dict(),
            "claim_boundary": (
                "Product replay semantic linkage explains existing replay "
                "artifacts only; it does not replay products, execute products, "
                "refresh artifacts, or create benchmark evidence."
            ),
        },
        "root_cause_native_core_audit_review_coverage_gate": (
            closure_root_cause_native_audit_coverage_projection.to_closure_dict()
        ),
        "root_cause_native_core_audit_review_manifest": (
            closure_root_cause_native_audit_manifest_projection.to_closure_dict()
        ),
        "root_cause_catalog_flow_fallback_removal_shortest_path": (
            closure_root_cause_catalog_flow_shortest_path_projection.to_closure_dict()
        ),
        "root_cause_catalog_flow_governance": (
            closure_root_cause_catalog_flow_governance_projection.to_closure_dict()
        ),
        "root_cause_catalog_flow_fallback_removal_parity_gate": (
            closure_root_cause_catalog_flow_parity_gate_projection.to_closure_dict()
        ),
        "catalog_flow_fallback_removal_queue": (
            closure_catalog_flow_fallback_removal_queue
        ),
        "root_cause_repair_review": closure_root_cause_repair_review,
        "evidence_query_repair_path_review": (
            closure_evidence_query_repair_path_review
        ),
        "evidence_query_ux": closure_evidence_query_ux,
        "evidence_query_catalog_flow_filter": project_evidence_query_catalog_flow_filter(
            evidence_query_catalog_filter,
            evidence_query_summary,
            direct_artifact_present=evidence_query is not None,
        ),
        "evidence_query_native_core_audit_filter": project_evidence_query_native_core_audit_filter(
            evidence_query_native_core_audit_filter,
            evidence_query_summary,
            native_audit_summary,
            direct_query_artifact_present=evidence_query is not None,
            direct_audit_artifact_present=native_audit is not None,
        ),
        "repair_paths": closure_repair_paths,
        "repair_path_diagnostics": (
            closure_repair_path_diagnostics_projection.to_dict()
        ),
        "repair_path_groups": closure_repair_path_groups,
        "repair_path_group_plan": closure_repair_path_group_plan,
        "evidence_query_snapshot_to_release_link": closure_snapshot_to_release_link,
        "claim_boundary": "Non-GPU review closure summarizes evidence wiring, review gaps, and repair UX only; it does not create execute-mode benchmark, CUDA timing, or mature-product performance claims.",
    }



def extract_non_gpu_review_closure(results_root: Path) -> Dict[str, Any]:
    source_context = load_non_gpu_review_source_context(results_root)
    documents = source_context.documents
    closure_sources = source_context.closure
    derived_sources = source_context.derived
    query_sources = source_context.query

    release = documents.release
    root_cause = documents.root_cause
    evidence_query = documents.evidence_query
    release_summary = documents.release_summary
    root_cause_summary = documents.root_cause_summary
    evidence_query_summary = documents.evidence_query_summary
    native_audit_summary = documents.native_audit_summary
    evidence_query_operation_descriptor = (
        documents.evidence_query_operation_descriptor
    )
    closure = closure_sources.document
    closure_repair_paths = closure_sources.repair_paths
    closure_repair_path_groups = closure_sources.repair_path_groups
    closure_repair_path_group_plan = closure_sources.repair_path_group_plan
    closure_repair_path_diagnostics_projection = (
        project_final_report_repair_path_diagnostics(closure_repair_paths)
    )
    closure_repair_path_group_projection = project_final_report_repair_path_groups(
        closure,
        release_summary,
        closure_repair_path_groups,
        closure_repair_path_group_plan,
        truthy_coercer=truthy_value,
    )
    root_cause_cards = derived_sources.root_cause_cards
    root_cause_card_summaries = derived_sources.root_cause_card_summaries
    root_cause_catalog_flow_card_stats = (
        derived_sources.root_cause_catalog_flow_card_stats
    )
    first_root_cause_card = derived_sources.first_root_cause_card
    root_cause_review = derived_sources.root_cause_review
    evidence_query_review = derived_sources.evidence_query_review
    closure_snapshot_diff_review = closure_sources.snapshot_diff_review
    closure_snapshot_to_release_link = closure_sources.snapshot_to_release_link
    closure_blocker_repair_card_link = closure_sources.blocker_repair_card_link
    closure_evidence_query_explain_tree_review = (
        closure_sources.evidence_query_explain_tree_review
    )
    closure_evidence_query_blocker_drilldown_review = (
        closure_sources.evidence_query_blocker_drilldown_review
    )
    closure_evidence_query_readiness_path_drilldown_review = (
        closure_sources.evidence_query_readiness_path_drilldown_review
    )
    closure_evidence_query_cross_artifact_review = (
        closure_sources.evidence_query_cross_artifact_review
    )
    closure_evidence_query_product_replay_linkage = (
        closure_sources.evidence_query_product_replay_linkage
    )
    closure_root_cause_native_audit_coverage = (
        closure_sources.root_cause_native_audit_coverage
    )
    closure_evidence_query_source_location_category_coverage = (
        closure_sources.evidence_query_source_location_category_coverage
    )
    closure_evidence_query_catalog_flow_filter = (
        closure_sources.evidence_query_catalog_flow_filter
    )
    closure_root_cause_catalog_flow_fallback_shortest_path = (
        closure_sources.root_cause_catalog_flow_fallback_shortest_path
    )
    closure_root_cause_catalog_flow_fallback_parity_gate = (
        closure_sources.root_cause_catalog_flow_fallback_parity_gate
    )
    closure_evidence_query_repair_group_plan = (
        closure_sources.evidence_query_repair_group_plan
    )
    evidence_query_catalog_filter = query_sources.catalog_filter
    def preserve_missing_value(value: Any) -> Any:
        return value

    closure_product_replay_projection = project_release_pipeline_product_replay(
        release_summary,
        closure_evidence_query_cross_artifact_review,
        closure_evidence_query_product_replay_linkage,
        evidence_query_summary,
        count_coercer=preserve_missing_value,
        truthy_coercer=preserve_missing_value,
        preserve_missing=True,
    )
    closure_readiness_path_projection = (
        project_final_report_evidence_query_readiness_path(
            release_summary,
            closure_evidence_query_readiness_path_drilldown_review,
            evidence_query_summary,
            count_coercer=preserve_missing_value,
            truthy_coercer=preserve_missing_value,
            preserve_missing=True,
        )
    )
    closure_blocker_drilldown_projection = (
        project_final_report_evidence_query_blocker_drilldown(
            release_summary,
            closure_evidence_query_blocker_drilldown_review,
            evidence_query_summary,
            count_coercer=preserve_missing_value,
        )
    )
    closure_source_exactness_projection = (
        project_final_report_evidence_query_source_exactness(
            release_summary,
            closure_evidence_query_repair_group_plan,
            closure_evidence_query_explain_tree_review,
            evidence_query_summary,
            count_coercer=preserve_missing_value,
        )
    )
    closure_cross_artifact_projection = (
        project_final_report_evidence_query_cross_artifact(
            release_summary,
            closure_evidence_query_cross_artifact_review,
            evidence_query_summary,
            count_coercer=preserve_missing_value,
            truthy_coercer=preserve_missing_value,
            preserve_missing=True,
        )
    )
    closure_product_replay_root_cause_projection = project_root_cause_product_replay(
        release_summary,
        root_cause_review,
        root_cause_summary,
        count_coercer=preserve_missing_value,
        truthy_coercer=preserve_missing_value,
        preserve_missing=True,
    )
    closure_root_cause_native_audit_coverage_projection = (
        project_root_cause_native_audit_coverage(
            release_summary,
            closure_root_cause_native_audit_coverage,
            count_coercer=preserve_missing_value,
            truthy_coercer=preserve_missing_value,
            preserve_missing=True,
        )
    )
    closure_root_cause_native_audit_manifest_projection = (
        project_root_cause_native_audit_manifest(
            release_summary,
            {},
            count_coercer=preserve_missing_value,
            preserve_missing=True,
        )
    )
    closure_root_cause_catalog_flow_shortest_path_projection = (
        project_root_cause_catalog_flow_shortest_path(
            closure_root_cause_catalog_flow_fallback_shortest_path,
            release_summary,
            root_cause_summary,
        )
    )
    closure_root_cause_catalog_flow_parity_gate_projection = (
        project_root_cause_catalog_flow_parity_gate(
            closure_root_cause_catalog_flow_fallback_parity_gate,
            release_summary,
            root_cause_summary,
        )
    )
    closure_root_cause_catalog_flow_governance_projection = (
        project_root_cause_catalog_flow_governance(
            root_cause_summary,
        )
    )
    closure_catalog_flow_fallback_removal_queue = (
        project_catalog_flow_fallback_removal_queue(
            evidence_query_catalog_filter,
        )
    )
    closure_catalog_flow_fallback_removal_queue_compatibility_fields = {
        field_name: closure_catalog_flow_fallback_removal_queue.get(field_name)
        for field_name in CATALOG_FLOW_FALLBACK_REMOVAL_QUEUE_COUNT_FIELDS
    }
    closure_repair_navigation_projection = (
        project_final_report_evidence_query_repair_navigation(
            release_summary,
            closure_evidence_query_repair_group_plan,
            value_coercer=preserve_missing_value,
        )
    )
    closure_repair_guidance_projection = (
        project_final_report_evidence_query_repair_guidance(
            release_summary,
            closure_evidence_query_repair_group_plan,
            count_coercer=preserve_missing_value,
            presence_coercer=preserve_missing_value,
            preserve_missing=True,
        )
    )
    closure_source_location_projection = (
        project_final_report_evidence_query_source_location(
            release_summary,
            closure_evidence_query_repair_group_plan,
            closure_evidence_query_source_location_category_coverage,
            count_coercer=preserve_missing_value,
        )
    )
    closure_release_linkage_projection = project_final_report_release_linkage(
        release_summary,
        closure_snapshot_to_release_link,
        closure_blocker_repair_card_link,
        count_coercer=int_value,
        truthy_coercer=truthy_value,
    )
    closure_evidence_query_ux = project_final_report_evidence_query_ux_closure(
        evidence_query_summary,
        direct_artifact_present=evidence_query is not None,
        operation=first_present(
            evidence_query_summary.get("query_operation"),
            get_path(evidence_query, ("query", "operation")),
        ),
        operation_human_label=first_present(
            evidence_query_operation_descriptor.get("human_label"),
            get_path(evidence_query, ("query", "operation_label")),
        ),
        operation_target=first_present(
            evidence_query_operation_descriptor.get("target"),
            get_path(evidence_query, ("query", "operation_target")),
        ),
        claim_boundary=first_present(
            get_path(evidence_query, ("query_ux", "claim_boundary")),
            "Evidence-query UX is a read-only projection of existing evidence; it does not execute repairs or create benchmark evidence.",
        ),
    )
    closure_root_cause_repair_review = (
        project_final_report_root_cause_repair_review(
            root_cause_review,
            root_cause_summary,
            direct_artifact_present=root_cause is not None,
            root_cause_cards=root_cause_cards,
            repair_card_summaries=root_cause_card_summaries,
            first_repair_card=first_root_cause_card,
            catalog_flow_card_stats=root_cause_catalog_flow_card_stats,
            catalog_flow_governance=(
                closure_root_cause_catalog_flow_governance_projection.to_root_cause_review_dict()
            ),
            catalog_flow_queue=(
                closure_catalog_flow_fallback_removal_queue_compatibility_fields
            ),
            catalog_flow_shortest_path=(
                closure_root_cause_catalog_flow_shortest_path_projection.to_root_cause_review_dict()
            ),
            catalog_flow_parity_gate=(
                closure_root_cause_catalog_flow_parity_gate_projection.to_root_cause_review_dict()
            ),
            product_replay=(
                closure_product_replay_root_cause_projection.to_root_cause_review_dict()
            ),
        )
    )
    closure_evidence_query_repair_path_review = (
        project_final_report_evidence_query_repair_path_review(
            evidence_query_review,
            evidence_query_summary,
            direct_artifact_present=evidence_query is not None,
        )
    )

    return compose_non_gpu_review_closure(
        source_context,
        NonGpuReviewClosureViews(
            repair_path_diagnostics=closure_repair_path_diagnostics_projection,
            repair_path_groups=closure_repair_path_group_projection,
            release_linkage=closure_release_linkage_projection,
            repair_navigation=closure_repair_navigation_projection,
            repair_guidance=closure_repair_guidance_projection,
            source_location=closure_source_location_projection,
            catalog_flow_shortest_path=(
                closure_root_cause_catalog_flow_shortest_path_projection
            ),
            catalog_flow_parity_gate=(
                closure_root_cause_catalog_flow_parity_gate_projection
            ),
            source_exactness=closure_source_exactness_projection,
            blocker_drilldown=closure_blocker_drilldown_projection,
            readiness_path=closure_readiness_path_projection,
            cross_artifact=closure_cross_artifact_projection,
            product_replay=closure_product_replay_projection,
            root_cause_native_audit_coverage=(
                closure_root_cause_native_audit_coverage_projection
            ),
            root_cause_native_audit_manifest=(
                closure_root_cause_native_audit_manifest_projection
            ),
            root_cause_catalog_flow_governance=(
                closure_root_cause_catalog_flow_governance_projection
            ),
            catalog_flow_fallback_removal_queue=(
                closure_catalog_flow_fallback_removal_queue
            ),
            root_cause_repair_review=closure_root_cause_repair_review,
            evidence_query_repair_path_review=(
                closure_evidence_query_repair_path_review
            ),
            evidence_query_ux=closure_evidence_query_ux,
        ),
    )


BOTTLENECKS = (
    (
        "KV cache decode",
        "Full-prefix recompute and dense attention score growth.",
        "Cached decode, Paged KV allocation, prefix sharing.",
        "KV memory pressure, block fragmentation, and block-table lookup locality.",
    ),
    (
        "Serving concurrency",
        "Single request latency hides queueing and batching effects.",
        "Continuous batching, admission policy sweeps, runtime policy sweeps.",
        "TTFT/P99 and fairness become scheduler-policy questions.",
    ),
    (
        "CUDA operators",
        "CPU reference exposes correctness but not GPU memory hierarchy behavior.",
        "Naive and optimized CUDA skeletons with event-timing wrappers.",
        "Memory coalescing, shared memory reuse, occupancy, and launch overhead.",
    ),
    (
        "Quantized weights",
        "FP16/FP32 weights dominate memory traffic and storage.",
        "INT4/INT8 and GGUF block dequant helpers.",
        "Dequant compute, group metadata overhead, and accuracy drift.",
    ),
    (
        "Edge inference",
        "Model loading, CPU memory bandwidth, and thread scheduling dominate.",
        "llama.cpp/GGUF benchmark harness and Android deployment scaffold.",
        "Thermal throttling, mmap behavior, NEON path, and device storage limits.",
    ),
)

def product_profile_command(product_key: str, mode: str) -> str:
    command = [
        "python3",
        "-m",
        f"scripts.product_profiles.profile_{product_key}",
        "--repo-root",
        ".",
        "--config",
        f"configs/product_profiles/{product_key}.example.yaml",
    ]
    if mode == "replay":
        command.extend(["--replay", "--evidence-dir", f"profiling/{product_key}"])
    else:
        command.append("--dry-run")
    return " ".join(command)


def product_profile_fallback_commands() -> Tuple[str, ...]:
    keys = default_product_profile_keys()
    return tuple(product_profile_command(product, "dry-run") for product in keys) + tuple(
        product_profile_command(product, "replay") for product in keys
    )


FALLBACK_COMMANDS = (
    "python3 scripts/collect_system_info.py --repo-root . --output results/raw/system_info.json",
    "python3 scripts/check_cuda_environment.py --project-root . --output results/raw/cuda_environment.json",
    "python3 -m mini_llm.shape_probe --output results/raw/shape_probe.json",
    "python3 scripts/run_vllm_benchmark.py --repo-root . --output results/raw/vllm_serving_benchmark.json",
    "python3 scripts/run_trtllm_benchmark.py --repo-root . --output results/raw/trtllm_benchmark.json",
    "python3 scripts/run_llama_cpp_benchmark.py --repo-root . --output results/raw/llama_cpp_benchmark.json",
    "python3 scripts/summarize_benchmarks.py --repo-root . --results results/raw --output results/tables/benchmark_summary.md --json-output results/raw/benchmark_summary.json",
    "python3 scripts/export_prometheus_metrics.py --repo-root . --results results/raw --output results/tables/inferlab_metrics.prom",
    "python3 scripts/plan_runtime_deploy.py --repo-root . --output reports/runtime_deploy_plan.md --json-output results/raw/runtime_deploy_plan.json",
    "python3 scripts/plan_android_deploy.py --repo-root . --output reports/android_deploy_plan.md --json-output results/raw/android_deploy_plan.json",
    "python3 scripts/parse_profiling_artifacts.py --repo-root . --artifact-root profiling --output results/raw/profiling_artifacts.json --report reports/profiling_artifacts.md",
    "python3 scripts/evaluate_backend_quality.py --repo-root . --results results/raw --output results/raw/backend_quality_eval.json --report reports/quality_evaluation.md",
    "python3 scripts/compare_backends.py --repo-root . --results results/raw --output results/raw/backend_comparison.json --report reports/backend_comparison.md",
    "python3 scripts/run_cuda_profile_matrix.py --repo-root . --output results/raw/cuda_profile_matrix.json --report reports/cuda_profile_matrix.md",
    "python3 scripts/run_llama_cpp_matrix.py --repo-root . --output results/raw/llama_cpp_matrix.json --report reports/llama_cpp_matrix.md",
    "python3 scripts/run_android_edge_benchmark.py --repo-root . --output results/raw/android_edge_benchmark.json --report reports/android_edge_benchmark.md",
    "python3 scripts/verify_source_traces.py --repo-root . --output results/raw/source_trace_verification.json --report reports/source_trace_verification.md",
    "python3 scripts/render_evidence_checklist.py --repo-root . --output results/raw/evidence_checklist.json --report reports/evidence_checklist.md",
    "python3 -m advanced_serving.structured_outputs --repo-root . --output results/raw/structured_outputs.json --report reports/structured_outputs.md",
    "python3 -m advanced_serving.pooling_benchmark --repo-root . --output results/raw/pooling_benchmark.json --report reports/pooling_benchmark.md",
    "./build/inferlab_kserve_llm_d_plan --repo-root . --output results/raw/kserve_llm_d_plan.json --report reports/kserve_llm_d_plan.md",
    "./build/inferlab_advanced_kv_router --repo-root . --output results/raw/native_advanced_kv_router.json",
    "./build/inferlab_disaggregated_serving --repo-root . --output results/raw/native_disaggregated_serving.json",
    "./build/inferlab_slo_capacity_plan --repo-root . --output results/raw/native_slo_capacity_plan.json",
    "./build/inferlab_long_context_plan --repo-root . --output results/raw/native_long_context_matrix.json",
    "./build/inferlab_speculative_decode_plan --repo-root . --output results/raw/native_speculative_decode_plan.json",
    "./build/inferlab_observability_evidence --repo-root . --output results/raw/native_observability_evidence.json",
    "./build/inferlab_streaming_sse_plan --repo-root . --output results/raw/native_streaming_sse_plan.json",
    "./build/inferlab_radix_prefix_cache_plan --repo-root . --output results/raw/native_radix_prefix_cache_plan.json",
    "./build/inferlab_tensor_parallel_comm_plan --repo-root . --output results/raw/native_tensor_parallel_comm_plan.json",
    "./build/inferlab_lora_adapter_cache_plan --repo-root . --output results/raw/native_lora_adapter_cache_plan.json",
    "./build/inferlab_chunked_prefill_scheduler_plan --repo-root . --output results/raw/native_chunked_prefill_scheduler_plan.json",
    "./build/inferlab_cost_accounting_plan --repo-root . --output results/raw/native_cost_accounting_plan.json",
    "./build/inferlab_kv_offload_hierarchy_plan --repo-root . --output results/raw/native_kv_offload_hierarchy_plan.json",
    "./build/inferlab_engine_memory_planner --repo-root . --output results/raw/native_engine_memory_planner.json",
    "./build/inferlab_gateway_policy_matrix_plan --repo-root . --output results/raw/native_gateway_policy_matrix_plan.json",
    "./build/inferlab_replica_placement_plan --repo-root . --output results/raw/native_replica_placement_plan.json",
    "./build/inferlab_product_profile_contract_check --repo-root . --results results/raw --output results/raw/native_product_profile_contract_check.json --format json",
    "./build/inferlab_product_integration_inventory --repo-root . --output results/raw/native_product_integration_inventory.json",
    "./build/inferlab_claim_scope_contract --repo-root . --matrix configs/benchmark_matrix.yaml --results results/raw --output results/raw/native_claim_scope_contract.json --format json",
    "./build/inferlab_runtime_evidence_freshness_gate --repo-root . --results results/raw --output results/raw/native_runtime_evidence_freshness_gate.json --format json",
    "./build/inferlab_kv_paged_attention_deep_dive --repo-root . --output results/raw/native_kv_paged_attention_deep_dive.json",
    "./build/inferlab_kv_lifecycle_deep_dive --repo-root . --output results/raw/native_kv_lifecycle_deep_dive.json",
    "./build/inferlab_scheduler_batching_deep_dive --repo-root . --output results/raw/native_scheduler_batching_deep_dive.json",
    "./build/inferlab_cuda_quant_microkernel_deep_dive --repo-root . --output results/raw/native_cuda_quant_microkernel_deep_dive.json",
    "./build/inferlab_kv_tier_decision_deep_dive --repo-root . --output results/raw/native_kv_tier_decision_deep_dive.json",
    "./build/inferlab_pd_split_crossover_deep_dive --repo-root . --output results/raw/native_pd_split_crossover_deep_dive.json",
    "./build/inferlab_scheduler_root_cause_deep_dive --repo-root . --output results/raw/native_scheduler_root_cause_deep_dive.json",
    "./build/inferlab_kernel_compatibility_matrix --repo-root . --output results/raw/native_kernel_compatibility_matrix.json",
    "./build/inferlab_product_replay_semantic_events --repo-root . --results results/raw --output results/raw/product_replay_semantic_events.json --format json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_streaming_backpressure_deep_dive --output results/raw/native_streaming_backpressure_deep_dive.json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_structured_output_parser_contract --output results/raw/native_structured_output_parser_contract.json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_autoscaler_cost_slo_deep_dive --output results/raw/native_autoscaler_cost_slo_deep_dive.json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_lora_adapter_cache_deep_dive --output results/raw/native_lora_adapter_cache_deep_dive.json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_grammar_backend_matrix --output results/raw/native_grammar_backend_matrix.json",
    "./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_edge_compiler_artifact_matrix --output results/raw/native_edge_compiler_artifact_matrix.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_kv_transport_timeline_deep_dive --output results/raw/native_kv_transport_timeline_deep_dive.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_radix_prefix_cache_deep_dive --output results/raw/native_radix_prefix_cache_deep_dive.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_kv_compression_blend_risk --output results/raw/native_kv_compression_blend_risk.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_chunked_prefill_stall_free_deep_dive --output results/raw/native_chunked_prefill_stall_free_deep_dive.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_tenant_quota_admission_deep_dive --output results/raw/native_tenant_quota_admission_deep_dive.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_live_request_migration_deep_dive --output results/raw/native_live_request_migration_deep_dive.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_quant_correctness_contract --output results/raw/native_quant_correctness_contract.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_paged_kv_gather_kernel_contract --output results/raw/native_paged_kv_gather_kernel_contract.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_flashinfer_replay_alignment --output results/raw/native_flashinfer_replay_alignment.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_trtllm_engine_quant_alignment --output results/raw/native_trtllm_engine_quant_alignment.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_gateway_policy_matrix --output results/raw/native_gateway_policy_matrix.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_deployment_readiness_matrix --output results/raw/native_deployment_readiness_matrix.json",
    "./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_openai_api_conformance_matrix --output results/raw/native_openai_api_conformance_matrix.json",
    "./build/inferlab_product_replay_alignment --repo-root . --results results/raw --output results/raw/native_product_replay_alignment.json --format json",
    "./build/inferlab_deep_dive_contract_check --repo-root . --results results/raw --output results/raw/native_deep_dive_contract_check.json --format json",
    "./build/inferlab_third_party_dependency_gate --repo-root . --output results/raw/native_third_party_dependency_gate.json --format json",
    "./build/inferlab_evidence_warehouse_index --repo-root . --results results/raw --output results/raw/native_evidence_warehouse_index.json --format json",
    "./build/inferlab_kernel_microbench_harness --repo-root . --output results/raw/native_kernel_microbench_harness.json --format json",
    "./build/inferlab_release_evidence_pipeline --repo-root . --results results/raw --output results/raw/native_release_evidence_pipeline.json --format json",
    "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query confidence_summary --output results/raw/native_evidence_query_result.json --format json",
    "./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation explain_blocker --selector report_wiring_missing --selector-repair-card-output results/raw/native_evidence_query_selector_repair_card_export.json --format json",
    "./build/inferlab_dependency_sbom_gate --repo-root . --output results/raw/native_dependency_sbom_gate.json --format json",
    "./build/inferlab_artifact_schema_registry --repo-root . --output results/raw/native_artifact_schema_registry.json --format json",
    "./build/inferlab_artifact_catalog_single_source --repo-root . --output results/raw/native_artifact_catalog_single_source.json --format json",
    "./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json",
    *product_profile_fallback_commands(),
)


def markdown_files_with_issues(directory: Path, recursive: bool = False) -> Tuple[List[Path], List[Dict[str, Any]]]:
    issues: List[Dict[str, Any]] = []
    try:
        exists = directory.exists()
    except OSError as exc:
        return [], [{"path": path_label(directory), "status": "directory_probe_failed", "error": redact_path_error(str(exc), directory)}]
    if not exists:
        return [], []
    try:
        iterator = directory.rglob("*.md") if recursive else directory.glob("*.md")
        paths: List[Path] = []
        for path in iterator:
            try:
                if path.exists():
                    paths.append(path)
            except OSError as exc:
                issues.append({"path": path_label(path), "status": "file_probe_failed", "error": redact_path_error(str(exc), path)})
        return sorted(paths), issues
    except OSError as exc:
        return [], [{"path": path_label(directory), "status": "directory_scan_failed", "error": redact_path_error(str(exc), directory)}]


def read_text_with_status(path: Path, limit: int = 4000) -> Tuple[str, Dict[str, Any]]:
    exists, probe_error = safe_path_probe(path)
    if probe_error:
        return "", {"path": path_label(path), "status": "path_probe_failed", "error": probe_error}
    if not exists:
        return "", {"path": path_label(path), "status": "missing", "error": ""}
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return "", {"path": path_label(path), "status": "read_failed", "error": redact_path_error(str(exc), path)}
    return text[:limit], {"path": path_label(path), "status": "ok", "error": ""}


def read_text(path: Path, limit: int = 4000) -> str:
    text, _ = read_text_with_status(path, limit)
    return text


def extract_benchmark_matrix_commands(root: Path) -> Tuple[List[str], Dict[str, Any]]:
    return reproducible_commands_from_matrix_with_status(root / "configs" / "benchmark_matrix.yaml")


def reproducible_commands_with_status(root: Path) -> Tuple[List[str], Dict[str, Any]]:
    matrix_commands, matrix_status = extract_benchmark_matrix_commands(root)
    if matrix_commands:
        return [
            "# Derived from configs/benchmark_matrix.yaml; refresh commands stay catalog-owned.",
            *matrix_commands,
            "# Final report rendering command.",
            "python3 scripts/render_final_report.py --repo-root . --results results/raw --summary results/raw/benchmark_summary.json --output reports/final_report.md --json-output results/raw/final_report_index.json",
        ], {**matrix_status, "fallback_used": False}
    return list(FALLBACK_COMMANDS), {**matrix_status, "fallback_used": True}


def reproducible_commands(root: Path) -> List[str]:
    commands, _ = reproducible_commands_with_status(root)
    return commands


def first_heading(text: str, fallback: str) -> str:
    for line in text.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    return fallback


def classify_report(relative: str, title: str) -> str:
    lower = f"{relative} {title}".lower()
    if relative.endswith("final_report.md"):
        return "final"
    if "profile" in lower:
        return "profile"
    if "deep_dive" in lower or "deep-dive" in lower:
        return "deep-dive"
    if "contract" in lower:
        return "contract"
    if "evidence" in lower or "quality" in lower or "comparison" in lower:
        return "evidence"
    if "plan" in lower or "deploy" in lower or "runtime" in lower:
        return "plan"
    if "benchmark" in lower or "matrix" in lower:
        return "benchmark"
    return "capability"


def bool_text(value: bool) -> str:
    return "yes" if value else "no"


def extract_untested(evaluation_text: str) -> List[str]:
    lines = evaluation_text.splitlines()
    output: List[str] = []
    in_section = False
    for line in lines:
        if line.startswith("## "):
            in_section = line.strip().lower() == "## untested"
            continue
        if in_section and line.startswith("- "):
            output.append(line[2:].strip())
    return output


def extract_report_index_with_issues(root: Path) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    reports: List[Dict[str, Any]] = []
    paths, issues = markdown_files_with_issues(root / "reports")
    for path in paths:
        if path.name == "final_report.md":
            continue
        relative = path.relative_to(root).as_posix()
        text, read_status = read_text_with_status(path)
        if read_status.get("status") != "ok":
            issues.append({**read_status, "kind": "report_read", "relative_path": relative})
        title = first_heading(text, path.stem)
        reports.append({
            "path": relative,
            "title": title,
            "kind": classify_report(relative, title),
            "read_status": read_status.get("status", "unknown"),
            "read_error": read_status.get("error", ""),
        })
    return reports, issues


def report_kind_counts(reports: List[Dict[str, Any]]) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for report in reports:
        kind = str(report.get("kind", "unknown"))
        counts[kind] = counts.get(kind, 0) + 1
    return dict(sorted(counts.items()))


def extract_note_index_with_issues(root: Path) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    notes: List[Dict[str, Any]] = []
    issues: List[Dict[str, Any]] = []
    for base in ("source_notes", "serving", "android", "deploy", "monitoring"):
        directory = root / base
        paths, directory_issues = markdown_files_with_issues(directory, recursive=True)
        issues.extend(directory_issues)
        for path in paths:
            relative = path.relative_to(root).as_posix()
            text, read_status = read_text_with_status(path)
            if read_status.get("status") != "ok":
                issues.append({**read_status, "kind": "note_read", "relative_path": relative})
            notes.append({
                "path": relative,
                "title": first_heading(text, path.stem),
                "read_status": read_status.get("status", "unknown"),
                "read_error": read_status.get("error", ""),
            })
    return notes, issues


def summary_sections(summary: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if not isinstance(summary, dict):
        return []
    sections = summary.get("sections")
    if not isinstance(sections, list):
        return []
    output: List[Dict[str, Any]] = []
    for section in sections:
        if not isinstance(section, dict):
            continue
        metrics = section.get("metrics")
        metric_count = len(metrics) if isinstance(metrics, list) else 0
        section_evidence = section.get("evidence")
        evidence = (
            dict(section_evidence)
            if isinstance(section_evidence, dict)
            else {}
        )
        selected: List[Tuple[str, Any]] = []
        if isinstance(metrics, list):
            for item in metrics:
                if not isinstance(item, dict):
                    continue
                name = str(item.get("name", ""))
                value = item.get("value")
                if any(
                    token in name
                    for token in (
                        "latency",
                        "throughput",
                        "tokens",
                        "bytes",
                        "status",
                        "available",
                        "mode",
                        "artifact_type",
                        "blocker",
                        "decision",
                        "executed",
                        "replayed",
                        "ready",
                        "claim_scope",
                        "confidence",
                        "lineage",
                        "root_cause",
                        "repair",
                        "dependency",
                        "sbom",
                        "correlation",
                        "operator",
                        "stage",
                        "query",
                    )
                ):
                    selected.append((name, value))
                if len(selected) >= 6:
                    break
        confidence = evidence.get("confidence")
        if confidence is None:
            confidence = selected_metric_value(metrics, "evidence_confidence")
        confidence_text = str(confidence) if confidence is not None else "unknown"
        output.append(
            {
                "title": section.get("title", "unknown"),
                "source": section.get("source", "unknown"),
                "status": section.get("status", "unknown"),
                "evidence_confidence": confidence_text,
                "evidence": evidence,
                "metric_count": metric_count,
                "selected": selected,
            }
        )
    return output


def evidence_confidence_rollup(sections: List[Dict[str, Any]]) -> Dict[str, Any]:
    counts = {tier: 0 for tier in CONFIDENCE_TIERS}
    artifacts_by_tier: Dict[str, List[Dict[str, Any]]] = {tier: [] for tier in CONFIDENCE_TIERS}
    for section in sections:
        tier = str(section.get("evidence_confidence", "unknown"))
        if tier not in counts:
            tier = "unknown"
        counts[tier] += 1
        artifacts_by_tier[tier].append(
            {
                "title": section.get("title", "unknown"),
                "source": section.get("source", "unknown"),
                "status": section.get("status", "unknown"),
            }
        )
    badges = [
        {
            "tier": tier,
            "count": counts[tier],
            "description": CONFIDENCE_TIER_DESCRIPTIONS[tier],
        }
        for tier in CONFIDENCE_TIERS
    ]
    strongest = "unknown"
    for tier in CONFIDENCE_TIERS:
        if counts[tier] > 0:
            strongest = tier
            break
    return {
        "section_count": len(sections),
        "badges": badges,
        "artifacts_by_tier": artifacts_by_tier,
        "strongest_present_tier": strongest,
        "claim_boundary": "Confidence tiers classify evidence strength only; they do not turn dry-run, fixture, or replay evidence into execute-mode benchmark claims.",
    }


def extract_catalog_drift_review(results_root: Path) -> Dict[str, Any]:
    source = "native_artifact_catalog_single_source.json"
    return project_artifact_catalog_governance(load_json(results_root / source))


def module_status(root: Path, results_root: Path) -> List[Dict[str, Any]]:
    rows: List[Dict[str, Any]] = []
    for module in MODULES:
        result_sources, catalog_resolution = module_result_sources(module, results_root)
        code_probes = [safe_path_probe(root / item) for item in module["code"]]
        report_probes = [safe_path_probe(root / item) for item in module["reports"]]
        note_probes = [safe_path_probe(root / item) for item in module["source_notes"]]
        result_probes = [safe_path_probe(results_root / item) for item in result_sources]
        result_exists = [exists for exists, _ in result_probes]
        path_probe_failed_code = [item for item, (_, error) in zip(module["code"], code_probes) if error]
        path_probe_failed_reports = [item for item, (_, error) in zip(module["reports"], report_probes) if error]
        path_probe_failed_notes = [item for item, (_, error) in zip(module["source_notes"], note_probes) if error]
        path_probe_failed_results = [item for item, (_, error) in zip(result_sources, result_probes) if error]
        row = {
            "name": module["name"],
            "why": module["why"],
            "code_ready": any(exists for exists, _ in code_probes) if code_probes else False,
            "report_ready": any(exists for exists, _ in report_probes) if report_probes else False,
            "notes_ready": any(exists for exists, _ in note_probes) if note_probes else False,
            "result_count": sum(1 for item in result_exists if item),
            "result_total": len(result_exists),
            "missing_results": [
                item for item, (exists, error) in zip(result_sources, result_probes) if not exists and not error
            ],
            "path_probe_failed_code": path_probe_failed_code,
            "path_probe_failed_reports": path_probe_failed_reports,
            "path_probe_failed_notes": path_probe_failed_notes,
            "path_probe_failed_results": path_probe_failed_results,
            "path_probe_failed_count": len(path_probe_failed_code)
            + len(path_probe_failed_reports)
            + len(path_probe_failed_notes)
            + len(path_probe_failed_results),
        }
        if catalog_resolution:
            row["catalog_source_resolution"] = catalog_resolution
        rows.append(row)
    return rows


def cost_records(path: Path) -> List[Dict[str, Any]]:
    data = load_json(path)
    if not isinstance(data, dict):
        return []
    records = data.get("records")
    return records if isinstance(records, list) else []


def render_cost_section(records: List[Dict[str, Any]], cost_path: Path) -> List[str]:
    lines = ["## Cost Record", ""]
    if not records:
        lines.extend(
            [
                f"No cost record was found at `{cost_path}`.",
                "",
                "| Provider | Instance | GPU | Hours | Unit cost | Total cost | Purpose |",
                "|---|---|---|---:|---:|---:|---|",
                "| TBD | TBD | TBD | TBD | TBD | TBD | GPU/vLLM/TensorRT-LLM runtime validation |",
            ]
        )
        return lines
    lines.extend(["| Provider | Instance | GPU | Hours | Unit cost | Total cost | Purpose |", "|---|---|---|---:|---:|---:|---|"])
    for record in records:
        lines.append(
            "| {provider} | {instance} | {gpu} | {hours} | {unit_cost} | {total_cost} | {purpose} |".format(
                provider=record.get("provider", ""),
                instance=record.get("instance", ""),
                gpu=record.get("gpu", ""),
                hours=record.get("hours", ""),
                unit_cost=record.get("unit_cost", ""),
                total_cost=record.get("total_cost", ""),
                purpose=record.get("purpose", ""),
            )
        )
    return lines


class _UnknownMarkdownFields(dict):
    """Render missing scorecard fields explicitly without adding fallback policy."""

    def __missing__(self, key: str) -> str:
        return "unknown"


def render_evidence_query_ux_scorecard(scorecard: Dict[str, Any]) -> List[str]:
    """Project the canonical Evidence Query UX scorecard into bounded Markdown rows."""

    values = _UnknownMarkdownFields(scorecard)
    templates = (
        "- Evidence-query UX operation: operation `{evidence_query_ux_operation}` "
        "(`{evidence_query_ux_operation_human_label}`); target "
        "`{evidence_query_ux_operation_target}`; target present "
        "`{evidence_query_ux_operation_target_present}`; snapshot inputs "
        "required/configured "
        "`{evidence_query_ux_operation_snapshot_inputs_required}`/"
        "`{evidence_query_ux_operation_snapshot_inputs_configured}`; selected result "
        "`{evidence_query_ux_selected_result_kind}` "
        "present/input-ready/payload-count "
        "`{evidence_query_ux_selected_result_present}`/"
        "`{evidence_query_ux_selected_result_input_ready}`/"
        "`{evidence_query_ux_selected_result_payload_count}`; selector "
        "`{evidence_query_ux_selector_kind}` status/matches/ambiguous "
        "`{evidence_query_ux_selector_resolution_status}`/"
        "`{evidence_query_ux_selector_match_count}`/"
        "`{evidence_query_ux_selector_ambiguous}`; operation catalog "
        "supported/read-only/snapshot-required "
        "`{evidence_query_operation_catalog_supported_count}`/"
        "`{evidence_query_operation_catalog_read_only_count}`/"
        "`{evidence_query_operation_catalog_snapshot_required_count}`.",
        "- Evidence-query selector repair export: status "
        "`{evidence_query_ux_selector_repair_card_export_status}`; "
        "limit/full/configured/written/external-status "
        "`{evidence_query_ux_selector_repair_card_embedded_limit}`/"
        "`{evidence_query_ux_selector_repair_card_full_export_count}`/"
        "`{evidence_query_ux_selector_repair_card_external_export_configured}`/"
        "`{evidence_query_ux_selector_repair_card_external_export_written}`/"
        "`{evidence_query_ux_selector_repair_card_external_export_status}`; focus "
        "`{evidence_query_ux_selector_repair_card_export_review_status}` "
        "reason/external-required "
        "`{evidence_query_ux_selector_repair_card_export_blocker_reason}`/"
        "`{evidence_query_ux_selector_repair_card_external_file_required}`; "
        "cards/emitted/truncated/source-locations/gaps "
        "`{evidence_query_ux_selector_repair_card_count}`/"
        "`{evidence_query_ux_selector_repair_card_emitted_count}`/"
        "`{evidence_query_ux_selector_repair_card_truncated}`/"
        "`{evidence_query_ux_selector_source_location_count}`/"
        "`{evidence_query_ux_selector_source_location_gap_count}`; card environments "
        "current-repo/current-server/target-machine/target-product/source-gap-cards "
        "`{evidence_query_ux_selector_repair_card_current_repo_count}`/"
        "`{evidence_query_ux_selector_repair_card_current_server_count}`/"
        "`{evidence_query_ux_selector_repair_card_target_machine_count}`/"
        "`{evidence_query_ux_selector_repair_card_target_product_count}`/"
        "`{evidence_query_ux_selector_repair_card_source_location_gap_card_count}`; "
        "actions current-repo/current-server/target-machine/target-product "
        "`{evidence_query_ux_selector_current_repo_action_count}`/"
        "`{evidence_query_ux_selector_current_server_action_count}`/"
        "`{evidence_query_ux_selector_target_machine_action_count}`/"
        "`{evidence_query_ux_selector_target_product_action_count}`; action plan "
        "`{evidence_query_ux_selector_action_plan_decision}` "
        "steps/current-repo/target-or-validation/source-gaps/readiness-blockers/state "
        "`{evidence_query_ux_selector_action_plan_step_count}`/"
        "`{evidence_query_ux_selector_action_plan_current_repo_step_count}`/"
        "`{evidence_query_ux_selector_action_plan_target_or_validation_step_count}`/"
        "`{evidence_query_ux_selector_action_plan_source_location_gap_count}`/"
        "`{evidence_query_ux_selector_action_plan_readiness_blocker_count}`/"
        "`{evidence_query_ux_selector_action_plan_readiness_state}`.",
        "- Evidence-query snapshot diff: decision "
        "`{evidence_query_snapshot_diff_decision}`; compared/changed "
        "`{evidence_query_snapshot_diff_content_compared}`/"
        "`{evidence_query_snapshot_diff_content_changed}`; semantic compared "
        "`{evidence_query_snapshot_diff_semantic_compared}`; added/removed/changed "
        "top-level keys `{evidence_query_snapshot_diff_added_top_level_key_count}`/"
        "`{evidence_query_snapshot_diff_removed_top_level_key_count}`/"
        "`{evidence_query_snapshot_diff_changed_top_level_value_count}`; semantic "
        "decision `{evidence_query_snapshot_diff_semantic_decision}`; deep "
        "compared/truncated `{evidence_query_snapshot_diff_deep_semantic_compared}`/"
        "`{evidence_query_snapshot_diff_deep_semantic_truncated}`; "
        "added/removed/changed JSON paths "
        "`{evidence_query_snapshot_diff_added_deep_path_count}`/"
        "`{evidence_query_snapshot_diff_removed_deep_path_count}`/"
        "`{evidence_query_snapshot_diff_changed_deep_value_count}`; deep decision "
        "`{evidence_query_snapshot_diff_deep_semantic_decision}`; read issues "
        "`{evidence_query_snapshot_diff_read_issue_count}`; size delta "
        "`{evidence_query_snapshot_diff_size_delta_bytes}`; release focus "
        "present/status/reason/review/current-repo-actions/changed-deep-paths "
        "`{evidence_query_snapshot_to_release_focus_present}`/"
        "`{evidence_query_snapshot_to_release_focus_link_status}`/"
        "`{evidence_query_snapshot_to_release_focus_blocker_reason}`/"
        "`{evidence_query_snapshot_to_release_focus_review_required}`/"
        "`{evidence_query_snapshot_to_release_focus_current_repo_action_count}`/"
        "`{evidence_query_snapshot_to_release_focus_changed_deep_path_count}`.",
        "- Evidence-query explain and drilldown: explain-tree "
        "nodes/exact/gaps/current-repo-gaps/target-gaps/percent "
        "`{evidence_query_ux_explain_tree_node_count}`/"
        "`{evidence_query_ux_explain_tree_source_exact_node_count}`/"
        "`{evidence_query_ux_explain_tree_source_gap_node_count}`/"
        "`{evidence_query_ux_explain_tree_current_repo_source_gap_count}`/"
        "`{evidence_query_ux_explain_tree_target_or_validation_source_gap_count}`/"
        "`{evidence_query_ux_explain_tree_source_exactness_percent}`; blocker "
        "drilldown records/categories/source-location-gaps/claims/exact-locations "
        "`{evidence_query_ux_blocker_drilldown_record_count}`/"
        "`{evidence_query_ux_blocker_drilldown_category_count}`/"
        "`{evidence_query_ux_blocker_drilldown_source_location_gap_count}`/"
        "`{evidence_query_ux_blocker_drilldown_affected_claim_count}`/"
        "`{evidence_query_ux_blocker_drilldown_exact_wiring_location_count}`; actions "
        "current-repo/current-server/target-machine/target-product "
        "`{evidence_query_ux_blocker_drilldown_current_repo_action_count}`/"
        "`{evidence_query_ux_blocker_drilldown_current_server_action_count}`/"
        "`{evidence_query_ux_blocker_drilldown_target_machine_action_count}`/"
        "`{evidence_query_ux_blocker_drilldown_target_product_action_count}`.",
        "- Evidence-query repair navigation: cards/groups/grouped-cards "
        "`{evidence_query_ux_repair_card_count}`/"
        "`{evidence_query_ux_repair_card_group_count}`/"
        "`{evidence_query_ux_repair_card_group_card_count}`; groups "
        "current-repo/current-server/target-machine/target-product/source-gaps "
        "`{evidence_query_ux_repair_card_group_current_repo_count}`/"
        "`{evidence_query_ux_repair_card_group_current_server_count}`/"
        "`{evidence_query_ux_repair_card_group_target_machine_count}`/"
        "`{evidence_query_ux_repair_card_group_target_product_count}`/"
        "`{evidence_query_ux_repair_card_group_source_location_gap_count}`; exact "
        "locations/affected claims "
        "`{evidence_query_ux_repair_group_exact_wiring_location_count}`/"
        "`{evidence_query_ux_repair_group_affected_claim_count}`; primary "
        "`{evidence_query_ux_repair_group_primary_environment}`/"
        "`{evidence_query_ux_repair_group_primary_category}`/"
        "`{evidence_query_ux_repair_group_primary_artifact}`; primary "
        "action/source-location present "
        "`{evidence_query_ux_repair_group_primary_action_present}`/"
        "`{evidence_query_ux_repair_group_primary_source_location_present}`; "
        "navigation steps/current-repo/target-or-validation "
        "`{evidence_query_ux_repair_group_navigation_step_count}`/"
        "`{evidence_query_ux_repair_group_navigation_current_repo_step_count}`/"
        "`{evidence_query_ux_repair_group_navigation_target_or_validation_step_count}`; "
        "source-location worklist items/current-repo/target-or-validation/gaps "
        "`{evidence_query_ux_source_location_worklist_item_count}`/"
        "`{evidence_query_ux_source_location_worklist_current_repo_item_count}`/"
        "`{evidence_query_ux_source_location_worklist_target_or_validation_item_count}`/"
        "`{evidence_query_ux_source_location_worklist_gap_count}`.",
        "- Evidence-query repair path quality: source-location categories "
        "total/active/exact/gaps/current-repo-gaps/target-gaps/gap-cards/percent "
        "`{evidence_query_ux_source_location_category_count}`/"
        "`{evidence_query_ux_source_location_active_category_count}`/"
        "`{evidence_query_ux_source_location_exact_category_count}`/"
        "`{evidence_query_ux_source_location_gap_category_count}`/"
        "`{evidence_query_ux_source_location_current_repo_gap_category_count}`/"
        "`{evidence_query_ux_source_location_target_or_validation_gap_category_count}`/"
        "`{evidence_query_ux_source_location_gap_repair_card_count}`/"
        "`{evidence_query_ux_source_location_category_exactness_percent}`; path "
        "ranks/current-repo/target-or-validation/location-gaps "
        "`{evidence_query_ux_repair_card_path_rank_count}`/"
        "`{evidence_query_ux_repair_card_path_current_repo_rank_count}`/"
        "`{evidence_query_ux_repair_card_path_target_or_validation_rank_count}`/"
        "`{evidence_query_ux_repair_card_path_source_location_gap_count}`; "
        "rank-readiness links/current-repo/target-or-validation/report/replay/execute "
        "`{evidence_query_ux_repair_card_rank_readiness_link_count}`/"
        "`{evidence_query_ux_repair_card_rank_readiness_current_repo_link_count}`/"
        "`{evidence_query_ux_repair_card_rank_readiness_target_or_validation_link_count}`/"
        "`{evidence_query_ux_repair_card_rank_readiness_report_ready_link_count}`/"
        "`{evidence_query_ux_repair_card_rank_readiness_replay_ready_link_count}`/"
        "`{evidence_query_ux_repair_card_rank_readiness_execute_ready_link_count}`; "
        "source-exact cards/exact/gaps/current-repo-gaps/target-gaps/percent "
        "`{evidence_query_ux_repair_card_source_exactness_card_count}`/"
        "`{evidence_query_ux_repair_card_source_exactness_exact_card_count}`/"
        "`{evidence_query_ux_repair_card_source_exactness_gap_card_count}`/"
        "`{evidence_query_ux_repair_card_source_exactness_current_repo_gap_count}`/"
        "`{evidence_query_ux_repair_card_source_exactness_target_or_validation_gap_count}`/"
        "`{evidence_query_ux_repair_card_source_exactness_percent}`; blocker "
        "categories/locations/report/replay/execute path blockers "
        "`{evidence_query_ux_blocker_category_count}`/"
        "`{evidence_query_ux_exact_wiring_location_count}`/"
        "`{evidence_query_ux_report_ready_blocker_count}`/"
        "`{evidence_query_ux_replay_ready_blocker_count}`/"
        "`{evidence_query_ux_execute_ready_blocker_count}`.",
    )
    return [template.format_map(values) for template in templates]


def render_release_pipeline_evidence_query_repairs(
    scorecard: Dict[str, Any],
) -> List[str]:
    """Render the typed release-pipeline Evidence Query repair projections."""

    values = _UnknownMarkdownFields(scorecard)
    templates = (
        "- Evidence Query repair groups consumed by release pipeline: "
        "groups/grouped-cards "
        "`{release_pipeline_evidence_query_repair_group_count}`/"
        "`{release_pipeline_evidence_query_repair_group_card_count}`; groups "
        "current-repo/current-server/target-machine/target-product/source-gaps "
        "`{release_pipeline_evidence_query_repair_group_current_repo_count}`/"
        "`{release_pipeline_evidence_query_repair_group_current_server_count}`/"
        "`{release_pipeline_evidence_query_repair_group_target_machine_count}`/"
        "`{release_pipeline_evidence_query_repair_group_target_product_count}`/"
        "`{release_pipeline_evidence_query_repair_group_source_location_gap_count}`; "
        "explain-tree nodes/exact/gaps/current-repo-gaps/target-gaps/percent "
        "`{release_pipeline_evidence_query_explain_tree_node_count}`/"
        "`{release_pipeline_evidence_query_explain_tree_source_exact_node_count}`/"
        "`{release_pipeline_evidence_query_explain_tree_source_gap_node_count}`/"
        "`{release_pipeline_evidence_query_explain_tree_current_repo_source_gap_count}`/"
        "`{release_pipeline_evidence_query_explain_tree_target_or_validation_source_gap_count}`/"
        "`{release_pipeline_evidence_query_explain_tree_source_exactness_percent}`; "
        "blocker drilldown records/categories/source-gaps/claims/exact-locations "
        "`{release_pipeline_evidence_query_blocker_drilldown_record_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_category_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_source_location_gap_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_affected_claim_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_exact_wiring_location_count}`; "
        "actions current-repo/current-server/target-machine/target-product "
        "`{release_pipeline_evidence_query_blocker_drilldown_current_repo_action_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_current_server_action_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_target_machine_action_count}`/"
        "`{release_pipeline_evidence_query_blocker_drilldown_target_product_action_count}`.",
        "- Evidence Query repair narrative consumed by release pipeline: summary "
        "`{release_pipeline_evidence_query_repair_group_summary_present}`; exact "
        "locations/affected claims "
        "`{release_pipeline_evidence_query_repair_group_exact_wiring_location_count}`/"
        "`{release_pipeline_evidence_query_repair_group_affected_claim_count}`; "
        "narrative present/decision/readiness/steps/current-repo/target-or-validation/"
        "source-gaps/readiness-blockers/claims/exact-locations "
        "`{release_pipeline_evidence_query_repair_narrative_present}`/"
        "`{release_pipeline_evidence_query_repair_narrative_decision}`/"
        "`{release_pipeline_evidence_query_repair_narrative_readiness_state}`/"
        "`{release_pipeline_evidence_query_repair_narrative_step_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_current_repo_action_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_target_or_validation_action_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_source_location_gap_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_readiness_blocker_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_affected_claim_count}`/"
        "`{release_pipeline_evidence_query_repair_narrative_exact_wiring_location_count}`; "
        "primary environment/category/artifact "
        "`{release_pipeline_evidence_query_repair_group_primary_environment}`/"
        "`{release_pipeline_evidence_query_repair_group_primary_category}`/"
        "`{release_pipeline_evidence_query_repair_group_primary_artifact}`; primary "
        "action/source-location present "
        "`{release_pipeline_evidence_query_repair_group_primary_action_present}`/"
        "`{release_pipeline_evidence_query_repair_group_primary_source_location_present}`; "
        "navigation steps/current-repo/target-or-validation "
        "`{release_pipeline_evidence_query_repair_group_navigation_step_count}`/"
        "`{release_pipeline_evidence_query_repair_group_navigation_current_repo_step_count}`/"
        "`{release_pipeline_evidence_query_repair_group_navigation_target_or_validation_step_count}`.",
        "- Evidence Query source worklist consumed by release pipeline: "
        "items/current-repo/target-or-validation/gaps "
        "`{release_pipeline_evidence_query_source_location_worklist_item_count}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_current_repo_item_count}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_target_or_validation_item_count}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_gap_count}`; focus "
        "present/first-item/first-gap/current-repo-action/target-action "
        "`{release_pipeline_evidence_query_source_location_worklist_focus_present}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_focus_first_item_present}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_focus_first_gap_present}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_focus_first_item_current_repo_action_required}`/"
        "`{release_pipeline_evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required}`; "
        "categories total/active/exact/gaps/current-repo-gaps/target-gaps/gap-cards/percent "
        "`{release_pipeline_evidence_query_source_location_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_active_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_exact_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_gap_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_current_repo_gap_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_target_or_validation_gap_category_count}`/"
        "`{release_pipeline_evidence_query_source_location_gap_repair_card_count}`/"
        "`{release_pipeline_evidence_query_source_location_category_exactness_percent}`.",
        "- Evidence Query repair-path quality consumed by release pipeline: catalog "
        "field coverage surfaces/fallback/required/covered/gaps/parity "
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_surface_count}`/"
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_fallback_surface_count}`/"
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_required_field_count}`/"
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_covered_field_count}`/"
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_field_gap_count}`/"
        "`{release_pipeline_evidence_query_catalog_flow_consumer_field_coverage_parity_refresh_required_count}`; "
        "path ranks/current-repo/target-or-validation/location-gaps "
        "`{release_pipeline_evidence_query_repair_card_path_rank_count}`/"
        "`{release_pipeline_evidence_query_repair_card_path_current_repo_rank_count}`/"
        "`{release_pipeline_evidence_query_repair_card_path_target_or_validation_rank_count}`/"
        "`{release_pipeline_evidence_query_repair_card_path_source_location_gap_count}`; "
        "rank-readiness links/current-repo/target-or-validation/report/replay/execute "
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_link_count}`/"
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_current_repo_link_count}`/"
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_target_or_validation_link_count}`/"
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_report_ready_link_count}`/"
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_replay_ready_link_count}`/"
        "`{release_pipeline_evidence_query_repair_card_rank_readiness_execute_ready_link_count}`; "
        "source-exact cards/exact/gaps/current-repo-gaps/target-gaps/percent "
        "`{release_pipeline_evidence_query_repair_card_source_exactness_card_count}`/"
        "`{release_pipeline_evidence_query_repair_card_source_exactness_exact_card_count}`/"
        "`{release_pipeline_evidence_query_repair_card_source_exactness_gap_card_count}`/"
        "`{release_pipeline_evidence_query_repair_card_source_exactness_current_repo_gap_count}`/"
        "`{release_pipeline_evidence_query_repair_card_source_exactness_target_or_validation_gap_count}`/"
        "`{release_pipeline_evidence_query_repair_card_source_exactness_percent}`.",
    )
    return [template.format_map(values) for template in templates]


def render_release_pipeline_cross_artifact(scorecard: Dict[str, Any]) -> List[str]:
    """Render the canonical cross-artifact blocker explanation projection."""

    values = _UnknownMarkdownFields(scorecard)
    cross_artifact = (
        "- Evidence Query cross-artifact blocker explanation consumed by release "
        "pipeline: status "
        "`{release_pipeline_evidence_query_cross_artifact_blocker_explanation_status}`; "
        "release focus "
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_decision}` via "
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_repair_lane}`; "
        "affected claims/repair paths "
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_affected_claim_count}`/"
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_repair_path_count}`; "
        "first path family/environment "
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_first_repair_family}`/"
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_first_repair_environment}`; "
        "families active/total "
        "`{release_pipeline_evidence_query_cross_artifact_active_family_count}`/"
        "`{release_pipeline_evidence_query_cross_artifact_family_count}`; blockers "
        "`{release_pipeline_evidence_query_cross_artifact_total_blocker_count}`; "
        "current-repo/target-or-validation actions "
        "`{release_pipeline_evidence_query_cross_artifact_current_repo_action_count}`/"
        "`{release_pipeline_evidence_query_cross_artifact_target_or_validation_action_count}`; "
        "source-location gaps "
        "`{release_pipeline_evidence_query_cross_artifact_source_location_gap_count}`; "
        "primary family/reason/environment "
        "`{release_pipeline_evidence_query_cross_artifact_primary_family}`/"
        "`{release_pipeline_evidence_query_cross_artifact_primary_blocker_reason}`/"
        "`{release_pipeline_evidence_query_cross_artifact_primary_environment}`; action "
        "`{release_pipeline_evidence_query_cross_artifact_release_focus_primary_action}`. "
        "This is read-only evidence correlation, not repair execution or validation."
    ).format_map(values)
    product_replay = (
        "- Cross-artifact product replay family detail: present "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_present}`; "
        "status "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_status}`; "
        "blockers "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_blocker_count}` "
        "split into semantic-linkage blockers "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count}` "
        "and replay/execute missing evidence "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count}`; "
        "target-or-validation actions "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count}`; "
        "source artifacts "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_source_artifact_count}` "
        "first "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_first_source_artifact}`; "
        "next action "
        "`{release_pipeline_evidence_query_cross_artifact_product_replay_family_next_action}`. "
        "This stays a release-gate explanation and does not import logs, replay "
        "products, execute products, or create benchmark numbers."
    ).format_map(values)
    return [cross_artifact, product_replay]


def render_release_pipeline_readiness_path(scorecard: Dict[str, Any]) -> str:
    """Render the canonical read-only readiness-path drilldown projection."""

    return (
        "- Evidence Query readiness-path drilldown consumed by release pipeline: "
        "target/state/actionability/primary-environment "
        "`{release_pipeline_evidence_query_readiness_path_target}`/"
        "`{release_pipeline_evidence_query_readiness_path_state}`/"
        "`{release_pipeline_evidence_query_readiness_path_actionability_status}`/"
        "`{release_pipeline_evidence_query_readiness_path_primary_environment}`; "
        "blocker reason "
        "`{release_pipeline_evidence_query_readiness_path_blocker_reason}`; "
        "current-repo/validation-or-target/source-metadata/next-action flags "
        "`{release_pipeline_evidence_query_readiness_path_current_repo_actionable}`/"
        "`{release_pipeline_evidence_query_readiness_path_validation_or_target_required}`/"
        "`{release_pipeline_evidence_query_readiness_path_source_location_metadata_required}`/"
        "`{release_pipeline_evidence_query_readiness_path_next_action_present}`; "
        "blockers/path-steps/current-repo/target-or-validation/source-gaps "
        "`{release_pipeline_evidence_query_readiness_path_blocking_repair_card_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_step_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_current_repo_step_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_target_or_validation_step_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_source_location_gap_count}`; "
        "catalog-flow/fallback-removal/field-coverage gaps "
        "`{release_pipeline_evidence_query_readiness_path_catalog_flow_discrepancy_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_fallback_removal_blocked_count}`/"
        "`{release_pipeline_evidence_query_readiness_path_field_coverage_gap_count}`. "
        "This is read-only path triage, not proof of readiness."
    ).format_map(_UnknownMarkdownFields(scorecard))


def render_release_pipeline_native_audit(
    scorecard: Dict[str, Any], release_pipeline: Dict[str, Any]
) -> str:
    """Render the Native Audit version-2 release projection."""

    return (
        "- Native-core static audit: decision `{decision}`; progress "
        "`{progress}`; reviewed/fixed/deferred `{reviewed}`/`{fixed}`/"
        "`{deferred}`; open/high-priority `{open_items}`/`{high_priority}`; "
        "completion `{completion}%`; manifest invalid/unmatched "
        "`{invalid}`/`{unmatched}`; coverage blocker `{coverage_blocker}`; "
        "repair paths `{paths}`; first location `{first_location}`. "
        "This is static source-review evidence, not runtime or target-machine "
        "execute evidence."
    ).format(
        decision=scorecard.get(
            "evidence_query_native_core_audit_audit_decision", "unknown"
        ),
        progress=scorecard.get(
            "evidence_query_native_core_audit_audit_progress_state", "unknown"
        ),
        reviewed=scorecard.get(
            "evidence_query_native_core_audit_reviewed_file_count", "unknown"
        ),
        fixed=scorecard.get(
            "evidence_query_native_core_audit_reviewed_with_fix_file_count",
            "unknown",
        ),
        deferred=scorecard.get(
            "evidence_query_native_core_audit_deferred_with_reason_file_count",
            "unknown",
        ),
        open_items=scorecard.get(
            "evidence_query_native_core_audit_open_review_item_count", "unknown"
        ),
        high_priority=scorecard.get(
            "evidence_query_native_core_audit_high_priority_review_item_count",
            "unknown",
        ),
        completion=scorecard.get(
            "evidence_query_native_core_audit_audit_completion_percent",
            "unknown",
        ),
        invalid=scorecard.get(
            "evidence_query_native_core_audit_review_manifest_invalid_override_count",
            "unknown",
        ),
        unmatched=scorecard.get(
            "evidence_query_native_core_audit_review_manifest_unmatched_override_count",
            "unknown",
        ),
        coverage_blocker=scorecard.get(
            "evidence_query_native_core_audit_audit_review_coverage_primary_blocker",
            "unknown",
        ),
        paths=scorecard.get(
            "release_pipeline_native_core_audit_repair_path_count", "unknown"
        ),
        first_location=release_pipeline.get(
            "native_core_audit_first_source_location", "unknown"
        ),
    )


def render_product_replay_release_evidence(scorecard: Dict[str, Any]) -> List[str]:
    """Render Product Replay semantic, root-cause, and family-gate evidence."""

    lines: List[str] = []
    lines.append(
        "- Product replay semantic linkage consumed by release pipeline: status `{status}`; semantic/alignment inputs present `{semantic}`/`{alignment}`; pending events `{pending_events}`; sample replay present `{sample_replay}`; replay-supported events `{supported_events}`; aligned rules `{aligned_rules}`; catalog correlations ready/blocked `{ready_corr}`/`{blocked_corr}`; blockers `{blockers}`; target-or-validation actions `{target}`; repair paths `{repair_paths}` with source-location gaps `{repair_gaps}`; first blocker `{first_blocker}`; next action `{next_action}`. Correlations are computed inside Evidence Query from the canonical rule registry and current native artifacts; this does not run replay or execute product benchmarks.".format(
            status=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_status",
                "unknown",
            ),
            semantic=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_events_present",
                "unknown",
            ),
            alignment=scorecard.get(
                "release_pipeline_evidence_query_product_replay_alignment_present",
                "unknown",
            ),
            pending_events=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_pending_event_count",
                "unknown",
            ),
            sample_replay=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_sample_replay_present",
                "unknown",
            ),
            supported_events=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_replay_supported_event_count",
                "unknown",
            ),
            aligned_rules=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_aligned_rule_count",
                "unknown",
            ),
            ready_corr=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_ready_correlation_count",
                "unknown",
            ),
            blocked_corr=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_blocked_correlation_count",
                "unknown",
            ),
            blockers=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_blocker_count",
                "unknown",
            ),
            target=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_target_or_validation_action_count",
                "unknown",
            ),
            repair_paths=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_repair_path_count",
                "unknown",
            ),
            repair_gaps=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count",
                "unknown",
            ),
            first_blocker=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_first_blocker",
                "unknown",
            ),
            next_action=scorecard.get(
                "release_pipeline_evidence_query_product_replay_semantic_linkage_next_action",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Product replay semantic linkage root cause: status `{status}`; root causes `{roots}`; blockers `{blockers}`; pending events/rules `{pending_events}`/`{pending_rules}`; blocked correlations `{blocked_corr}`; target-or-validation actions `{target}`; blocks release `{blocks_release}`; repair paths `{repair_paths}` with source-location gaps `{repair_gaps}`; first blocker `{first_blocker}`; next action `{next_action}`. This is release-gate explanation only; it does not run replay or execute product benchmarks.".format(
            status=scorecard.get(
                "root_cause_product_replay_semantic_linkage_status",
                "unknown",
            ),
            roots=scorecard.get(
                "root_cause_product_replay_semantic_linkage_root_cause_count",
                "unknown",
            ),
            blockers=scorecard.get(
                "root_cause_product_replay_semantic_linkage_blocker_count",
                "unknown",
            ),
            pending_events=scorecard.get(
                "root_cause_product_replay_semantic_linkage_pending_event_count",
                "unknown",
            ),
            pending_rules=scorecard.get(
                "root_cause_product_replay_semantic_linkage_pending_rule_count",
                "unknown",
            ),
            blocked_corr=scorecard.get(
                "root_cause_product_replay_semantic_linkage_blocked_correlation_count",
                "unknown",
            ),
            target=scorecard.get(
                "root_cause_product_replay_semantic_linkage_target_or_validation_action_count",
                "unknown",
            ),
            blocks_release=scorecard.get(
                "root_cause_product_replay_semantic_linkage_blocks_release",
                "unknown",
            ),
            repair_paths=scorecard.get(
                "root_cause_product_replay_semantic_linkage_repair_path_count",
                "unknown",
            ),
            repair_gaps=scorecard.get(
                "root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count",
                "unknown",
            ),
            first_blocker=scorecard.get(
                "root_cause_product_replay_semantic_linkage_first_blocker",
                "unknown",
            ),
            next_action=scorecard.get(
                "root_cause_product_replay_semantic_linkage_next_action",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Product replay family gate root cause: status `{status}`; root causes `{roots}`; blockers `{blockers}` split into semantic-linkage `{semantic}` and replay/execute-missing `{missing}`; source artifacts `{sources}`; replay-target actions `{replay_target}`; execute-target actions `{execute_target}`; legacy target aggregate `{target}`; typed split `{typed_split}`; blocks release `{blocks_release}`; repair paths `{repair_paths}` with source-location gaps `{repair_gaps}`; first source `{first_source}`; next action `{next_action}`. This is cross-artifact release-gate explanation only; it does not import logs, run replay, or execute product benchmarks.".format(
            status=scorecard.get(
                "root_cause_product_replay_family_gate_status",
                "unknown",
            ),
            roots=scorecard.get(
                "root_cause_product_replay_family_gate_root_cause_count",
                "unknown",
            ),
            blockers=scorecard.get(
                "root_cause_product_replay_family_gate_blocker_count",
                "unknown",
            ),
            semantic=scorecard.get(
                "root_cause_product_replay_family_gate_semantic_linkage_blocker_count",
                "unknown",
            ),
            missing=scorecard.get(
                "root_cause_product_replay_family_gate_replay_or_execute_missing_count",
                "unknown",
            ),
            sources=scorecard.get(
                "root_cause_product_replay_family_gate_source_artifact_count",
                "unknown",
            ),
            target=scorecard.get(
                "root_cause_product_replay_family_gate_target_or_validation_action_count",
                "unknown",
            ),
            replay_target=scorecard.get(
                "root_cause_product_replay_family_gate_target_product_replay_action_count",
                "unknown",
            ),
            execute_target=scorecard.get(
                "root_cause_product_replay_family_gate_target_gpu_execute_action_count",
                "unknown",
            ),
            typed_split=scorecard.get(
                "root_cause_product_replay_family_gate_typed_environment_split_present",
                "unknown",
            ),
            blocks_release=scorecard.get(
                "root_cause_product_replay_family_gate_blocks_release",
                "unknown",
            ),
            repair_paths=scorecard.get(
                "root_cause_product_replay_family_gate_repair_path_count",
                "unknown",
            ),
            repair_gaps=scorecard.get(
                "root_cause_product_replay_family_gate_repair_path_source_location_gap_count",
                "unknown",
            ),
            first_source=scorecard.get(
                "root_cause_product_replay_family_gate_first_source_artifact",
                "unknown",
            ),
            next_action=scorecard.get(
                "root_cause_product_replay_family_gate_next_action",
                "unknown",
            ),
        )
    )
    return lines


def render_evidence_query_selector_triage(scorecard: Dict[str, Any]) -> List[str]:
    """Render focused selector resolution and action-plan triage surfaces."""

    lines: List[str] = []
    lines.append(
        "- Evidence Query show-artifact focused payload: candidates `{candidates}`; exact-match `{exact}`; ambiguous `{ambiguous}`; strategy `{strategy}`. This is selector hygiene for read-only triage, not artifact refresh or validation.".format(
            candidates=scorecard.get(
                "evidence_query_ux_show_artifact_candidate_count",
                "unknown",
            ),
            exact=scorecard.get(
                "evidence_query_ux_show_artifact_exact_match",
                "unknown",
            ),
            ambiguous=scorecard.get(
                "evidence_query_ux_show_artifact_selector_ambiguous",
                "unknown",
            ),
            strategy=scorecard.get(
                "evidence_query_ux_show_artifact_selection_strategy",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Evidence Query focused payload selector-focus: present `{present}`; status `{status}`; matches `{matches}`; ambiguous `{ambiguous}`; source-location gaps `{source_gaps}`. This mirrors selector resolution inside explain/suggest/repair-group/path payloads so the focused view remains self-describing.".format(
            present=scorecard.get(
                "evidence_query_ux_payload_selector_focus_present",
                "unknown",
            ),
            status=scorecard.get(
                "evidence_query_ux_payload_selector_focus_status",
                "unknown",
            ),
            matches=scorecard.get(
                "evidence_query_ux_payload_selector_focus_match_count",
                "unknown",
            ),
            ambiguous=scorecard.get(
                "evidence_query_ux_payload_selector_focus_ambiguous",
                "unknown",
            ),
            source_gaps=scorecard.get(
                "evidence_query_ux_payload_selector_focus_source_location_gap_count",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Release Pipeline consumed focused selector-focus: present `{present}`; status `{status}`; matches `{matches}`; ambiguous `{ambiguous}`; source-location gaps `{source_gaps}`. This confirms the release gate can explain focused-query ambiguity without parsing the full Evidence Query payload.".format(
            present=scorecard.get(
                "release_pipeline_evidence_query_payload_selector_focus_present",
                "unknown",
            ),
            status=scorecard.get(
                "release_pipeline_evidence_query_payload_selector_focus_status",
                "unknown",
            ),
            matches=scorecard.get(
                "release_pipeline_evidence_query_payload_selector_focus_match_count",
                "unknown",
            ),
            ambiguous=scorecard.get(
                "release_pipeline_evidence_query_payload_selector_focus_ambiguous",
                "unknown",
            ),
            source_gaps=scorecard.get(
                "release_pipeline_evidence_query_payload_selector_focus_source_location_gap_count",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Evidence Query selector action-plan consumed by release pipeline: decision `{decision}`; steps/current-repo/target-or-validation/source-gaps/readiness-blockers `{steps}`/`{repo}`/`{target}`/`{source_gaps}`/`{blockers}`; readiness state `{state}`. This is read-only triage guidance, not a repaired or executed state.".format(
            decision=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_decision",
                "unknown",
            ),
            steps=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_step_count",
                "unknown",
            ),
            repo=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_current_repo_step_count",
                "unknown",
            ),
            target=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_target_or_validation_step_count",
                "unknown",
            ),
            source_gaps=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_source_location_gap_count",
                "unknown",
            ),
            blockers=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_readiness_blocker_count",
                "unknown",
            ),
            state=scorecard.get(
                "release_pipeline_evidence_query_selector_action_plan_readiness_state",
                "unknown",
            ),
        )
    )
    return lines


def render_non_gpu_review_overview_section(
    closure: Mapping[str, Any],
) -> List[str]:
    """Render closure coverage and repair-path evidence."""
    lines: List[str] = []
    lines.extend(["", "### Non-GPU Review Closure", ""])
    release_pipeline = dict_value(closure.get("release_pipeline"))
    present = ", ".join(f"`{name}`" for name in closure.get("present_artifacts", [])) or "none"
    missing = ", ".join(f"`{name}`" for name in closure.get("missing_artifacts", [])) or "none"
    lines.append(f"- Present review artifacts: {present}.")
    lines.append(f"- Missing review artifacts: {missing}.")
    lines.append(
        "- Release pipeline: present `{present}`; closure `{closure_present}`; decision `{decision}`; signals `{signals}`.".format(
            present=release_pipeline.get("present", False),
            closure_present=release_pipeline.get("has_non_gpu_review_closure", False),
            decision=release_pipeline.get("release_decision", "unknown"),
            signals=release_pipeline.get("non_gpu_review_signal_count", "unknown"),
        )
    )
    repair_paths = closure.get("repair_paths", [])
    if isinstance(repair_paths, list):
        lines.append(
            "- Repair paths: count `{count}`; groups `{groups}`; environment groups `{environment_groups}`; category groups `{category_groups}`; current-repo action groups `{current_repo_groups}`; target/validation action groups `{target_validation_groups}`; group-plan steps `{plan_steps}`; current-repo plan steps `{current_repo_plan_steps}`; target/validation plan steps `{target_validation_plan_steps}`; first plan step `{first_plan_step}`; first group `{first_group}`; source-location gaps `{location_gaps}`; evidence counters `{evidence_counters}`; catalog-flow `{catalog}`; catalog-flow location gaps `{catalog_location_gaps}`; catalog-flow evidence counters `{catalog_evidence_counters}`; Evidence Query snapshot diff `{snapshot_diff}`; snapshot diff location gaps `{snapshot_diff_location_gaps}`; snapshot diff evidence counters `{snapshot_diff_evidence_counters}`; native-audit evidence counters `{native_audit_evidence_counters}`; current-repo `{current}`; target-machine `{target}`.".format(
                count=release_pipeline.get("repair_path_count", len(repair_paths)),
                groups=release_pipeline.get("repair_path_group_count", "unknown"),
                environment_groups=release_pipeline.get(
                    "repair_path_environment_group_count", "unknown"
                ),
                category_groups=release_pipeline.get(
                    "repair_path_category_group_count", "unknown"
                ),
                current_repo_groups=release_pipeline.get(
                    "repair_path_current_repo_action_group_count", "unknown"
                ),
                target_validation_groups=release_pipeline.get(
                    "repair_path_target_or_validation_action_group_count",
                    "unknown",
                ),
                plan_steps=release_pipeline.get(
                    "repair_path_group_plan_step_count", "unknown"
                ),
                current_repo_plan_steps=release_pipeline.get(
                    "repair_path_group_plan_current_repo_step_count",
                    "unknown",
                ),
                target_validation_plan_steps=release_pipeline.get(
                    "repair_path_group_plan_target_or_validation_step_count",
                    "unknown",
                ),
                first_plan_step=release_pipeline.get(
                    "first_repair_path_group_plan_step", "unknown"
                ),
                first_group=release_pipeline.get("first_repair_path_group", "unknown"),
                location_gaps=release_pipeline.get(
                    "repair_path_source_location_gap_count", "unknown"
                ),
                evidence_counters=release_pipeline.get(
                    "repair_path_evidence_counter_count", "unknown"
                ),
                catalog=release_pipeline.get("catalog_flow_repair_path_count", "unknown"),
                catalog_location_gaps=release_pipeline.get(
                    "catalog_flow_repair_path_source_location_gap_count",
                    "unknown",
                ),
                catalog_evidence_counters=release_pipeline.get(
                    "catalog_flow_repair_path_evidence_counter_count",
                    "unknown",
                ),
                snapshot_diff=release_pipeline.get(
                    "evidence_query_snapshot_diff_repair_path_count",
                    "unknown",
                ),
                snapshot_diff_location_gaps=release_pipeline.get(
                    "evidence_query_snapshot_diff_repair_path_source_location_gap_count",
                    "unknown",
                ),
                snapshot_diff_evidence_counters=release_pipeline.get(
                    "evidence_query_snapshot_diff_repair_path_evidence_counter_count",
                    "unknown",
                ),
                native_audit_evidence_counters=release_pipeline.get(
                    "native_core_audit_repair_path_evidence_counter_count",
                    "unknown",
                ),
                current=release_pipeline.get("current_repo_repair_count", "unknown"),
                target=release_pipeline.get("target_machine_repair_count", "unknown"),
            )
        )
        if repair_paths and isinstance(repair_paths[0], dict):
            first_path = repair_paths[0]
            lines.append(
                "- First repair path: `{category}` from `{source}`; owner `{owner}`; first location `{location}`; action {action}.".format(
                    category=first_path.get("category", "unknown"),
                    source=first_path.get("source", "unknown"),
                    owner=first_path.get("repair_environment", "unknown"),
                    location=first_path.get("first_source_location", "unknown"),
                    action=first_path.get("recommended_action", "unknown"),
                )
            )
    return lines


def render_non_gpu_root_cause_query_section(
    closure: Mapping[str, Any],
    scorecard: Mapping[str, Any],
) -> List[str]:
    """Render Root Cause and canonical Evidence Query evidence."""
    lines: List[str] = []
    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    evidence_query_review = dict_value(
        closure.get("evidence_query_repair_path_review")
    )
    lines.append(
        "- Root-cause repair review: direct artifact `{direct}`; repair cards `{cards}`; current-repo `{current}`; replay-target `{replay}`; execute-target `{execute}`; legacy target-machine `{target}`; split `{split}`; split schema gap `{split_gap}`; legacy fallback `{fallback}`; flow-check `{flow}`.".format(
            direct=root_cause_review.get("direct_artifact_present", False),
            cards=root_cause_review.get("repair_card_count", "unknown"),
            current=root_cause_review.get("current_repo_repair_card_count", "unknown"),
            replay=root_cause_review.get("target_product_replay_repair_card_count", "unknown"),
            execute=root_cause_review.get("target_gpu_execute_repair_card_count", "unknown"),
            target=root_cause_review.get("target_machine_repair_card_count", "unknown"),
            split=root_cause_review.get("repair_environment_split_status", "unknown"),
            split_gap=root_cause_review.get("repair_environment_split_schema_gap", "unknown"),
            fallback=root_cause_review.get("repair_environment_legacy_fallback_used", "unknown"),
            flow=root_cause_review.get("flow_check_repair_card_count", "unknown"),
        )
    )
    lines.append(
        "- Root-cause catalog-flow blockers: root causes `{root_causes}`; review present `{present}`; discrepancies `{discrepancies}`; input gaps `{input_gaps}`; single-source repair cards `{single_source_cards}`; parity refresh required `{parity_refresh}`; surface ready `{surface_ready}`; surface fallback `{surface_fallback}`; surface parity refresh `{surface_parity}`; fallback-removal shortest-path root causes `{shortest_path_root_causes}`; shortest-path steps `{shortest_path_steps}`; current-repo/target/source-gaps `{shortest_path_repo}`/`{shortest_path_target}`/`{shortest_path_gaps}`; release-pipeline root causes/steps/source-gaps/repair-paths `{pipeline_fallback_roots}`/`{pipeline_fallback_steps}`/`{pipeline_fallback_gaps}`/`{pipeline_fallback_paths}`.".format(
            root_causes=root_cause_review.get("catalog_flow_root_cause_count", "unknown"),
            present=root_cause_review.get("catalog_flow_discrepancy_review_present", "unknown"),
            discrepancies=root_cause_review.get("catalog_flow_discrepancy_count", "unknown"),
            input_gaps=root_cause_review.get("catalog_flow_input_gap_count", "unknown"),
            single_source_cards=root_cause_review.get(
                "catalog_flow_single_source_repair_card_count", "unknown"
            ),
            parity_refresh=root_cause_review.get(
                "catalog_flow_single_source_parity_refresh_required_count",
                "unknown",
            ),
            surface_ready=root_cause_review.get(
                "catalog_flow_surface_migration_ready_count", "unknown"
            ),
            surface_fallback=root_cause_review.get(
                "catalog_flow_surface_migration_fallback_count", "unknown"
            ),
            surface_parity=root_cause_review.get(
                "catalog_flow_surface_parity_refresh_required_count", "unknown"
            ),
            shortest_path_root_causes=root_cause_review.get(
                "catalog_flow_fallback_removal_shortest_path_root_cause_count",
                "unknown",
            ),
            shortest_path_steps=root_cause_review.get(
                "catalog_flow_fallback_removal_shortest_path_step_count",
                "unknown",
            ),
            shortest_path_repo=root_cause_review.get(
                "catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
                "unknown",
            ),
            shortest_path_target=root_cause_review.get(
                "catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
                "unknown",
            ),
            shortest_path_gaps=root_cause_review.get(
                "catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                "unknown",
            ),
            pipeline_fallback_roots=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count",
                "unknown",
            ),
            pipeline_fallback_steps=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_step_count",
                "unknown",
            ),
            pipeline_fallback_gaps=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                "unknown",
            ),
            pipeline_fallback_paths=scorecard.get(
                "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_count",
                "unknown",
            ),
        )
    )
    first_root_cause_card = dict_value(root_cause_review.get("first_repair_card"))
    if first_root_cause_card:
        lines.append(
            "- First root-cause repair card: `{artifact}` / `{category}`; owner `{owner}`; priority `{priority}`; action {action}.".format(
                artifact=first_root_cause_card.get("artifact", "unknown"),
                category=first_root_cause_card.get("blocker_category", "unknown"),
                owner=first_root_cause_card.get("repair_environment", "unknown"),
                priority=first_root_cause_card.get("priority", "unknown"),
                action=first_root_cause_card.get("suggested_repair", "unknown"),
            )
        )
    lines.append(
        "- Root-cause source locations: review `{present}`; cards `{cards}`; first-card locations `{locations}`; first location `{first}`.".format(
            present=root_cause_review.get("source_location_review_present", "unknown"),
            cards=root_cause_review.get("source_location_card_count", "unknown"),
            locations=root_cause_review.get("first_card_source_location_count", "unknown"),
            first=root_cause_review.get("first_source_location", "unknown"),
        )
    )
    lines.append(
        "- Root-cause catalog-flow source locations: cards `{cards}`; location cards `{location_cards}`; first location `{first}`.".format(
            cards=root_cause_review.get("catalog_flow_repair_card_count", "unknown"),
            location_cards=root_cause_review.get("catalog_flow_source_location_card_count", "unknown"),
            first=root_cause_review.get("catalog_flow_first_source_location", "unknown"),
        )
    )
    root_cause_repair_cards = list_value(root_cause_review.get("repair_cards"))
    if root_cause_repair_cards:
        lines.extend(
            [
                "",
                "| Root-cause card | Artifact | Category | Owner | Locations | First location | Action |",
                "|---|---|---|---|---:|---|---|",
            ]
        )
        for card in root_cause_repair_cards[:5]:
            if not isinstance(card, dict):
                continue
            lines.append(
                "| `{card}` | `{artifact}` | `{category}` | `{owner}` | `{locations}` | {first_location} | {action} |".format(
                    card=markdown_cell(card.get("card_id"), 80),
                    artifact=markdown_cell(card.get("artifact"), 80),
                    category=markdown_cell(card.get("category"), 48),
                    owner=markdown_cell(card.get("repair_environment"), 48),
                    locations=markdown_cell(card.get("source_location_count"), 24),
                    first_location=markdown_cell(card.get("first_source_location"), 120),
                    action=markdown_cell(card.get("action"), 140),
                )
            )
    lines.append(
        "- Evidence-query first action: upstream `{upstream}`; repair-path query `{query}`; paths `{paths}`; source gaps `{gaps}`; source-location gaps `{location_gaps}`; first `{target}`/`{state}`; action {action}.".format(
            upstream=evidence_query_review.get("upstream_artifact", "native_evidence_query_result.json"),
            query=evidence_query_review.get("repair_path_query_present", "unknown"),
            paths=evidence_query_review.get("repair_path_count", "unknown"),
            gaps=evidence_query_review.get("repair_path_source_gap_count", "unknown"),
            location_gaps=evidence_query_review.get("repair_path_source_location_gap_count", "unknown"),
            target=evidence_query_review.get("first_target", "unknown"),
            state=evidence_query_review.get("first_state", "unknown"),
            action=evidence_query_review.get("first_action", "unknown"),
        )
    )
    lines.append(f"- Claim boundary: {closure.get('claim_boundary', 'Evidence-only closure; no execute benchmark claim.')}")
    return lines



def render_non_gpu_review_scorecard_overview(
    scorecard: Dict[str, Any],
) -> List[str]:
    lines: List[str] = []
    coverage = dict_value(scorecard.get("artifact_coverage"))
    blockers = dict_value(scorecard.get("blocker_counts"))
    lines.extend(["", "### Non-GPU Review Scorecard", ""])
    lines.append(
        "- Decision: `{review}`; release decision `{release}`; blocking signals `{signals}`.".format(
            review=scorecard.get("review_decision", "unknown"),
            release=scorecard.get("release_decision", "unknown"),
            signals=scorecard.get("blocking_signal_count", "unknown"),
        )
    )
    lines.append(
        "- Artifact coverage: present `{present}` / required `{required}`; missing `{missing}`.".format(
            present=coverage.get("present_count", "unknown"),
            required=coverage.get("required_count", "unknown"),
            missing=coverage.get("missing_count", "unknown"),
        )
    )
    lines.append(
        "- Release execution evidence: executed markers `{executed}`; execute-verified inputs `{execute_verified}`.".format(
            executed=scorecard.get("release_pipeline_executed_input_count", "unknown"),
            execute_verified=scorecard.get(
                "release_pipeline_execute_verified_input_count", "unknown"
            ),
        )
    )
    lines.append(
        "- Blockers: repair paths `{paths}`; source-location gaps `{path_gaps}`; catalog-flow paths `{catalog_paths}`; root-cause cards `{cards}`; Evidence Query source gaps `{query_gaps}`.".format(
            paths=blockers.get("repair_paths", "unknown"),
            path_gaps=blockers.get("release_pipeline_repair_path_source_location_gaps", "unknown"),
            catalog_paths=blockers.get("release_pipeline_catalog_flow_repair_paths", "unknown"),
            cards=blockers.get("root_cause_repair_cards", "unknown"),
            query_gaps=blockers.get("evidence_query_repair_path_source_gaps", "unknown"),
        )
    )
    lines.append(
        "- Catalog fallback-removal parity gate: present `{present}`; decision `{decision}`; removal allowed `{allowed}`; blocks removal `{blocks}`; first surface `{first_surface}`; totals blockers/parity-refresh/current-repo/target-machine/source-gaps `{total_blockers}`/`{total_parity}`/`{total_repo}`/`{total_target}`/`{total_gaps}`; direct Evidence Query blockers/parity/source-gaps `{query_blockers}`/`{query_parity}`/`{query_gaps}`; root-cause root-causes/blockers/parity/source-gaps `{root_causes}`/`{root_blockers}`/`{root_parity}`/`{root_gaps}`; release root-cause root-causes/blockers/parity/source-gaps `{pipeline_roots}`/`{pipeline_blockers}`/`{pipeline_parity}`/`{pipeline_gaps}`; release repair paths/source-gaps `{pipeline_paths}`/`{pipeline_path_gaps}`; root-cause repair paths/source-gaps `{pipeline_root_paths}`/`{pipeline_root_path_gaps}`.".format(
            present=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_present",
                "unknown",
            ),
            decision=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_decision",
                "unknown",
            ),
            allowed=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_removal_allowed",
                "unknown",
            ),
            blocks=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_blocks_removal",
                "unknown",
            ),
            first_surface=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_first_surface",
                "unknown",
            ),
            total_blockers=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_total_blocker_count",
                "unknown",
            ),
            total_parity=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_total_parity_refresh_required_count",
                "unknown",
            ),
            total_repo=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_total_current_repo_action_count",
                "unknown",
            ),
            total_target=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_total_target_machine_action_count",
                "unknown",
            ),
            total_gaps=scorecard.get(
                "catalog_flow_fallback_removal_parity_gate_total_source_location_gap_count",
                "unknown",
            ),
            query_blockers=scorecard.get(
                "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_blocker_count",
                "unknown",
            ),
            query_parity=scorecard.get(
                "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
                "unknown",
            ),
            query_gaps=scorecard.get(
                "release_pipeline_evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
                "unknown",
            ),
            root_causes=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count",
                "unknown",
            ),
            root_blockers=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count",
                "unknown",
            ),
            root_parity=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
                "unknown",
            ),
            root_gaps=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
                "unknown",
            ),
            pipeline_roots=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count",
                "unknown",
            ),
            pipeline_blockers=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count",
                "unknown",
            ),
            pipeline_parity=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
                "unknown",
            ),
            pipeline_gaps=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
                "unknown",
            ),
            pipeline_paths=scorecard.get(
                "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_path_count",
                "unknown",
            ),
            pipeline_path_gaps=scorecard.get(
                "release_pipeline_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
                "unknown",
            ),
            pipeline_root_paths=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_count",
                "unknown",
            ),
            pipeline_root_path_gaps=scorecard.get(
                "release_pipeline_root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
                "unknown",
            ),
        )
    )
    blocker_rows, omitted_blocker_count = sorted_nonzero_blocker_rows(blockers)
    if blocker_rows:
        lines.extend(
            [
                "",
                "| Blocker signal | Count |",
                "|---|---:|",
            ]
        )
        for signal, count in blocker_rows:
            lines.append(
                "| `{signal}` | `{count}` |".format(
                    signal=markdown_cell(signal, 96),
                    count=markdown_cell(count, 24),
                )
            )
        if omitted_blocker_count > 0:
            lines.append(
                "| `_omitted_nonzero_blockers` | `{count}` |".format(
                    count=omitted_blocker_count
                )
            )
    lines.extend(render_evidence_query_ux_scorecard(scorecard))
    return lines


def render_non_gpu_review_scorecard_release_details(
    scorecard: Dict[str, Any],
    closure: Dict[str, Any],
) -> List[str]:
    lines: List[str] = []
    ownership = dict_value(scorecard.get("repair_ownership"))
    release_pipeline = dict_value(closure.get("release_pipeline"))
    root_causes = list_value(scorecard.get("deduped_root_causes"))
    lines.append(
        "- Deduped root causes: `{count}` distinct causes from `{signals}` raw blocking-signal groups.".format(
            count=scorecard.get("deduped_root_cause_count", len(root_causes)),
            signals=scorecard.get("blocking_signal_count", "unknown"),
        )
    )
    if root_causes:
        lines.extend(
            [
                "",
                "| Root cause | Owner | Representative count | Raw signals | Action |",
                "|---|---|---:|---:|---|",
            ]
        )
        for cause in root_causes[:6]:
            if not isinstance(cause, dict):
                continue
            lines.append(
                "| `{root}` | `{owner}` | `{count}` | `{signals}` | {action} |".format(
                    root=markdown_cell(cause.get("root_cause"), 80),
                    owner=markdown_cell(cause.get("owner"), 60),
                    count=markdown_cell(cause.get("representative_count"), 24),
                    signals=markdown_cell(cause.get("raw_signal_count"), 24),
                    action=markdown_cell(cause.get("recommended_action"), 140),
                )
            )
    lines.append(
        "- Repair ownership: current repo `{current}`; target machine `{target}`; flow-check `{flow}`.".format(
            current=ownership.get("current_repo", "unknown"),
            target=ownership.get("target_machine", "unknown"),
            flow=ownership.get("flow_check", "unknown"),
        )
    )
    lines.append(
        "- Release pipeline catalog-flow repair paths: paths `{paths}`; field-coverage paths `{field_paths}`; fallback shortest-path repairs `{fallback_paths}`; fallback shortest-path location gaps `{fallback_location_gaps}`; location gaps `{location_gaps}`; evidence counters `{evidence_counters}`; first location `{first_location}`; fallback first location `{fallback_first_location}`; explain cards `{cards}`; discrepancies `{discrepancies}`; input gaps `{input_gaps}`; single-source repair cards `{single_source_cards}`; parity refresh required `{parity_refresh}`; surface ready `{surface_ready}`; surface fallback `{surface_fallback}`; surface parity refresh `{surface_parity}`.".format(
            paths=scorecard.get("release_pipeline_catalog_flow_repair_path_count", "unknown"),
            field_paths=scorecard.get(
                "release_pipeline_catalog_flow_field_coverage_repair_path_count",
                "unknown",
            ),
            fallback_paths=scorecard.get(
                "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_count",
                "unknown",
            ),
            fallback_location_gaps=scorecard.get(
                "release_pipeline_catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
                "unknown",
            ),
            location_gaps=scorecard.get(
                "release_pipeline_catalog_flow_repair_path_source_location_gap_count",
                "unknown",
            ),
            evidence_counters=scorecard.get(
                "release_pipeline_catalog_flow_repair_path_evidence_counter_count",
                "unknown",
            ),
            first_location=release_pipeline.get(
                "catalog_flow_first_source_location", "unknown"
            ),
            fallback_first_location=release_pipeline.get(
                "catalog_flow_fallback_removal_shortest_path_first_source_location",
                "unknown",
            ),
            cards=release_pipeline.get("explain_catalog_flow_discrepancy_card_count", "unknown"),
            discrepancies=release_pipeline.get("explain_catalog_flow_discrepancy_count", "unknown"),
            input_gaps=release_pipeline.get("explain_catalog_flow_input_gap_count", "unknown"),
            single_source_cards=release_pipeline.get(
                "explain_catalog_flow_single_source_repair_card_count",
                "unknown",
            ),
            parity_refresh=release_pipeline.get(
                "explain_catalog_flow_single_source_parity_refresh_required_count",
                "unknown",
            ),
            surface_ready=release_pipeline.get(
                "explain_catalog_flow_surface_migration_ready_count", "unknown"
            ),
            surface_fallback=release_pipeline.get(
                "explain_catalog_flow_surface_migration_fallback_count", "unknown"
            ),
            surface_parity=release_pipeline.get(
                "explain_catalog_flow_surface_parity_refresh_required_count",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Release pipeline Evidence Query snapshot-diff repair paths: paths `{paths}`; location gaps `{location_gaps}`; first location `{first_location}`; added/removed/changed JSON paths `{added}`/`{removed}`/`{changed}`; truncated `{truncated}`; decision `{decision}`.".format(
            paths=scorecard.get(
                "release_pipeline_evidence_query_snapshot_diff_repair_path_count",
                "unknown",
            ),
            location_gaps=scorecard.get(
                "release_pipeline_evidence_query_snapshot_diff_repair_path_source_location_gap_count",
                "unknown",
            ),
            first_location=release_pipeline.get(
                "evidence_query_snapshot_diff_first_source_location",
                "unknown",
            ),
            added=release_pipeline.get(
                "evidence_query_snapshot_diff_added_deep_path_count",
                "unknown",
            ),
            removed=release_pipeline.get(
                "evidence_query_snapshot_diff_removed_deep_path_count",
                "unknown",
            ),
            changed=release_pipeline.get(
                "evidence_query_snapshot_diff_changed_deep_value_count",
                "unknown",
            ),
            truncated=release_pipeline.get(
                "evidence_query_snapshot_diff_deep_semantic_truncated",
                "unknown",
            ),
            decision=release_pipeline.get(
                "evidence_query_snapshot_diff_deep_semantic_decision",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Snapshot-to-release link: links `{links}`; blockers `{blockers}`; status `{status}`; reason `{reason}`; group-plan step `{group_plan_step}`; current-repo actions `{current_repo_actions}`; linked repair path `{linked_path}`.".format(
            links=scorecard.get(
                "release_pipeline_evidence_query_snapshot_to_release_link_count",
                "unknown",
            ),
            blockers=scorecard.get(
                "release_pipeline_evidence_query_snapshot_to_release_blocker_count",
                "unknown",
            ),
            status=release_pipeline.get(
                "evidence_query_snapshot_to_release_link_status",
                "unknown",
            ),
            reason=release_pipeline.get(
                "evidence_query_snapshot_to_release_blocker_reason",
                "unknown",
            ),
            group_plan_step=scorecard.get(
                "release_pipeline_evidence_query_snapshot_to_release_group_plan_step",
                "unknown",
            ),
            current_repo_actions=scorecard.get(
                "release_pipeline_evidence_query_snapshot_to_release_current_repo_action_count",
                "unknown",
            ),
            linked_path=release_pipeline.get(
                "evidence_query_snapshot_to_release_linked_repair_path_id",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Blocker repair-card link: links `{links}`; linked cards-to-paths `{linked}`; status `{status}`; Evidence Query read `{root_read}`; root-cause cards `{root_cards}`; unmatched release paths `{unmatched_paths}`; unmatched root causes `{unmatched_roots}`; current-repo actions `{current_repo_actions}`; target/validation actions `{target_actions}`; first path `{first_path}`; first category `{first_category}`; first location `{first_location}`.".format(
            links=scorecard.get(
                "release_pipeline_blocker_repair_card_link_count",
                "unknown",
            ),
            linked=scorecard.get(
                "release_pipeline_blocker_repair_card_linked_count",
                "unknown",
            ),
            status=scorecard.get(
                "release_pipeline_blocker_repair_card_link_status",
                release_pipeline.get("blocker_repair_card_link_status", "unknown"),
            ),
            root_read=release_pipeline.get(
                "evidence_query_read_status", "unknown"
            ),
            root_cards=release_pipeline.get(
                "blocker_repair_card_root_cause_repair_card_count",
                "unknown",
            ),
            unmatched_paths=scorecard.get(
                "release_pipeline_blocker_repair_card_unmatched_release_path_count",
                "unknown",
            ),
            unmatched_roots=scorecard.get(
                "release_pipeline_blocker_repair_card_unmatched_root_cause_count",
                "unknown",
            ),
            current_repo_actions=scorecard.get(
                "release_pipeline_blocker_repair_card_current_repo_action_count",
                "unknown",
            ),
            target_actions=scorecard.get(
                "release_pipeline_blocker_repair_card_target_or_validation_action_count",
                "unknown",
            ),
            first_path=release_pipeline.get(
                "blocker_repair_card_first_release_repair_path_id",
                "unknown",
            ),
            first_category=release_pipeline.get(
                "blocker_repair_card_first_release_repair_path_category",
                "unknown",
            ),
            first_location=release_pipeline.get(
                "blocker_repair_card_first_release_repair_source_location",
                "unknown",
            ),
        )
    )
    lines.extend(render_release_pipeline_evidence_query_repairs(scorecard))
    lines.extend(render_release_pipeline_cross_artifact(scorecard))
    lines.extend(render_product_replay_release_evidence(scorecard))
    lines.extend(render_evidence_query_selector_triage(scorecard))
    lines.append(render_release_pipeline_readiness_path(scorecard))
    lines.append(
        render_release_pipeline_native_audit(scorecard, release_pipeline)
    )
    return lines


def render_non_gpu_review_scorecard_actions(
    scorecard: Dict[str, Any],
    closure: Dict[str, Any],
) -> List[str]:
    lines: List[str] = []
    root_cause_review = dict_value(closure.get("root_cause_repair_review"))
    lines.append(
        "- Root-cause catalog-flow blockers: root causes `{root_causes}`; discrepancies `{discrepancies}`; input gaps `{input_gaps}`; single-source repair cards `{single_source_cards}`; parity refresh required `{parity_refresh}`; surface ready `{surface_ready}`; surface fallback `{surface_fallback}`; surface parity refresh `{surface_parity}`; fallback-removal shortest-path root causes `{shortest_path_root_causes}`; shortest-path steps `{shortest_path_steps}`; current-repo/target/source-gaps `{shortest_path_repo}`/`{shortest_path_target}`/`{shortest_path_gaps}`; metadata-policy gaps `{metadata_gaps}`; metadata-policy cards `{metadata_cards}`; metadata-policy fields `{metadata_fields}`.".format(
            root_causes=scorecard.get("root_cause_catalog_flow_root_cause_count", "unknown"),
            discrepancies=scorecard.get("root_cause_catalog_flow_discrepancy_count", "unknown"),
            input_gaps=scorecard.get("root_cause_catalog_flow_input_gap_count", "unknown"),
            single_source_cards=scorecard.get(
                "root_cause_catalog_flow_single_source_repair_card_count",
                "unknown",
            ),
            parity_refresh=scorecard.get(
                "root_cause_catalog_flow_single_source_parity_refresh_required_count",
                "unknown",
            ),
            surface_ready=scorecard.get(
                "root_cause_catalog_flow_surface_migration_ready_count", "unknown"
            ),
            surface_fallback=scorecard.get(
                "root_cause_catalog_flow_surface_migration_fallback_count", "unknown"
            ),
            surface_parity=scorecard.get(
                "root_cause_catalog_flow_surface_parity_refresh_required_count",
                "unknown",
            ),
            shortest_path_root_causes=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count",
                "unknown",
            ),
            shortest_path_steps=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_shortest_path_step_count",
                "unknown",
            ),
            shortest_path_repo=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
                "unknown",
            ),
            shortest_path_target=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
                "unknown",
            ),
            shortest_path_gaps=scorecard.get(
                "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                "unknown",
            ),
            metadata_gaps=scorecard.get(
                "root_cause_catalog_metadata_policy_gap_root_cause_count",
                "unknown",
            ),
            metadata_cards=scorecard.get(
                "root_cause_catalog_flow_metadata_policy_card_count",
                "unknown",
            ),
            metadata_fields=scorecard.get(
                "root_cause_catalog_flow_metadata_derived_policy_field_count",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Root-cause catalog-flow action source: actions `{actions}`; location gaps `{location_gaps}`; first location `{first_location}`.".format(
            actions=scorecard.get("root_cause_catalog_flow_action_source_count", "unknown"),
            location_gaps=scorecard.get(
                "root_cause_catalog_flow_action_source_location_gap_count",
                "unknown",
            ),
            first_location=root_cause_review.get("catalog_flow_first_source_location", "unknown"),
        )
    )
    lines.append(
        "- Evidence-query catalog-flow filter: decision `{decision}`; discrepancies `{discrepancies}`; input gaps `{input_gaps}`; repair cards `{cards}`; single-source repair cards `{single_source_cards}`; parity refresh required `{parity_refresh}`; surface ready `{surface_ready}`; surface fallback `{surface_fallback}`; surface parity refresh `{surface_parity}`; fallback-removal shortest path state `{shortest_path_state}`; shortest path steps `{shortest_path_steps}`; current-repo steps `{shortest_path_current_repo_steps}`; target/validation steps `{shortest_path_target_steps}`; source-location gaps `{shortest_path_location_gaps}`; field coverage surfaces `{field_surfaces}`; field fallback surfaces `{field_fallback_surfaces}`; required fields `{required_fields}`; field gaps `{field_gaps}`; field parity refresh `{field_parity_refresh}`; field repair cards `{field_repair_cards}`; current-repo field cards `{field_current_repo_cards}`; target/validation field cards `{field_target_cards}`; metadata-policy cards `{metadata_cards}`; metadata-policy fields `{metadata_fields}`.".format(
            decision=scorecard.get("evidence_query_catalog_flow_filter_decision", "unknown"),
            discrepancies=scorecard.get("evidence_query_catalog_flow_discrepancy_count", "unknown"),
            input_gaps=scorecard.get("evidence_query_catalog_flow_input_gap_count", "unknown"),
            cards=scorecard.get("evidence_query_catalog_flow_repair_card_count", "unknown"),
            single_source_cards=scorecard.get(
                "evidence_query_catalog_flow_single_source_repair_card_count",
                "unknown",
            ),
            parity_refresh=scorecard.get(
                "evidence_query_catalog_flow_single_source_parity_refresh_required_count",
                "unknown",
            ),
            surface_ready=scorecard.get(
                "evidence_query_catalog_flow_surface_migration_ready_count", "unknown"
            ),
            surface_fallback=scorecard.get(
                "evidence_query_catalog_flow_surface_migration_fallback_count", "unknown"
            ),
            surface_parity=scorecard.get(
                "evidence_query_catalog_flow_surface_parity_refresh_required_count",
                "unknown",
            ),
            shortest_path_state=scorecard.get(
                "catalog_flow_fallback_removal_shortest_path_state",
                "unknown",
            ),
            shortest_path_steps=scorecard.get(
                "catalog_flow_fallback_removal_shortest_path_step_count",
                "unknown",
            ),
            shortest_path_current_repo_steps=scorecard.get(
                "catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
                "unknown",
            ),
            shortest_path_target_steps=scorecard.get(
                "catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
                "unknown",
            ),
            shortest_path_location_gaps=scorecard.get(
                "catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                "unknown",
            ),
            field_surfaces=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_surface_count",
                "unknown",
            ),
            field_fallback_surfaces=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_fallback_surface_count",
                "unknown",
            ),
            required_fields=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_required_field_count",
                "unknown",
            ),
            field_gaps=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_field_gap_count",
                "unknown",
            ),
            field_parity_refresh=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_parity_refresh_required_count",
                "unknown",
            ),
            field_repair_cards=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_repair_card_count",
                "unknown",
            ),
            field_current_repo_cards=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
                "unknown",
            ),
            field_target_cards=scorecard.get(
                "evidence_query_catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
                "unknown",
            ),
            metadata_cards=scorecard.get(
                "evidence_query_catalog_flow_metadata_policy_card_count",
                "unknown",
            ),
            metadata_fields=scorecard.get(
                "evidence_query_catalog_flow_metadata_derived_policy_field_count",
                "unknown",
            ),
        )
    )
    lines.append(
        "- Next action: `{source}` -> {action}".format(
            source=scorecard.get("next_action_source", "unknown"),
            action=scorecard.get("next_action", "unknown"),
        )
    )
    top_actions = list_value(scorecard.get("top_repair_actions"))
    if top_actions:
        lines.extend(["", "| Source | Artifact | Category | Owner | Priority | Action |", "|---|---|---|---|---:|---|"])
        for action in top_actions[:5]:
            if not isinstance(action, dict):
                continue
            lines.append(
                "| `{source}` | `{artifact}` | `{category}` | `{owner}` | `{priority}` | {repair} |".format(
                    source=action.get("source", "unknown"),
                    artifact=action.get("artifact", "unknown"),
                    category=action.get("category", "unknown"),
                    owner=action.get("repair_environment", "unknown"),
                    priority=action.get("priority", "unknown"),
                    repair=action.get("action", "unknown"),
                )
            )
    action_source_trace = list_value(scorecard.get("action_source_trace"))
    if action_source_trace:
        lines.extend(
            [
                "",
                "| Action source | Upstream artifact | Status | Actions | Gaps | Location gaps | Selected |",
                "|---|---|---|---:|---:|---:|---|",
            ]
        )
        for item in action_source_trace:
            if not isinstance(item, dict):
                continue
            lines.append(
                "| `{source}` | `{artifact}` | `{status}` | `{actions}` | `{gaps}` | `{location_gaps}` | `{selected}` |".format(
                    source=item.get("source", "unknown"),
                    artifact=item.get("upstream_artifact", "unknown"),
                    status=item.get("checklist_status", "unknown"),
                    actions=item.get("action_count", "unknown"),
                    gaps=item.get("source_gap_count", "unknown"),
                    location_gaps=item.get("source_location_gap_count", "unknown"),
                    selected=item.get("selected_for_top_actions", "unknown"),
                )
            )
    lines.append(f"- Claim boundary: {scorecard.get('claim_boundary', 'Evidence-only scorecard; no execute benchmark claim.')}")
    return lines


def render_non_gpu_review_scorecard_section(
    scorecard: Dict[str, Any],
    closure: Dict[str, Any],
) -> List[str]:
    lines = render_non_gpu_review_scorecard_overview(scorecard)
    lines.extend(render_non_gpu_review_scorecard_release_details(scorecard, closure))
    lines.extend(render_non_gpu_review_scorecard_actions(scorecard, closure))
    return lines


def render_final_report_supporting_sections(
    payload: Dict[str, Any],
) -> List[str]:
    lines: List[str] = []
    lines.extend(["", "## Bottleneck Migration", "", "| Area | Original bottleneck | Optimization path | New bottleneck or risk |", "|---|---|---|---|"])
    for area, original, optimization, risk in BOTTLENECKS:
        lines.append(f"| {area} | {original} | {optimization} | {risk} |")

    command_source = payload.get("benchmark_matrix_command_source", {})
    lines.extend(["", "## Reproducible Commands"])
    if isinstance(command_source, dict):
        lines.append("")
        lines.append(
            "- Benchmark matrix command source: `{status}`; fallback used: `{fallback}`.".format(
                status=command_source.get("status", "unknown"),
                fallback=command_source.get("fallback_used", "unknown"),
            )
        )
    lines.extend(["", "```bash"])
    lines.extend(payload.get("reproducible_commands", FALLBACK_COMMANDS))
    lines.append("```")

    lines.extend(["", "## Source Notes And Reports", "", "### Reports", ""])
    if payload["reports"]:
        counts = payload.get("report_kind_counts", {})
        if counts:
            lines.append("| Kind | Count |")
            lines.append("|---|---:|")
            for kind, count in counts.items():
                lines.append(f"| {kind} | {count} |")
            lines.append("")
        lines.append("| Kind | Report | Title |")
        lines.append("|---|---|---|")
        for report in payload["reports"]:
            lines.append(f"| {report.get('kind', 'unknown')} | `{report['path']}` | {report['title']} |")
    else:
        lines.append("- No report files found.")
    lines.extend(["", "### Source / Runtime Notes", ""])
    if payload["notes"]:
        for note in payload["notes"]:
            lines.append(f"- `{note['path']}`: {note['title']}")
    else:
        lines.append("- No source notes found.")
    scan_issues = payload.get("report_scan_issues", []) + payload.get("note_scan_issues", [])
    for issue in payload.get("read_issues", []):
        if issue not in scan_issues:
            scan_issues.append(issue)
    if scan_issues:
        lines.extend(["", "### Scan / Read Issues", ""])
        for issue in scan_issues[:10]:
            if not isinstance(issue, dict):
                continue
            lines.append(
                "- `{status}` while scanning/reading `{path}`: {error}".format(
                    status=issue.get("status", "unknown"),
                    path=issue.get("path", ""),
                    error=issue.get("error", ""),
                )
            )

    lines.extend(["", "## Missing Runtime Evidence", ""])
    missing_any = False
    for module in payload["modules"]:
        if not module["missing_results"]:
            continue
        missing_any = True
        lines.append(f"- {module['name']}: " + ", ".join(f"`{item}`" for item in module["missing_results"]))
    if not missing_any:
        lines.append("- No missing result files from the module catalog.")

    lines.extend(["", "## Untested / Deferred Items", ""])
    if payload["untested"]:
        for item in payload["untested"][:20]:
            lines.append(f"- {item}")
    else:
        lines.append("- No untested section was found in `evaluation.md`.")

    lines.extend([""])
    lines.extend(render_cost_section(payload["cost_records"], Path(payload["cost_path"])))
    lines.extend(
        [
            "",
            "## Resume-Ready Summary",
            "",
            "- Code and scaffolding can be reviewed on the current CPU-only server.",
            "- Runtime measurements should be filled on machines with CUDA/vLLM/TensorRT-LLM, llama.cpp models, Android/ARM devices, and Prometheus/Grafana.",
            "- Every claimed performance number should map back to a JSON artifact under `results/raw` and a command in this report.",
            "",
        ]
    )
    return lines


def render_markdown(payload: Dict[str, Any]) -> str:
    scorecard = dict_value(payload.get("non_gpu_review_scorecard"))
    lines: List[str] = [
        "# InferLab Final Report",
        "",
        f"- Generated: `{payload['generated_at']}`",
        f"- Results root: `{payload['results_root']}`",
        "",
        "## Executive Summary",
        "",
        "InferLab is a cloud plus edge LLM inference optimization lab. It connects Mini Transformer references, native C++/CUDA primitives, KV/PagedAttention scheduling, quantization/GGUF parsing, vLLM/TensorRT-LLM serving harnesses, llama.cpp edge benchmarking, Android deployment scaffolding, and Prometheus/Grafana observability.",
        "",
        "The report is generated from repository artifacts. Missing runtime evidence is listed explicitly so later GPU, serving, Android, and monitoring machines can fill the remaining data without changing the report structure.",
        "",
        "## Architecture",
        "",
        "```mermaid",
        "flowchart LR",
        "  prompts[Eval prompts] --> serving[vLLM / TensorRT-LLM / llama.cpp harnesses]",
        "  serving --> raw[results/raw JSON]",
        "  native[C++/CUDA native probes] --> raw",
        "  mini[Mini Transformer analysis] --> raw",
        "  raw --> summary[benchmark summary]",
        "  summary --> report[final report]",
        "  raw --> metrics[Prometheus textfile metrics]",
        "  metrics --> grafana[Grafana dashboard]",
        "```",
        "",
        "## Module Coverage",
        "",
        "| Module | Why it matters | Code | Notes | Report | Results | Probe Fail |",
        "|---|---|---:|---:|---:|---:|---:|",
    ]
    for row in payload["modules"]:
        lines.append(
            f"| {row['name']} | {row['why']} | {bool_text(row['code_ready'])} | {bool_text(row['notes_ready'])} | {bool_text(row['report_ready'])} | {row['result_count']}/{row['result_total']} | {row.get('path_probe_failed_count', 0)} |"
        )

    lines.extend(["", "## Benchmark Summary", ""])
    if payload["summary_sections"]:
        lines.extend(["| Section | Source | Status | Confidence | Metric count | Selected metrics |", "|---|---|---:|---:|---:|---|"])
        for section in payload["summary_sections"]:
            selected = ", ".join(f"{name}={value}" for name, value in section["selected"]) or ""
            lines.append(
                f"| {section['title']} | `{section['source']}` | `{section['status']}` | `{section.get('evidence_confidence', 'unknown')}` | {section['metric_count']} | {selected} |"
            )
    else:
        lines.append("No `benchmark_summary.json` was found. Run the summary command after runtime validation is enabled.")

    lines.extend(["", "## Evidence Confidence Rollup", ""])
    rollup = payload.get("evidence_confidence_rollup", {})
    badges = rollup.get("badges", []) if isinstance(rollup, dict) else []
    if badges:
        lines.extend(["| Tier | Count | Meaning |", "|---|---:|---|"])
        for badge in badges:
            if not isinstance(badge, dict):
                continue
            lines.append(
                f"| `{badge.get('tier', 'unknown')}` | {badge.get('count', 0)} | {badge.get('description', '')} |"
            )
        lines.append("")
        lines.append(
            "- Strongest present tier: `{}`.".format(
                rollup.get("strongest_present_tier", "unknown")
            )
        )
        lines.append(f"- Claim boundary: {rollup.get('claim_boundary', 'Confidence tiers classify evidence only.')}")
    else:
        lines.append("No evidence confidence rollup was available because `benchmark_summary.json` was not loaded.")

    lines.extend(["", "## Evidence Governance And High-Value Engineering", ""])
    catalog_resolution = payload.get("catalog_source_resolution", {})
    if catalog_resolution:
        lines.append(
            "- Catalog source status: `{status}`; policy `{policy}`; resolved sources: `{resolved}`; extra catalog sources: `{extra}`; extension-only sources `{extension_only}`; shared sources `{shared}`.".format(
                status=catalog_resolution.get("status", "unknown"),
                policy=catalog_resolution.get("source_policy", "unknown"),
                resolved=catalog_resolution.get("resolved_count", "unknown"),
                extra=catalog_resolution.get("extra_catalog_count", "unknown"),
                extension_only=catalog_resolution.get(
                    "extension_only_count", "unknown"
                ),
                shared=catalog_resolution.get("shared_count", "unknown"),
            )
        )
        catalog_error = catalog_resolution.get("catalog_error")
        if catalog_error:
            lines.append(
                "- Catalog source error: {}.".format(markdown_inline_code(catalog_error))
            )
    catalog_drift = payload.get("catalog_drift_review", {})
    if catalog_drift:
        lines.append(
            "- Catalog drift review: present `{present}`; registry decision `{decision}`; matrix entries `{entries}`; missing surfaces `{missing}`; retained fallback surfaces `{fallback}`; surface-ready `{surface_ready}`; surface fallback `{surface_fallback}`; surface parity refresh `{surface_parity}`; consumer migration plans `{consumer_plans}`; fallback removal blocked `{fallback_blocked}`; fallback shortest path `{fallback_path_state}` with steps `{fallback_path_steps}`, repo steps `{fallback_path_repo_steps}`, target steps `{fallback_path_target_steps}`, source-location gaps `{fallback_path_gaps}`, first surface `{fallback_path_first_surface}`; fallback parity gate `{parity_gate_decision}`; removal allowed `{parity_gate_allowed}`; gate blockers `{parity_gate_blockers}`; gate parity refresh `{parity_gate_refresh}`; gate repo/target actions `{parity_gate_repo_actions}`/`{parity_gate_target_actions}`; gate source-location gaps `{parity_gate_source_gaps}`; gate first surface `{parity_gate_first_surface}`; field coverage surfaces `{field_surfaces}`; field-covered surfaces `{field_catalog_surfaces}`; field fallback surfaces `{field_fallback_surfaces}`; required fields `{field_required}`; covered fields `{field_covered}`; field gaps `{field_gaps}`; field parity refresh `{field_parity}`; current-repo surface actions `{surface_repo_actions}`; target-machine surface actions `{surface_target_actions}`; drift decision `{drift}`; parity decision `{parity}`; repair cards `{repair_cards}`; parity refresh required `{parity_refresh}`; metadata decision `{metadata_decision}`; derived policy artifacts `{derived_artifacts}`; derived policy fields `{derived_fields}`.".format(
                present=catalog_drift.get("present", "unknown"),
                decision=catalog_drift.get("registry_decision", "unknown"),
                entries=catalog_drift.get("downstream_wiring_matrix_entry_count", "unknown"),
                missing=catalog_drift.get("catalog_drift_missing_surface_count", "unknown"),
                fallback=catalog_drift.get("catalog_fallback_retained_surface_count", "unknown"),
                surface_ready=catalog_drift.get(
                    "catalog_surface_migration_ready_count", "unknown"
                ),
                surface_fallback=catalog_drift.get(
                    "catalog_surface_migration_fallback_count", "unknown"
                ),
                surface_parity=catalog_drift.get(
                    "catalog_surface_parity_refresh_required_count", "unknown"
                ),
                consumer_plans=catalog_drift.get(
                    "surface_consumer_migration_plan_count", "unknown"
                ),
                fallback_blocked=catalog_drift.get(
                    "surface_consumer_fallback_removal_blocked_count",
                    "unknown",
                ),
                fallback_path_state=catalog_drift.get(
                    "fallback_removal_shortest_path_state", "unknown"
                ),
                fallback_path_steps=catalog_drift.get(
                    "fallback_removal_shortest_path_step_count", "unknown"
                ),
                fallback_path_repo_steps=catalog_drift.get(
                    "fallback_removal_shortest_path_current_repo_step_count",
                    "unknown",
                ),
                fallback_path_target_steps=catalog_drift.get(
                    "fallback_removal_shortest_path_target_machine_step_count",
                    "unknown",
                ),
                fallback_path_gaps=catalog_drift.get(
                    "fallback_removal_shortest_path_source_location_gap_count",
                    "unknown",
                ),
                fallback_path_first_surface=catalog_drift.get(
                    "fallback_removal_shortest_path_first_surface", "unknown"
                ),
                parity_gate_decision=catalog_drift.get(
                    "fallback_removal_parity_gate_decision", "unknown"
                ),
                parity_gate_allowed=catalog_drift.get(
                    "fallback_removal_parity_gate_removal_allowed", "unknown"
                ),
                parity_gate_blockers=catalog_drift.get(
                    "fallback_removal_parity_gate_blocker_count", "unknown"
                ),
                parity_gate_refresh=catalog_drift.get(
                    "fallback_removal_parity_gate_parity_refresh_required_count",
                    "unknown",
                ),
                parity_gate_repo_actions=catalog_drift.get(
                    "fallback_removal_parity_gate_current_repo_action_count",
                    "unknown",
                ),
                parity_gate_target_actions=catalog_drift.get(
                    "fallback_removal_parity_gate_target_machine_action_count",
                    "unknown",
                ),
                parity_gate_source_gaps=catalog_drift.get(
                    "fallback_removal_parity_gate_source_location_gap_count",
                    "unknown",
                ),
                parity_gate_first_surface=catalog_drift.get(
                    "fallback_removal_parity_gate_first_surface", "unknown"
                ),
                field_surfaces=catalog_drift.get(
                    "consumer_field_coverage_surface_count", "unknown"
                ),
                field_catalog_surfaces=catalog_drift.get(
                    "consumer_field_coverage_catalog_managed_surface_count",
                    "unknown",
                ),
                field_fallback_surfaces=catalog_drift.get(
                    "consumer_field_coverage_fallback_surface_count",
                    "unknown",
                ),
                field_required=catalog_drift.get(
                    "consumer_field_coverage_required_field_count", "unknown"
                ),
                field_covered=catalog_drift.get(
                    "consumer_field_coverage_covered_field_count", "unknown"
                ),
                field_gaps=catalog_drift.get(
                    "consumer_field_coverage_field_gap_count", "unknown"
                ),
                field_parity=catalog_drift.get(
                    "consumer_field_coverage_parity_refresh_required_count",
                    "unknown",
                ),
                surface_repo_actions=catalog_drift.get(
                    "catalog_surface_current_repo_action_count", "unknown"
                ),
                surface_target_actions=catalog_drift.get(
                    "catalog_surface_target_machine_action_count", "unknown"
                ),
                drift=catalog_drift.get("drift_decision", "unknown"),
                parity=catalog_drift.get("parity_decision", "unknown"),
                repair_cards=catalog_drift.get("single_source_repair_card_count", "unknown"),
                parity_refresh=catalog_drift.get("single_source_parity_refresh_required_count", "unknown"),
                metadata_decision=catalog_drift.get("metadata_decision", "unknown"),
                derived_artifacts=catalog_drift.get(
                    "registry_derived_policy_artifact_count", "unknown"
                ),
                derived_fields=catalog_drift.get(
                    "registry_derived_policy_field_count", "unknown"
                ),
            )
        )
    governance_sources = set(payload.get("evidence_governance_sources", []))
    governance_sections = [
        section
        for section in payload["summary_sections"]
        if section.get("source") in governance_sources
    ]
    if governance_sections:
        for section in governance_sections:
            selected = ", ".join(f"{name}={value}" for name, value in section["selected"]) or ""
            lines.append(
                f"- `{section['source']}`: status `{section['status']}`; selected metrics: {selected}."
            )
        lines.append("- These artifacts are evidence-governance and migration-readiness surfaces; they do not turn dry-run manifests into measured benchmark results.")
    else:
        lines.append("No evidence-governance summary was found. Refresh the high-value native artifacts and `benchmark_summary.json` on the target machine before making release or dependency claims.")

    closure = payload.get("non_gpu_review_closure", {})
    if isinstance(closure, dict):
        lines.extend(render_non_gpu_review_overview_section(closure))
        lines.extend(render_non_gpu_root_cause_query_section(closure, scorecard))

    if scorecard:
        lines.extend(
            render_non_gpu_review_scorecard_section(
                scorecard,
                dict_value(closure),
            )
        )

    lines.extend(render_final_report_supporting_sections(payload))
    return "\n".join(lines)


def build_payload(args: argparse.Namespace) -> Dict[str, Any]:
    root = Path(args.root).resolve()
    results_root = resolve_repo_path(root, args.results)
    summary_path = resolve_repo_path(root, args.summary)
    evaluation_path = resolve_repo_path(root, args.evaluation)
    costs_path = resolve_repo_path(root, args.costs)
    summary = load_json(summary_path)
    evaluation_text, evaluation_read_status = read_text_with_status(evaluation_path, limit=20000)
    costs = cost_records(costs_path)
    governance_sources, catalog_resolution = evidence_governance_sources(results_root)
    sections = summary_sections(summary)
    non_gpu_closure = extract_non_gpu_review_closure(results_root)
    commands, command_source_status = reproducible_commands_with_status(root)
    reports, report_scan_issues = extract_report_index_with_issues(root)
    notes, note_scan_issues = extract_note_index_with_issues(root)
    read_issues: List[Dict[str, Any]] = [
        issue
        for issue in report_scan_issues + note_scan_issues
        if str(issue.get("kind", "")).endswith("_read")
    ]
    if evaluation_read_status.get("status") != "ok":
        read_issues.append({**evaluation_read_status, "kind": "evaluation_read"})
    return {
        "schema_version": 1,
        "generated_at": utc_now(),
        **path_payload(root, "root", "root_path_redacted"),
        **path_payload(results_root, "results_root", "results_root_path_redacted"),
        **path_payload(summary_path, "summary_path", "summary_path_redacted"),
        **path_payload(costs_path, "cost_path", "cost_path_redacted"),
        "modules": module_status(root, results_root),
        "evidence_governance_sources": governance_sources,
        "catalog_source_resolution": catalog_resolution,
        "catalog_drift_review": extract_catalog_drift_review(results_root),
        "non_gpu_review_closure": non_gpu_closure,
        "non_gpu_review_scorecard": non_gpu_review_scorecard(non_gpu_closure),
        "summary_sections": sections,
        "evidence_confidence_rollup": evidence_confidence_rollup(sections),
        "reports": reports,
        "report_kind_counts": report_kind_counts(reports),
        "notes": notes,
        "report_scan_issues": report_scan_issues,
        "note_scan_issues": note_scan_issues,
        "scan_issue_count": len(report_scan_issues) + len(note_scan_issues),
        "evaluation_read_status": evaluation_read_status,
        "read_issues": read_issues,
        "read_issue_count": len(read_issues),
        "untested": extract_untested(evaluation_text),
        "cost_records": costs,
        "reproducible_commands": commands,
        "benchmark_matrix_command_source": command_source_status,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", "--repo-root", dest="root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--summary", default="results/raw/benchmark_summary.json")
    parser.add_argument("--evaluation", default="../evaluation.md")
    parser.add_argument("--costs", default="configs/cost_records.json")
    parser.add_argument("--output", default="reports/final_report.md")
    parser.add_argument("--json-output", default="results/raw/final_report_index.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    payload = build_payload(args)
    output = resolve_repo_path(root, args.output)
    json_output = resolve_repo_path(root, args.json_output)
    write_text_file(output, render_markdown(payload))
    write_text_file(json_output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")
    print(f"wrote {path_label(json_output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
