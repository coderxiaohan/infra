#!/usr/bin/env python3
"""Preflight or run a llama.cpp GGUF benchmark and parse timing logs."""

from __future__ import annotations

import argparse
import json
import platform
import re
import shutil
from time_utils import utc_now_precise as now_utc
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from bounded_process import run_bounded_process
from artifact_json import artifact_status_exit_code, try_write_json_file
from cli_validation import require_float_range, require_int_range
from command_redaction import (
    artifact_command_content_values,
    artifact_command_parts as shared_artifact_command_parts,
    artifact_path_payload as path_payload,
    redact_artifact_output,
    redact_path_error,
    redact_sensitive_text,
    redacted_path_label as path_label,
)
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from path_status import safe_is_file
from quiet_cli_output import add_quiet_success_argument, emit_success


DEFAULT_PROMPT = "Explain why KV cache changes LLM decode performance in one sentence."
LLAMA_CPP_MERGED_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024
TIMING_RE = re.compile(
    r"(?P<label>load time|prompt eval time|eval time|total time)\s*=\s*"
    r"(?P<ms>[0-9.]+)\s*ms"
    r"(?:\s*/\s*(?P<tokens>[0-9]+)\s*tokens?)?"
    r"(?:\s*\(\s*(?P<ms_per_token>[0-9.]+)\s*ms per token,\s*"
    r"(?P<tokens_per_second>[0-9.]+)\s*tokens per second\s*\))?",
    re.IGNORECASE,
)
SUMMARY_RATE_RE = re.compile(
    r"\[\s*Prompt:\s*(?P<prompt>[0-9.]+)\s*t/s\s*\|\s*"
    r"Generation:\s*(?P<generation>[0-9.]+)\s*t/s\s*\]",
    re.IGNORECASE,
)
BENCH_RATE_RE = re.compile(r"(?P<rate>[0-9]+(?:\.[0-9]+)?)")
BENCH_TEST_RE = re.compile(r"^(?P<kind>pp|tg)(?P<tokens>[0-9]+)?$", re.IGNORECASE)


def artifact_command_parts(parts: List[str]) -> List[str]:
    return shared_artifact_command_parts(
        parts,
        path_options=("-m", "--model", "--model-path"),
        content_options=("-p", "--prompt"),
    )


def artifact_output_tail(text: str, command: List[str], cwd: Optional[Path], max_lines: int) -> str:
    tail = "\n".join(str(text).splitlines()[-max_lines:])
    return redact_artifact_output(
        tail,
        paths=[cwd, *command],
        contents=artifact_command_content_values(command, content_options=("-p", "--prompt")),
    )


def resolve_tool(value: str, candidates: List[str], repo_root: Path) -> Optional[str]:
    if value:
        path = resolve_repo_path(repo_root, value)
        is_file, _ = safe_is_file(path)
        if is_file:
            return str(path)
        resolved = shutil.which(value)
        return resolved
    for candidate in candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    return None


def run_command(command: List[str], timeout_seconds: float, cwd: Optional[Path] = None) -> Dict[str, Any]:
    missing_executable = False
    launch_error_type = ""

    def format_launch_error(error: BaseException) -> str:
        nonlocal launch_error_type, missing_executable
        if isinstance(error, FileNotFoundError):
            missing_executable = True
            return "missing_executable"
        launch_error_type = type(error).__name__
        return redact_path_error(error, cwd, *command) if cwd else redact_path_error(error, *command)

    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=LLAMA_CPP_MERGED_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=LLAMA_CPP_MERGED_OUTPUT_LIMIT_BYTES,
        launch_error_formatter=format_launch_error,
        merge_stderr_into_stdout=True,
    )
    output = capture.stdout.decode("utf-8", errors="replace")
    result = {
        **path_payload(cwd or "", "cwd"),
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "output_tail": artifact_output_tail(output, command, cwd, 40 if capture.timed_out else 80),
        "_raw_output_for_parse": output,
    }
    if capture.status in {"ok", "nonzero_exit"}:
        return result
    result["returncode"] = None
    if capture.status == "timed_out":
        result["timeout"] = True
        return result
    if capture.status == "launch_failed" and missing_executable:
        result["output_tail"] = "missing executable"
        return result

    result["error_type"] = launch_error_type or capture.status
    if not result["output_tail"]:
        result["output_tail"] = redact_sensitive_text(capture.error or capture.status)
    return result


