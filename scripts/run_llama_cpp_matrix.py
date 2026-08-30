#!/usr/bin/env python3
"""Plan or execute a llama.cpp matrix across quant, thread, context, batch, and offload settings."""

from __future__ import annotations

import argparse
import itertools
import json
import shlex
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

from bounded_process import command_result_succeeded as command_ok, run_bounded_process
from cli_validation import require_int_range
from command_redaction import (
    artifact_command_parts as shared_artifact_command_parts,
    artifact_output_tail,
    artifact_path_payload as path_payload,
    command_line,
    redact_path_error,
    redact_sensitive_text,
    redacted_path_label as path_label,
)
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import try_write_text_file as write_text_file

try:
    from scripts.path_status import safe_exists
except ModuleNotFoundError:
    from path_status import safe_exists


LLAMA_CPP_MATRIX_STREAM_LIMIT_BYTES = 16 * 1024 * 1024


def artifact_command_parts(parts: Sequence[str]) -> List[str]:
    return shared_artifact_command_parts(
        parts,
        path_options=("--model", "--output", "--prompt-file", "--llama-cli", "--llama-bench"),
    )


def artifact_row(row: Dict[str, Any]) -> Dict[str, Any]:
    safe = dict(row)
    if safe.get("model"):
        safe.update(path_payload(str(safe["model"]), "model", "model_redacted"))
    return safe


def artifact_matrix(matrix: Dict[str, Any]) -> Dict[str, Any]:
    safe = dict(matrix)
    models = []
    for model in matrix.get("models", []):
        safe_model = dict(model)
        if safe_model.get("path"):
            safe_model.update(path_payload(str(safe_model["path"])))
        models.append(safe_model)
    safe["models"] = models
    return safe


def run(command: Sequence[str], timeout: int, cwd: Path | None = None) -> Dict[str, Any]:
    command_parts = [str(part) for part in command]
    redacted_command = artifact_command_parts(command_parts)
    capture = run_bounded_process(
        command_parts,
        cwd=cwd,
        timeout_seconds=float(timeout),
        stdout_limit_bytes=LLAMA_CPP_MATRIX_STREAM_LIMIT_BYTES,
        stderr_limit_bytes=LLAMA_CPP_MATRIX_STREAM_LIMIT_BYTES,
        launch_error_formatter=lambda exc: (
            "missing_executable"
            if isinstance(exc, FileNotFoundError)
            else redact_path_error(exc, cwd, *command_parts)
        ),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    result = {
        "command": redacted_command,
        **path_payload(cwd or "", "cwd"),
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_tail": artifact_output_tail(stdout, command_parts, cwd, 40),
        "stderr_tail": artifact_output_tail(stderr, command_parts, cwd, 40),
        "timed_out": capture.timed_out,
    }
    if capture.status in {"ok", "nonzero_exit"}:
        return result
    result["returncode"] = None
    if capture.status == "launch_failed" and capture.error == "missing_executable":
        result["elapsed_ms"] = 0
        result["stderr_tail"] = "missing executable"
        result["error"] = "missing_executable"
    elif capture.status == "timed_out":
        result["error"] = redact_sensitive_text(f"timed out after {timeout}s")
    else:
        diagnostic = redact_sensitive_text(capture.error or capture.status)
        if not result["stderr_tail"]:
            result["stderr_tail"] = diagnostic
        result["error"] = diagnostic
    return result


def default_matrix() -> Dict[str, Any]:
    return {
        "models": [{"name": "model", "path": "/models/model.gguf", "quant": "unknown"}],
        "threads": [1, 2, 4, 8],
        "ctx_size": [2048, 4096],
        "batch_size": [128, 512],
        "ngl": [0],
        "n_predict": [128],
        "prompt_file": "configs/eval_prompts.jsonl",
    }


def matrix_schema_error(path: Path, field: str, message: str, value: Any) -> Dict[str, Any]:
    return {
        "status": "invalid_matrix_config_schema",
        "error": message,
        "field": field,
        **path_payload(path),
        "value_type": type(value).__name__,
    }


def matrix_string_value(path: Path, field: str, value: Any, allow_empty: bool = False) -> Tuple[Optional[str], Optional[Dict[str, Any]]]:
    if isinstance(value, bool) or not isinstance(value, str):
        return None, matrix_schema_error(path, field, f"{field} must be a string", value)
    parsed = value.strip()
    if not allow_empty and not parsed:
        return None, matrix_schema_error(path, field, f"{field} must be a non-empty string", value)
    return parsed, None


def matrix_int_value(path: Path, field: str, value: Any, minimum: int, maximum: int) -> Tuple[Optional[int], Optional[Dict[str, Any]]]:
    if isinstance(value, bool):
        return None, matrix_schema_error(path, field, f"{field} must be an integer in [{minimum}, {maximum}]", value)
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, float) and value.is_integer():
        parsed = int(value)
    else:
        return None, matrix_schema_error(path, field, f"{field} must be an integer in [{minimum}, {maximum}]", value)
    if parsed < minimum or parsed > maximum:
        return None, matrix_schema_error(path, field, f"{field} must be an integer in [{minimum}, {maximum}]", value)
    return parsed, None


