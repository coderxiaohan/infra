#!/usr/bin/env python3
"""Compare InferLab GGUF probe output against a legal external fixture."""

from __future__ import annotations

import argparse
import json
import tempfile
from time_utils import utc_now_precise as now_utc
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence

from artifact_json import artifact_status_exit_code, load_json_dict
from bounded_process import run_bounded_process
from cli_validation import require_float_range
from command_redaction import (
    artifact_output_tail,
    artifact_path_payload as path_payload,
    redact_path_error,
    redacted_path_label as path_label,
)
from path_status import safe_is_file
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


DEFAULT_COMPARE_FIELDS = (
    "metadata_kv_count",
    "tensor_count",
    "quantized_tensor_count",
    "tensor_data_start_offset",
    "known_tensor_data_bytes",
    "tensor_type_names",
    "tensor_names",
)

BAD_ARTIFACT_STATUSES = {
    "invalid_artifact",
    "invalid_encoding",
    "invalid_json",
    "missing",
    "path_probe_failed",
    "unreadable_json",
}
PROBE_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024


def load_json(path: Path) -> Dict[str, Any]:
    return load_json_dict(
        path,
        missing={"status": "missing", **path_payload(path)},
    ) or {}


def artifact_problem(payload: Dict[str, Any]) -> Optional[str]:
    status = payload.get("status")
    if isinstance(status, str) and status in BAD_ARTIFACT_STATUSES:
        return status
    return None


def comparison_path_metadata(
    args: argparse.Namespace,
    repo_root: Path,
    fixture: Path,
    probe_exe: Path,
    probe_output: Optional[Path] = None,
) -> Dict[str, Any]:
    expected_json = resolve_repo_path(repo_root, args.expected_json) if args.expected_json else ""
    return {
        **path_payload(repo_root, "repo_root"),
        **path_payload(args.fixture, "fixture_path", "fixture_path_redacted"),
        **path_payload(fixture, "resolved_fixture_path"),
        **path_payload(args.expected_json, "expected_json", "expected_json_path_redacted"),
        **path_payload(expected_json, "resolved_expected_json"),
        **path_payload(args.probe_exe, "probe_exe", "probe_exe_path_redacted"),
        **path_payload(probe_exe, "resolved_probe_exe"),
        **path_payload(probe_output, "resolved_probe_output"),
    }


def invalid_comparison_payload(
    args: argparse.Namespace,
    repo_root: Path,
    *,
    status: str,
    reason: str,
    fixture: Path,
    probe_exe: Path,
    probe_output: Optional[Path] = None,
    probe_result: Optional[Dict[str, Any]] = None,
    artifact_payload: Optional[Dict[str, Any]] = None,
    observed: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **comparison_path_metadata(args, repo_root, fixture, probe_exe, probe_output),
        "status": status,
        "reason": reason,
        "probe_result": probe_result or {},
        "artifact_status": artifact_payload.get("status") if isinstance(artifact_payload, dict) else None,
        "artifact_error": artifact_payload.get("error") if isinstance(artifact_payload, dict) else None,
        "observed": observed or {},
        "expected": {},
        "mismatch_count": 0,
        "mismatches": [],
    }


def run_probe(probe_exe: Path, fixture: Path, output: Path, timeout_seconds: float) -> Dict[str, Any]:
    command = [
        str(probe_exe),
        "--input",
        str(fixture),
        "--output",
        str(output),
    ]
    capture = run_bounded_process(
        command,
        cwd=None,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=PROBE_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=PROBE_OUTPUT_LIMIT_BYTES,
        merge_stderr_into_stdout=True,
        launch_error_formatter=lambda exc: redact_path_error(exc, probe_exe, fixture, output),
    )
    captured_output = capture.stdout.decode("utf-8", errors="replace")
    result: Dict[str, Any] = {
        "returncode": capture.returncode if capture.status in {"ok", "nonzero_exit"} else None,
        "timeout": capture.timed_out,
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "captured_output_bytes": capture.stdout_bytes,
        "output_tail": artifact_output_tail(captured_output, command, None, 20),
    }
    if capture.status not in {"ok", "nonzero_exit"}:
        result["error_type"] = {
            "launch_failed": "ProcessLaunchError",
            "capture_failed": "ProcessCaptureError",
            "timed_out": "TimeoutExpired",
            "output_limit_exceeded": "OutputLimitExceeded",
            "invalid_arguments": "InvalidArguments",
        }.get(capture.status, "ProcessExecutionError")
    if capture.error:
        result["error"] = capture.error
    return result


