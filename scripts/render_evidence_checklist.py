#!/usr/bin/env python3
"""Render a closure checklist for the original InferLab project plan."""

from __future__ import annotations

import argparse
import json
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List

from artifact_json import load_json_dict_or_empty as load
from command_redaction import artifact_path_payload as path_payload, redact_path_error, redacted_path_label as path_label
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


REQUIREMENTS = [
    {"module": "Mini Transformer", "artifact": "mini_transformer_cache_compare.json", "needs_real_hardware": False},
    {"module": "CUDA kernels", "artifact": "cuda_profile_matrix.json", "needs_real_hardware": True},
    {"module": "KV/PagedAttention", "artifact": "scheduler_trace.json", "needs_real_hardware": False},
    {"module": "vLLM serving", "artifact": "vllm_serving_benchmark.json", "needs_real_hardware": True},
    {"module": "TensorRT-LLM", "artifact": "trtllm_benchmark.json", "needs_real_hardware": True},
    {"module": "Quantization", "artifact": "backend_quality_eval.json", "needs_real_hardware": True},
    {"module": "llama.cpp edge", "artifact": "llama_cpp_matrix.json", "needs_real_hardware": True},
    {"module": "GGUF parser", "artifact": "gguf_fixture_comparison.json", "needs_real_hardware": True},
    {"module": "Android/ARM", "artifact": "android_edge_benchmark.json", "needs_real_hardware": True},
    {"module": "Monitoring", "artifact": "profiling_artifacts.json", "needs_real_hardware": True},
    {
        "module": "Source traces",
        "artifact": "source_trace_verification.json",
        "needs_real_hardware": False,
        "incomplete_action": "configure pinned upstream checkouts and rerun source verification",
    },
    {"module": "Backend comparison", "artifact": "backend_comparison.json", "needs_real_hardware": False},
]

INCOMPLETE_STATUSES = {
    "error",
    "fail",
    "failed",
    "invalid_artifact",
    "invalid_encoding",
    "invalid_json",
    "mismatch",
    "missing",
    "observed_only",
    "path_probe_failed",
    "planned",
    "preflight_only",
    "skipped",
    "timeout",
    "unreadable_json",
}


def path_probe(path: Path) -> Dict[str, Any]:
    try:
        exists = path.exists()
    except OSError as exc:
        return {"exists": False, "status": "path_probe_failed", "error": redact_path_error(exc, path)}
    return {"exists": exists, "status": "ok" if exists else "missing", "error": ""}


def status_of(data: Dict[str, Any], probe: Dict[str, Any]) -> str:
    probe_status = str(probe.get("status", ""))
    if probe_status == "path_probe_failed":
        return probe_status
    if not bool(probe.get("exists")):
        return "missing"
    status = str(data.get("status", "present"))
    return status


def is_complete_status(status: str) -> bool:
    normalized = status.strip().lower()
    if normalized in INCOMPLETE_STATUSES:
        return False
    if normalized.startswith(("invalid", "missing", "partial", "stale")):
        return False
    return True


def next_action_for_requirement(requirement: Dict[str, Any], complete: bool) -> str:
    if complete:
        return "done"
    configured_action = requirement.get("incomplete_action")
    if isinstance(configured_action, str) and configured_action.strip():
        return configured_action
    if requirement["needs_real_hardware"]:
        return "run on target environment"
    return "refresh aggregate report"


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    results = resolve_repo_path(repo_root, args.results)
    records: List[Dict[str, Any]] = []
    for req in REQUIREMENTS:
        path = results / req["artifact"]
        probe = path_probe(path)
        data = load(path)
        status = status_of(data, probe)
        complete = bool(probe["exists"]) and is_complete_status(status)
        records.append({
            **req,
            **path_payload(path),
            "exists": bool(probe["exists"]),
            "path_probe_status": probe["status"],
            "path_probe_error": probe["error"],
            "status": status,
            "complete": complete,
            "next_action": next_action_for_requirement(req, complete),
        })
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        **path_payload(args.results, "results_root", "results_root_path_redacted"),
        **path_payload(results, "resolved_results_root"),
        "status": "ok" if all(item["complete"] for item in records) else "open",
        "records": records,
        "summary": {
            "requirement_count": len(records),
            "complete_count": sum(1 for item in records if item["complete"]),
            "open_count": sum(1 for item in records if not item["complete"]),
            "hardware_dependent_open_count": sum(1 for item in records if item["needs_real_hardware"] and not item["complete"]),
        },
    }


def markdown(payload: Dict[str, Any]) -> str:
    lines = [
        "# Runtime Evidence Checklist",
        "",
        f"- Generated: `{payload['timestamp_utc']}`",
        f"- Status: `{payload['status']}`",
        f"- Complete: `{payload['summary']['complete_count']}/{payload['summary']['requirement_count']}`",
        "",
        "| Module | Artifact | Status | Complete | Next action |",
        "|---|---|---|---:|---|",
    ]
    for item in payload["records"]:
        lines.append(f"| {item['module']} | `{item['artifact']}` | `{item['status']}` | {item['complete']} | {item['next_action']} |")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output", default="results/raw/evidence_checklist.json")
    parser.add_argument("--report", default="reports/evidence_checklist.md")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    report = resolve_repo_path(repo_root, args.report)
    write_text_file(report, markdown(payload))
    print(f"wrote {path_label(output)}")
    print(f"wrote {path_label(report)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
