#!/usr/bin/env python3
"""Import profiler/log artifacts into a normalized InferLab JSON report.

The parser is intentionally tolerant: binary Nsight report files are recorded
with metadata, while text logs, CSV exports, perf outputs, Prometheus snapshots,
and llama.cpp timing logs are parsed for portable metrics when possible.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    from scripts.command_redaction import artifact_path_payload as path_payload, artifact_preview_fields, redact_path_error, redact_sensitive_text, redacted_path_label as metric_path_label
except ModuleNotFoundError:
    scripts_dir = Path(__file__).resolve().parent
    if str(scripts_dir) not in sys.path:
        sys.path.insert(0, str(scripts_dir))
    from command_redaction import artifact_path_payload as path_payload, artifact_preview_fields, redact_path_error, redact_sensitive_text, redacted_path_label as metric_path_label  # type: ignore
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from cli_validation import require_int_range
from time_utils import utc_now
from text_output import write_text_file


PROFILE_EXTENSIONS = {
    ".txt",
    ".log",
    ".csv",
    ".prom",
    ".out",
    ".stderr",
    ".stdout",
    ".nsys-rep",
    ".ncu-rep",
    ".sqlite",
}

PERF_COUNTER_RE = re.compile(
    r"^\s*(?P<value>[0-9][0-9,.' ]*|\d+(?:\.\d+)?)\s+"
    r"(?P<counter>[A-Za-z0-9_.:/%-]+)"
)
LLAMA_TIMING_RE = re.compile(
    r"(?P<label>load time|prompt eval time|eval time|total time)\s*=\s*"
    r"(?P<ms>[0-9.]+)\s*ms"
    r"(?:\s*/\s*(?P<tokens>[0-9]+)\s*tokens?)?"
    r"(?:\s*\(\s*(?P<ms_per_token>[0-9.]+)\s*ms per token,\s*"
    r"(?P<tokens_per_second>[0-9.]+)\s*tokens per second\s*\))?",
    re.IGNORECASE,
)
PROM_RE = re.compile(r"^(?P<name>[a-zA-Z_:][a-zA-Z0-9_:]*)(?:\{(?P<labels>[^}]*)\})?\s+(?P<value>[-+0-9.eE]+)")


def read_text(path: Path, limit: int) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")[:limit]
    except OSError as exc:
        return f"<read error: {redact_path_error(exc, path)}>"


def number(value: str) -> Optional[float]:
    cleaned = value.replace(",", "").replace("'", "").replace(" ", "")
    try:
        return float(cleaned)
    except ValueError:
        return None


def path_issue(path: Path, status: str, exc: OSError) -> Dict[str, Any]:
    safe_path = metric_path_label(path.as_posix())
    return {
        "path": safe_path,
        "path_redacted": bool(path.as_posix() and safe_path != path.as_posix()),
        "status": status,
        "error": redact_path_error(exc, path),
    }


def parse_perf(text: str) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    for line in text.splitlines():
        match = PERF_COUNTER_RE.match(line)
        if not match:
            continue
        value = number(match.group("value"))
        if value is None:
            continue
        counter = match.group("counter").strip()
        metrics.append({"name": counter, "value": value, "source": "perf_stat"})
    return metrics


def parse_llama(text: str) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    for match in LLAMA_TIMING_RE.finditer(text):
        label = match.group("label").lower().replace(" ", "_")
        metrics.append({"name": f"{label}_ms", "value": float(match.group("ms")), "source": "llama_timing"})
        if match.group("tokens"):
            metrics.append({"name": f"{label}_tokens", "value": int(match.group("tokens")), "source": "llama_timing"})
        if match.group("tokens_per_second"):
            metrics.append({"name": f"{label}_tokens_per_second", "value": float(match.group("tokens_per_second")), "source": "llama_timing"})
    return metrics


def parse_prometheus(text: str, limit: int) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        match = PROM_RE.match(stripped)
        if not match:
            continue
        value = number(match.group("value"))
        if value is None:
            continue
        metrics.append(
            {
                "name": match.group("name"),
                "value": value,
                "labels": redact_sensitive_text(match.group("labels") or ""),
                "source": "prometheus_text",
            }
        )
        if len(metrics) >= limit:
            break
    return metrics


def parse_csv_metrics(path: Path, limit: int) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8", errors="replace", newline="") as file:
            reader = csv.DictReader(file)
            for index, row in enumerate(reader):
                if index >= limit:
                    break
                metric_name = row.get("Metric Name") or row.get("metric") or row.get("name") or row.get("Name")
                metric_value = row.get("Metric Value") or row.get("value") or row.get("Value")
                if metric_name and metric_value:
                    value = number(str(metric_value))
                    if value is not None:
                        metrics.append(
                            {
                                "name": redact_sensitive_text(str(metric_name)),
                                "value": value,
                                "source": "csv",
                                "row": index,
                                **path_payload(path),
                            }
                        )
    except OSError:
        return []
    return metrics


def classify(path: Path) -> str:
    lowered = path.as_posix().lower()
    if "nsight_compute" in lowered or path.suffix == ".ncu-rep":
        return "nsight_compute"
    if "vllm" in lowered:
        return "vllm"
    if "trtllm" in lowered:
        return "trtllm"
    if "llama" in lowered:
        return "llama_cpp"
    if "android" in lowered or "simpleperf" in lowered:
        return "android"
    if path.suffix == ".prom":
        return "prometheus"
    if "perf" in lowered:
        return "perf"
    return "generic"


def parse_file(path: Path, root: Path, text_limit: int, metric_limit: int) -> Dict[str, Any]:
    rel = path.relative_to(root).as_posix() if path.is_relative_to(root) else path.as_posix()
    safe_rel = metric_path_label(rel)
    try:
        stat = path.stat()
    except OSError as exc:
        return {
            "path": safe_rel,
            "path_redacted": bool(rel and safe_rel != rel),
            "category": classify(path),
            "suffix": path.suffix,
            "size_bytes": 0,
            "modified_time": "",
            "metrics": [],
            "text_preview": "",
            "status": "unreadable_profile_artifact",
            "read_error": redact_path_error(exc, path),
        }
    record: Dict[str, Any] = {
        "path": safe_rel,
        "path_redacted": bool(rel and safe_rel != rel),
        "category": classify(path),
        "suffix": path.suffix,
        "size_bytes": stat.st_size,
        "modified_time": datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat(),
        "metrics": [],
        "text_preview": "",
    }
    if path.suffix in {".ncu-rep", ".nsys-rep", ".sqlite"}:
        record["binary_artifact"] = True
        return record
    text = read_text(path, text_limit)
    record.update(
        artifact_preview_fields(
            "\n".join(text.splitlines()[:20]),
            field_name="text_preview",
            max_chars=4000,
        )
    )
    metrics = []
    metrics.extend(parse_perf(text))
    metrics.extend(parse_llama(text))
    metrics.extend(parse_prometheus(text, metric_limit))
    if path.suffix == ".csv":
        metrics.extend(parse_csv_metrics(path, metric_limit))
    deduped: List[Dict[str, Any]] = []
    seen = set()
    for metric in metrics:
        key = (metric.get("source"), metric.get("name"), metric.get("value"), metric.get("row"))
        if key in seen:
            continue
        seen.add(key)
        deduped.append(metric)
        if len(deduped) >= metric_limit:
            break
    record["metrics"] = deduped
    return record


def iter_artifacts(root: Path) -> Tuple[List[Path], List[Dict[str, Any]]]:
    issues: List[Dict[str, Any]] = []
    try:
        exists = root.exists()
    except OSError as exc:
        return [], [path_issue(root, "unreadable_artifact_root_probe", exc)]
    if not exists:
        return [], []

    paths: List[Path] = []
    try:
        for path in root.rglob("*"):
            try:
                if path.is_file() and path.suffix.lower() in PROFILE_EXTENSIONS:
                    paths.append(path)
            except OSError as exc:
                issues.append(path_issue(path, "unreadable_profile_file_probe", exc))
    except OSError as exc:
        issues.append(path_issue(root, "unreadable_profile_dir_scan", exc))
    return sorted(paths), issues


def summarize(files: List[Dict[str, Any]]) -> Dict[str, Any]:
    categories: Dict[str, Dict[str, Any]] = {}
    for item in files:
        category = item["category"]
        bucket = categories.setdefault(category, {"files": 0, "bytes": 0, "metrics": 0})
        bucket["files"] += 1
        bucket["bytes"] += item.get("size_bytes", 0)
        bucket["metrics"] += len(item.get("metrics", []))
    return {
        "file_count": len(files),
        "total_bytes": sum(item.get("size_bytes", 0) for item in files),
        "metric_count": sum(len(item.get("metrics", [])) for item in files),
        "categories": categories,
    }


def markdown(payload: Dict[str, Any]) -> str:
    lines = [
        "# Profiling Artifact Import",
        "",
        f"- Generated: `{payload['timestamp_utc']}`",
        f"- Artifact root: `{payload['artifact_root']}`",
        "",
        "## Summary",
        "",
        "| Category | Files | Bytes | Metrics |",
        "|---|---:|---:|---:|",
    ]
    categories = payload["summary"]["categories"]
    for category, item in sorted(categories.items()):
        lines.append(f"| {category} | {item['files']} | {item['bytes']} | {item['metrics']} |")
    if not categories:
        lines.append("| _none_ | 0 | 0 | 0 |")
    lines.extend(["", "## Files", "", "| Path | Category | Bytes | Metrics |", "|---|---|---:|---:|"])
    for item in payload["files"]:
        lines.append(f"| `{item['path']}` | {item['category']} | {item['size_bytes']} | {len(item.get('metrics', []))} |")
    if payload.get("discovery_issues"):
        lines.extend(["", "## Discovery Issues", "", "| Path | Status | Error |", "|---|---|---|"])
        for issue in payload["discovery_issues"]:
            error = str(issue.get("error", "")).replace("|", "\\|")
            lines.append(f"| `{issue.get('path')}` | `{issue.get('status')}` | {error} |")
    lines.extend(["", "## Notes", "", "- Binary `.ncu-rep` and `.nsys-rep` files are tracked by path and size; export CSV/text alongside them to expose detailed metrics.", "- Prometheus text snapshots, perf stat logs, llama.cpp timing logs, and simple CSV metric exports are parsed into normalized metric rows.", "- This importer does not execute profilers; it only converts already-collected artifacts into reportable JSON."])
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--artifact-root", default="profiling")
    parser.add_argument("--output", default="results/raw/profiling_artifacts.json")
    parser.add_argument("--report", default="reports/profiling_artifacts.md")
    parser.add_argument("--text-limit", type=int, default=200000)
    parser.add_argument("--metric-limit", type=int, default=2000)
    args = parser.parse_args()
    require_int_range("--text-limit", args.text_limit, 1, 100_000_000)
    require_int_range("--metric-limit", args.metric_limit, 0, 1_000_000)

    repo_root = resolved_repo_root(args.repo_root)
    root = resolve_repo_path(repo_root, args.artifact_root)
    artifact_paths, discovery_issues = iter_artifacts(root)
    files = [parse_file(path, root, args.text_limit, args.metric_limit) for path in artifact_paths]
    summary = summarize(files)
    summary["discovery_issue_count"] = len(discovery_issues)
    payload = {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "repo_root": metric_path_label(repo_root.as_posix()),
        "artifact_root": metric_path_label(str(args.artifact_root)),
        "resolved_artifact_root": metric_path_label(root.as_posix()),
        "paths_redacted": True,
        "summary": summary,
        "discovery_issues": discovery_issues,
        "files": files,
    }
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    report = resolve_repo_path(repo_root, args.report)
    write_text_file(report, markdown(payload))
    print(f"wrote {metric_path_label(output.as_posix())}")
    print(f"wrote {metric_path_label(report.as_posix())}")


if __name__ == "__main__":
    main()
