#!/usr/bin/env python3
"""Portable TensorRT-LLM preflight and benchmark harness.

The script is intentionally usable on CPU-only machines: without TensorRT-LLM,
CUDA, a built engine, or an explicit command it emits a skipped/preflight JSON
payload instead of failing. Move the repository to a CUDA/TensorRT-LLM machine
and pass either --command for an exact local invocation or --mode bench with
--engine-dir to collect real measurements.
"""

from __future__ import annotations

import argparse
import importlib.metadata
import importlib.util
import math
import platform
import re
import shlex
import shutil
import sys
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

from bounded_process import run_bounded_process
from artifact_json import try_write_json_file
from cli_validation import require_int_range
from command_redaction import (
    artifact_command_parts as shared_artifact_command_parts,
    artifact_path_or_value as artifact_model_value,
    artifact_path_payload as path_payload,
    looks_like_local_path,
    redact_artifact_output,
    redact_path_error,
    redact_sensitive_text,
    redact_value,
    redacted_path_label as path_label,
)
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from path_status import safe_exists as safe_path_exists, safe_is_dir as safe_path_is_dir
from quiet_cli_output import add_quiet_success_argument, emit_success


FLOAT_RE = r"([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)"
LATENCY_RE = re.compile(
    rf"(?i)\b(?:(p50|p90|p95|p99|avg|average|mean)\s+)?latency"
    rf"(?:\s*\([^)]*\))?\s*[:=]\s*{FLOAT_RE}\s*(ms|milliseconds|s|sec|seconds)?"
)
THROUGHPUT_RE = re.compile(
    rf"(?i)\b(?:(token|tokens|request|requests)\s+)?throughput"
    rf"(?:\s*\([^)]*\))?\s*[:=]\s*{FLOAT_RE}\s*([a-zA-Z_/.-]+)?"
)
TOKEN_RATE_RE = re.compile(rf"(?i)\b{FLOAT_RE}\s*(?:tokens/s|tok/s|token/s)\b")
REQUEST_RATE_RE = re.compile(rf"(?i)\b{FLOAT_RE}\s*(?:requests/s|req/s|request/s)\b")
COMMAND_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024


def tail_text(text: str, max_lines: int = 120, max_chars: int = 12000) -> str:
    lines = text.splitlines()
    tail = "\n".join(lines[-max_lines:])
    if len(tail) > max_chars:
        return tail[-max_chars:]
    return tail


def module_probe(module_name: str, package_name: Optional[str] = None) -> Dict[str, Any]:
    result: Dict[str, Any] = {"module": module_name, "available": False, "version": None}
    if importlib.util.find_spec(module_name) is None:
        return result
    result["available"] = True
    try:
        result["version"] = importlib.metadata.version(package_name or module_name)
    except importlib.metadata.PackageNotFoundError:
        result["version"] = None
    except Exception as exc:  # pragma: no cover - defensive metadata guard
        result["version_error"] = redact_path_error(str(exc), Path.cwd())
    return result


def artifact_command_parts(parts: Sequence[str]) -> List[str]:
    return shared_artifact_command_parts(parts, path_options=("--engine_dir", "--engine-dir"))


def tool_probe(name: str, override: str = "", repo_root: Optional[Path] = None) -> Dict[str, Any]:
    candidate = override or name
    path = shutil.which(candidate) if candidate else None
    path_probe_error = ""
    if override:
        override_path = resolve_repo_path(repo_root, override) if repo_root else Path(override)
        override_exists, path_probe_error = safe_path_exists(override_path)
        if override_exists:
            path = str(override_path)
    return {
        "name": name,
        "requested": artifact_path_or_value(candidate),
        **path_payload(path or "missing"),
        "available": bool(path),
        "path_probe_error": redact_path_error(path_probe_error, override_path) if path_probe_error and override else path_probe_error,
    }