def matrix_int_list(
    path: Path,
    matrix: Dict[str, Any],
    field: str,
    default_values: List[int],
    minimum: int,
    maximum: int,
) -> Tuple[Optional[List[int]], Optional[Dict[str, Any]]]:
    raw = matrix.get(field, default_values)
    if not isinstance(raw, list) or not raw:
        return None, matrix_schema_error(path, field, f"{field} must be a non-empty integer list", raw)
    values: List[int] = []
    for index, item in enumerate(raw, start=1):
        parsed, error = matrix_int_value(path, f"{field}[{index}]", item, minimum, maximum)
        if error is not None:
            return None, error
        values.append(int(parsed))
    return values, None


def normalize_matrix_config(matrix: Dict[str, Any], path: Path) -> Tuple[Dict[str, Any], Optional[Dict[str, Any]]]:
    defaults = default_matrix()
    raw_models = matrix.get("models", defaults["models"])
    if not isinstance(raw_models, list) or not raw_models:
        return {}, matrix_schema_error(path, "models", "models must be a non-empty object list", raw_models)
    models: List[Dict[str, str]] = []
    seen_model_names: set[str] = set()
    for index, model in enumerate(raw_models, start=1):
        if not isinstance(model, dict):
            return {}, matrix_schema_error(path, f"models[{index}]", f"models[{index}] must be an object", model)
        name, error = matrix_string_value(path, f"models[{index}].name", model.get("name", f"model_{index}"))
        if error is not None:
            return {}, error
        if name in seen_model_names:
            return {}, matrix_schema_error(path, f"models[{index}].name", f"duplicate model name {name!r}", name)
        seen_model_names.add(str(name))
        model_path, error = matrix_string_value(path, f"models[{index}].path", model.get("path", ""))
        if error is not None:
            return {}, error
        quant, error = matrix_string_value(path, f"models[{index}].quant", model.get("quant", "unknown"))
        if error is not None:
            return {}, error
        models.append({"name": str(name), "path": str(model_path), "quant": str(quant)})

    threads, error = matrix_int_list(path, matrix, "threads", defaults["threads"], 1, 4096)
    if error is not None:
        return {}, error
    ctx_size, error = matrix_int_list(path, matrix, "ctx_size", defaults["ctx_size"], 1, 10_000_000)
    if error is not None:
        return {}, error
    batch_size, error = matrix_int_list(path, matrix, "batch_size", defaults["batch_size"], 1, 10_000_000)
    if error is not None:
        return {}, error
    ngl, error = matrix_int_list(path, matrix, "ngl", defaults["ngl"], -1, 1_000_000)
    if error is not None:
        return {}, error
    n_predict, error = matrix_int_list(path, matrix, "n_predict", defaults["n_predict"], 1, 10_000_000)
    if error is not None:
        return {}, error
    prompt_file, error = matrix_string_value(path, "prompt_file", matrix.get("prompt_file", defaults["prompt_file"]), allow_empty=True)
    if error is not None:
        return {}, error

    return {
        "models": models,
        "threads": threads,
        "ctx_size": ctx_size,
        "batch_size": batch_size,
        "ngl": ngl,
        "n_predict": n_predict,
        "prompt_file": prompt_file,
    }, None


