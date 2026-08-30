#!/usr/bin/env python3
"""Shared helpers for InferLab advanced serving scripts."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Sequence

try:
    from scripts.artifact_json import write_json_file as shared_write_json_file
    from scripts.bounded_http import BoundedHttpBody, read_bounded_http_body
    from scripts.bounded_process import run_bounded_process
    from scripts.cli_validation import (
        normalize_env_var_name,
        require_float_range,
        require_int_range,
    )
    from scripts.path_status import safe_exists as safe_path_exists
    from scripts.repo_paths import default_repo_root
    from scripts.time_utils import utc_now
    from scripts.command_redaction import (
        artifact_path_or_value,
        artifact_path_payload,
        artifact_preview_fields,
        artifact_command_parts as shared_artifact_command_parts,
        artifact_command_template_parts,
        redact_artifact_output,
        redact_path_error,
        redact_sensitive_text,
        redact_value,
        redacted_command_line,
        redacted_path_label,
    )
except ModuleNotFoundError:
    scripts_dir = Path(__file__).resolve().parents[1] / "scripts"
    if str(scripts_dir) not in sys.path:
        sys.path.insert(0, str(scripts_dir))
    from artifact_json import write_json_file as shared_write_json_file  # type: ignore
    from bounded_http import BoundedHttpBody, read_bounded_http_body  # type: ignore
    from bounded_process import run_bounded_process  # type: ignore
    from cli_validation import (  # type: ignore
        normalize_env_var_name,
        require_float_range,
        require_int_range,
    )
    from path_status import safe_exists as safe_path_exists  # type: ignore
    from repo_paths import default_repo_root  # type: ignore
    from time_utils import utc_now  # type: ignore
    from command_redaction import (  # type: ignore
        artifact_path_or_value,
        artifact_path_payload,
        artifact_preview_fields,
        artifact_command_parts as shared_artifact_command_parts,
        artifact_command_template_parts,
        redact_artifact_output,
        redact_path_error,
        redact_sensitive_text,
        redact_value,
        redacted_command_line,
        redacted_path_label,
    )


DEFAULT_BACKENDS = {
    "vllm": {"base_url": "http://127.0.0.1:8000/v1", "model": "model"},
    "sglang": {"base_url": "http://127.0.0.1:30000/v1", "model": "model"},
    "llama_cpp": {"base_url": "http://127.0.0.1:8080/v1", "model": "model"},
    "tgi": {"base_url": "http://127.0.0.1:8081", "model": "model"},
}

COMMAND_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024
HTTP_RESPONSE_LIMIT_BYTES = 16 * 1024 * 1024
CONTENT_FIELD_NAMES = {
    "content",
    "completion",
    "completion_text",
    "document",
    "documents",
    "embedding",
    "embeddings",
    "function",
    "function_call",
    "arguments",
    "input",
    "logprob",
    "logprobs",
    "message",
    "messages",
    "negative",
    "negatives",
    "parameters",
    "positive",
    "prompt",
    "prefix",
    "query",
    "research_question",
    "research_questions",
    "raw_output",
    "response",
    "text",
    "token",
    "token_logprobs",
    "tokens",
    "tool_call",
    "tool_calls",
    "top_logprobs",
    "vector",
    "vectors",
}

PATH_FIELD_NAMES = {
    "adapter",
    "adapter_path",
    "artifact",
    "base_model",
    "binary",
    "cache",
    "checkpoint",
    "config",
    "dir",
    "directory",
    "engine",
    "file",
    "fixture",
    "manifest",
    "model",
    "model_path",
    "output",
    "path",
    "profile",
    "report",
    "repo_root",
    "root",
    "schema_path",
    "template",
    "tokenizer",
    "tokenizer_path",
}


class ConfigDict(dict):
    def __init__(self, data: Dict[str, Any], read_status_value: Dict[str, Any]):
        super().__init__(data)
        self.read_status = read_status_value


_REPO_ROOT = default_repo_root()


def set_repo_root(value: str | Path) -> Path:
    global _REPO_ROOT
    _REPO_ROOT = Path(value).expanduser().resolve()
    return _REPO_ROOT


def repo_root() -> Path:
    return _REPO_ROOT


def resolve_path(path: str | Path) -> Path:
    p = Path(path).expanduser()
    if p.is_absolute():
        return p
    return _REPO_ROOT / p


def path_label(path: str | Path) -> str:
    return redacted_path_label(resolve_path(path).as_posix())


def path_payload(
    path: str | Path,
    field_name: str = "path",
    redacted_field_name: str | None = None,
) -> Dict[str, Any]:
    raw_path = "" if path in ("", None) else resolve_path(path).as_posix()
    return artifact_path_payload(raw_path, field_name, redacted_field_name)


def read_status(status: str, path: Path, error: str = "", fallback_used: bool = False) -> Dict[str, Any]:
    raw_path = path.as_posix()
    safe_path = redacted_path_label(raw_path)
    payload: Dict[str, Any] = {
        "status": status,
        "path": safe_path,
        "path_redacted": bool(raw_path and safe_path != raw_path),
        "fallback_used": fallback_used,
    }
    if error:
        payload["error"] = redact_path_error(error, path)
    return payload


def read_json_with_status(path: str | Path, default: Dict[str, Any]) -> tuple[Dict[str, Any], Dict[str, Any]]:
    p = resolve_path(path)
    exists, exists_error = safe_path_exists(p)
    if exists_error:
        status = read_status("path_probe_failed", p, exists_error, True)
        return ConfigDict(dict(default), status), status
    if not exists:
        status = read_status("missing", p, fallback_used=True)
        return ConfigDict(dict(default), status), status
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except OSError as exc:
        status = read_status("unreadable_json", p, redact_path_error(exc, p), True)
        return ConfigDict(dict(default), status), status
    except UnicodeDecodeError as exc:
        status = read_status("invalid_encoding", p, redact_path_error(exc, p), True)
        return ConfigDict(dict(default), status), status
    except json.JSONDecodeError as exc:
        status = read_status("invalid_json", p, redact_path_error(exc, p), True)
        return ConfigDict(dict(default), status), status
    if not isinstance(data, dict):
        status = read_status("invalid_json_root", p, f"expected object, got {type(data).__name__}", True)
        return ConfigDict(dict(default), status), status
    status = read_status("ok", p)
    return ConfigDict(data, status), status


def read_json(path: str | Path, default: Dict[str, Any]) -> Dict[str, Any]:
    data, _ = read_json_with_status(path, default)
    return data


def read_jsonl_with_status(path: str | Path, fallback: List[Dict[str, Any]]) -> tuple[List[Dict[str, Any]], Dict[str, Any]]:
    p = resolve_path(path)
    exists, exists_error = safe_path_exists(p)
    if exists_error:
        return fallback, read_status("path_probe_failed", p, exists_error, True)
    if not exists:
        return fallback, read_status("missing", p, fallback_used=True)
    try:
        lines = p.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        return fallback, read_status("unreadable_jsonl", p, redact_path_error(exc, p), True)
    except UnicodeDecodeError as exc:
        return fallback, read_status("invalid_jsonl_encoding", p, redact_path_error(exc, p), True)
    rows: List[Dict[str, Any]] = []
    errors: List[Dict[str, Any]] = []
    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("{"):
            try:
                row = json.loads(stripped)
            except json.JSONDecodeError as exc:
                errors.append(
                    {
                        "line": line_number,
                        "error": redact_sensitive_text(str(exc)),
                        **artifact_preview_fields(stripped, max_chars=160),
                    }
                )
                continue
        else:
            row = {"id": f"line_{line_number}", "text": stripped}
        if isinstance(row, dict):
            rows.append(row)
        else:
            errors.append({"line": line_number, "error": f"expected object, got {type(row).__name__}"})
    if rows:
        status = read_status("partial_jsonl" if errors else "ok", p)
        if errors:
            status["error_count"] = len(errors)
            status["errors"] = errors[:10]
        return rows, status
    status = read_status("empty_or_invalid_jsonl", p, fallback_used=True)
    if errors:
        status["error_count"] = len(errors)
        status["errors"] = errors[:10]
    return fallback, status


def shell_join(parts: Sequence[Any]) -> str:
    return redacted_command_line(parts)


def artifact_command_parts(command: Sequence[Any], path_options: Sequence[str] = ()) -> List[str]:
    return shared_artifact_command_parts(command, path_options=path_options)


def command_record(name: str, command: Sequence[Any], stage: str = "execute") -> Dict[str, Any]:
    redacted = artifact_command_parts(command)
    templated = artifact_command_template_parts(command)
    return {
        "name": name,
        "stage": stage,
        "command": redacted,
        "shell": redacted_command_line(redacted),
        "shell_template": redacted_command_line(templated),
        "template_policy": "local paths are represented by target-machine placeholders",
    }


def run(
    command: Sequence[Any],
    timeout: float,
    cwd: Path | None = None,
    *,
    environment: Mapping[str, str] | None = None,
) -> Dict[str, Any]:
    command_parts = [str(part) for part in command]
    redacted_command = artifact_command_parts(command_parts)
    cwd_raw = cwd.as_posix() if cwd else ""
    cwd_safe = redacted_path_label(cwd_raw) if cwd_raw else ""
    cwd_redacted = bool(cwd_raw and cwd_safe != cwd_raw)
    capture = run_bounded_process(
        command_parts,
        cwd=cwd,
        timeout_seconds=timeout,
        stdout_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        env=environment,
        launch_error_formatter=lambda exc: redact_path_error(exc, cwd, *command_parts),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    result = {
        "command": redacted_command,
        "cwd": cwd_safe,
        "cwd_redacted": cwd_redacted,
        "returncode": capture.returncode if capture.status in {"ok", "nonzero_exit"} else None,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_tail": redact_artifact_output("\n".join(stdout.splitlines()[-80:]), paths=[cwd, *command_parts]),
        "stderr_tail": redact_artifact_output("\n".join(stderr.splitlines()[-80:]), paths=[cwd, *command_parts]),
        "timed_out": capture.timed_out,
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "stdout_bytes": capture.stdout_bytes,
        "stderr_bytes": capture.stderr_bytes,
    }
    if capture.timed_out:
        result["error"] = redact_sensitive_text(f"timed out after {timeout}s")
    elif capture.output_limit_exceeded:
        result["error"] = "command output limit exceeded"
    elif capture.error:
        result["error"] = capture.error
    elif capture.status not in {"ok", "nonzero_exit"}:
        result["error"] = f"command execution failed: {capture.status}"
    return result


def command_succeeded(result: Dict[str, Any]) -> bool:
    return result.get("returncode") == 0 and not result.get("timed_out") and "error" not in result


def decode_bounded_json_body(body: BoundedHttpBody) -> Dict[str, Any]:
    result: Dict[str, Any] = {
        "response_body_status": body.status,
        "response_body_bytes": body.observed_bytes,
        "response_body_limit_bytes": HTTP_RESPONSE_LIMIT_BYTES,
        "response_body_complete": body.status == "ok",
    }
    if body.status != "ok":
        result["error"] = redact_sensitive_text(body.error or body.status)
        return result
    try:
        text = body.data.decode("utf-8")
    except UnicodeDecodeError as exc:
        result["decode_error"] = redact_sensitive_text(str(exc))
        result["error"] = "HTTP response body is not valid UTF-8 JSON"
        return result
    try:
        result["json"] = artifact_safe_value(json.loads(text))
    except json.JSONDecodeError as exc:
        result["decode_error"] = redact_sensitive_text(str(exc))
        result.update(artifact_preview_fields(text, max_chars=1000))
    return result


def post_json(url: str, payload: Dict[str, Any], timeout: float, api_key: str = "") -> Dict[str, Any]:
    request = urllib.request.Request(url, data=json.dumps(payload).encode("utf-8"), headers={"Content-Type": "application/json"}, method="POST")
    if api_key:
        request.add_header("Authorization", f"Bearer {api_key}")
    started = time.perf_counter()
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = read_bounded_http_body(response, HTTP_RESPONSE_LIMIT_BYTES)
            elapsed_ms = round((time.perf_counter() - started) * 1000.0, 3)
            return {
                "status_code": response.status,
                "elapsed_ms": elapsed_ms,
                **decode_bounded_json_body(body),
            }
    except urllib.error.HTTPError as exc:
        body = read_bounded_http_body(exc, HTTP_RESPONSE_LIMIT_BYTES)
        elapsed_ms = round((time.perf_counter() - started) * 1000.0, 3)
        return {
            "status_code": exc.code,
            "elapsed_ms": elapsed_ms,
            **decode_bounded_json_body(body),
        }
    except Exception as exc:
        return {
            "status_code": 0,
            "elapsed_ms": round((time.perf_counter() - started) * 1000.0, 3),
            "response_body_status": "request_failed",
            "response_body_bytes": 0,
            "response_body_limit_bytes": HTTP_RESPONSE_LIMIT_BYTES,
            "response_body_complete": False,
            "error": redact_sensitive_text(str(exc)),
        }


def post_json_ok(result: Dict[str, Any]) -> bool:
    status_code = result.get("status_code")
    return (
        isinstance(status_code, int)
        and 200 <= status_code < 400
        and "error" not in result
        and "decode_error" not in result
    )


def api_key_from_env(env_name: str) -> str:
    name = normalize_env_var_name("--api-key-env", env_name)
    return os.environ.get(name, "") if name else ""


def api_key_metadata(env_name: str, api_key: str) -> Dict[str, Any]:
    name = normalize_env_var_name("--api-key-env", env_name)
    return {"env": name, "configured": bool(api_key)}


def write_json(path: str | Path, payload: Dict[str, Any]) -> None:
    p = resolve_path(path)
    output_payload = dict(payload)
    config = output_payload.get("config")
    if isinstance(config, ConfigDict):
        output_payload["config"] = artifact_safe_value(dict(config))
        output_payload.setdefault("config_read_status", config.read_status)
    elif isinstance(config, dict):
        output_payload["config"] = artifact_safe_value(config)
    shared_write_json_file(p, output_payload)


def content_field_name(key: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", key.lower()).strip("_")


def is_path_field_name(key: str) -> bool:
    normalized = content_field_name(key)
    return (
        normalized in PATH_FIELD_NAMES
        or normalized.endswith("_path")
        or normalized.endswith("_dir")
        or normalized.endswith("_file")
        or normalized.endswith("_root")
    )


def content_fingerprint(value: Any) -> Dict[str, Any]:
    redacted = redact_value(value)
    text = json.dumps(redacted, ensure_ascii=False, sort_keys=True) if isinstance(redacted, (dict, list)) else str(redacted)
    return {
        "redacted": True,
        "chars": len(text),
        "sha256": hashlib.sha256(text.encode("utf-8")).hexdigest(),
    }


def artifact_safe_value(value: Any, key: str = "") -> Any:
    if isinstance(value, ConfigDict):
        safe = ConfigDict({}, value.read_status)
        safe.update({item_key: artifact_safe_value(item_value, str(item_key)) for item_key, item_value in value.items()})
        return safe
    normalized = content_field_name(key)
    if normalized in CONTENT_FIELD_NAMES and value not in ("", None):
        payload = content_fingerprint(value)
        if isinstance(value, list):
            payload["item_count"] = len(value)
        return payload
    if isinstance(value, str) and is_path_field_name(key):
        return artifact_path_or_value(value)
    if isinstance(value, dict):
        return {item_key: artifact_safe_value(item_value, str(item_key)) for item_key, item_value in value.items()}
    if isinstance(value, list):
        return [artifact_safe_value(item) for item in value]
    return redact_value(value, key)


def validate_common_args(namespace: argparse.Namespace) -> None:
    if hasattr(namespace, "timeout_seconds"):
        require_float_range("--timeout-seconds", namespace.timeout_seconds, 0.001, 86_400.0)


def config_int_value(name: str, value: Any, minimum: int, maximum: int) -> int:
    if isinstance(value, bool):
        raise SystemExit(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}")
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, float):
        if not math.isfinite(value) or not value.is_integer():
            raise SystemExit(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}")
        parsed = int(value)
    elif isinstance(value, str):
        try:
            parsed = int(value.strip())
        except ValueError as exc:
            raise SystemExit(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}") from exc
    else:
        raise SystemExit(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}")
    require_int_range(name, parsed, minimum, maximum)
    return parsed


def config_int_list(config: Dict[str, Any], key: str, minimum: int, maximum: int) -> List[int]:
    raw = config.get(key)
    if not isinstance(raw, list) or not raw:
        raise SystemExit(f"config.{key} must be a non-empty integer list")
    return [
        config_int_value(f"config.{key}[{index}]", value, minimum, maximum)
        for index, value in enumerate(raw, start=1)
    ]


def config_raw_string_value(name: str, value: Any, allow_empty: bool = False) -> str:
    if isinstance(value, bool) or not isinstance(value, str):
        raise SystemExit(f"{name} must be a string, got {value!r}")
    if not allow_empty and not value.strip():
        raise SystemExit(f"{name} must be a non-empty string")
    return value


def config_string_value(name: str, value: Any, allow_empty: bool = False) -> str:
    return config_raw_string_value(name, value, allow_empty=allow_empty).strip()


def config_backend_map(
    config: Mapping[str, Any],
    default_backends: Mapping[str, Any],
    extra_string_defaults: Mapping[str, str] | None = None,
) -> Dict[str, Dict[str, str]]:
    raw = config.get("backends", default_backends)
    if not isinstance(raw, dict) or not raw:
        raise SystemExit("config.backends must be a non-empty object")
    extras = extra_string_defaults or {}
    backends: Dict[str, Dict[str, str]] = {}
    for name, backend in raw.items():
        backend_name = config_string_value("config.backends.<name>", name)
        if not isinstance(backend, dict):
            raise SystemExit(f"config.backends.{backend_name} must be an object")
        normalized = {
            "base_url": config_string_value(
                f"config.backends.{backend_name}.base_url", backend.get("base_url", "")
            ),
            "model": config_string_value(
                f"config.backends.{backend_name}.model", backend.get("model", "model")
            ),
        }
        for field, default in extras.items():
            normalized[field] = config_string_value(
                f"config.backends.{backend_name}.{field}", backend.get(field, default)
            )
        backends[backend_name] = normalized
    return backends


def config_string_list(config: Dict[str, Any], key: str, allowed: Iterable[str] | None = None) -> List[str]:
    raw = config.get(key)
    if not isinstance(raw, list) or not raw:
        raise SystemExit(f"config.{key} must be a non-empty string list")
    allowed_set = set(allowed or [])
    values: List[str] = []
    for index, item in enumerate(raw, start=1):
        value = config_string_value(f"config.{key}[{index}]", item)
        if allowed_set and value not in allowed_set:
            raise SystemExit(f"config.{key}[{index}] must be one of {sorted(allowed_set)}, got {item!r}")
        values.append(value)
    return values


def config_object_list(config: Dict[str, Any], key: str) -> List[Dict[str, Any]]:
    raw = config.get(key)
    if not isinstance(raw, list) or not raw:
        raise SystemExit(f"config.{key} must be a non-empty object list")
    values: List[Dict[str, Any]] = []
    for index, item in enumerate(raw, start=1):
        if not isinstance(item, dict):
            raise SystemExit(f"config.{key}[{index}] must be an object")
        values.append(dict(item))
    return values


class RepoRootArgumentParser(argparse.ArgumentParser):
    def _configure_repo_root(self, namespace: argparse.Namespace) -> argparse.Namespace:
        if hasattr(namespace, "repo_root"):
            set_repo_root(namespace.repo_root)
        validate_common_args(namespace)
        return namespace

    def parse_args(self, args: Sequence[str] | None = None, namespace: argparse.Namespace | None = None) -> argparse.Namespace:
        return self._configure_repo_root(super().parse_args(args, namespace))

    def parse_known_args(
        self,
        args: Sequence[str] | None = None,
        namespace: argparse.Namespace | None = None,
    ) -> tuple[argparse.Namespace, List[str]]:
        parsed, unknown = super().parse_known_args(args, namespace)
        return self._configure_repo_root(parsed), unknown


def base_parser(description: str) -> argparse.ArgumentParser:
    parser = RepoRootArgumentParser(description=description)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--execute", action="store_true", help="Run against real services/tools. Default only plans or simulates.")
    parser.add_argument("--timeout-seconds", type=float, default=120.0)
    return parser


def status_from_execution(execute: bool, failures: int = 0, record_count: int | None = None) -> str:
    if execute:
        if record_count == 0:
            return "empty_execution_plan"
        return "ok" if failures == 0 else "failed"
    return "planned"


def safe_div(a: float, b: float) -> float:
    return a / b if b else 0.0