def run_command(command: Sequence[str], timeout_seconds: int, cwd: Optional[Path] = None) -> Dict[str, Any]:
    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        launch_error_formatter=lambda exc: redact_path_error(exc, cwd, *command),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    result: Dict[str, Any] = {
        **path_payload(cwd or "", "cwd"),
        "returncode": capture.returncode if capture.status in {"ok", "nonzero_exit"} else None,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_tail": redact_artifact_output(tail_text(stdout), paths=[cwd, *command]),
        "stderr_tail": redact_artifact_output(tail_text(stderr), paths=[cwd, *command]),
        "timed_out": capture.timed_out,
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "stdout_bytes": capture.stdout_bytes,
        "stderr_bytes": capture.stderr_bytes,
    }
    if capture.error:
        result["error"] = capture.error
    if capture.status not in {"ok", "nonzero_exit"}:
        result["error_type"] = {
            "launch_failed": "ProcessLaunchError",
            "capture_failed": "ProcessCaptureError",
            "timed_out": "TimeoutExpired",
            "output_limit_exceeded": "OutputLimitExceeded",
            "invalid_arguments": "InvalidArguments",
        }.get(capture.status, "ProcessExecutionError")
    return result


def nvidia_smi_probe() -> Dict[str, Any]:
    path = shutil.which("nvidia-smi")
    result: Dict[str, Any] = {**path_payload(path or "missing"), "available": bool(path), "gpus": []}
    if not path:
        return result
    completed = run_command(
        [
            "nvidia-smi",
            "--query-gpu=name,memory.total,driver_version",
            "--format=csv,noheader",
        ],
        timeout_seconds=10,
    )
    result["query"] = completed
    if completed.get("returncode") == 0:
        gpus: List[Dict[str, str]] = []
        for line in str(completed.get("stdout_tail", "")).splitlines():
            parts = [part.strip() for part in line.split(",")]
            if len(parts) >= 3:
                gpus.append(
                    {
                        "name": parts[0],
                        "memory_total": parts[1],
                        "driver_version": parts[2],
                    }
                )
        result["gpus"] = gpus
    return result


def torch_cuda_probe() -> Dict[str, Any]:
    result: Dict[str, Any] = {"available": False, "version": None, "cuda_available": False}
    if importlib.util.find_spec("torch") is None:
        return result
    try:
        import torch  # type: ignore

        result["available"] = True
        result["version"] = getattr(torch, "__version__", None)
        result["cuda_available"] = bool(torch.cuda.is_available())
        result["cuda_device_count"] = int(torch.cuda.device_count()) if result["cuda_available"] else 0
        if result["cuda_available"] and torch.cuda.device_count() > 0:
            result["cuda_device_name"] = torch.cuda.get_device_name(0)
    except Exception as exc:  # pragma: no cover - depends on local torch install
        result["error"] = redact_path_error(str(exc), Path.cwd())
    return result


