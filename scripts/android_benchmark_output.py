#!/usr/bin/env python3
"""Bounded llama.cpp output projections for the Android ADB runner."""

from __future__ import annotations

import math
import re
import shlex
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Mapping, Sequence

from command_redaction import (
    REDACTED_VALUE,
    artifact_command_parts as shared_artifact_command_parts,
    command_line,
    redact_path_error,
    redacted_path_label as path_label,
)


DECIMAL_PATTERN = r"(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+)"
TIMING_RE = re.compile(
    rf"(?P<label>prompt eval time|eval time|total time)\s*=\s*(?P<ms>{DECIMAL_PATTERN})\s*ms"
    r"(?P<rest>[^\r\n]*)",
    re.I,
)
TOKEN_COUNT_RE = re.compile(r"/\s*(?P<tokens>[0-9]+)\s*tokens?", re.I)
TOKENS_PER_SECOND_RE = re.compile(rf"(?P<tps>{DECIMAL_PATTERN})\s*tokens per second", re.I)
BACKEND_NAMES = ("cpu", "vulkan", "opencl", "kompute", "cuda", "sycl", "metal")
MAX_LLAMA_OUTPUT_BYTES = 16 * 1024 * 1024
MAX_DIAGNOSTIC_LINE_BYTES = 4096
MAX_SIMPLEPERF_COUNTER_VALUE = (1 << 63) - 1
SIMPLEPERF_EVENT_NAMES = (
    "cpu-cycles",
    "instructions",
    "cache-misses",
    "cache-references",
    "branch-misses",
    "branch-instructions",
    "task-clock",
    "cpu-clock",
    "context-switches",
    "cpu-migrations",
    "page-faults",
    "minor-faults",
    "major-faults",
)
LLAMA_DIAGNOSTIC_PREFIX_RE = re.compile(
    r"^\s*(?:llama_|ggml_|load_backend:|main:|system_info:|adb:|sh:|/system/bin/sh:|simpleperf:)",
    re.I,
)
LLAMA_TIMING_PREFIX_RE = re.compile(r"^\s*(?:llama_perf_|llama_print_timings:)", re.I)
SIMPLEPERF_NUMBER_PATTERN = r"(?:[0-9]+|[0-9]{1,3}(?:,[0-9]{3})+|[0-9]{1,3}(?:_[0-9]{3})+)(?:\.[0-9]+)?"
SIMPLEPERF_COUNTER_RE = re.compile(
    rf"^\s*(?P<value>{SIMPLEPERF_NUMBER_PATTERN}|<not counted>)"
    rf"(?:\s*(?:\((?P<parenthesized_unit>[A-Za-z]{{1,16}})\)|"
    rf"(?P<bare_unit>ns|us|ms|msec|s|sec|seconds?)))?\s+"
    rf"(?P<event>{'|'.join(re.escape(name) for name in SIMPLEPERF_EVENT_NAMES)})"
    r"(?::[A-Za-z0-9_-]+)?\b",
    re.I,
)
SIMPLEPERF_TOTAL_TEST_TIME_RE = re.compile(
    rf"^\s*Total test time:\s*(?P<seconds>{SIMPLEPERF_NUMBER_PATTERN})\s*(?:s|sec(?:ond)?s?)\.?\s*$",
    re.I,
)
SIMPLEPERF_SECTION_HEADER_RE = re.compile(r"^\s*Performance counter statistics:?\s*$", re.I)
SIMPLEPERF_COUNTER_CANDIDATE_RE = re.compile(
    rf"^\s*(?:{SIMPLEPERF_NUMBER_PATTERN}|<[^>]{{1,64}}>)\s+",
    re.I,
)
SIMPLEPERF_EVENT_MENTION_RE = re.compile(
    rf"\b(?:{'|'.join(re.escape(name) for name in SIMPLEPERF_EVENT_NAMES)})\b",
    re.I,
)


def decoded_process_output(value: Any) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value if isinstance(value, str) else ""


def _redact_prompt_shell(text: str) -> str:
    try:
        parts = shlex.split(text)
    except ValueError:
        return REDACTED_VALUE
    for index, part in enumerate(parts[:-1]):
        if part == "-p":
            parts[index + 1] = REDACTED_VALUE
    return shlex.join(parts)


def artifact_command_parts(parts: Sequence[str]) -> List[str]:
    redacted = shared_artifact_command_parts(parts, content_options=("-s",))
    for index, part in enumerate(list(redacted)):
        if " -p " in part:
            redacted[index] = _redact_prompt_shell(part)
        elif part.startswith("/") and not part.startswith(("/data/", "/proc/", "/sys/")):
            redacted[index] = path_label(part)
    return redacted