def version_probe(tool: Optional[str], repo_root: Path) -> Dict[str, Any]:
    if not tool:
        return {"available": False, "path": None}
    result = run_command([tool, "--version"], timeout_seconds=5.0, cwd=repo_root)
    return {
        "available": result.get("returncode") == 0,
        **path_payload(tool),
        "returncode": result.get("returncode"),
        "output_tail": result.get("output_tail"),
    }


def load_prompt(args: argparse.Namespace, repo_root: Path) -> Tuple[str, Dict[str, Any]]:
    if args.prompt:
        return args.prompt, {"source": "argument", "path": None}
    if args.prompt_file:
        path = resolve_repo_path(repo_root, args.prompt_file)
        is_file, probe_error = safe_is_file(path)
        if not is_file:
            reason = "path_probe_failed" if probe_error else "missing file"
            meta: Dict[str, Any] = {"source": "default", **path_payload(path), "reason": reason}
            if probe_error:
                meta["error"] = probe_error
            return DEFAULT_PROMPT, meta
        try:
            with path.open("r", encoding="utf-8") as file:
                invalid_rows: List[Dict[str, Any]] = []
                for line_number, line in enumerate(file, start=1):
                    stripped = line.strip()
                    if not stripped:
                        continue
                    if stripped.startswith("{"):
                        try:
                            row = json.loads(stripped)
                        except json.JSONDecodeError as exc:
                            invalid_rows.append(
                                {
                                    "line": line_number,
                                    "status": "invalid_prompt_json",
                                    "error": redact_sensitive_text(str(exc)),
                                }
                            )
                            continue
                        if not isinstance(row, dict):
                            invalid_rows.append({"line": line_number, "status": "invalid_prompt_row", "value_type": type(row).__name__})
                            continue
                        prompt = str(row.get("prompt", ""))
                    else:
                        prompt = stripped
                    if prompt:
                        meta = {
                            "source": "file",
                            **path_payload(path),
                            "line": line_number,
                        }
                        if invalid_rows:
                            meta["status"] = "partial_prompt_file"
                            meta["errors"] = invalid_rows
                        return prompt, meta
        except OSError as exc:
            return DEFAULT_PROMPT, {"source": "default", **path_payload(path), "reason": "unreadable_prompt_file", "error": redact_path_error(exc, path)}
        except UnicodeDecodeError as exc:
            return DEFAULT_PROMPT, {"source": "default", **path_payload(path), "reason": "invalid_prompt_encoding", "error": redact_path_error(exc, path)}
        return DEFAULT_PROMPT, {"source": "default", **path_payload(path), "reason": "empty file"}
    return DEFAULT_PROMPT, {"source": "default", "path": None}


def parse_llama_timings(output: str) -> Dict[str, Any]:
    timings: Dict[str, Any] = {}
    for match in TIMING_RE.finditer(output):
        label = match.group("label").lower().replace(" ", "_")
        entry: Dict[str, Any] = {"ms": float(match.group("ms"))}
        if match.group("tokens") is not None:
            entry["tokens"] = int(match.group("tokens"))
        if match.group("ms_per_token") is not None:
            entry["ms_per_token"] = float(match.group("ms_per_token"))
        if match.group("tokens_per_second") is not None:
            entry["tokens_per_second"] = float(match.group("tokens_per_second"))
        timings[label] = entry
    return timings


def parse_llama_summary_rates(output: str) -> Dict[str, float]:
    match = SUMMARY_RATE_RE.search(output)
    if not match:
        return {}
    return {
        "prompt_eval_tokens_per_s": float(match.group("prompt")),
        "eval_tokens_per_s": float(match.group("generation")),
    }


def parse_llama_bench_rates(output: str) -> Dict[str, Any]:
    rates: Dict[str, Any] = {}
    for line in output.splitlines():
        stripped = line.strip()
        if not stripped.startswith("|") or stripped.startswith("|---") or stripped.startswith("| model"):
            continue
        cells = [cell.strip() for cell in stripped.strip("|").split("|")]
        if len(cells) < 7:
            continue
        test = cells[5]
        rate_cell = cells[6]
        test_match = BENCH_TEST_RE.match(test)
        rate_match = BENCH_RATE_RE.search(rate_cell)
        if not test_match or not rate_match:
            continue
        key_prefix = "prompt_eval" if test_match.group("kind").lower() == "pp" else "eval"
        rates[f"{key_prefix}_tokens_per_s"] = float(rate_match.group("rate"))
        if test_match.group("tokens"):
            rates[f"{key_prefix}_tokens"] = int(test_match.group("tokens"))
    return rates