def load_matrix(path: Path) -> Tuple[Dict[str, Any], Optional[Dict[str, Any]]]:
    exists, exists_error = safe_exists(path)
    if exists_error:
        return {}, {"status": "matrix_config_probe_failed", "error": redact_path_error(exists_error, path), **path_payload(path)}
    if not exists:
        return default_matrix(), None
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        return {}, {"status": "unreadable_matrix_config", "error": redact_path_error(exc, path), **path_payload(path)}
    except UnicodeDecodeError as exc:
        return {}, {"status": "invalid_matrix_config_encoding", "error": redact_path_error(exc, path), **path_payload(path)}
    except json.JSONDecodeError as exc:
        return {}, {"status": "invalid_matrix_config_json", "error": redact_path_error(exc, path), **path_payload(path)}
    if not isinstance(payload, dict):
        return {}, {
            "status": "invalid_matrix_config_schema",
            "error": "matrix config root must be a JSON object",
            **path_payload(path),
            "value_type": type(payload).__name__,
        }
    return normalize_matrix_config(payload, path)


def load_child_status(path: Path) -> str:
    exists, exists_error = safe_exists(path)
    if exists_error:
        return "artifact_probe_failed"
    if not exists:
        return "missing_artifact"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError:
        return "unreadable_json"
    except UnicodeDecodeError:
        return "invalid_encoding"
    except json.JSONDecodeError:
        return "invalid_json"
    if not isinstance(payload, dict):
        return "invalid_artifact"
    status = payload.get("status")
    return str(status) if isinstance(status, str) and status else "missing_status"


def aggregate_status(records: List[Dict[str, Any]], executed: bool) -> str:
    if not executed:
        return "planned"
    statuses = [str(item.get("status", "missing")) for item in records]
    if statuses and all(status == "ok" for status in statuses):
        return "ok"
    if any(status == "failed" for status in statuses):
        return "failed"
    if any(status == "timeout" for status in statuses):
        return "failed"
    if any(status.startswith("partial") for status in statuses):
        return "partial"
    return "missing" if not statuses else "failed"


def scenarios(matrix: Dict[str, Any]) -> List[Dict[str, Any]]:
    models = matrix.get("models") or [{"name": "model", "path": "/models/model.gguf", "quant": "unknown"}]
    rows = []
    for model, threads, ctx, batch, ngl, n_predict in itertools.product(
        models,
        matrix.get("threads", [4]),
        matrix.get("ctx_size", [2048]),
        matrix.get("batch_size", [512]),
        matrix.get("ngl", [0]),
        matrix.get("n_predict", [128]),
    ):
        name = f"{model.get('name','model')}_t{threads}_c{ctx}_b{batch}_ngl{ngl}_n{n_predict}"
        rows.append({
            "name": name.replace("/", "_"),
            "model": model.get("path", ""),
            "model_name": model.get("name", "model"),
            "quant": model.get("quant", "unknown"),
            "threads": threads,
            "ctx_size": ctx,
            "batch_size": batch,
            "ngl": ngl,
            "n_predict": n_predict,
        })
    return rows