def extract_observed(probe_payload: Dict[str, Any]) -> Dict[str, Any]:
    gguf = probe_payload.get("gguf") if isinstance(probe_payload.get("gguf"), dict) else probe_payload
    tensors = gguf.get("tensors") if isinstance(gguf.get("tensors"), list) else []
    tensor_names = [
        str(tensor.get("name"))
        for tensor in tensors
        if isinstance(tensor, dict) and "name" in tensor
    ]
    tensor_types = {
        str(tensor.get("name")): tensor.get("type_name")
        for tensor in tensors
        if isinstance(tensor, dict) and "name" in tensor
    }
    data_sizes = [
        tensor.get("data_size_bytes")
        for tensor in tensors
        if isinstance(tensor, dict) and isinstance(tensor.get("data_size_bytes"), int)
    ]
    return {
        "metadata_kv_count": gguf.get("metadata_kv_count"),
        "tensor_count": gguf.get("tensor_count"),
        "quantized_tensor_count": gguf.get("quantized_tensor_count"),
        "tensor_data_start_offset": gguf.get("tensor_data_start_offset"),
        "known_tensor_data_bytes": gguf.get("known_tensor_data_bytes"),
        "tensor_type_names": sorted(set(str(value) for value in tensor_types.values() if value is not None)),
        "tensor_names": tensor_names,
        "tensor_types": tensor_types,
        "largest_tensor_data_bytes": max(data_sizes) if data_sizes else None,
        "layout_segment_count": len(gguf.get("layout_segments", []))
        if isinstance(gguf.get("layout_segments"), list)
        else None,
        "source": gguf.get("source"),
        "input_path": gguf.get("input_path"),
    }


def normalize_expected(expected: Dict[str, Any]) -> Dict[str, Any]:
    if isinstance(expected.get("gguf"), dict):
        return extract_observed(expected)
    return expected


def compare_fields(
    expected: Dict[str, Any],
    observed: Dict[str, Any],
    fields: Sequence[str],
) -> List[Dict[str, Any]]:
    mismatches: List[Dict[str, Any]] = []
    for field in fields:
        expected_value = expected.get(field)
        observed_value = observed.get(field)
        if expected_value != observed_value:
            mismatches.append(
                {
                    "field": field,
                    "expected": expected_value,
                    "observed": observed_value,
                }
            )
    return mismatches


def skipped_payload(
    args: argparse.Namespace,
    repo_root: Path,
    reason: str,
    path_probe_errors: Dict[str, str] | None = None,
) -> Dict[str, Any]:
    fixture = resolve_repo_path(repo_root, args.fixture)
    expected_json = resolve_repo_path(repo_root, args.expected_json) if args.expected_json else Path()
    probe_exe = resolve_repo_path(repo_root, args.probe_exe)
    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **comparison_path_metadata(args, repo_root, fixture, probe_exe),
        "status": "skipped",
        "reason": reason,
        "path_probe_errors": {key: redact_path_error(value, fixture, expected_json, probe_exe) for key, value in (path_probe_errors or {}).items()},
        "observed": {},
        "mismatches": [],
    }