def build_cli_command(args: argparse.Namespace, llama_cli: str, prompt: str, model_path: Path) -> List[str]:
    command = [
        llama_cli,
        "-m",
        str(model_path),
        "-p",
        prompt,
        "-n",
        str(args.n_predict),
        "-c",
        str(args.ctx_size),
        "-b",
        str(args.batch_size),
        "-t",
        str(args.threads),
    ]
    if args.ubatch_size > 0:
        command.extend(["-ub", str(args.ubatch_size)])
    if args.ngl is not None:
        command.extend(["-ngl", str(args.ngl)])
    command.extend(args.extra_arg)
    return command


def build_bench_command(args: argparse.Namespace, llama_bench: str, model_path: Path) -> List[str]:
    command = [
        llama_bench,
        "-m",
        str(model_path),
        "-p",
        str(args.prompt_tokens),
        "-n",
        str(args.n_predict),
        "-t",
        str(args.threads),
    ]
    if args.ngl is not None:
        command.extend(["-ngl", str(args.ngl)])
    command.extend(args.bench_extra_arg)
    return command


def collect_environment(llama_cli: Optional[str], llama_bench: Optional[str], repo_root: Path) -> Dict[str, Any]:
    return {
        "timestamp_utc": now_utc(),
        "system": platform.system(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python": platform.python_version(),
        "tools": {
            "llama_cli": version_probe(llama_cli, repo_root),
            "llama_bench": version_probe(llama_bench, repo_root),
            "perf": {**path_payload(shutil.which("perf") or "missing"), "available": bool(shutil.which("perf"))},
        },
    }


def skipped_payload(
    args: argparse.Namespace,
    repo_root: Path,
    environment: Dict[str, Any],
    reasons: List[str],
    prompt_meta: Dict[str, Any],
    model_path: Path,
    model_probe_error: str = "",
) -> Dict[str, Any]:
    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **path_payload(repo_root, "repo_root"),
        "status": "skipped",
        "mode": args.mode,
        "backend_label": args.backend_label,
        **path_payload(args.model, "model", "model_path_redacted"),
        **path_payload(model_path if args.model else "", "resolved_model"),
        "prompt": prompt_meta,
        "environment": environment,
        "preflight": {
            "can_run_cli": False,
            "can_run_bench": False,
            "reasons": reasons,
            "model_probe_error": model_probe_error,
        },
        "summary": {},
        "command": [],
        "run": {},
    }


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    llama_cli = resolve_tool(args.llama_cli, ["llama-cli", "main"], repo_root)
    llama_bench = resolve_tool(args.llama_bench, ["llama-bench"], repo_root)
    prompt, prompt_meta = load_prompt(args, repo_root)
    environment = collect_environment(llama_cli, llama_bench, repo_root)

    reasons: List[str] = []
    model_path = resolve_repo_path(repo_root, args.model) if args.model else Path()
    model_is_file, model_probe_error = safe_is_file(model_path) if args.model else (False, "")
    model_exists = bool(args.model and model_is_file)
    can_run_cli = bool(llama_cli and model_exists)
    can_run_bench = bool(llama_bench and model_exists)
    if not args.model:
        reasons.append("model path not provided")
    elif not model_exists:
        reasons.append("model path probe failed" if model_probe_error else "model file missing")
    if not llama_cli:
        reasons.append(f"llama-cli missing: {args.llama_cli}" if args.llama_cli else "llama-cli missing")
    if args.mode == "bench" and not llama_bench:
        reasons.append(f"llama-bench missing: {args.llama_bench}" if args.llama_bench else "llama-bench missing")
    if args.mode == "preflight":
        reasons.append("preflight mode requested")
    if args.mode == "auto" and not (can_run_cli or can_run_bench):
        return skipped_payload(args, repo_root, environment, reasons, prompt_meta, model_path, model_probe_error)
    if args.mode == "cli" and not can_run_cli:
        return skipped_payload(args, repo_root, environment, reasons, prompt_meta, model_path, model_probe_error)
    if args.mode == "bench" and not can_run_bench:
        return skipped_payload(args, repo_root, environment, reasons, prompt_meta, model_path, model_probe_error)
    if args.mode == "preflight":
        return skipped_payload(args, repo_root, environment, reasons, prompt_meta, model_path, model_probe_error)

    run_mode = args.mode
    if run_mode == "auto":
        run_mode = "cli" if can_run_cli else "bench"
    command = (
        build_cli_command(args, str(llama_cli), prompt, model_path)
        if run_mode == "cli"
        else build_bench_command(args, str(llama_bench), model_path)
    )
    run = run_command(command, timeout_seconds=args.timeout_seconds, cwd=repo_root)
    output = str(run.pop("_raw_output_for_parse", run.get("output_tail", "")))
    timings = parse_llama_timings(output)
    summary_rates = parse_llama_summary_rates(output) or parse_llama_bench_rates(output)
    has_key_timing = bool(timings.get("eval_time") or timings.get("total_time"))
    has_key_metric = bool(has_key_timing or summary_rates)
    if run.get("returncode") == 0 and has_key_metric:
        status = "ok"
    elif run.get("returncode") == 0:
        status = "partial_no_timing"
    else:
        status = "failed"
    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **path_payload(repo_root, "repo_root"),
        "status": status,
        "mode": run_mode,
        "backend_label": args.backend_label,
        **path_payload(args.model, "model", "model_path_redacted"),
        **path_payload(model_path if args.model else "", "resolved_model"),
        "prompt": prompt_meta,
        "config": {
            "threads": args.threads,
            "ctx_size": args.ctx_size,
            "batch_size": args.batch_size,
            "ubatch_size": args.ubatch_size,
            "n_predict": args.n_predict,
            "prompt_tokens": args.prompt_tokens,
            "ngl": args.ngl,
        },
        "environment": environment,
        "preflight": {
            "can_run_cli": can_run_cli,
            "can_run_bench": can_run_bench,
            "reasons": reasons,
            "model_probe_error": model_probe_error,
        },
        "command": artifact_command_parts(command),
        "run": run,
        "timings": timings,
        "summary_rates": summary_rates,
        "generated_text": "",
        "generated_text_source": "not_extracted_from_mixed_llama_cpp_stdout",
        "metric_status": "parsed_timing" if has_key_timing else "parsed_summary_rate" if summary_rates else "missing_timing",
        "summary": {
            "load_ms": timings.get("load_time", {}).get("ms"),
            "prompt_eval_ms": timings.get("prompt_eval_time", {}).get("ms"),
            "prompt_eval_tokens": timings.get("prompt_eval_time", {}).get("tokens")
            or summary_rates.get("prompt_eval_tokens"),
            "prompt_eval_tokens_per_s": timings.get("prompt_eval_time", {}).get("tokens_per_second")
            or summary_rates.get("prompt_eval_tokens_per_s"),
            "eval_ms": timings.get("eval_time", {}).get("ms"),
            "eval_tokens": timings.get("eval_time", {}).get("tokens")
            or summary_rates.get("eval_tokens"),
            "eval_tokens_per_s": timings.get("eval_time", {}).get("tokens_per_second")
            or summary_rates.get("eval_tokens_per_s"),
            "total_ms": timings.get("total_time", {}).get("ms"),
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    add_quiet_success_argument(parser)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--mode", choices=["auto", "preflight", "cli", "bench"], default="auto")
    parser.add_argument("--llama-cli", default="", help="Path/name for llama.cpp llama-cli or legacy main.")
    parser.add_argument("--llama-bench", default="", help="Path/name for llama-bench.")
    parser.add_argument("--model", default="", help="Path to a legal GGUF model file.")
    parser.add_argument("--prompt", default="")
    parser.add_argument("--prompt-file", default="configs/eval_prompts.jsonl")
    parser.add_argument("--backend-label", default="llama.cpp")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--ctx-size", type=int, default=2048)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--ubatch-size", type=int, default=0)
    parser.add_argument("--n-predict", type=int, default=64)
    parser.add_argument("--prompt-tokens", type=int, default=128)
    parser.add_argument("--ngl", type=int, default=None)
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    parser.add_argument("--extra-arg", action="append", default=[])
    parser.add_argument("--bench-extra-arg", action="append", default=[])
    parser.add_argument("--output", default="results/raw/llama_cpp_benchmark.json")
    parser.add_argument(
        "--allow-non-ok-status",
        action="store_true",
        help="Return zero after writing a non-ok diagnostic artifact.",
    )
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--threads", args.threads, 1, 4096)
    require_int_range("--ctx-size", args.ctx_size, 1, 10000000)
    require_int_range("--batch-size", args.batch_size, 1, 10000000)
    require_int_range("--ubatch-size", args.ubatch_size, 0, 10000000)
    require_int_range("--n-predict", args.n_predict, 1, 10000000)
    require_int_range("--prompt-tokens", args.prompt_tokens, 1, 10000000)
    if args.ngl is not None:
        require_int_range("--ngl", args.ngl, -1, 1000000)
    require_float_range("--timeout-seconds", args.timeout_seconds, 0.001, 86400.0)


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
    emit_success(path_label(output), quiet=args.quiet)
    return artifact_status_exit_code(payload, allow_non_ok=args.allow_non_ok_status)


if __name__ == "__main__":
    raise SystemExit(main())