def command_for(args: argparse.Namespace, row: Dict[str, Any], output_path: Path, prompt_file: str) -> List[str]:
    command = [
        "python3",
        "scripts/run_llama_cpp_benchmark.py",
        "--repo-root",
        ".",
        "--mode",
        args.mode,
        "--model",
        row["model"],
        "--threads",
        str(row["threads"]),
        "--ctx-size",
        str(row["ctx_size"]),
        "--batch-size",
        str(row["batch_size"]),
        "--n-predict",
        str(row["n_predict"]),
        "--output",
        str(output_path),
    ]
    if row.get("ngl") is not None:
        command.extend(["--ngl", str(row["ngl"])])
    if prompt_file:
        command.extend(["--prompt-file", prompt_file])
    if args.llama_cli:
        command.extend(["--llama-cli", args.llama_cli])
    if args.llama_bench:
        command.extend(["--llama-bench", args.llama_bench])
    return command


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    matrix_path = resolve_repo_path(repo_root, args.matrix)
    matrix, matrix_error = load_matrix(matrix_path)
    run_dir = resolve_repo_path(repo_root, args.run_dir)
    if matrix_error is not None:
        return {
            "schema_version": 1,
            "timestamp_utc": utc_now(),
            **path_payload(repo_root, "repo_root"),
            "status": "invalid_matrix_config",
            "executed": False,
            **path_payload(args.matrix, "matrix_path", "matrix_path_redacted"),
            **path_payload(matrix_path, "resolved_matrix_path"),
            **path_payload(args.run_dir, "run_dir", "run_dir_path_redacted"),
            **path_payload(run_dir, "resolved_run_dir"),
            "matrix": artifact_matrix(matrix),
            "matrix_config_error": matrix_error,
            "summary": {
                "scenario_count": 0,
                "executed_count": 0,
                "ok_count": 0,
                "partial_count": 0,
                "failed_count": 0,
            },
            "records": [],
        }
    rows = scenarios(matrix)
    records = []
    for row in rows:
        output_path = run_dir / f"{row['name']}.json"
        command = command_for(args, row, output_path, str(matrix.get("prompt_file", "")))
        safe_command = artifact_command_parts(command)
        record = {
            "scenario": artifact_row(row),
            **path_payload(output_path, "output", "output_redacted"),
            "command": safe_command,
            "shell": command_line(artifact_command_parts(safe_command)),
            "status": "planned",
        }
        if args.execute:
            try:
                output_path.parent.mkdir(parents=True, exist_ok=True)
            except OSError as exc:
                record["status"] = "failed"
                record["result"] = {
                    "returncode": None,
                    "timed_out": False,
                    "error": redact_path_error(exc, output_path),
                    "error_type": "output_directory_create_failed",
                }
                record["child_status"] = "not_attempted"
                records.append(record)
                continue
            result = run(command, args.timeout_seconds, cwd=repo_root)
            child_status = load_child_status(output_path) if command_ok(result) else ""
            if result.get("timed_out"):
                record["status"] = "timeout"
            elif command_ok(result) and child_status == "ok":
                record["status"] = "ok"
            elif command_ok(result):
                record["status"] = f"partial_child_{child_status}"
            else:
                record["status"] = "failed"
            record["result"] = result
            record["child_status"] = child_status
        records.append(record)
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "status": aggregate_status(records, bool(args.execute)),
        "executed": bool(args.execute),
        **path_payload(args.matrix, "matrix_path", "matrix_path_redacted"),
        **path_payload(matrix_path, "resolved_matrix_path"),
        **path_payload(args.run_dir, "run_dir", "run_dir_path_redacted"),
        **path_payload(run_dir, "resolved_run_dir"),
        "matrix": artifact_matrix(matrix),
        "matrix_config_error": None,
        "summary": {
            "scenario_count": len(records),
            "executed_count": sum(1 for item in records if item["status"] != "planned"),
            "ok_count": sum(1 for item in records if item["status"] == "ok"),
            "partial_count": sum(1 for item in records if str(item["status"]).startswith("partial")),
            "failed_count": sum(1 for item in records if item["status"] == "failed"),
            "timeout_count": sum(1 for item in records if item["status"] == "timeout"),
        },
        "records": records,
    }




def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--matrix", default="configs/llama_cpp_matrix.example.json")
    parser.add_argument("--mode", default="auto", choices=["auto", "cli", "bench", "preflight"])
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--llama-cli", default="")
    parser.add_argument("--llama-bench", default="")
    parser.add_argument("--run-dir", default="results/raw/llama_cpp_matrix")
    parser.add_argument("--timeout-seconds", type=int, default=900)
    parser.add_argument("--output", default="results/raw/llama_cpp_matrix.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    require_int_range("--timeout-seconds", args.timeout_seconds, 1, 86_400)
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    output_status = write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    if output_status["status"] == "ok":
        print(f"wrote {path_label(output)}")
    else:
        print(f"failed to write {path_label(output)}: {output_status.get('error')}")
    return 0 if output_status["status"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
