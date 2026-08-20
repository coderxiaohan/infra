#!/usr/bin/env python3
"""Export InferLab benchmark JSON into Prometheus textfile metrics.

This exporter is designed for migration-friendly observability. It does not
need CUDA, vLLM, TensorRT-LLM, llama.cpp, or Prometheus to be installed on the
current machine. It converts already-produced JSON artifacts into the text
exposition format used by node_exporter's textfile collector.
"""

from __future__ import annotations

import argparse
import json
import math
import re
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, Iterable, Iterator, List, Optional, Sequence, Tuple

from artifact_catalog_glue import (
    CatalogSourceResolution,
    compose_catalog_extensions,
    resolve_catalog_sources,
)
from artifact_json import load_json_dict as load_json
from command_redaction import path_was_redacted
from cli_validation import require_int_range
from evidence_projection import (
    NATIVE_CORE_AUDIT_PROMETHEUS_METRIC_NAMES,
    artifact_catalog_prometheus_metric_pairs,
    artifact_evidence_projection,
    default_repo_root,
    mode_matches,
    path_label,
    project_artifact_catalog_governance,
    resolve_repo_path,
    safe_path_probe,
)
from json_projection import get_path
from text_output import write_text_file


EXPECTED_RAW_SOURCES = compose_catalog_extensions(
    insertions=(
        ("benchmark_summary.json", "gguf_fixture_comparison.json"),
    ),
)

SKIP_KEYS = {
    "stdout",
    "stderr",
    "stdout_tail",
    "stderr_tail",
    "output_tail",
    "raw_output_tail",
    "prompt",
    "prompts",
    "generated_text",
    "completion_text",
    "completion_preview",
    "text",
    "tokens",
    "token_ids",
}

STATUS_VALUE = {
    "ok": 1.0,
    "pass": 1.0,
    "passed": 1.0,
    "success": 1.0,
    "ready": 1.0,
    "true": 1.0,
    "partial": 0.5,
    "partial_no_timing": 0.5,
    "partial_no_metrics": 0.5,
    "preflight_only": 0.0,
    "planned": 0.0,
    "skipped": 0.0,
    "missing": 0.0,
    "timeout": 0.0,
    "invalid_artifact": 0.0,
    "invalid_encoding": 0.0,
    "invalid_json": 0.0,
    "path_probe_failed": 0.0,
    "unreadable_json": 0.0,
    "failed": 0.0,
    "fail": 0.0,
    "error": 0.0,
    "false": 0.0,
}

LABEL_SAFE_RE = re.compile(r"[^a-zA-Z0-9_:.-]+")


def label_value(value: Any) -> str:
    text = str(value)
    text = text.replace("\\", "\\\\")
    text = text.replace("\n", "\\n")
    text = text.replace('"', '\\"')
    return text


def clean_label_fragment(value: Any) -> str:
    text = str(value).strip()
    return LABEL_SAFE_RE.sub("_", text) if text else "unknown"