def command_projection(parts: Sequence[str]) -> Dict[str, Any]:
    command = artifact_command_parts(parts)
    return {"command": command, "shell": command_line(command)}


def validate_output_paths(output: Path, report: Path, protected: List[Path]) -> None:
    candidates = [("output", output), ("report", report), *[("protected input", path) for path in protected]]
    resolved: List[tuple[str, Path]] = []
    for label, path in candidates:
        try:
            resolved.append((label, path.resolve(strict=False)))
        except (OSError, RuntimeError) as exc:
            raise ValueError(f"{label} path cannot be resolved: {redact_path_error(exc, path)}") from exc
    def aliases(left: Path, right: Path) -> bool:
        if left == right:
            return True
        try:
            return left.exists() and right.exists() and left.samefile(right)
        except OSError as exc:
            raise ValueError(f"path identity cannot be inspected: {redact_path_error(exc, left, right)}") from exc

    if aliases(resolved[0][1], resolved[1][1]):
        raise ValueError("--output and --report must resolve to different paths")
    for label, path in resolved[2:]:
        if aliases(resolved[0][1], path) or aliases(resolved[1][1], path):
            raise ValueError(f"artifact destination aliases {label} {path_label(path)}")


def parse_llama_timings(text: str) -> Dict[str, Any]:
    values: Dict[str, Any] = {}
    for line in text.splitlines():
        if not LLAMA_TIMING_PREFIX_RE.search(line):
            continue
        match = TIMING_RE.search(line)
        if match is None:
            continue
        key = match.group("label").lower().replace(" ", "_")
        if len(match.group("ms")) > 64:
            continue
        milliseconds = float(match.group("ms"))
        if not math.isfinite(milliseconds) or milliseconds <= 0.0:
            continue
        item: Dict[str, Any] = {"ms": milliseconds}
        token_match = TOKEN_COUNT_RE.search(match.group("rest"))
        if token_match:
            if len(token_match.group("tokens")) > 10:
                continue
            tokens = int(token_match.group("tokens"))
            if tokens <= 0 or tokens > 10_000_000:
                continue
            item["tokens"] = tokens
        throughput_match = TOKENS_PER_SECOND_RE.search(match.group("rest"))
        if throughput_match:
            if len(throughput_match.group("tps")) > 64:
                continue
            tokens_per_second = float(throughput_match.group("tps"))
            if not math.isfinite(tokens_per_second) or tokens_per_second <= 0.0:
                continue
            item["tokens_per_second"] = tokens_per_second
        if key in values:
            return {}
        values[key] = item
    return values


def _simpleperf_number(value: str, *, strictly_positive: bool = False) -> int | float | None:
    normalized = value.replace(",", "").replace("_", "")
    if len(normalized) > 64:
        return None
    if "." not in normalized:
        number: int | float = int(normalized)
    else:
        number = float(normalized)
    if not math.isfinite(number) or number < 0 or number > MAX_SIMPLEPERF_COUNTER_VALUE:
        return None
    if strictly_positive and number <= 0:
        return None
    return number


