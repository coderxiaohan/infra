#!/usr/bin/env python3
"""Run or preflight a vLLM OpenAI-compatible serving benchmark."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import sys
import time
import urllib.error
import urllib.request
from time_utils import utc_now_precise as now_utc
from pathlib import Path
from statistics import mean, median
from typing import Any, Dict, Iterable, List, Optional, Tuple

from bounded_http import BoundedHttpBody, read_bounded_http_body
from bounded_process import run_bounded_process
from artifact_json import try_write_json_file
from cli_validation import require_float_range, require_int_range
from command_redaction import (
    artifact_path_fields as path_payload,
    artifact_path_or_value as artifact_model_value,
    artifact_path_payload,
    artifact_preview_fields,
    looks_like_local_path,
    redact_path_error,
    redact_sensitive_text,
    redact_value,
    redacted_path_label as path_label,
)
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from path_status import safe_exists as safe_path_exists
from quiet_cli_output import add_quiet_success_argument, emit_success

ENV_VAR_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
DIAGNOSTIC_OUTPUT_LIMIT_BYTES = 64 * 1024
HTTP_RESPONSE_LIMIT_BYTES = 16 * 1024 * 1024
MAX_USAGE_TOKEN_COUNT = 1_000_000_000


DEFAULT_PROMPTS = [
    {"id": "short_math", "prompt": "What is 17 + 25?", "category": "sanity"},
    {
        "id": "short_explain",
        "prompt": "Explain KV cache in one sentence.",
        "category": "inference",
    },
    {
        "id": "long_context_seed",
        "prompt": "List the main latency components of an LLM inference server.",
        "category": "serving",
    },
]


def run_command(command: List[str], timeout_seconds: float = 15.0) -> Dict[str, Any]:
    capture = run_bounded_process(
        command,
        cwd=None,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=DIAGNOSTIC_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=DIAGNOSTIC_OUTPUT_LIMIT_BYTES,
        merge_stderr_into_stdout=True,
        launch_error_formatter=lambda exc: (
            "missing" if isinstance(exc, FileNotFoundError) else redact_sensitive_text(str(exc))
        ),
    )
    output = capture.stdout.decode("utf-8", errors="replace").strip()
    result: Dict[str, Any] = {
        "returncode": capture.returncode if capture.status in {"ok", "nonzero_exit"} else None,
        "output": redact_sensitive_text(output),
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "captured_output_bytes": capture.stdout_bytes,
    }
    if capture.status == "launch_failed" and capture.error == "missing":
        result["output"] = "missing"
    if capture.timed_out:
        result["timeout"] = True
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


def module_available(name: str) -> bool:
    return importlib.util.find_spec(name) is not None


def torch_cuda_probe() -> Dict[str, Any]:
    if not module_available("torch"):
        return {"available": False, "reason": "torch module missing"}
    try:
        import torch  # type: ignore

        return {
            "available": bool(torch.cuda.is_available()),
            "device_count": int(torch.cuda.device_count()),
            "cuda_version": getattr(torch.version, "cuda", None),
            "torch_version": getattr(torch, "__version__", None),
        }
    except Exception as error:  # pragma: no cover - defensive optional dependency probe.
        return {"available": False, "reason": redact_path_error(error, Path.cwd())}


def collect_environment() -> Dict[str, Any]:
    nvidia_smi_path = shutil.which("nvidia-smi")
    nvidia_smi = (
        run_command(
            [
                "nvidia-smi",
                "--query-gpu=name,memory.total,driver_version",
                "--format=csv,noheader",
            ]
        )
        if nvidia_smi_path
        else {"returncode": None, "output": "missing"}
    )
    torch_cuda = torch_cuda_probe()
    return {
        "python": sys.version.split()[0],
        "tools": artifact_path_payload(
            nvidia_smi_path or "missing",
            "nvidia_smi",
            "nvidia_smi_path_redacted",
        ),
        "vllm": {"available": module_available("vllm")},
        "torch_cuda": torch_cuda,
        "nvidia_smi": nvidia_smi,
        "gpu_available": bool(
            nvidia_smi_path
            and nvidia_smi.get("returncode") == 0
            and str(nvidia_smi.get("output", "")).strip()
        )
        or bool(torch_cuda.get("available")),
    }


def load_prompts(path: Path, max_prompts: int) -> Tuple[List[Dict[str, str]], Dict[str, Any]]:
    limit = max(0, max_prompts)
    if limit == 0:
        return [], {**path_payload(path), "status": "disabled", "fallback": None}
    exists, exists_error = safe_path_exists(path)
    if exists_error:
        prompts = DEFAULT_PROMPTS[:limit]
        return prompts, {
            **path_payload(path),
            "status": "path_probe_failed",
            "fallback": "built_in_default_prompts",
            "error": exists_error,
        }
    if not exists:
        prompts = DEFAULT_PROMPTS[:limit]
        return prompts, {
            **path_payload(path),
            "status": "missing",
            "fallback": "built_in_default_prompts",
        }

    prompts: List[Dict[str, str]] = []
    errors: List[Dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as file:
            for line_number, line in enumerate(file, start=1):
                stripped = line.strip()
                if not stripped:
                    continue
                try:
                    row = json.loads(stripped)
                except json.JSONDecodeError as exc:
                    errors.append(
                        {
                            "line": line_number,
                            "status": "invalid_prompt_json",
                            "error": redact_sensitive_text(str(exc)),
                        }
                    )
                    continue
                if not isinstance(row, dict):
                    errors.append({"line": line_number, "status": "invalid_prompt_row", "value_type": type(row).__name__})
                    continue
                prompt = str(row.get("prompt", ""))
                if not prompt:
                    continue
                prompts.append(
                    {
                        "id": str(row.get("id", f"prompt_{line_number}")),
                        "prompt": prompt,
                        "category": str(row.get("category", "uncategorized")),
                    }
                )
                if len(prompts) >= limit:
                    break
    except OSError as exc:
        prompts = DEFAULT_PROMPTS[:limit]
        return prompts, {**path_payload(path), "status": "unreadable_prompt_file", "fallback": "built_in_default_prompts", "error": redact_path_error(exc, path)}
    except UnicodeDecodeError as exc:
        prompts = DEFAULT_PROMPTS[:limit]
        return prompts, {**path_payload(path), "status": "invalid_prompt_encoding", "fallback": "built_in_default_prompts", "error": redact_path_error(exc, path)}
    if not prompts:
        prompts = DEFAULT_PROMPTS[:limit]
        return prompts, {**path_payload(path), "status": "no_valid_prompts", "fallback": "built_in_default_prompts", "errors": errors}
    source: Dict[str, Any] = {**path_payload(path), "status": "partial_prompt_file" if errors else "ok", "fallback": None}
    if errors:
        source["errors"] = errors
    return prompts, source


def percentile(values: List[float], quantile: float) -> Optional[float]:
    if not values:
        return None
    sorted_values = sorted(values)
    index = min(len(sorted_values) - 1, max(0, int(round((len(sorted_values) - 1) * quantile))))
    return sorted_values[index]


def content_to_text(value: Any) -> str:
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        parts = []
        for item in value:
            text = content_to_text(item)
            if text:
                parts.append(text)
        return "".join(parts)
    if isinstance(value, dict):
        for key in ("text", "content"):
            text = content_to_text(value.get(key))
            if text:
                return text
    return ""


def completion_text(response: Dict[str, Any]) -> str:
    choices = response.get("choices")
    if not isinstance(choices, list) or not choices:
        return ""
    first = choices[0]
    if not isinstance(first, dict):
        return ""
    text = content_to_text(first.get("text"))
    if text:
        return text
    message = first.get("message")
    if isinstance(message, dict):
        return content_to_text(message.get("content"))
    return ""


def build_payload(prompt: str, args: argparse.Namespace) -> Dict[str, Any]:
    if args.request_kind == "chat":
        return {
            "model": args.model,
            "messages": [{"role": "user", "content": prompt}],
            "max_tokens": args.max_tokens,
            "temperature": args.temperature,
        }
    return {
        "model": args.model,
        "prompt": prompt,
        "max_tokens": args.max_tokens,
        "temperature": args.temperature,
    }


def usage_token_count(value: Any) -> Optional[int]:
    if (
        type(value) is int
        and 0 <= value <= MAX_USAGE_TOKEN_COUNT
    ):
        return value
    return None


def has_usable_usage(value: Any) -> bool:
    if not isinstance(value, dict):
        return False
    completion_tokens = usage_token_count(value.get("completion_tokens"))
    return completion_tokens is not None and completion_tokens > 0


def post_json(
    url: str,
    payload: Dict[str, Any],
    timeout_seconds: float,
    api_key: Optional[str],
) -> Tuple[int, Dict[str, Any], float, Dict[str, Any]]:
    request_body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=request_body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    if api_key:
        request.add_header("Authorization", f"Bearer {api_key}")

    started = time.perf_counter()
    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            body = read_bounded_http_body(response, HTTP_RESPONSE_LIMIT_BYTES)
            status = response.status
            elapsed_ms = (time.perf_counter() - started) * 1000.0
            parsed, body_observation = decode_response_body(body)
            return status, parsed, elapsed_ms, body_observation
    except urllib.error.HTTPError as error:
        body = read_bounded_http_body(error, HTTP_RESPONSE_LIMIT_BYTES)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        parsed, body_observation = decode_response_body(body)
        return error.code, parsed, elapsed_ms, body_observation
    except Exception as error:
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        return 0, {"error": redact_sensitive_text(str(error))}, elapsed_ms, {
            "response_body_status": "request_failed",
            "response_body_bytes": 0,
            "response_body_limit_bytes": HTTP_RESPONSE_LIMIT_BYTES,
            "response_body_complete": False,
        }


def decode_response_body(body: BoundedHttpBody) -> Tuple[Dict[str, Any], Dict[str, Any]]:
    observation = {
        "response_body_status": body.status,
        "response_body_bytes": body.observed_bytes,
        "response_body_limit_bytes": HTTP_RESPONSE_LIMIT_BYTES,
        "response_body_complete": body.status == "ok",
    }
    if body.status != "ok":
        return {"error": redact_sensitive_text(body.error or body.status)}, observation
    try:
        decoded = body.data.decode("utf-8")
    except UnicodeDecodeError as error:
        return {
            "error": "HTTP response body is not valid UTF-8 JSON",
            "decode_error": redact_sensitive_text(str(error)),
        }, observation
    try:
        parsed_any = json.loads(decoded)
        parsed = parsed_any if isinstance(parsed_any, dict) else {"response": parsed_any}
    except json.JSONDecodeError as error:
        parsed = {
            "error": redact_sensitive_text(decoded[:1000]),
            "decode_error": redact_sensitive_text(str(error)),
        }
    return parsed, observation


def benchmark_requests(
    prompts: Iterable[Dict[str, str]],
    args: argparse.Namespace,
    api_key: Optional[str],
) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    endpoint = args.endpoint
    if not endpoint.startswith("/"):
        endpoint = "/" + endpoint
    if args.request_kind == "chat" and endpoint == "/v1/completions":
        endpoint = "/v1/chat/completions"
    url = args.base_url.rstrip("/") + endpoint

    request_results: List[Dict[str, Any]] = []
    benchmark_started = time.perf_counter()
    for index, prompt in enumerate(prompts):
        payload = build_payload(prompt["prompt"], args)
        status_code, response, elapsed_ms, body_observation = post_json(
            url, payload, args.request_timeout_seconds, api_key
        )
        usage = response.get("usage") if isinstance(response, dict) else {}
        prompt_tokens = (
            usage_token_count(usage.get("prompt_tokens"))
            if isinstance(usage, dict)
            else None
        )
        completion_tokens = (
            usage_token_count(usage.get("completion_tokens"))
            if isinstance(usage, dict)
            else None
        )
        text = completion_text(response)
        redacted_text = redact_sensitive_text(text)
        text_hash = hashlib.sha256(redacted_text.encode("utf-8")).hexdigest() if redacted_text else ""
        http_ok = 200 <= status_code < 300
        has_completion_text = bool(text)
        has_usage = has_usable_usage(usage)
        decode_error = response.get("decode_error") if isinstance(response, dict) else None
        response_valid = not decode_error and (has_completion_text or has_usage)
        result: Dict[str, Any] = {
            "index": index,
            "prompt_id": prompt["id"],
            "category": prompt["category"],
            "status_code": status_code,
            "http_ok": http_ok,
            "ok": http_ok and response_valid,
            "response_valid": response_valid,
            "has_completion_text": has_completion_text,
            "has_usage": has_usage,
            "elapsed_ms": elapsed_ms,
            "prompt_chars": len(prompt["prompt"]),
            "completion_chars": len(redacted_text),
            "completion_sha256": text_hash,
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens,
            **body_observation,
        }
        if args.completion_preview_chars > 0:
            result.update(
                artifact_preview_fields(
                    redacted_text,
                    field_name="completion_preview",
                    max_chars=args.completion_preview_chars,
                )
            )
        if args.include_completion_text:
            result["completion_text"] = redacted_text
            result["completion_text_artifact_policy"] = "unsafe_opt_in_redacted_text"
        else:
            result["completion_text_artifact_policy"] = "content_hash_only"
        if completion_tokens is not None and completion_tokens > 0:
            result["ms_per_completion_token"] = elapsed_ms / float(completion_tokens)
        if not result["ok"]:
            if isinstance(response, dict):
                result["error"] = redact_value(
                    response.get("error")
                    or response.get("decode_error")
                    or "missing completion text and usage in response"
                )
            else:
                result["error"] = redact_value(response)
        request_results.append(result)

    total_elapsed_ms = (time.perf_counter() - benchmark_started) * 1000.0
    successful = [item for item in request_results if item["ok"]]
    latencies = [float(item["elapsed_ms"]) for item in successful]
    output_tokens = [
        item["completion_tokens"]
        for item in successful
        if type(item.get("completion_tokens")) is int
    ]
    total_output_tokens = sum(output_tokens)
    total_seconds = total_elapsed_ms / 1000.0 if total_elapsed_ms > 0 else 0.0
    summary = {
        "request_count": len(request_results),
        "completed_requests": len(successful),
        "failed_requests": len(request_results) - len(successful),
        "total_elapsed_ms": total_elapsed_ms,
        "avg_latency_ms": mean(latencies) if latencies else None,
        "median_latency_ms": median(latencies) if latencies else None,
        "p90_latency_ms": percentile(latencies, 0.9),
        "min_latency_ms": min(latencies) if latencies else None,
        "max_latency_ms": max(latencies) if latencies else None,
        "total_completion_tokens": total_output_tokens if output_tokens else None,
        "request_throughput_per_s": len(successful) / total_seconds if total_seconds else None,
        "completion_token_throughput_per_s": (
            total_output_tokens / total_seconds if total_seconds and output_tokens else None
        ),
    }
    return request_results, summary


def preflight(args: argparse.Namespace, environment: Dict[str, Any]) -> Dict[str, Any]:
    reasons: List[str] = []
    if not args.base_url:
        reasons.append("base_url not provided; HTTP benchmark was not attempted")
    if not environment["vllm"]["available"]:
        reasons.append("vLLM module is not installed for local server launch")
    if not environment["gpu_available"]:
        reasons.append("GPU was not detected by nvidia-smi or torch.cuda")
    return {
        "can_run_local_vllm": bool(
            environment["vllm"]["available"] and environment["gpu_available"]
        ),
        "can_benchmark_external_endpoint": bool(args.base_url),
        "reasons": reasons,
    }


def build_payload_root(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    prompt_file = resolve_repo_path(repo_root, args.prompt_file)
    prompts, prompt_source = load_prompts(prompt_file, args.max_prompts)
    environment = collect_environment()
    preflight_payload = preflight(args, environment)
    api_key = os.environ.get(args.api_key_env) if args.api_key_env else None

    requests: List[Dict[str, Any]] = []
    summary: Dict[str, Any] = {}
    if args.base_url:
        requests, summary = benchmark_requests(prompts, args, api_key)
        if summary.get("request_count") == 0:
            status = "skipped"
        else:
            status = "ok" if summary.get("failed_requests") == 0 else "partial"
        mode = "external_openai_compatible_endpoint"
    else:
        status = "skipped"
        mode = "preflight_only"

    return {
        "schema_version": 1,
        "timestamp_utc": now_utc(),
        **artifact_path_payload(repo_root, "repo_root"),
        "status": status,
        "mode": mode,
        "model": artifact_model_value(args.model),
        "model_path_redacted": looks_like_local_path(args.model),
        "base_url": redact_sensitive_text(args.base_url),
        "endpoint": args.endpoint,
        "request_kind": args.request_kind,
        "completion_text_policy": {
            "include_completion_text": bool(args.include_completion_text),
            "completion_preview_chars": max(0, args.completion_preview_chars),
            "completion_preview_policy": "content_hash_only",
            "full_completion_text_policy": "unsafe_opt_in_redacted_text" if args.include_completion_text else "content_hash_only",
            "completion_hash": "sha256",
        },
        "prompt_count": len(prompts),
        "prompt_source": prompt_source,
        "environment": environment,
        "preflight": preflight_payload,
        "recommended_local_launch_command": [
            "python3",
            "-m",
            "vllm.entrypoints.openai.api_server",
            "--model",
            artifact_model_value(args.model),
            "--host",
            "0.0.0.0",
            "--port",
            str(args.recommended_port),
        ],
        "requests": requests,
        "summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    add_quiet_success_argument(parser)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--output", default="results/raw/vllm_serving_benchmark.json")
    parser.add_argument("--prompt-file", default="configs/eval_prompts.jsonl")
    parser.add_argument("--max-prompts", type=int, default=3)
    parser.add_argument("--model", default="TinyLlama/TinyLlama-1.1B-Chat-v1.0")
    parser.add_argument("--base-url", default="")
    parser.add_argument("--endpoint", default="/v1/completions")
    parser.add_argument("--request-kind", choices=("completion", "chat"), default="completion")
    parser.add_argument("--max-tokens", type=int, default=64)
    parser.add_argument("--temperature", type=float, default=0.0)
    parser.add_argument("--request-timeout-seconds", type=float, default=60.0)
    parser.add_argument("--api-key-env", default="OPENAI_API_KEY")
    parser.add_argument("--recommended-port", type=int, default=8000)
    parser.add_argument(
        "--include-completion-text",
        action="store_true",
        help="Store full completion text in raw artifacts. Disabled by default to avoid leaking generated content.",
    )
    parser.add_argument(
        "--completion-preview-chars",
        type=int,
        default=0,
        help="Store at most this many completion characters as a preview. Defaults to 0.",
    )
    return parser.parse_args()


def normalize_env_var_name(label: str, value: str) -> str:
    name = value.strip()
    if not name:
        return ""
    if not ENV_VAR_NAME_RE.fullmatch(name):
        raise SystemExit(f"{label} must be an environment variable name, not a secret value")
    return name


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--max-prompts", args.max_prompts, 0, 100000)
    require_int_range("--max-tokens", args.max_tokens, 1, 1000000)
    require_float_range("--temperature", args.temperature, 0.0, 5.0)
    require_float_range("--request-timeout-seconds", args.request_timeout_seconds, 0.001, 86400.0)
    require_int_range("--recommended-port", args.recommended_port, 1, 65535)
    require_int_range("--completion-preview-chars", args.completion_preview_chars, 0, 1000000)
    args.api_key_env = normalize_env_var_name("--api-key-env", args.api_key_env)


def main() -> int:
    args = parse_args()
    validate_args(args)
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload_root(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_status = try_write_json_file(output, payload)
    if write_status["status"] != "ok":
        print(f"failed to write {path_label(output)}: {write_status.get('error')}")
        return 1
    emit_success(path_label(output), quiet=args.quiet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