def collect_environment(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    tools = {
        "trtllm_bench": tool_probe("trtllm-bench", args.trtllm_bench, repo_root),
        "trtllm_build": tool_probe("trtllm-build", args.trtllm_build, repo_root),
        "trtllm_serve": tool_probe("trtllm-serve", args.trtllm_serve, repo_root),
        "nvidia_smi": tool_probe("nvidia-smi"),
    }
    return {
        "python": sys.version.split()[0],
        "platform": platform.platform(),
        "machine": platform.machine(),
        "modules": {
            "tensorrt_llm": module_probe("tensorrt_llm"),
            "tensorrt": module_probe("tensorrt"),
            "mpi4py": module_probe("mpi4py"),
        },
        "torch": torch_cuda_probe(),
        "nvidia_smi": nvidia_smi_probe(),
        "tools": tools,
    }


def parse_user_command(command: str) -> Tuple[List[str], Optional[str]]:
    if not command:
        return [], None
    try:
        return shlex.split(command), None
    except ValueError as exc:
        return [], redact_sensitive_text(str(exc))


def build_trtllm_bench_command(args: argparse.Namespace, environment: Dict[str, Any], engine_dir: Path) -> List[str]:
    path = environment["tools"]["trtllm_bench"].get("path")
    if not path:
        return []
    command = [
        str(path),
        "--engine_dir",
        str(engine_dir),
        "--batch_size",
        str(args.batch_size),
        "--input_output_len",
        f"{args.input_tokens},{args.output_tokens}",
    ]
    if args.tokenizer:
        command.extend(["--tokenizer", args.tokenizer])
    for extra_arg in args.extra_arg:
        command.append(extra_arg)
    return command


def normalize_latency_ms(value: float, unit: Optional[str]) -> Optional[float]:
    if not math.isfinite(value) or value < 0:
        return None
    if unit and unit.lower() in {"s", "sec", "seconds"}:
        value *= 1000.0
    if not math.isfinite(value):
        return None
    return round(value, 6)


def parse_nonnegative_metric(value: str) -> Optional[float]:
    parsed = float(value)
    if not math.isfinite(parsed) or parsed < 0:
        return None
    return round(parsed, 6)


def parse_metrics(text: str) -> Dict[str, Any]:
    metrics: Dict[str, Any] = {}
    for match in LATENCY_RE.finditer(text):
        label = (match.group(1) or "latency").lower()
        value = float(match.group(2))
        unit = match.group(3)
        key = {
            "avg": "avg_latency_ms",
            "average": "avg_latency_ms",
            "mean": "avg_latency_ms",
            "p50": "p50_latency_ms",
            "p90": "p90_latency_ms",
            "p95": "p95_latency_ms",
            "p99": "p99_latency_ms",
            "latency": "latency_ms",
        }.get(label, f"{label}_latency_ms")
        normalized = normalize_latency_ms(value, unit)
        if normalized is not None:
            metrics[key] = normalized
    for match in THROUGHPUT_RE.finditer(text):
        label = (match.group(1) or "").lower()
        value = parse_nonnegative_metric(match.group(2))
        if value is None:
            continue
        unit = (match.group(3) or "").lower()
        if "request" in label or "req" in unit:
            metrics["request_throughput_per_s"] = value
        elif "token" in label or "tok" in unit:
            metrics["token_throughput_per_s"] = value
        else:
            metrics["throughput_per_s"] = value
    if "token_throughput_per_s" not in metrics:
        token_match = TOKEN_RATE_RE.search(text)
        if token_match:
            value = parse_nonnegative_metric(token_match.group(1))
            if value is not None:
                metrics["token_throughput_per_s"] = value
    if "request_throughput_per_s" not in metrics:
        request_match = REQUEST_RATE_RE.search(text)
        if request_match:
            value = parse_nonnegative_metric(request_match.group(1))
            if value is not None:
                metrics["request_throughput_per_s"] = value
    return metrics


def build_preflight(args: argparse.Namespace, environment: Dict[str, Any], command: Sequence[str], engine_dir: Path) -> Dict[str, Any]:
    reasons: List[str] = []
    engine_exists, engine_probe_error = safe_path_is_dir(engine_dir) if args.engine_dir else (False, "")
    trtllm_available = bool(environment["modules"]["tensorrt_llm"].get("available"))
    trt_available = bool(environment["modules"]["tensorrt"].get("available"))
    gpu_available = bool(environment["torch"].get("cuda_available")) or bool(
        environment["nvidia_smi"].get("gpus")
    )
    bench_available = bool(environment["tools"]["trtllm_bench"].get("available"))

    if not trtllm_available:
        reasons.append("Python module tensorrt_llm is not importable")
    if not trt_available:
        reasons.append("Python module tensorrt is not importable")
    if not gpu_available:
        reasons.append("No CUDA GPU detected by torch or nvidia-smi")
    if args.mode == "bench" and not bench_available:
        reasons.append("trtllm-bench executable is not available")
    if args.mode == "bench" and not engine_exists:
        reasons.append("--engine-dir probe failed" if engine_probe_error else "--engine-dir is missing or does not exist")
    if args.mode == "command" and not command:
        reasons.append("--command is required in command mode")
    if args.mode == "auto" and not command and not (bench_available and engine_exists):
        reasons.append("No --command was provided and no runnable trtllm-bench + engine directory was found")

    can_run_trtllm_bench = bool(
        bench_available
        and engine_exists
        and trtllm_available
        and trt_available
        and gpu_available
    )
    return {
        "can_run_command": bool(command),
        "can_run_trtllm_bench": can_run_trtllm_bench,
        "engine_dir_exists": engine_exists,
        "engine_probe_error": engine_probe_error,
        "gpu_available": gpu_available,
        "tensorrt_llm_available": trtllm_available,
        "tensorrt_available": trt_available,
        "reasons": reasons,
    }


def resolve_command(args: argparse.Namespace, environment: Dict[str, Any], engine_dir: Path) -> Tuple[List[str], Optional[str], str]:
    command, parse_error = parse_user_command(args.command)
    if parse_error:
        return [], parse_error, "command"
    if args.mode == "command":
        return command, None, "command"
    if args.mode == "bench":
        return build_trtllm_bench_command(args, environment, engine_dir), None, "bench"
    if command:
        return command, None, "command"
    bench_command = build_trtllm_bench_command(args, environment, engine_dir)
    if args.engine_dir and bench_command:
        return bench_command, None, "bench"
    return [], None, "preflight"


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    environment = collect_environment(args, repo_root)
    engine_dir = resolve_repo_path(repo_root, args.engine_dir) if args.engine_dir else Path()
    command, command_error, effective_mode = resolve_command(args, environment, engine_dir)
    preflight = build_preflight(args, environment, command, engine_dir)
    run: Dict[str, Any] = {}
    metrics: Dict[str, Any] = {}
    status = "skipped"

    if command_error:
        status = "error"
        preflight["reasons"].append(f"Unable to parse --command: {command_error}")
    elif args.mode == "preflight" or not command:
        status = "ok" if args.mode == "preflight" else "skipped"
    elif effective_mode == "bench" and not preflight["can_run_trtllm_bench"]:
        status = "skipped"
    else:
        run = run_command(command, args.timeout_seconds, cwd=repo_root)
        combined = "\n".join([str(run.get("stdout_tail", "")), str(run.get("stderr_tail", ""))])
        metrics = parse_metrics(combined)
        if run.get("returncode") == 0 and not run.get("timed_out") and metrics:
            status = "ok"
        elif run.get("returncode") == 0 and not run.get("timed_out"):
            status = "partial_no_metrics"
        else:
            status = "failed"

    summary = dict(metrics)
    if run:
        summary["returncode"] = run.get("returncode")
        summary["elapsed_ms"] = run.get("elapsed_ms")
        summary["metric_status"] = "parsed_metrics" if metrics else "missing_metrics"

    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "status": status,
        "mode": effective_mode,
        "requested_mode": args.mode,
        "model": artifact_model_value(args.model),
        "model_path_redacted": looks_like_local_path(args.model),
        **path_payload(args.engine_dir, "engine_dir", "engine_dir_path_redacted"),
        **path_payload(engine_dir if args.engine_dir else "", "resolved_engine_dir"),
        "environment": environment,
        "preflight": preflight,
        "config": {
            "batch_size": args.batch_size,
            "input_tokens": args.input_tokens,
            "output_tokens": args.output_tokens,
            "timeout_seconds": args.timeout_seconds,
            "tokenizer": artifact_model_value(args.tokenizer),
            "tokenizer_path_redacted": looks_like_local_path(args.tokenizer),
            "extra_arg": artifact_command_parts(args.extra_arg),
        },
        "command": artifact_command_parts(command),
        "run": run,
        "metrics": metrics,
        "summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    add_quiet_success_argument(parser)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--mode", choices=("auto", "preflight", "bench", "command"), default="auto")
    parser.add_argument("--command", default="", help="Exact TensorRT-LLM benchmark command to run.")
    parser.add_argument("--trtllm-bench", default="", help="Path or executable name for trtllm-bench.")
    parser.add_argument("--trtllm-build", default="", help="Path or executable name for trtllm-build.")
    parser.add_argument("--trtllm-serve", default="", help="Path or executable name for trtllm-serve.")
    parser.add_argument("--engine-dir", default="", help="Built TensorRT-LLM engine directory.")
    parser.add_argument("--model", default="", help="Model identifier recorded in the output metadata.")
    parser.add_argument("--tokenizer", default="", help="Tokenizer path/identifier for trtllm-bench when needed.")
    parser.add_argument("--batch-size", type=int, default=1)
    parser.add_argument("--input-tokens", type=int, default=128)
    parser.add_argument("--output-tokens", type=int, default=32)
    parser.add_argument("--timeout-seconds", type=int, default=300)
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="Extra argument appended to the best-effort trtllm-bench command. Repeat as needed.",
    )
    parser.add_argument("--output", default="results/raw/trtllm_benchmark.json")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--batch-size", args.batch_size, 1, 1_000_000)
    require_int_range("--input-tokens", args.input_tokens, 1, 10_000_000)
    require_int_range("--output-tokens", args.output_tokens, 1, 10_000_000)
    require_int_range("--timeout-seconds", args.timeout_seconds, 1, 86_400)


def main() -> int:
    args = parse_args()
    validate_args(args)
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_status = try_write_json_file(output, payload)
    if write_status["status"] != "ok":
        print(f"failed to write {path_label(output)}: {write_status.get('error')}")
        return 1
    emit_success(
        f"wrote {path_label(output)} status={payload['status']} mode={payload['mode']}",
        quiet=args.quiet,
    )
    return 1 if payload["status"] == "error" else 0


if __name__ == "__main__":
    raise SystemExit(main())