def parse_simpleperf_stat(text: str, requested: bool) -> Dict[str, Any]:
    base = {
        "requested": requested,
        "status": "not_requested" if not requested else "unavailable",
        "counters": {},
        "not_counted_events": [],
        "total_test_time_seconds": None,
        "issue_count": 0,
        "summary": {"counter_count": 0, "not_counted_count": 0, "section_count": 0},
        "provenance": "simpleperf_stat_stderr" if requested else "not_requested",
        "claim_boundary": "simpleperf covers the wrapped external process interval; it does not prove per-operator attribution, backend delegation, frequency stability, JNI behavior or workload-exclusive energy",
    }
    if not requested:
        return base
    counters: Dict[str, Any] = {}
    not_counted: List[str] = []
    total_seconds: int | float | None = None
    issues = 0
    duplicate = False
    section_count = 0
    in_counter_section = False
    for line in text.splitlines():
        if len(line.encode("utf-8", errors="replace")) > MAX_DIAGNOSTIC_LINE_BYTES:
            issues += int(bool(in_counter_section or SIMPLEPERF_EVENT_MENTION_RE.search(line)))
            continue
        if SIMPLEPERF_SECTION_HEADER_RE.fullmatch(line):
            section_count += 1
            duplicate = duplicate or section_count > 1
            issues += int(section_count > 1)
            in_counter_section = True
            continue
        if not in_counter_section:
            lowered = line.lower()
            if "total test time:" in lowered or SIMPLEPERF_EVENT_MENTION_RE.search(lowered):
                issues += 1
            continue
        counter_match = SIMPLEPERF_COUNTER_RE.search(line)
        if counter_match is not None:
            event = counter_match.group("event").lower()
            if event in counters or event in not_counted:
                duplicate = True
                issues += 1
                continue
            raw_value = counter_match.group("value")
            if raw_value.lower() == "<not counted>":
                not_counted.append(event)
                continue
            value = _simpleperf_number(raw_value)
            if value is None:
                issues += 1
                continue
            counters[event] = {
                "value": value,
                "reported_unit": (
                    counter_match.group("parenthesized_unit")
                    or counter_match.group("bare_unit")
                    or ""
                ).lower(),
                "value_semantics": "simpleperf_reported_counter_value",
            }
            continue
        total_match = SIMPLEPERF_TOTAL_TEST_TIME_RE.search(line)
        if total_match is not None:
            if total_seconds is not None:
                duplicate = True
                issues += 1
                continue
            total_seconds = _simpleperf_number(total_match.group("seconds"), strictly_positive=True)
            issues += int(total_seconds is None)
            in_counter_section = False
            continue
        if SIMPLEPERF_COUNTER_CANDIDATE_RE.search(line) or SIMPLEPERF_EVENT_MENTION_RE.search(line):
            issues += 1
    if duplicate:
        counters = {}
        not_counted = []
        total_seconds = None
        status = "ambiguous_duplicate_observation"
    elif counters and not issues and not not_counted and total_seconds is not None:
        status = "observed"
    elif counters:
        status = "partial"
    elif not_counted:
        status = "unavailable_not_counted"
    else:
        status = "unavailable"
    return {
        **base,
        "status": status,
        "counters": counters,
        "not_counted_events": sorted(not_counted),
        "total_test_time_seconds": total_seconds,
        "issue_count": issues,
        "summary": {
            "counter_count": len(counters),
            "not_counted_count": len(not_counted),
            "section_count": section_count,
        },
    }


def simpleperf_placeholder(requested: bool, requested_status: str) -> Dict[str, Any]:
    observation = parse_simpleperf_stat("", requested)
    if requested:
        observation["status"] = requested_status
    return observation


def project_android_scenario_observation(
    output_observation: Any,
    command_succeeded: bool,
    simpleperf_requested: bool,
) -> Dict[str, Any]:
    observation = output_observation if isinstance(output_observation, Mapping) else {}
    timings = observation.get("timings") if isinstance(observation.get("timings"), dict) else {}
    backend = observation.get("backend_observation")
    if not isinstance(backend, dict):
        backend = {"status": "unavailable", "reported_backends": []}
    simpleperf = observation.get("simpleperf_observation")
    if not isinstance(simpleperf, dict):
        simpleperf = simpleperf_placeholder(simpleperf_requested, "unavailable")
    projection_status = str(observation.get("status", "unavailable"))
    has_key_timing = bool(timings.get("eval_time") or timings.get("total_time"))
    if not command_succeeded:
        status = "failed"
    elif not has_key_timing:
        status = f"partial_{'no_timing' if projection_status == 'observed' else projection_status}"
    elif simpleperf_requested and simpleperf.get("status") != "observed":
        status = f"partial_simpleperf_{simpleperf.get('status', 'unavailable')}"
    else:
        status = "ok"
    return {
        "status": status,
        "timings": timings,
        "backend_observation": backend,
        "simpleperf_observation": simpleperf,
        "metric_status": "parsed_timing" if has_key_timing else projection_status,
    }


def aggregate_android_runtime_observations(
    records: Sequence[Mapping[str, Any]],
    requested_gpu_layers: int | None,
    simpleperf_requested: bool,
) -> Dict[str, Any]:
    reported_backends = sorted({
        str(backend)
        for record in records
        for backend in record.get("backend_observation", {}).get("reported_backends", [])
    })
    simpleperf_observed_count = sum(
        bool(record.get("attempted"))
        and record.get("simpleperf_observation", {}).get("status") == "observed"
        for record in records
    )
    simpleperf_gap_count = sum(
        bool(record.get("attempted"))
        and simpleperf_requested
        and record.get("simpleperf_observation", {}).get("status") != "observed"
        for record in records
    )
    return {
        "backend_observation": {
            "status": "reported_by_runtime_log" if reported_backends else "unavailable",
            "requested_gpu_layers": requested_gpu_layers,
            "reported_backends": reported_backends,
            "claim_boundary": "runtime log names do not prove operator delegation, layer placement or acceleration coverage",
        },
        "simpleperf_summary": {
            "simpleperf_requested": simpleperf_requested,
            "simpleperf_observed_count": simpleperf_observed_count,
            "simpleperf_gap_count": simpleperf_gap_count,
        },
    }