def tri_state_label(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "unknown"
    text = str(value).strip().lower()
    return text if text in {"true", "false", "unknown"} else clean_label_fragment(text)


def metric_line(name: str, labels: Dict[str, Any], value: float) -> str:
    label_text = ",".join(f'{key}="{label_value(val)}"' for key, val in sorted(labels.items()))
    return f"{name}{{{label_text}}} {format_value(value)}"


def format_value(value: float) -> str:
    if math.isfinite(value) and value.is_integer():
        return str(int(value))
    return f"{value:.12g}"


def numeric_value(value: Any) -> Optional[float]:
    if isinstance(value, bool):
        return 1.0 if value else 0.0
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        number = float(value)
        return number if math.isfinite(number) else None
    if isinstance(value, str):
        normalized = value.strip().lower()
        status = STATUS_VALUE.get(normalized)
        if status is not None:
            return status
        if normalized.startswith(("invalid", "missing", "stale")):
            return 0.0
        if normalized.startswith("partial"):
            return 0.5
        if normalized.startswith("preflight"):
            return 0.0
    return None


def iter_numeric_leaves(
    value: Any,
    path: Sequence[str] = (),
    max_depth: int = 8,
) -> Iterator[Tuple[str, float]]:
    if len(path) > max_depth:
        return
    number = numeric_value(value)
    if number is not None:
        yield (".".join(path) if path else "value", number)
        return
    if isinstance(value, dict):
        for key, child in sorted(value.items()):
            if key in SKIP_KEYS:
                continue
            yield from iter_numeric_leaves(child, (*path, clean_label_fragment(key)), max_depth)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from iter_numeric_leaves(child, (*path, str(index)), max_depth)


def header_lines() -> List[str]:
    return [
        "# HELP inferlab_export_info Metadata for the latest InferLab metrics export.",
        "# TYPE inferlab_export_info gauge",
        "# HELP inferlab_source_present Whether an expected InferLab JSON artifact exists.",
        "# TYPE inferlab_source_present gauge",
        "# HELP inferlab_source_status Numeric status value for an InferLab JSON source.",
        "# TYPE inferlab_source_status gauge",
        "# HELP inferlab_source_artifact_info Artifact execution semantics and claim-boundary labels for an InferLab JSON source.",
        "# TYPE inferlab_source_artifact_info gauge",
        "# HELP inferlab_source_claim_scope_present Whether an InferLab JSON source exposes claim-scope metadata.",
        "# TYPE inferlab_source_claim_scope_present gauge",
        "# HELP inferlab_source_blocker_count Canonical blocker count projected from an InferLab JSON source.",
        "# TYPE inferlab_source_blocker_count gauge",
        "# HELP inferlab_source_evidence_confidence_info Evidence confidence tier for a raw InferLab JSON source.",
        "# TYPE inferlab_source_evidence_confidence_info gauge",
        "# HELP inferlab_summary_metric Numeric benchmark summary metric by source and section.",
        "# TYPE inferlab_summary_metric gauge",
        "# HELP inferlab_raw_metric Numeric raw JSON metric by source and dotted path.",
        "# TYPE inferlab_raw_metric gauge",
        "# HELP inferlab_catalog_source_count Artifact source counts from catalog resolution.",
        "# TYPE inferlab_catalog_source_count gauge",
        "# HELP inferlab_catalog_source_resolution_info Catalog consumption status for Prometheus export.",
        "# TYPE inferlab_catalog_source_resolution_info gauge",
        "# HELP inferlab_artifact_catalog_info Artifact catalog drift-review decision labels.",
        "# TYPE inferlab_artifact_catalog_info gauge",
        "# HELP inferlab_artifact_catalog_metric Numeric artifact catalog wiring and drift-review metrics.",
        "# TYPE inferlab_artifact_catalog_metric gauge",
        "# HELP inferlab_evidence_query_info Non-execute evidence query decision labels.",
        "# TYPE inferlab_evidence_query_info gauge",
        "# HELP inferlab_evidence_query_metric Numeric evidence query and repair-path metrics.",
        "# TYPE inferlab_evidence_query_metric gauge",
        "# HELP inferlab_native_core_audit_register_info Static native-core audit decision labels.",
        "# TYPE inferlab_native_core_audit_register_info gauge",
        "# HELP inferlab_native_core_audit_register_metric Numeric static native-core audit coverage and closure metrics.",
        "# TYPE inferlab_native_core_audit_register_metric gauge",
    ]


def source_status(data: Optional[Dict[str, Any]], present: bool, probe_error: str = "") -> str:
    if probe_error:
        return "path_probe_failed"
    if not present:
        return "missing"
    if isinstance(data, dict):
        status = data.get("status")
        if isinstance(status, str) and status:
            return status
    return "present"


def export_summary_metrics(summary: Dict[str, Any]) -> List[str]:
    lines: List[str] = []
    sections = summary.get("sections")
    if not isinstance(sections, list):
        return lines
    for section in sections:
        if not isinstance(section, dict):
            continue
        title = section.get("title", "unknown")
        source = section.get("source", "unknown")
        status = section.get("status", "unknown")
        status_value = numeric_value(status)
        if status_value is not None:
            lines.append(
                metric_line(
                    "inferlab_source_status",
                    {"source": source, "section": title, "status": status},
                    status_value,
                )
            )
        metrics = section.get("metrics", [])
        if not isinstance(metrics, list):
            continue
        for item in metrics:
            if not isinstance(item, dict):
                continue
            metric_name = item.get("name", "unknown")
            number = numeric_value(item.get("value"))
            if number is None:
                continue
            lines.append(
                metric_line(
                    "inferlab_summary_metric",
                    {"source": source, "section": title, "metric": metric_name},
                    number,
                )
            )
    return lines


def export_raw_metrics(
    results_dir: Path,
    sources: Iterable[str],
    include_raw: bool,
    max_raw_metrics: int,
) -> List[str]:
    lines: List[str] = []
    raw_count = 0
    for source in sources:
        path = results_dir / source
        present, probe_error = safe_path_probe(path)
        data = load_json(path) if present else None
        lines.append(metric_line("inferlab_source_present", {"source": source}, 1.0 if present else 0.0))
        status = source_status(data, present, probe_error)
        status_number = numeric_value(status)
        if status_number is not None:
            lines.append(
                metric_line(
                    "inferlab_source_status",
                    {"source": source, "section": "raw", "status": status},
                    status_number,
                )
            )
        if present:
            evidence = artifact_evidence_projection(data, source)
            has_claim_scope = bool(evidence["claim_scope_present"])
            lines.append(
                metric_line(
                    "inferlab_source_artifact_info",
                    {
                        "source": source,
                        "artifact_type": clean_label_fragment(
                            evidence["artifact_type"]
                        ),
                        "mode": clean_label_fragment(evidence["mode"]),
                        "executed": tri_state_label(evidence["executed"]),
                        "replayed": tri_state_label(evidence["replayed"]),
                        "claim_scope_present": (
                            "true" if has_claim_scope else "false"
                        ),
                        "release_decision": clean_label_fragment(
                            evidence["release_decision"]
                        ),
                    },
                    1.0,
                )
            )
            lines.append(
                metric_line(
                    "inferlab_source_claim_scope_present",
                    {"source": source},
                    1.0 if has_claim_scope else 0.0,
                )
            )
            if isinstance(evidence["blocker_count"], int):
                lines.append(
                    metric_line(
                        "inferlab_source_blocker_count",
                        {"source": source},
                        float(evidence["blocker_count"]),
                    )
                )
            lines.append(
                metric_line(
                    "inferlab_source_evidence_confidence_info",
                    {
                        "source": source,
                        "confidence": evidence["confidence"],
                    },
                    1.0,
                )
            )
        if not include_raw or data is None:
            continue
        for metric_path, number in iter_numeric_leaves(data):
            if raw_count >= max_raw_metrics:
                return lines
            lines.append(metric_line("inferlab_raw_metric", {"source": source, "path": metric_path}, number))
            raw_count += 1
    return lines


def export_catalog_resolution_metrics(resolution: CatalogSourceResolution) -> List[str]:
    lines = [
        metric_line(
            "inferlab_catalog_source_resolution_info",
            {
                "status": resolution.status,
                "source_policy": resolution.source_policy,
                "catalog_path": resolution.catalog_path,
            },
            1.0,
        )
    ]
    lines.append(
        metric_line(
            "inferlab_catalog_source_error_present",
            {"status": resolution.status, "source_policy": resolution.source_policy},
            1.0 if resolution.catalog_error else 0.0,
        )
    )
    for kind, value in (
        ("extension", resolution.extension_count),
        ("catalog", resolution.catalog_count),
        ("resolved", resolution.resolved_count),
        ("extra_catalog", resolution.extra_catalog_count),
        ("extension_only", resolution.extension_only_count),
        ("shared", resolution.shared_count),
        ("fallback_deprecated", resolution.fallback_count),
        ("fallback_only_deprecated", resolution.fallback_only_count),
    ):
        lines.append(
            metric_line(
                "inferlab_catalog_source_count",
                {
                    "kind": kind,
                    "status": resolution.status,
                    "source_policy": resolution.source_policy,
                },
                float(value),
            )
        )
    return lines


def export_artifact_catalog_metrics(data: Optional[Dict[str, Any]]) -> List[str]:
    if not isinstance(data, dict):
        return []
    catalog_review = project_artifact_catalog_governance(data)

    labels = {
        "source": "native_artifact_catalog_single_source.json",
        "registry_decision": clean_label_fragment(
            catalog_review.get("registry_decision", "unknown")
        ),
        "drift_decision": clean_label_fragment(
            catalog_review.get("drift_decision", "unknown")
        ),
        "parity_decision": clean_label_fragment(
            catalog_review.get("parity_decision", "unknown")
        ),
        "fallback_removal_queue_state": clean_label_fragment(
            catalog_review.get("fallback_removal_queue_state", "unknown")
        ),
        "fallback_removal_shortest_path_state": clean_label_fragment(
            catalog_review.get("fallback_removal_shortest_path_state", "unknown")
        ),
        "fallback_removal_shortest_path_environment": clean_label_fragment(
            catalog_review.get(
                "fallback_removal_shortest_path_primary_environment",
                "unknown",
            )
        ),
        "fallback_removal_parity_gate_decision": clean_label_fragment(
            catalog_review.get("fallback_removal_parity_gate_decision", "unknown")
        ),
        "claim_scope": "non_execute_catalog_registry",
    }
    lines = [metric_line("inferlab_artifact_catalog_info", labels, 1.0)]
    metric_items = artifact_catalog_prometheus_metric_pairs(catalog_review)
    for name, value in metric_items:
        number = numeric_value(value)
        if number is None:
            continue
        lines.append(
            metric_line(
                "inferlab_artifact_catalog_metric",
                {"source": "native_artifact_catalog_single_source.json", "metric": name},
                number,
            )
        )
    return lines


def evidence_query_operation_and_repair_metric_pairs(
    summary: Dict[str, Any],
) -> List[Tuple[str, Any]]:
    return [
        ("query_result_count", summary.get("query_result_count")),
        ("blocked_result_count", summary.get("blocked_result_count")),
        ("query_operation_selector_present", summary.get("query_operation_selector_present")),
        ("query_operation_target_present", summary.get("query_operation_target_present")),
        (
            "query_operation_snapshot_inputs_required",
            summary.get("query_operation_snapshot_inputs_required"),
        ),
        (
            "query_operation_snapshot_inputs_configured",
            summary.get("query_operation_snapshot_inputs_configured"),
        ),
        (
            "query_operation_selected_result_present",
            summary.get("query_operation_selected_result_present"),
        ),
        (
            "query_operation_selected_result_input_ready",
            summary.get("query_operation_selected_result_input_ready"),
        ),
        (
            "query_operation_selected_result_payload_count",
            summary.get("query_operation_selected_result_payload_count"),
        ),
        (
            "query_operation_show_artifact_candidate_count",
            summary.get("query_operation_show_artifact_candidate_count"),
        ),
        (
            "query_operation_show_artifact_candidate_truncated",
            summary.get("query_operation_show_artifact_candidate_truncated"),
        ),
        (
            "query_operation_show_artifact_exact_match",
            summary.get("query_operation_show_artifact_exact_match"),
        ),
        (
            "query_operation_show_artifact_selector_ambiguous",
            summary.get("query_operation_show_artifact_selector_ambiguous"),
        ),
        (
            "query_operation_payload_selector_focus_present",
            summary.get("query_operation_payload_selector_focus_present"),
        ),
        (
            "query_operation_payload_selector_focus_match_count",
            summary.get("query_operation_payload_selector_focus_match_count"),
        ),
        (
            "query_operation_payload_selector_focus_ambiguous",
            summary.get("query_operation_payload_selector_focus_ambiguous"),
        ),
        (
            "query_operation_payload_selector_focus_source_location_gap_count",
            summary.get("query_operation_payload_selector_focus_source_location_gap_count"),
        ),
        (
            "query_operation_selector_match_count",
            summary.get("query_operation_selector_match_count"),
        ),
        (
            "query_operation_selector_ambiguous",
            summary.get("query_operation_selector_ambiguous"),
        ),
        (
            "query_operation_selector_repair_card_count",
            summary.get("query_operation_selector_repair_card_count"),
        ),
        (
            "query_operation_selector_repair_card_emitted_count",
            summary.get("query_operation_selector_repair_card_emitted_count"),
        ),
        (
            "query_operation_selector_repair_card_truncated",
            summary.get("query_operation_selector_repair_card_truncated"),
        ),
        (
            "query_operation_selector_repair_card_embedded_limit",
            summary.get("query_operation_selector_repair_card_embedded_limit"),
        ),
        (
            "query_operation_selector_repair_card_full_export_count",
            summary.get("query_operation_selector_repair_card_full_export_count"),
        ),
        (
            "query_operation_selector_repair_card_external_export_configured",
            summary.get("query_operation_selector_repair_card_external_export_configured"),
        ),
        (
            "query_operation_selector_repair_card_external_export_written",
            summary.get("query_operation_selector_repair_card_external_export_written"),
        ),
        (
            "query_operation_selector_repair_card_current_repo_count",
            summary.get("query_operation_selector_repair_card_current_repo_count"),
        ),
        (
            "query_operation_selector_repair_card_current_server_count",
            summary.get("query_operation_selector_repair_card_current_server_count"),
        ),
        (
            "query_operation_selector_repair_card_target_machine_count",
            summary.get("query_operation_selector_repair_card_target_machine_count"),
        ),
        (
            "query_operation_selector_repair_card_target_product_count",
            summary.get("query_operation_selector_repair_card_target_product_count"),
        ),
        (
            "query_operation_selector_repair_card_source_location_gap_card_count",
            summary.get(
                "query_operation_selector_repair_card_source_location_gap_card_count"
            ),
        ),
        (
            "query_operation_selector_repair_card_external_file_required",
            summary.get("query_operation_selector_repair_card_external_file_required"),
        ),
        (
            "query_operation_selector_source_location_count",
            summary.get("query_operation_selector_source_location_count"),
        ),
        (
            "query_operation_selector_source_location_gap_count",
            summary.get("query_operation_selector_source_location_gap_count"),
        ),
        (
            "query_operation_selector_current_repo_action_count",
            summary.get("query_operation_selector_current_repo_action_count"),
        ),
        (
            "query_operation_selector_current_server_action_count",
            summary.get("query_operation_selector_current_server_action_count"),
        ),
        (
            "query_operation_selector_target_machine_action_count",
            summary.get("query_operation_selector_target_machine_action_count"),
        ),
        (
            "query_operation_selector_target_product_action_count",
            summary.get("query_operation_selector_target_product_action_count"),
        ),
        (
            "query_operation_selector_action_plan_present",
            summary.get("query_operation_selector_action_plan_present"),
        ),
        (
            "query_operation_selector_action_plan_step_count",
            summary.get("query_operation_selector_action_plan_step_count"),
        ),
        (
            "query_operation_selector_action_plan_current_repo_step_count",
            summary.get("query_operation_selector_action_plan_current_repo_step_count"),
        ),
        (
            "query_operation_selector_action_plan_target_or_validation_step_count",
            summary.get("query_operation_selector_action_plan_target_or_validation_step_count"),
        ),
        (
            "query_operation_selector_action_plan_source_location_gap_count",
            summary.get("query_operation_selector_action_plan_source_location_gap_count"),
        ),
        (
            "query_operation_selector_action_plan_readiness_blocker_count",
            summary.get("query_operation_selector_action_plan_readiness_blocker_count"),
        ),
        (
            "operation_catalog_supported_count",
            summary.get("operation_catalog_supported_count"),
        ),
        (
            "operation_catalog_read_only_count",
            summary.get("operation_catalog_read_only_count"),
        ),
        (
            "operation_catalog_snapshot_required_count",
            summary.get("operation_catalog_snapshot_required_count"),
        ),
        (
            "operation_catalog_selector_recommended_count",
            summary.get("operation_catalog_selector_recommended_count"),
        ),
        (
            "operation_catalog_target_required_count",
            summary.get("operation_catalog_target_required_count"),
        ),
        ("query_ux_repair_card_count", summary.get("query_ux_repair_card_count")),
        ("query_ux_repair_card_group_count", summary.get("query_ux_repair_card_group_count")),
        (
            "query_ux_repair_card_group_card_count",
            summary.get("query_ux_repair_card_group_card_count"),
        ),
        (
            "query_ux_repair_card_group_current_repo_count",
            summary.get("query_ux_repair_card_group_current_repo_count"),
        ),
        (
            "query_ux_repair_card_group_current_server_count",
            summary.get("query_ux_repair_card_group_current_server_count"),
        ),
        (
            "query_ux_repair_card_group_target_machine_count",
            summary.get("query_ux_repair_card_group_target_machine_count"),
        ),
        (
            "query_ux_repair_card_group_target_product_count",
            summary.get("query_ux_repair_card_group_target_product_count"),
        ),
        (
            "query_ux_repair_card_group_source_location_gap_count",
            summary.get("query_ux_repair_card_group_source_location_gap_count"),
        ),
        (
            "query_ux_repair_group_summary_present",
            summary.get("query_ux_repair_group_summary_present"),
        ),
        (
            "query_ux_repair_group_exact_wiring_location_count",
            summary.get("query_ux_repair_group_exact_wiring_location_count"),
        ),
        (
            "query_ux_repair_group_affected_claim_count",
            summary.get("query_ux_repair_group_affected_claim_count"),
        ),
        (
            "query_ux_repair_narrative_present",
            summary.get("query_ux_repair_narrative_present"),
        ),
        (
            "query_ux_repair_narrative_step_count",
            summary.get("query_ux_repair_narrative_step_count"),
        ),
        (
            "query_ux_repair_narrative_current_repo_action_count",
            summary.get("query_ux_repair_narrative_current_repo_action_count"),
        ),
        (
            "query_ux_repair_narrative_target_or_validation_action_count",
            summary.get(
                "query_ux_repair_narrative_target_or_validation_action_count"
            ),
        ),
        (
            "query_ux_repair_narrative_source_location_gap_count",
            summary.get("query_ux_repair_narrative_source_location_gap_count"),
        ),
        (
            "query_ux_repair_narrative_readiness_blocker_count",
            summary.get("query_ux_repair_narrative_readiness_blocker_count"),
        ),
        (
            "query_ux_repair_narrative_affected_claim_count",
            summary.get("query_ux_repair_narrative_affected_claim_count"),
        ),
        (
            "query_ux_repair_narrative_exact_wiring_location_count",
            summary.get("query_ux_repair_narrative_exact_wiring_location_count"),
        ),
        (
            "query_ux_repair_group_primary_action_present",
            summary.get("query_ux_repair_group_primary_action_present"),
        ),
        (
            "query_ux_repair_group_primary_source_location_present",
            summary.get("query_ux_repair_group_primary_source_location_present"),
        ),
        (
            "query_ux_repair_group_navigation_step_count",
            summary.get("query_ux_repair_group_navigation_step_count"),
        ),
        (
            "query_ux_repair_group_navigation_current_repo_step_count",
            summary.get("query_ux_repair_group_navigation_current_repo_step_count"),
        ),
        (
            "query_ux_repair_group_navigation_target_or_validation_step_count",
            summary.get(
                "query_ux_repair_group_navigation_target_or_validation_step_count"
            ),
        ),
    ]


def evidence_query_navigation_metric_pairs(
    summary: Dict[str, Any],
) -> List[Tuple[str, Any]]:
    return [
        (
            "query_ux_source_location_worklist_item_count",
            summary.get("query_ux_source_location_worklist_item_count"),
        ),
        (
            "query_ux_source_location_worklist_current_repo_item_count",
            summary.get("query_ux_source_location_worklist_current_repo_item_count"),
        ),
        (
            "query_ux_source_location_worklist_target_or_validation_item_count",
            summary.get(
                "query_ux_source_location_worklist_target_or_validation_item_count"
            ),
        ),
        (
            "query_ux_source_location_worklist_gap_count",
            summary.get("query_ux_source_location_worklist_gap_count"),
        ),
        (
            "query_ux_source_location_worklist_focus_present",
            summary.get("query_ux_source_location_worklist_focus_present"),
        ),
        (
            "query_ux_source_location_worklist_focus_first_item_present",
            summary.get("query_ux_source_location_worklist_focus_first_item_present"),
        ),
        (
            "query_ux_source_location_worklist_focus_first_gap_present",
            summary.get("query_ux_source_location_worklist_focus_first_gap_present"),
        ),
        (
            "query_ux_source_location_worklist_focus_first_item_current_repo_action_required",
            summary.get(
                "query_ux_source_location_worklist_focus_first_item_current_repo_action_required"
            ),
        ),
        (
            "query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required",
            summary.get(
                "query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required"
            ),
        ),
        (
            "query_ux_source_location_category_count",
            summary.get("query_ux_source_location_category_count"),
        ),
        (
            "query_ux_source_location_active_category_count",
            summary.get("query_ux_source_location_active_category_count"),
        ),
        (
            "query_ux_source_location_exact_category_count",
            summary.get("query_ux_source_location_exact_category_count"),
        ),
        (
            "query_ux_source_location_gap_category_count",
            summary.get("query_ux_source_location_gap_category_count"),
        ),
        (
            "query_ux_source_location_current_repo_gap_category_count",
            summary.get("query_ux_source_location_current_repo_gap_category_count"),
        ),
        (
            "query_ux_source_location_target_or_validation_gap_category_count",
            summary.get(
                "query_ux_source_location_target_or_validation_gap_category_count"
            ),
        ),
        (
            "query_ux_source_location_gap_repair_card_count",
            summary.get("query_ux_source_location_gap_repair_card_count"),
        ),
        (
            "query_ux_source_location_category_exactness_percent",
            summary.get("query_ux_source_location_category_exactness_percent"),
        ),
        (
            "query_ux_repair_card_path_rank_count",
            summary.get("query_ux_repair_card_path_rank_count"),
        ),
        (
            "query_ux_repair_card_path_current_repo_rank_count",
            summary.get("query_ux_repair_card_path_current_repo_rank_count"),
        ),
        (
            "query_ux_repair_card_path_target_or_validation_rank_count",
            summary.get("query_ux_repair_card_path_target_or_validation_rank_count"),
        ),
        (
            "query_ux_repair_card_path_source_location_gap_count",
            summary.get("query_ux_repair_card_path_source_location_gap_count"),
        ),
        (
            "query_ux_repair_card_rank_readiness_link_count",
            summary.get("query_ux_repair_card_rank_readiness_link_count"),
        ),
        (
            "query_ux_repair_card_rank_readiness_current_repo_link_count",
            summary.get(
                "query_ux_repair_card_rank_readiness_current_repo_link_count"
            ),
        ),
        (
            "query_ux_repair_card_rank_readiness_target_or_validation_link_count",
            summary.get(
                "query_ux_repair_card_rank_readiness_target_or_validation_link_count"
            ),
        ),
        (
            "query_ux_repair_card_rank_readiness_report_ready_link_count",
            summary.get(
                "query_ux_repair_card_rank_readiness_report_ready_link_count"
            ),
        ),
        (
            "query_ux_repair_card_rank_readiness_replay_ready_link_count",
            summary.get(
                "query_ux_repair_card_rank_readiness_replay_ready_link_count"
            ),
        ),
        (
            "query_ux_repair_card_rank_readiness_execute_ready_link_count",
            summary.get(
                "query_ux_repair_card_rank_readiness_execute_ready_link_count"
            ),
        ),
        (
            "query_ux_repair_card_source_exactness_card_count",
            summary.get("query_ux_repair_card_source_exactness_card_count"),
        ),
        (
            "query_ux_repair_card_source_exactness_exact_card_count",
            summary.get("query_ux_repair_card_source_exactness_exact_card_count"),
        ),
        (
            "query_ux_repair_card_source_exactness_gap_card_count",
            summary.get("query_ux_repair_card_source_exactness_gap_card_count"),
        ),
        (
            "query_ux_repair_card_source_exactness_current_repo_gap_count",
            summary.get("query_ux_repair_card_source_exactness_current_repo_gap_count"),
        ),
        (
            "query_ux_repair_card_source_exactness_target_or_validation_gap_count",
            summary.get(
                "query_ux_repair_card_source_exactness_target_or_validation_gap_count"
            ),
        ),
        (
            "query_ux_repair_card_source_exactness_percent",
            summary.get("query_ux_repair_card_source_exactness_percent"),
        ),
        (
            "query_ux_explain_tree_node_count",
            summary.get("query_ux_explain_tree_node_count"),
        ),
        (
            "query_ux_explain_tree_source_exact_node_count",
            summary.get("query_ux_explain_tree_source_exact_node_count"),
        ),
        (
            "query_ux_explain_tree_source_gap_node_count",
            summary.get("query_ux_explain_tree_source_gap_node_count"),
        ),
        (
            "query_ux_explain_tree_current_repo_source_gap_count",
            summary.get("query_ux_explain_tree_current_repo_source_gap_count"),
        ),
        (
            "query_ux_explain_tree_target_or_validation_source_gap_count",
            summary.get(
                "query_ux_explain_tree_target_or_validation_source_gap_count"
            ),
        ),
        (
            "query_ux_explain_tree_source_exactness_percent",
            summary.get("query_ux_explain_tree_source_exactness_percent"),
        ),
        (
            "query_ux_blocker_drilldown_record_count",
            summary.get("query_ux_blocker_drilldown_record_count"),
        ),
        (
            "query_ux_blocker_drilldown_category_count",
            summary.get("query_ux_blocker_drilldown_category_count"),
        ),
        (
            "query_ux_blocker_drilldown_source_location_gap_count",
            summary.get("query_ux_blocker_drilldown_source_location_gap_count"),
        ),
        (
            "query_ux_blocker_drilldown_affected_claim_count",
            summary.get("query_ux_blocker_drilldown_affected_claim_count"),
        ),
        (
            "query_ux_blocker_drilldown_exact_wiring_location_count",
            summary.get("query_ux_blocker_drilldown_exact_wiring_location_count"),
        ),
        (
            "query_ux_blocker_drilldown_current_repo_action_count",
            summary.get("query_ux_blocker_drilldown_current_repo_action_count"),
        ),
        (
            "query_ux_blocker_drilldown_current_server_action_count",
            summary.get("query_ux_blocker_drilldown_current_server_action_count"),
        ),
        (
            "query_ux_blocker_drilldown_target_machine_action_count",
            summary.get("query_ux_blocker_drilldown_target_machine_action_count"),
        ),
        (
            "query_ux_blocker_drilldown_target_product_action_count",
            summary.get("query_ux_blocker_drilldown_target_product_action_count"),
        ),
        ("query_ux_blocker_category_count", summary.get("query_ux_blocker_category_count")),
        ("query_ux_exact_wiring_location_count", summary.get("query_ux_exact_wiring_location_count")),
        ("query_ux_report_ready_blocker_count", summary.get("query_ux_report_ready_blocker_count")),
        ("query_ux_replay_ready_blocker_count", summary.get("query_ux_replay_ready_blocker_count")),
        ("query_ux_execute_ready_blocker_count", summary.get("query_ux_execute_ready_blocker_count")),
        (
            "query_ux_readiness_path_blocking_repair_card_count",
            summary.get("query_ux_readiness_path_blocking_repair_card_count"),
        ),
        (
            "query_ux_readiness_path_step_count",
            summary.get("query_ux_readiness_path_step_count"),
        ),
        (
            "query_ux_readiness_path_current_repo_step_count",
            summary.get("query_ux_readiness_path_current_repo_step_count"),
        ),
        (
            "query_ux_readiness_path_target_or_validation_step_count",
            summary.get("query_ux_readiness_path_target_or_validation_step_count"),
        ),
        (
            "query_ux_readiness_path_source_location_gap_count",
            summary.get("query_ux_readiness_path_source_location_gap_count"),
        ),
        (
            "query_ux_readiness_path_catalog_flow_discrepancy_count",
            summary.get("query_ux_readiness_path_catalog_flow_discrepancy_count"),
        ),
        (
            "query_ux_readiness_path_fallback_removal_blocked_count",
            summary.get("query_ux_readiness_path_fallback_removal_blocked_count"),
        ),
        (
            "query_ux_readiness_path_field_coverage_gap_count",
            summary.get("query_ux_readiness_path_field_coverage_gap_count"),
        ),
        (
            "query_ux_readiness_path_current_repo_actionable",
            summary.get("query_ux_readiness_path_current_repo_actionable"),
        ),
        (
            "query_ux_readiness_path_validation_or_target_required",
            summary.get("query_ux_readiness_path_validation_or_target_required"),
        ),
        (
            "query_ux_readiness_path_source_location_metadata_required",
            summary.get("query_ux_readiness_path_source_location_metadata_required"),
        ),
        (
            "query_ux_readiness_path_next_action_present",
            summary.get("query_ux_readiness_path_next_action_present"),
        ),
        ("catalog_flow_input_gap_count", summary.get("catalog_flow_input_gap_count")),
        ("catalog_flow_missing_surface_count", summary.get("catalog_flow_missing_surface_count")),
    ]


def evidence_query_catalog_native_snapshot_metric_pairs(
    summary: Dict[str, Any],
) -> List[Tuple[str, Any]]:
    catalog_fields = (
        "catalog_flow_fallback_retained_surface_count",
        "catalog_flow_single_source_repair_card_count",
        "catalog_flow_single_source_parity_refresh_required_count",
        "catalog_flow_surface_migration_ready_count",
        "catalog_flow_surface_migration_fallback_count",
        "catalog_flow_surface_parity_refresh_required_count",
        "catalog_flow_surface_current_repo_action_count",
        "catalog_flow_surface_target_machine_action_count",
        "catalog_flow_consumer_field_coverage_field_gap_count",
        "catalog_flow_missing_step_count",
        "catalog_flow_repair_card_count",
        "catalog_flow_discrepancy_count",
    )
    native_audit_fields = (
        "native_core_audit_blocker_count",
        "native_core_audit_source_root_count",
        "native_core_audit_source_root_issue_count",
        "native_core_audit_source_root_error_count",
        "native_core_audit_scope_blocker_count",
        "native_core_audit_in_scope_file_count",
        "native_core_audit_reviewed_file_count",
        "native_core_audit_reviewed_with_fix_file_count",
        "native_core_audit_deferred_with_reason_file_count",
        "native_core_audit_issue_count",
        "native_core_audit_source_unreadable_count",
        "native_core_audit_review_manifest_invalid_override_count",
        "native_core_audit_review_manifest_unmatched_override_count",
        "native_core_audit_closed_review_file_count",
        "native_core_audit_open_review_item_count",
        "native_core_audit_high_priority_review_item_count",
        "native_core_audit_audit_completion_percent",
        "native_core_audit_audit_review_coverage_open_file_count",
        "native_core_audit_audit_review_coverage_high_priority_open_file_count",
        "native_core_audit_audit_review_coverage_source_location_gap_count",
        "native_core_audit_audit_review_coverage_blocks_full_audit_closure",
        "native_core_audit_filter_read_issue_count",
    )
    return [
        (name, summary.get(name))
        for name in (*catalog_fields, *native_audit_fields)
    ]


def export_evidence_query_metrics(data: Optional[Dict[str, Any]]) -> List[str]:
    if not isinstance(data, dict):
        return []
    summary = get_path(data, ("summary",), {})
    if not isinstance(summary, dict):
        summary = {}
    labels = {
        "source": "native_evidence_query_result.json",
        "operation": clean_label_fragment(summary.get("query_operation", "unknown")),
        "query_decision": clean_label_fragment(summary.get("query_decision", "unknown")),
        "native_core_audit_top_review_priority": clean_label_fragment(
            summary.get("native_core_audit_top_review_priority", "unknown")
        ),
        "claim_scope": "read_only_evidence_query",
    }
    lines = [metric_line("inferlab_evidence_query_info", labels, 1.0)]
    metric_items: List[Tuple[str, Any]] = [
        *evidence_query_operation_and_repair_metric_pairs(summary),
        *evidence_query_navigation_metric_pairs(summary),
        *evidence_query_catalog_native_snapshot_metric_pairs(summary),
    ]
    for name, value in metric_items:
        number = numeric_value(value)
        if number is None:
            continue
        lines.append(
            metric_line(
                "inferlab_evidence_query_metric",
                {"source": "native_evidence_query_result.json", "metric": name},
                number,
            )
        )
    return lines


def export_native_core_audit_register_metrics(data: Optional[Dict[str, Any]]) -> List[str]:
    if not isinstance(data, dict):
        return []
    summary = get_path(data, ("summary",), {})
    if not isinstance(summary, dict):
        return []
    labels = {
        "source": "native_core_audit_register.json",
        "audit_decision": clean_label_fragment(summary.get("audit_decision", "unknown")),
        "audit_progress_state": clean_label_fragment(summary.get("audit_progress_state", "unknown")),
        "top_review_priority": clean_label_fragment(summary.get("top_review_priority", "unknown")),
        "claim_scope": "static_native_core_audit_register",
    }
    lines = [metric_line("inferlab_native_core_audit_register_info", labels, 1.0)]
    for name in NATIVE_CORE_AUDIT_PROMETHEUS_METRIC_NAMES:
        number = numeric_value(summary.get(name))
        if number is None:
            continue
        lines.append(
            metric_line(
                "inferlab_native_core_audit_register_metric",
                {"source": "native_core_audit_register.json", "metric": name},
                number,
            )
        )
    return lines


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw", help="Directory containing InferLab raw JSON artifacts.")
    parser.add_argument(
        "--summary",
        default="",
        help="Optional benchmark_summary.json path. Defaults to RESULTS/benchmark_summary.json.",
    )
    parser.add_argument(
        "--output",
        default="results/tables/inferlab_metrics.prom",
        help="Prometheus textfile output path.",
    )
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        help="Additional or replacement source JSON filename. Repeat as needed.",
    )
    parser.add_argument("--no-raw", action="store_true", help="Export only source presence and summary metrics.")
    parser.add_argument("--max-raw-metrics", type=int, default=800)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require_int_range("--max-raw-metrics", args.max_raw_metrics, 0, 1_000_000)
    repo_root = Path(args.repo_root).resolve()
    results_dir = resolve_repo_path(repo_root, args.results)
    summary_path = (
        resolve_repo_path(repo_root, args.summary)
        if args.summary
        else results_dir / "benchmark_summary.json"
    )
    output = resolve_repo_path(repo_root, args.output)
    catalog_resolution = (
        CatalogSourceResolution(
            names=tuple(args.source),
            status="cli_override",
            catalog_path=path_label(results_dir / "native_artifact_catalog_single_source.json"),
            catalog_path_redacted=path_was_redacted(
                results_dir / "native_artifact_catalog_single_source.json",
                path_label(results_dir / "native_artifact_catalog_single_source.json"),
            ),
            catalog_error="",
            source_policy="cli_override",
            extension_count=len(args.source),
            fallback_count=0,
            catalog_count=0,
            resolved_count=len(args.source),
            extra_catalog_count=0,
            extension_only_count=len(args.source),
            fallback_only_count=0,
            shared_count=0,
        )
        if args.source
        else resolve_catalog_sources(results_dir, EXPECTED_RAW_SOURCES)
    )
    sources = catalog_resolution.names

    lines = header_lines()
    lines.append(
        metric_line(
            "inferlab_export_info",
            {
                "generator": "export_prometheus_metrics.py",
                "results": path_label(results_dir),
                "timestamp_utc": utc_now(),
            },
            1.0,
        )
    )
    lines.extend(export_catalog_resolution_metrics(catalog_resolution))
    lines.extend(export_raw_metrics(results_dir, sources, not args.no_raw, args.max_raw_metrics))
    lines.extend(export_artifact_catalog_metrics(load_json(results_dir / "native_artifact_catalog_single_source.json")))
    lines.extend(export_evidence_query_metrics(load_json(results_dir / "native_evidence_query_result.json")))
    lines.extend(export_native_core_audit_register_metrics(load_json(results_dir / "native_core_audit_register.json")))
    summary = load_json(summary_path)
    if summary is not None:
        lines.extend(export_summary_metrics(summary))

    write_text_file(output, "\n".join(lines) + "\n")
    print(f"wrote {path_label(output)} metrics={max(0, len(lines) - len(header_lines()))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
