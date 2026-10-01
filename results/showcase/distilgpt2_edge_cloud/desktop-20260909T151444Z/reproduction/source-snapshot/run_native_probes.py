#!/usr/bin/env python3
"""Run native C++ probe executables when a build directory is available."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Dict, List, Sequence

try:
    from scripts.artifact_json import try_write_json_file as write_json
    from scripts.bounded_process import run_bounded_process
    from scripts.command_redaction import artifact_output_tail, artifact_path_payload, redact_path_error, redacted_path_label as path_label
    from scripts.generate_gguf_fixture import write_fixture
    from scripts.path_status import safe_exists, safe_is_file, safe_mtime_ns
    from scripts.repo_paths import default_repo_root
    from scripts.time_utils import utc_now_precise
except ModuleNotFoundError:
    from artifact_json import try_write_json_file as write_json
    from bounded_process import run_bounded_process
    from command_redaction import artifact_output_tail, artifact_path_payload, redact_path_error, redacted_path_label as path_label
    from generate_gguf_fixture import write_fixture
    from path_status import safe_exists, safe_is_file, safe_mtime_ns
    from repo_paths import default_repo_root
    from time_utils import utc_now_precise


PROBES = (
    ("inferlab_shape_probe", "native_shape_probe.json"),
    ("inferlab_scheduler_trace", "scheduler_trace.json"),
    ("inferlab_scheduler_pressure_sweep", "scheduler_pressure_sweep.json"),
    ("inferlab_scheduler_workload_sweep", "scheduler_workload_sweep.json"),
    ("inferlab_scheduler_capacity_sweep", "scheduler_capacity_sweep.json"),
    ("inferlab_scheduler_admission_policy_sweep", "scheduler_admission_policy_sweep.json"),
    ("inferlab_scheduler_runtime_policy_sweep", "scheduler_runtime_policy_sweep.json"),
    ("inferlab_scheduler_fairness_sweep", "scheduler_fairness_sweep.json"),
    ("inferlab_kv_allocation_compare", "kv_allocation_compare.json"),
    ("inferlab_prefix_cache_compare", "prefix_cache_compare.json"),
    ("inferlab_kv_fragmentation_probe", "kv_fragmentation_probe.json"),
    ("inferlab_paged_kv_locality_probe", "paged_kv_locality_probe.json"),
    ("inferlab_quantization_probe", "quantization_probe.json"),
    ("inferlab_quantization_sweep", "quantization_sweep.json"),
    ("inferlab_quantization_channel_compare", "quantization_channel_compare.json"),
    ("inferlab_attention_probe", "attention_probe.json"),
    ("inferlab_matrix_ops_profile", "matrix_ops_profile.json"),
    ("inferlab_decoder_probe", "decoder_probe.json"),
    ("inferlab_decoder_kv_probe", "decoder_kv_probe.json"),
    ("inferlab_gguf_probe", "gguf_probe.json"),
)

DEFAULT_PROBE_TIMEOUT_SECONDS = 300.0
NATIVE_PROBE_MERGED_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024
MINIMUM_PROBE_TIMEOUT_SECONDS = 0.001
MAXIMUM_PROBE_TIMEOUT_SECONDS = 86_400.0
NATIVE_PROBE_CAPTURE_FAILURE_REASONS = {
    "launch_failed": "native probe execution failed",
    "output_limit_exceeded": "native probe output limit exceeded",
    "capture_failed": "native probe output capture failed",
    "invalid_arguments": "native probe execution contract invalid",
}


def probe_timeout_is_valid(value: object) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and MINIMUM_PROBE_TIMEOUT_SECONDS <= value <= MAXIMUM_PROBE_TIMEOUT_SECONDS
        and math.isfinite(value)
    )


def artifact_status(path: Path, previous_mtime_ns: int | None = None) -> str:
    exists, current_mtime_ns, probe_error = safe_mtime_ns(path)
    if probe_error:
        return "unreadable_json"
    if not exists:
        return "missing_artifact"
    if previous_mtime_ns is not None and current_mtime_ns is not None and current_mtime_ns <= previous_mtime_ns:
        return "stale_artifact"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError:
        return "unreadable_json"
    except UnicodeDecodeError:
        return "invalid_encoding"
    except json.JSONDecodeError:
        return "invalid_json"
    return "present_json" if isinstance(payload, dict) else "invalid_artifact"


def run_probe(
    executable: Path,
    output: Path,
    repo_root: Path | None = None,
    extra_args: Sequence[str] | None = None,
    timeout_seconds: float = DEFAULT_PROBE_TIMEOUT_SECONDS,
) -> Dict[str, object]:
    repo_root = default_repo_root() if repo_root is None else repo_root
    timeout_valid = probe_timeout_is_valid(timeout_seconds)
    timeout_metadata = {
        "timeout_seconds": float(timeout_seconds) if timeout_valid else None,
        "timeout_disabled": False,
    }
    if not timeout_valid:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "failed",
            "reason": "invalid native probe timeout",
            **timeout_metadata,
        }
    executable_exists, executable_probe_error = safe_exists(executable)
    if executable_probe_error:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "skipped",
            "reason": "executable path probe failed",
            "path_probe_error": redact_path_error(executable_probe_error, executable),
            **timeout_metadata,
        }
    if not executable_exists:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "skipped",
            "reason": "executable missing",
            **timeout_metadata,
        }
    executable_is_file, executable_file_error = safe_is_file(executable)
    if executable_file_error:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "skipped",
            "reason": "executable file probe failed",
            "path_probe_error": redact_path_error(executable_file_error, executable),
            **timeout_metadata,
        }
    if not executable_is_file:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "skipped",
            "reason": "not a file",
            **timeout_metadata,
        }

    try:
        output.parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "failed",
            "reason": "failed to create output directory",
            "error": redact_path_error(exc, output),
            **timeout_metadata,
        }
    command = [
        str(executable),
        "--repo-root",
        str(repo_root),
        *(extra_args or ()),
        "--output",
        str(output),
    ]
    _, previous_mtime_ns, previous_mtime_error = safe_mtime_ns(output)
    capture = run_bounded_process(
        command,
        cwd=repo_root,
        timeout_seconds=float(timeout_seconds),
        stdout_limit_bytes=NATIVE_PROBE_MERGED_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=NATIVE_PROBE_MERGED_OUTPUT_LIMIT_BYTES,
        launch_error_formatter=lambda exc: redact_path_error(exc, executable, output, repo_root),
        merge_stderr_into_stdout=True,
    )
    output_text = capture.stdout.decode("utf-8", errors="replace")
    if capture.status == "timed_out":
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "failed",
            "reason": "native probe timed out",
            "returncode": None,
            "elapsed_ms": capture.elapsed_ms,
            "output_tail": artifact_output_tail(output_text, command, None, 12),
            **timeout_metadata,
        }
    if capture.status not in {"ok", "nonzero_exit"}:
        return {
            "name": executable.name,
            "output": path_label(output),
            "output_redacted": True,
            "status": "failed",
            "reason": NATIVE_PROBE_CAPTURE_FAILURE_REASONS.get(
                capture.status, "native probe execution failed"
            ),
            "returncode": None,
            "elapsed_ms": capture.elapsed_ms,
            "output_tail": artifact_output_tail(output_text, command, None, 12),
            "error": capture.error or capture.status,
            **timeout_metadata,
        }
    result: Dict[str, object] = {
        "name": executable.name,
        "output": path_label(output),
        "output_redacted": True,
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        **timeout_metadata,
    }
    if previous_mtime_error:
        result["previous_output_probe_error"] = previous_mtime_error
    if capture.returncode == 0 and previous_mtime_error:
        output_status = "baseline_probe_failed"
    else:
        output_status = artifact_status(output, previous_mtime_ns) if capture.returncode == 0 else ""
    if capture.returncode == 0 and output_status == "present_json":
        result["status"] = "passed"
    elif capture.returncode == 0:
        result["status"] = f"partial_{output_status}"
    else:
        result["status"] = "failed"
    result["artifact_status"] = output_status
    if output_text.strip():
        result["output_tail"] = artifact_output_tail(output_text.strip(), command, None, 12)
    return result


def run_all(
    build_dir: Path,
    results_dir: Path,
    repo_root: Path | None = None,
    timeout_seconds: float = DEFAULT_PROBE_TIMEOUT_SECONDS,
) -> Dict[str, object]:
    repo_root = default_repo_root() if repo_root is None else repo_root
    timeout_valid = probe_timeout_is_valid(timeout_seconds)
    normalized_timeout = float(timeout_seconds) if timeout_valid else None
    runs: List[Dict[str, object]] = []
    fixture_path = results_dir / "inferlab_fixture.gguf"
    for executable, output_name in PROBES:
        executable_path = build_dir / executable
        extra_args: List[str] = []
        executable_is_file = safe_is_file(executable_path)[0] if timeout_valid else False
        if executable == "inferlab_gguf_probe" and executable_is_file and timeout_valid:
            try:
                write_fixture(fixture_path)
                extra_args.extend(["--input", str(fixture_path)])
            except OSError as exc:
                runs.append(
                    {
                        "name": executable_path.name,
                        "output": path_label(results_dir / output_name),
                        "output_redacted": True,
                        "status": "failed",
                        "reason": "failed to create GGUF fixture",
                        "error": redact_path_error(exc, fixture_path),
                        "timeout_seconds": normalized_timeout,
                        "timeout_disabled": False,
                    }
                )
                continue
        runs.append(
            run_probe(
                executable_path,
                results_dir / output_name,
                repo_root,
                extra_args=extra_args,
                timeout_seconds=timeout_seconds,
            )
        )
    counts = {
        "passed": sum(1 for run in runs if run["status"] == "passed"),
        "partial": sum(1 for run in runs if str(run["status"]).startswith("partial")),
        "skipped": sum(1 for run in runs if run["status"] == "skipped"),
        "failed": sum(1 for run in runs if run["status"] == "failed"),
    }
    elapsed_values = [
        float(run["elapsed_ms"])
        for run in runs
        if isinstance(run.get("elapsed_ms"), (int, float))
    ]
    slowest = max(
        (
            run
            for run in runs
            if isinstance(run.get("elapsed_ms"), (int, float))
        ),
        key=lambda run: float(run["elapsed_ms"]),
        default=None,
    )
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now_precise(),
        **artifact_path_payload(build_dir, "build_dir"),
        **artifact_path_payload(repo_root, "repo_root"),
        **artifact_path_payload(results_dir, "results_dir"),
        "timeout_policy": {
            "per_probe_timeout_seconds": normalized_timeout,
            "timeout_disabled": False,
            "default_timeout_seconds": DEFAULT_PROBE_TIMEOUT_SECONDS,
            "minimum_timeout_seconds": MINIMUM_PROBE_TIMEOUT_SECONDS,
            "maximum_timeout_seconds": MAXIMUM_PROBE_TIMEOUT_SECONDS,
            "timeout_disable_supported": False,
            "disable_timeout_value": None,
        },
        "coverage_source": "native_probe_runner",
        "coverage_manifest": "scripts/run_native_probes.py::PROBES",
        "covered_probe_count": len(PROBES),
        "covered_probes": [
            {"target": executable, "artifact": output_name}
            for executable, output_name in PROBES
        ],
        "counts": counts,
        "timing": {
            "total_elapsed_ms": round(sum(elapsed_values), 3),
            "slowest_probe": slowest["name"] if slowest is not None else None,
            "slowest_probe_elapsed_ms": slowest["elapsed_ms"] if slowest is not None else None,
        },
        "probes": runs,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output", default="results/raw/native_probe_run.json")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return non-zero if any native probe fails or is skipped.",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=float,
        default=DEFAULT_PROBE_TIMEOUT_SECONDS,
        help=(
            "Per-probe timeout in seconds. Defaults to 300 and must stay in "
            "the bounded range [0.001, 86400]."
        ),
    )
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if not probe_timeout_is_valid(args.timeout_seconds):
        raise SystemExit("--timeout-seconds must be a finite value in [0.001, 86400]")


def main() -> None:
    args = parse_args()
    validate_args(args)
    repo_root = Path(args.repo_root).resolve()
    build_dir = Path(args.build_dir)
    if not build_dir.is_absolute():
        build_dir = repo_root / build_dir
    results_dir = Path(args.results)
    if not results_dir.is_absolute():
        results_dir = repo_root / results_dir
    output_path = Path(args.output)
    if not output_path.is_absolute():
        output_path = repo_root / output_path

    payload = run_all(build_dir, results_dir, repo_root, args.timeout_seconds)
    write_status = write_json(output_path, payload)
    if write_status["status"] != "ok":
        print(f"failed to write {path_label(output_path)}: {write_status.get('error')}")
        raise SystemExit(1)
    print(path_label(output_path))
    if args.strict and (
        payload["counts"]["failed"]
        or payload["counts"]["skipped"]
        or payload["counts"]["partial"]
    ):
        raise SystemExit(1)
    if payload["counts"]["failed"] or payload["counts"]["partial"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