def parse_llama_output(text: str, simpleperf_requested: bool = False) -> Dict[str, Any]:
    if len(text.encode("utf-8", errors="replace")) > MAX_LLAMA_OUTPUT_BYTES:
        return {
            "status": "output_limit_exceeded",
            "timings": {},
            "backend_observation": {"status": "unavailable", "reported_backends": []},
            "simpleperf_observation": {
                **parse_simpleperf_stat("", simpleperf_requested),
                "status": "output_limit_exceeded" if simpleperf_requested else "not_requested",
            },
        }
    reported_backends: List[str] = []
    for line in text.splitlines():
        lowered = line.lower()
        if not LLAMA_DIAGNOSTIC_PREFIX_RE.search(line) or ("backend" not in lowered and "offload" not in lowered):
            continue
        for backend in BACKEND_NAMES:
            if re.search(rf"\b{re.escape(backend)}\b", lowered) and backend not in reported_backends:
                reported_backends.append(backend)
    return {
        "status": "observed",
        "timings": parse_llama_timings(text),
        "backend_observation": {
            "status": "reported_by_runtime_log" if reported_backends else "unavailable",
            "reported_backends": reported_backends,
            "provenance": "llama_cpp_runtime_output_projection",
            "claim_boundary": "reported backend names do not prove operator delegation, layer placement or acceleration coverage",
        },
        "simpleperf_observation": parse_simpleperf_stat(text, simpleperf_requested),
    }


def llama_diagnostic_output(text: str) -> str:
    if len(text.encode("utf-8", errors="replace")) > MAX_LLAMA_OUTPUT_BYTES:
        return "llama.cpp output omitted: 16 MiB projection limit exceeded"
    retained: List[str] = []
    in_simpleperf_section = False
    for line in text.splitlines():
        if len(line.encode("utf-8", errors="replace")) > MAX_DIAGNOSTIC_LINE_BYTES:
            continue
        lowered = line.lower()
        timing = LLAMA_TIMING_PREFIX_RE.search(line) and TIMING_RE.search(line)
        runtime_log = LLAMA_DIAGNOSTIC_PREFIX_RE.search(line) and any(
            token in lowered for token in ("backend", "offload", "error", "failed", "fatal", "signal")
        )
        section_header = SIMPLEPERF_SECTION_HEADER_RE.fullmatch(line)
        if section_header:
            in_simpleperf_section = True
        simpleperf = section_header or (
            in_simpleperf_section
            and (
                SIMPLEPERF_COUNTER_RE.search(line)
                or SIMPLEPERF_COUNTER_CANDIDATE_RE.search(line)
                or SIMPLEPERF_TOTAL_TEST_TIME_RE.search(line)
            )
        )
        if timing or runtime_log or simpleperf:
            retained.append(line)
        if in_simpleperf_section and SIMPLEPERF_TOTAL_TEST_TIME_RE.search(line):
            in_simpleperf_section = False
    return "\n".join(retained[-120:])


def bounded_probe_stdout(text: str) -> Dict[str, Any]:
    encoded = text.encode("utf-8", errors="replace")
    if len(encoded) > 64 * 1024:
        return {"probe_text": "", "status": "output_limit_exceeded"}
    return {"probe_text": "\n".join(text.splitlines()[-120:]), "status": "observed"}


def aggregate_android_benchmark_status(
    records: List[Dict[str, Any]],
    execution_requested: bool,
    setup_failures: int = 0,
    device_evidence_status: str = "planned",
) -> str:
    if not execution_requested:
        return "planned"
    if setup_failures:
        return "failed"
    statuses = [str(item.get("status", "missing")) for item in records]
    if statuses and all(status == "ok" for status in statuses):
        return "ok" if device_evidence_status == "observed" else "partial_device_evidence"
    if any(status == "failed" for status in statuses):
        return "failed"
    if any(status.startswith("partial") for status in statuses):
        return "partial"
    return "missing" if not statuses else "failed"