def ensure_parent_dir(path: Path) -> Optional[str]:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        return redact_path_error(exc, path)
    return None


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    fixture = resolve_repo_path(repo_root, args.fixture)
    probe_exe = resolve_repo_path(repo_root, args.probe_exe)
    fixture_is_file, fixture_probe_error = safe_is_file(fixture)
    probe_is_file, probe_probe_error = safe_is_file(probe_exe)
    if not fixture_is_file:
        reason = "fixture path probe failed" if fixture_probe_error else "fixture file missing"
        errors = {"fixture": fixture_probe_error} if fixture_probe_error else {}
        return skipped_payload(args, repo_root, reason, errors)
    if not probe_is_file:
        reason = "GGUF probe executable path probe failed" if probe_probe_error else "GGUF probe executable missing"
        errors = {"probe_exe": probe_probe_error} if probe_probe_error else {}
        return skipped_payload(args, repo_root, reason, errors)

    with tempfile.TemporaryDirectory() as tmp:
        probe_output = resolve_repo_path(repo_root, args.probe_output) if args.probe_output else Path(tmp) / "gguf_probe.json"
        output_dir_error = ensure_parent_dir(probe_output)
        if output_dir_error:
            return invalid_comparison_payload(
                args,
                repo_root,
                status="probe_output_prepare_failed",
                reason=f"failed to create probe output directory: {output_dir_error}",
                fixture=fixture,
                probe_exe=probe_exe,
                probe_output=probe_output,
            )
        probe_result = run_probe(probe_exe, fixture, probe_output, args.timeout_seconds)
        if probe_result.get("returncode") != 0:
            return {
                "schema_version": 1,
                "timestamp_utc": now_utc(),
                **comparison_path_metadata(args, repo_root, fixture, probe_exe, probe_output),
                "status": "failed",
                "reason": "GGUF probe execution failed",
                "probe_result": probe_result,
                "observed": {},
                "mismatches": [],
            }
        observed_payload = load_json(probe_output)
        observed_problem = artifact_problem(observed_payload)
        if observed_problem is not None:
            return invalid_comparison_payload(
                args,
                repo_root,
                status="invalid_observed_artifact",
                reason=f"observed probe artifact is {observed_problem}",
                fixture=fixture,
                probe_exe=probe_exe,
                probe_output=probe_output,
                probe_result=probe_result,
                artifact_payload=observed_payload,
            )
        observed = extract_observed(observed_payload)

    expected: Optional[Dict[str, Any]] = None
    if args.expected_json:
        expected_path = resolve_repo_path(repo_root, args.expected_json)
        expected_payload = load_json(expected_path)
        expected_problem = artifact_problem(expected_payload)
        if expected_problem is not None:
            return invalid_comparison_payload(
                args,
                repo_root,
                status="invalid_expected_artifact",
                reason=f"expected comparison artifact is {expected_problem}",
                fixture=fixture,
                probe_exe=probe_exe,
                probe_output=None,
                probe_result=probe_result,
                artifact_payload=expected_payload,
                observed=observed,
            )
        expected = normalize_expected(expected_payload)

    fields = [field.strip() for field in args.fields.split(",") if field.strip()]
    mismatches = compare_fields(expected, observed, fields) if expected is not None else []
    status = "ok" if expected is not None and not mismatches else "mismatch"
    if expected is None:
        status = "observed_only"

    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **comparison_path_metadata(args, repo_root, fixture, probe_exe, probe_output),
        "status": status,
        "compare_fields": fields,
        "observed": observed,
        "expected": expected or {},
        "mismatch_count": len(mismatches),
        "mismatches": mismatches,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--fixture", required=True, help="Path to a legal GGUF fixture.")
    parser.add_argument("--expected-json", default="", help="Optional expected probe or flat comparison JSON.")
    parser.add_argument("--probe-exe", default="build/inferlab_gguf_probe")
    parser.add_argument("--probe-output", default="", help="Optional path for the raw probe JSON.")
    parser.add_argument("--output", default="results/raw/gguf_fixture_comparison.json")
    parser.add_argument("--timeout-seconds", type=float, default=60.0)
    parser.add_argument("--fields", default=",".join(DEFAULT_COMPARE_FIELDS))
    parser.add_argument(
        "--allow-non-ok-status",
        action="store_true",
        help="Return zero after writing a non-ok diagnostic artifact.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require_float_range("--timeout-seconds", args.timeout_seconds, 0.001, 86_400.0)
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(path_label(output))
    return artifact_status_exit_code(payload, allow_non_ok=args.allow_non_ok_status)


if __name__ == "__main__":
    raise SystemExit(main())