def public_command_results(records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    projected: List[Dict[str, Any]] = []
    for record in records:
        result = record.get("result")
        if not isinstance(result, dict):
            continue
        item = dict(result)
        item.pop("output_observation", None)
        projected.append(item)
    return projected


def _markdown_cell(value: Any) -> str:
    return (
        str(value if value is not None else "")
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace("`", "&#96;")
        .replace("|", "\\|")
        .replace("\n", " ")
    )


def render_android_benchmark_markdown(payload: Dict[str, Any]) -> str:
    device = payload.get("device_evidence", {})
    identity = device.get("device_identity", {})
    resource = device.get("resource_window", {})
    before = resource.get("before", {})
    after = resource.get("after", {})
    battery_interval = resource.get("battery_counter_interval", {})
    footprint = device.get("deployment_footprint", {})
    package = footprint.get("package", {})
    lines = [
        "# Android Edge Benchmark",
        "",
        f"- Generated: `{_markdown_cell(payload.get('timestamp_utc'))}`",
        f"- Mode: `{_markdown_cell(payload.get('mode'))}`",
        f"- Status: `{_markdown_cell(payload.get('status'))}`",
        f"- Executed: `{bool(payload.get('executed'))}`",
        "",
        "## Device Evidence",
        "",
        "| Model | SoC | API | ABI | Evidence |",
        "|---|---|---:|---|---|",
        "| "
        + " | ".join(
            _markdown_cell(value)
            for value in (
                identity.get("device_model"),
                identity.get("soc_identity"),
                identity.get("android_api_level"),
                identity.get("primary_abi"),
                device.get("status"),
            )
        )
        + " |",
        "",
        "| Resource | Before | After |",
        "|---|---:|---:|",
        f"| Maximum thermal mC | {_markdown_cell(before.get('thermal', {}).get('maximum_millicelsius'))} | {_markdown_cell(after.get('thermal', {}).get('maximum_millicelsius'))} |",
        f"| Thermal service status | {_markdown_cell(before.get('thermal', {}).get('thermal_service_status_name'))} | {_markdown_cell(after.get('thermal', {}).get('thermal_service_status_name'))} |",
        f"| Battery energy counter uWh | {_markdown_cell(before.get('battery', {}).get('energy_now'))} | {_markdown_cell(after.get('battery', {}).get('energy_now'))} |",
        f"| Battery charge counter uAh | {_markdown_cell(before.get('battery', {}).get('charge_counter'))} | {_markdown_cell(after.get('battery', {}).get('charge_counter'))} |",
        "",
        "## Footprint And Counter Method",
        "",
        f"- Battery method: `{_markdown_cell(battery_interval.get('measurement_method'))}`",
        f"- Signed stored-energy change (uWh): `{_markdown_cell(battery_interval.get('stored_energy_counter_change_uwh'))}`",
        f"- Runtime binary bytes: `{_markdown_cell(footprint.get('runtime_binary_bytes'))}`",
        f"- Model file bytes: `{_markdown_cell(footprint.get('model_file_bytes'))}`",
        f"- Package status/bytes: `{_markdown_cell(package.get('status'))}` / `{_markdown_cell(package.get('total_archive_bytes'))}`",
        "",
        "## Scenarios",
        "",
        "| Threads | Status | Command |",
        "|---:|---|---|",
    ]
    for item in payload.get("records", []):
        lines.append(
            f"| {_markdown_cell(item.get('threads'))} | `{_markdown_cell(item.get('status'))}` | `{_markdown_cell(item.get('shell'))}` |"
        )
    if payload.get("summary", {}).get("simpleperf_requested"):
        lines.extend(
            [
                "",
                "## Simpleperf",
                "",
                "| Threads | Status | CPU cycles | Instructions | Cache misses | Task clock | Test time (s) | Issues / not counted |",
                "|---:|---|---:|---:|---:|---:|---:|---:|",
            ]
        )
        for item in payload.get("records", []):
            observation = item.get("simpleperf_observation", {})
            counters = observation.get("counters", {})
            counter_value = lambda name: counters.get(name, {}).get("value")
            lines.append(
                "| "
                + " | ".join(
                    _markdown_cell(value)
                    for value in (
                        item.get("threads"),
                        observation.get("status"),
                        counter_value("cpu-cycles"),
                        counter_value("instructions"),
                        counter_value("cache-misses"),
                        counter_value("task-clock"),
                        observation.get("total_test_time_seconds"),
                        f"{observation.get('issue_count', 0)} / {observation.get('summary', {}).get('not_counted_count', 0)}",
                    )
                )
                + " |"
            )
    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- Use `--execute` on an adb-connected Android/ARM target.",
            "- Enable `--simpleperf` to collect CPU counters with the same command contract.",
            "- Signed battery-counter changes are whole-device observations, not workload-exclusive energy.",
            "- This standalone CLI runner does not validate the InferLab JNI/AAR lifecycle.",
        ]
    )
    return "\n".join(lines) + "\n"
