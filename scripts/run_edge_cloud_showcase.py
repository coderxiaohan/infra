#!/usr/bin/env python3
"""Run the representative InferLab edge-cloud showcase and write JSON artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shlex
import signal
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, Mapping, Sequence

try:
    from scripts.bounded_process import run_bounded_process
    from scripts.command_redaction import (
        artifact_command_parts,
        artifact_output_tail,
        redact_artifact_output,
        redact_path_error,
        redact_value,
    )
    from scripts.text_output import atomic_write_text
except ModuleNotFoundError:
    from bounded_process import run_bounded_process
    from command_redaction import (
        artifact_command_parts,
        artifact_output_tail,
        redact_artifact_output,
        redact_path_error,
        redact_value,
    )
    from text_output import atomic_write_text


DEFAULT_CONFIG = "configs/showcase_edge_cloud_distilgpt2.json"
DEFAULT_OUTPUT_ROOT = "results/showcase"
CAPTURE_LIMIT_BYTES = 16 * 1024 * 1024
RUN_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,79}$")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def default_run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def read_json_object(path: Path) -> Dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"unable to read JSON file: {redact_path_error(exc, path)}") from exc
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"invalid JSON file: {redact_path_error(exc, path)}") from exc
    if not isinstance(value, dict):
        raise ValueError("JSON root must be an object")
    return value


def write_json_object(path: Path, payload: Mapping[str, Any]) -> None:
    atomic_write_text(path, json.dumps(dict(payload), indent=2, sort_keys=True) + "\n")


def write_text(path: Path, value: str) -> None:
    atomic_write_text(path, value)


def require_object(config: Mapping[str, Any], key: str) -> Dict[str, Any]:
    value = config.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"{key} must be an object")
    return value


def require_string(config: Mapping[str, Any], key: str, prefix: str) -> str:
    value = config.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{prefix}.{key} must be a nonempty string")
    return value


def require_int(config: Mapping[str, Any], key: str, prefix: str, minimum: int, maximum: int) -> int:
    value = config.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{prefix}.{key} must be an integer in [{minimum}, {maximum}]")
    return value


def require_number(config: Mapping[str, Any], key: str, prefix: str, minimum: float, maximum: float) -> float:
    value = config.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not minimum <= float(value) <= maximum:
        raise ValueError(f"{prefix}.{key} must be a number in [{minimum}, {maximum}]")
    return float(value)


def validate_config(config: Any) -> Dict[str, Any]:
    if not isinstance(config, dict):
        raise ValueError("config root must be an object")
    if config.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")

    experiment = require_object(config, "experiment")
    for key in ("id", "title", "model_family", "prompt_id", "prompt", "claim_scope"):
        require_string(experiment, key, "experiment")
    require_int(experiment, "output_tokens", "experiment", 1, 4096)
    if not RUN_ID_RE.fullmatch(str(experiment["id"])):
        raise ValueError("experiment.id contains unsupported characters")

    paths = require_object(config, "paths")
    for key in (
        "build_dir",
        "llama_cli",
        "llama_bench",
        "llama_model",
        "vllm_python",
        "vllm_model",
        "trtllm_python",
        "trtllm_bench",
        "trtllm_engine_dir",
    ):
        require_string(paths, key, "paths")

    native = require_object(config, "native")
    require_int(native, "timeout_seconds", "native", 1, 86400)

    llama = require_object(config, "llama_cpp")
    for key in ("threads", "ctx_size", "batch_size", "ubatch_size", "timeout_seconds"):
        require_int(llama, key, "llama_cpp", 1, 86400)
    require_int(llama, "ngl", "llama_cpp", 0, 100000)

    vllm = require_object(config, "vllm")
    for key in ("host", "served_model_name", "dtype"):
        require_string(vllm, key, "vllm")
    if vllm["host"] not in {"127.0.0.1", "localhost", "::1"}:
        raise ValueError("vllm.host must be a loopback address")
    require_int(vllm, "port", "vllm", 1, 65535)
    require_int(vllm, "max_model_len", "vllm", 1, 10000000)
    for key in ("startup_timeout_seconds", "request_timeout_seconds", "shutdown_timeout_seconds"):
        require_int(vllm, key, "vllm", 1, 86400)
    require_number(vllm, "gpu_memory_utilization", "vllm", 0.01, 1.0)
    for key in ("enforce_eager", "use_v2_model_runner"):
        if type(vllm.get(key)) is not bool:
            raise ValueError(f"vllm.{key} must be a boolean")

    trtllm = require_object(config, "trtllm")
    for key in ("model", "mpi_interface"):
        require_string(trtllm, key, "trtllm")
    for key in ("batch_size", "max_num_tokens", "timeout_seconds"):
        require_int(trtllm, key, "trtllm", 1, 86400)
    require_int(trtllm, "warmup", "trtllm", 0, 100000)
    return config


def expand_config_path(repo_root: Path, value: str) -> Path:
    expanded = Path(os.path.expandvars(os.path.expanduser(value)))
    candidate = expanded if expanded.is_absolute() else repo_root / expanded
    # Keep virtualenv interpreter symlinks intact so Python can discover pyvenv.cfg.
    return Path(os.path.abspath(candidate))


def resolved_paths(repo_root: Path, config: Mapping[str, Any]) -> Dict[str, Path]:
    paths = require_object(config, "paths")
    return {key: expand_config_path(repo_root, str(value)) for key, value in paths.items()}


def relative_label(path: Path, repo_root: Path) -> str:
    try:
        return path.resolve().relative_to(repo_root.resolve()).as_posix()
    except ValueError:
        return f"external/{path.name}"


def command_metadata(command: Sequence[str]) -> list[str]:
    return artifact_command_parts(
        command,
        path_options=(
            "--config",
            "--dataset",
            "--engine_dir",
            "--model",
            "--model_path",
            "--run-dir",
        ),
    )


def run_stage(
    name: str,
    command: Sequence[str],
    *,
    repo_root: Path,
    log_dir: Path,
    timeout_seconds: float,
    env: Mapping[str, str] | None = None,
    sensitive_contents: Sequence[str] = (),
) -> Dict[str, Any]:
    print(f"[{name}] running", flush=True)
    capture = run_bounded_process(
        list(command),
        cwd=repo_root,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=CAPTURE_LIMIT_BYTES,
        stderr_limit_bytes=CAPTURE_LIMIT_BYTES,
        env=env,
        launch_error_formatter=lambda exc: redact_path_error(exc, repo_root, *command),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    write_text(
        log_dir / f"{name}.stdout.log",
        redact_artifact_output(stdout, paths=[repo_root, *command], contents=sensitive_contents),
    )
    write_text(
        log_dir / f"{name}.stderr.log",
        redact_artifact_output(stderr, paths=[repo_root, *command], contents=sensitive_contents),
    )
    passed = capture.status == "ok" and capture.returncode == 0
    result: Dict[str, Any] = {
        "name": name,
        "status": "passed" if passed else "failed",
        "capture_status": capture.status,
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "timed_out": capture.timed_out,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "command": command_metadata(command),
        "logs": [f"logs/{name}.stdout.log", f"logs/{name}.stderr.log"],
    }
    if stdout.strip():
        result["stdout_tail"] = artifact_output_tail(stdout, command, repo_root, 12)
    if stderr.strip():
        result["stderr_tail"] = artifact_output_tail(stderr, command, repo_root, 12)
    if capture.error:
        result["error"] = capture.error
    print(f"[{name}] {result['status']} ({capture.elapsed_ms:.1f} ms)", flush=True)
    return result


def enforce_artifact_status(
    stage: Dict[str, Any],
    artifact: Path,
    accepted: Iterable[str],
    repo_root: Path,
) -> None:
    stage["artifact"] = relative_label(artifact, repo_root)
    try:
        payload = read_json_object(artifact)
        artifact_status = str(payload.get("status", "present"))
    except ValueError as exc:
        artifact_status = "invalid_or_missing"
        stage["artifact_error"] = str(exc)
    stage["artifact_status"] = artifact_status
    if stage.get("status") == "passed" and artifact_status not in set(accepted):
        stage["status"] = "failed"
        stage["error"] = f"unexpected artifact status: {artifact_status}"
        print(f"[{stage.get('name', 'stage')}] artifact validation failed ({artifact_status})", flush=True)


def write_trtllm_dataset(path: Path, input_ids: Sequence[int], output_tokens: int) -> None:
    payload = {
        "task_id": 0,
        "input_ids": list(input_ids),
        "output_tokens": output_tokens,
    }
    # trtllm-bench consumes the dataset as JSONL even for a single request.
    write_text(path, json.dumps(payload, sort_keys=True) + "\n")


def prepare_inputs(config: Mapping[str, Any], repo_root: Path, run_dir: Path) -> int:
    try:
        from transformers import AutoTokenizer  # type: ignore
    except ImportError as exc:
        print(f"transformers import failed: {exc}", file=sys.stderr)
        return 1

    experiment = require_object(config, "experiment")
    paths = resolved_paths(repo_root, config)
    raw_dir = run_dir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    prompt = str(experiment["prompt"])
    try:
        tokenizer = AutoTokenizer.from_pretrained(
            str(paths["vllm_model"]),
            local_files_only=True,
        )
        input_ids = tokenizer.encode(prompt, add_special_tokens=False)
    except Exception as exc:
        print(f"tokenizer input preparation failed: {redact_path_error(exc, paths['vllm_model'])}", file=sys.stderr)
        return 1
    if not input_ids:
        print("tokenizer produced no input ids", file=sys.stderr)
        return 1

    prompt_file = raw_dir / "showcase_prompt.jsonl"
    prompt_row = {
        "id": experiment["prompt_id"],
        "prompt": prompt,
        "category": "edge_cloud_showcase",
    }
    write_text(prompt_file, json.dumps(prompt_row, sort_keys=True) + "\n")
    dataset_file = raw_dir / "trtllm_dataset.json"
    write_trtllm_dataset(dataset_file, input_ids, int(experiment["output_tokens"]))
    write_json_object(
        raw_dir / "runtime_inputs.json",
        {
            "schema_version": 1,
            "status": "ok",
            "model_family": experiment["model_family"],
            "prompt_id": experiment["prompt_id"],
            "prompt_sha256": hashlib.sha256(prompt.encode("utf-8")).hexdigest(),
            "prompt_tokens": len(input_ids),
            "trtllm_input_tokens": len(input_ids),
            "backend_input_contract": {
                "same_tokenized_prompt": True,
                "vllm_prompt_tokens": len(input_ids),
                "trtllm_prompt_tokens": len(input_ids),
            },
            "output_tokens": experiment["output_tokens"],
            "tokenizer_class": type(tokenizer).__name__,
            "tokenizer_vocab_size": len(tokenizer),
            "artifacts": ["raw/showcase_prompt.jsonl", "raw/trtllm_dataset.json"],
        },
    )
    print(f"prepared {len(input_ids)} prompt tokens", flush=True)
    return 0


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def fingerprint_path(path: Path) -> Dict[str, Any]:
    if path.is_file():
        return {
            "status": "ok",
            "kind": "file",
            "name": path.name,
            "files": 1,
            "bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
    if not path.is_dir():
        return {"status": "missing", "name": path.name}
    digest = hashlib.sha256()
    files = 0
    total_bytes = 0
    for child in sorted(item for item in path.rglob("*") if item.is_file()):
        relative = child.relative_to(path).as_posix()
        size = child.stat().st_size
        file_hash = sha256_file(child)
        digest.update(f"{relative}\0{size}\0{file_hash}\n".encode("utf-8"))
        files += 1
        total_bytes += size
    return {
        "status": "ok",
        "kind": "directory_manifest",
        "name": path.name,
        "files": files,
        "bytes": total_bytes,
        "sha256": digest.hexdigest(),
    }


def build_model_fingerprints(config: Mapping[str, Any], paths: Mapping[str, Path]) -> Dict[str, Any]:
    experiment = require_object(config, "experiment")
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "model_family": experiment["model_family"],
        "format_note": "The backends use the same DistilGPT2 model family in backend-native packaging; hashes are expected to differ across GGUF, Hugging Face, and TensorRT engine formats.",
        "artifacts": {
            "llama_cpp_gguf": fingerprint_path(paths["llama_model"]),
            "vllm_huggingface": fingerprint_path(paths["vllm_model"]),
            "trtllm_engine": fingerprint_path(paths["trtllm_engine_dir"]),
        },
    }


def validate_trtllm_runtime_contract(
    engine_payload: Mapping[str, Any],
    *,
    input_tokens: int,
    output_tokens: int,
    runtime_max_tokens: int,
) -> list[str]:
    build = engine_payload.get("build_config")
    if not isinstance(build, dict):
        return ["engine config is missing build_config"]
    plugin = build.get("plugin_config")
    if not isinstance(plugin, dict):
        return ["engine config is missing plugin_config"]
    errors: list[str] = []
    if plugin.get("multiple_profiles") is not False:
        errors.append("engine must use a dynamic token profile (multiple_profiles=false)")
    limits = {
        "max_input_len": input_tokens,
        "max_seq_len": input_tokens + output_tokens,
        "max_num_tokens": min(input_tokens, runtime_max_tokens),
    }
    for key, required in limits.items():
        value = build.get(key)
        if type(value) is not int or value < required:
            errors.append(f"{key} must be at least {required}, got {value}")
    if runtime_max_tokens > int(build.get("max_num_tokens") or 0):
        errors.append("runtime max_num_tokens exceeds the compiled engine limit")
    return errors


def build_vllm_server_command(config: Mapping[str, Any], paths: Mapping[str, Path]) -> list[str]:
    vllm = require_object(config, "vllm")
    command = [
        str(paths["vllm_python"]),
        "-m",
        "vllm.entrypoints.openai.api_server",
        "--model",
        str(paths["vllm_model"]),
        "--served-model-name",
        str(vllm["served_model_name"]),
        "--host",
        str(vllm["host"]),
        "--port",
        str(vllm["port"]),
        "--dtype",
        str(vllm["dtype"]),
        "--max-model-len",
        str(vllm["max_model_len"]),
        "--gpu-memory-utilization",
        str(vllm["gpu_memory_utilization"]),
    ]
    if vllm["enforce_eager"]:
        command.append("--enforce-eager")
    return command


def port_is_open(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.25):
            return True
    except OSError:
        return False


def health_is_ready(host: str, port: int) -> bool:
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        with opener.open(f"http://{host}:{port}/health", timeout=2.0) as response:
            return 200 <= int(response.status) < 300
    except (OSError, urllib.error.URLError):
        return False


def redacted_log_tail(path: Path, command: Sequence[str], repo_root: Path, max_lines: int = 20) -> str:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return redact_path_error(exc, path)
    return artifact_output_tail(text, command, repo_root, max_lines)


def start_vllm_service(
    config: Mapping[str, Any],
    paths: Mapping[str, Path],
    *,
    repo_root: Path,
    log_dir: Path,
    env: Mapping[str, str],
) -> tuple[subprocess.Popen[bytes] | None, Any, Dict[str, Any]]:
    vllm = require_object(config, "vllm")
    host = str(vllm["host"])
    port = int(vllm["port"])
    command = build_vllm_server_command(config, paths)
    stage: Dict[str, Any] = {
        "name": "vllm_startup",
        "status": "failed",
        "command": command_metadata(command),
        "logs": ["logs/vllm_server.log"],
    }
    if port_is_open(host, port):
        stage["error"] = "configured vLLM port is already in use"
        return None, None, stage

    log_path = log_dir / "vllm_server.log"
    log_handle = log_path.open("wb")
    started = time.monotonic()
    try:
        process = subprocess.Popen(
            command,
            cwd=repo_root,
            env=dict(env),
            stdin=subprocess.DEVNULL,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    except OSError as exc:
        log_handle.close()
        stage["error"] = redact_path_error(exc, repo_root, *command)
        stage["elapsed_ms"] = round((time.monotonic() - started) * 1000.0, 3)
        return None, None, stage

    deadline = started + float(vllm["startup_timeout_seconds"])
    while time.monotonic() < deadline:
        if process.poll() is not None:
            break
        if health_is_ready(host, port):
            stage["status"] = "passed"
            stage["elapsed_ms"] = round((time.monotonic() - started) * 1000.0, 3)
            stage["health_url"] = f"http://{host}:{port}/health"
            print(f"[vllm_startup] passed ({stage['elapsed_ms']:.1f} ms)", flush=True)
            return process, log_handle, stage
        time.sleep(0.5)

    stage["elapsed_ms"] = round((time.monotonic() - started) * 1000.0, 3)
    stage["returncode"] = process.poll()
    stage["error"] = "vLLM exited before health readiness" if process.poll() is not None else "vLLM startup timed out"
    log_handle.flush()
    stage["log_tail"] = redacted_log_tail(log_path, command, repo_root)
    print(f"[vllm_startup] failed ({stage['error']})", flush=True)
    return process, log_handle, stage


def stop_vllm_service(
    process: subprocess.Popen[bytes] | None,
    log_handle: Any,
    config: Mapping[str, Any],
) -> Dict[str, Any]:
    vllm = require_object(config, "vllm")
    started = time.monotonic()
    stage: Dict[str, Any] = {"name": "vllm_shutdown", "status": "passed"}
    signal_used = "already_stopped"
    if process is not None and process.poll() is None:
        signal_used = "SIGINT"
        try:
            os.killpg(process.pid, signal.SIGINT)
        except OSError:
            process.terminate()
        try:
            process.wait(timeout=float(vllm["shutdown_timeout_seconds"]))
        except subprocess.TimeoutExpired:
            signal_used = "SIGTERM"
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except OSError:
                process.terminate()
            try:
                process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                signal_used = "SIGKILL"
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except OSError:
                    process.kill()
                try:
                    process.wait(timeout=5.0)
                except (OSError, subprocess.TimeoutExpired) as exc:
                    stage["status"] = "failed"
                    stage["error"] = (
                        "vLLM did not exit after SIGKILL: "
                        f"{redact_path_error(exc)}"
                    )
    if log_handle is not None:
        log_handle.close()

    host = str(vllm["host"])
    port = int(vllm["port"])
    deadline = time.monotonic() + 10.0
    while port_is_open(host, port) and time.monotonic() < deadline:
        time.sleep(0.25)
    if port_is_open(host, port):
        stage["status"] = "failed"
        stage["error"] = "vLLM port remained open after shutdown"
    stage["signal"] = signal_used
    stage["returncode"] = process.poll() if process is not None else None
    stage["port_released"] = not port_is_open(host, port)
    stage["elapsed_ms"] = round((time.monotonic() - started) * 1000.0, 3)
    print(f"[vllm_shutdown] {stage['status']} ({stage['elapsed_ms']:.1f} ms)", flush=True)
    return stage


def build_trtllm_command(
    config: Mapping[str, Any],
    paths: Mapping[str, Path],
    dataset: Path,
    report_json: Path,
    output_json: Path,
) -> list[str]:
    trtllm = require_object(config, "trtllm")
    return [
        str(paths["trtllm_bench"]),
        "-m",
        str(trtllm["model"]),
        "--model_path",
        str(paths["vllm_model"]),
        "throughput",
        "--engine_dir",
        str(paths["trtllm_engine_dir"]),
        "--backend",
        "tensorrt",
        "--dataset",
        str(dataset),
        "--num_requests",
        "1",
        "--warmup",
        str(trtllm["warmup"]),
        "--max_batch_size",
        str(trtllm["batch_size"]),
        "--max_num_tokens",
        str(trtllm["max_num_tokens"]),
        "--report_json",
        str(report_json),
        "--output_json",
        str(output_json),
    ]












def planned_stage(name: str, command: Sequence[str], artifact: str = "") -> Dict[str, Any]:
    stage: Dict[str, Any] = {"name": name, "status": "planned", "command": command_metadata(command)}
    if artifact:
        stage["artifact"] = artifact
    return stage


def finalize_run(
    manifest: Dict[str, Any],
    run_dir: Path,
    repo_root: Path,
    *,
    experiment_id: str,
    update_stable: bool,
) -> None:
    manifest["finished_at_utc"] = utc_now()
    manifest["run_directory"] = relative_label(run_dir, repo_root)
    write_json_object(run_dir / "run_manifest.json", manifest)
    if update_stable:
        output_root = run_dir.parents[1]
        write_json_object(
            output_root / "latest.json",
            {
                "schema_version": 1,
                "timestamp_utc": utc_now(),
                "experiment_id": experiment_id,
                "run_directory": relative_label(run_dir, repo_root),
                "manifest": relative_label(run_dir / "run_manifest.json", repo_root),
                "comparison": relative_label(run_dir / "backend_comparison.json", repo_root),
                "status": manifest["status"],
            },
        )


def run_showcase(args: argparse.Namespace) -> int:
    repo_root = Path(args.repo_root).expanduser().resolve()
    config_path = expand_config_path(repo_root, args.config)
    config = validate_config(read_json_object(config_path))
    run_id = args.run_id or default_run_id()
    if not RUN_ID_RE.fullmatch(run_id):
        raise ValueError("run id contains unsupported characters")
    experiment = require_object(config, "experiment")
    output_root = expand_config_path(repo_root, args.output_root)
    run_dir = output_root / str(experiment["id"]) / run_id
    if run_dir.exists():
        raise ValueError("run directory already exists; choose a different --run-id")
    raw_dir = run_dir / "raw"
    log_dir = run_dir / "logs"
    raw_dir.mkdir(parents=True)
    log_dir.mkdir(parents=True)
    write_json_object(run_dir / "config.snapshot.json", redact_value(config))
    paths = resolved_paths(repo_root, config)
    script_path = repo_root / "scripts" / "run_edge_cloud_showcase.py"

    manifest: Dict[str, Any] = {
        "schema_version": 1,
        "started_at_utc": utc_now(),
        "run_id": run_id,
        "status": "running" if args.execute else "planned",
        "execution_mode": "execute" if args.execute else "dry_run",
        "experiment": {
            "id": experiment["id"],
            "model_family": experiment["model_family"],
            "prompt_id": experiment["prompt_id"],
            "claim_scope": experiment["claim_scope"],
        },
        "stages": [],
    }

    input_command = [
        str(paths["vllm_python"]),
        str(script_path),
        "--repo-root",
        str(repo_root),
        "--config",
        str(config_path),
        "--prepare-inputs-only",
        "--run-dir",
        str(run_dir),
    ]
    native_cfg = require_object(config, "native")
    llama_cfg = require_object(config, "llama_cpp")
    vllm_cfg = require_object(config, "vllm")
    trt_cfg = require_object(config, "trtllm")
    native_command = [
        sys.executable,
        str(repo_root / "scripts" / "run_native_probes.py"),
        "--repo-root",
        str(repo_root),
        "--build-dir",
        str(paths["build_dir"]),
        "--results",
        str(raw_dir),
        "--output",
        str(raw_dir / "native_probe_run.json"),
        "--timeout-seconds",
        str(native_cfg["timeout_seconds"]),
        "--strict",
    ]
    llama_command = [
        sys.executable,
        str(repo_root / "scripts" / "run_llama_cpp_benchmark.py"),
        "--repo-root",
        str(repo_root),
        "--mode",
        "cli",
        "--llama-cli",
        str(paths["llama_cli"]),
        "--llama-bench",
        str(paths["llama_bench"]),
        "--model",
        str(paths["llama_model"]),
        "--prompt",
        str(experiment["prompt"]),
        "--threads",
        str(llama_cfg["threads"]),
        "--ctx-size",
        str(llama_cfg["ctx_size"]),
        "--batch-size",
        str(llama_cfg["batch_size"]),
        "--ubatch-size",
        str(llama_cfg["ubatch_size"]),
        "--n-predict",
        str(experiment["output_tokens"]),
        "--ngl",
        str(llama_cfg["ngl"]),
        "--timeout-seconds",
        str(llama_cfg["timeout_seconds"]),
        "--extra-arg=--single-turn",
        "--output",
        str(raw_dir / "llama_cpp_benchmark.json"),
    ]
    vllm_server_command = build_vllm_server_command(config, paths)
    vllm_benchmark_command = [
        str(paths["vllm_python"]),
        str(repo_root / "scripts" / "run_vllm_benchmark.py"),
        "--repo-root",
        str(repo_root),
        "--prompt-file",
        str(raw_dir / "showcase_prompt.jsonl"),
        "--max-prompts",
        "1",
        "--model",
        str(vllm_cfg["served_model_name"]),
        "--base-url",
        f"http://{vllm_cfg['host']}:{vllm_cfg['port']}",
        "--endpoint",
        "/v1/completions",
        "--request-kind",
        "completion",
        "--max-tokens",
        str(experiment["output_tokens"]),
        "--temperature",
        "0",
        "--request-timeout-seconds",
        str(vllm_cfg["request_timeout_seconds"]),
        "--recommended-port",
        str(vllm_cfg["port"]),
        "--output",
        str(raw_dir / "vllm_serving_benchmark.json"),
    ]
    direct_trt_command = build_trtllm_command(
        config,
        paths,
        raw_dir / "trtllm_dataset.json",
        raw_dir / "trtllm_report.json",
        raw_dir / "trtllm_output.json",
    )
    trt_wrapper_command = [
        str(paths["trtllm_python"]),
        str(repo_root / "scripts" / "run_trtllm_benchmark.py"),
        "--repo-root",
        str(repo_root),
        "--mode",
        "command",
        "--command",
        shlex.join(direct_trt_command),
        "--trtllm-bench",
        str(paths["trtllm_bench"]),
        "--engine-dir",
        str(paths["trtllm_engine_dir"]),
        "--model",
        str(experiment["model_family"]),
        "--batch-size",
        str(trt_cfg["batch_size"]),
        "--input-tokens",
        "{runtime_inputs.trtllm_input_tokens}",
        "--output-tokens",
        str(experiment["output_tokens"]),
        "--timeout-seconds",
        str(trt_cfg["timeout_seconds"]),
        "--output",
        str(raw_dir / "trtllm_benchmark.json"),
    ]
    compare_command = [
        sys.executable,
        str(repo_root / "scripts" / "compare_backends.py"),
        "--repo-root",
        str(repo_root),
        "--results",
        str(raw_dir),
        "--output",
        str(run_dir / "backend_comparison.json"),
    ]

    if not args.execute:
        manifest["stages"] = [
            planned_stage("input_preparation", input_command, "raw/runtime_inputs.json"),
            planned_stage("model_fingerprints", [], "model_artifacts.json"),
            planned_stage("trtllm_engine_contract", [], "raw/runtime_inputs.json"),
            planned_stage("native_mechanisms", native_command, "raw/native_probe_run.json"),
            planned_stage("llama_cpp_edge", llama_command, "raw/llama_cpp_benchmark.json"),
            planned_stage("vllm_startup", vllm_server_command, "logs/vllm_server.log"),
            planned_stage("vllm_cloud", vllm_benchmark_command, "raw/vllm_serving_benchmark.json"),
            {"name": "vllm_shutdown", "status": "planned"},
            planned_stage("trtllm_cloud", trt_wrapper_command, "raw/trtllm_benchmark.json"),
            planned_stage("backend_comparison", compare_command, "backend_comparison.json"),
        ]
        finalize_run(
            manifest,
            run_dir,
            repo_root,
            experiment_id=str(experiment["id"]),
            update_stable=False,
        )
        print(f"planned showcase at {relative_label(run_dir, repo_root)}")
        return 0

    common_env = os.environ.copy()
    common_env.update({"HF_HUB_OFFLINE": "1", "TRANSFORMERS_OFFLINE": "1"})
    stages: list[Dict[str, Any]] = manifest["stages"]
    input_stage = run_stage(
        "input_preparation",
        input_command,
        repo_root=repo_root,
        log_dir=log_dir,
        timeout_seconds=120,
        env=common_env,
        sensitive_contents=(str(experiment["prompt"]),),
    )
    enforce_artifact_status(input_stage, raw_dir / "runtime_inputs.json", ("ok",), repo_root)
    stages.append(input_stage)

    fingerprint_started = time.monotonic()
    try:
        fingerprints = build_model_fingerprints(config, paths)
        write_json_object(run_dir / "model_artifacts.json", fingerprints)
        fingerprint_status = "passed" if all(
            item.get("status") == "ok" for item in fingerprints["artifacts"].values()
        ) else "failed"
        fingerprint_stage = {
            "name": "model_fingerprints",
            "status": fingerprint_status,
            "elapsed_ms": round((time.monotonic() - fingerprint_started) * 1000.0, 3),
            "artifact": "model_artifacts.json",
        }
    except OSError as exc:
        fingerprint_stage = {
            "name": "model_fingerprints",
            "status": "failed",
            "elapsed_ms": round((time.monotonic() - fingerprint_started) * 1000.0, 3),
            "error": redact_path_error(exc, *paths.values()),
        }
    stages.append(fingerprint_stage)

    if input_stage["status"] == "passed":
        try:
            runtime_inputs = read_json_object(raw_dir / "runtime_inputs.json")
            engine_payload = read_json_object(paths["trtllm_engine_dir"] / "config.json")
            contract_errors = validate_trtllm_runtime_contract(
                engine_payload,
                input_tokens=int(runtime_inputs["trtllm_input_tokens"]),
                output_tokens=int(experiment["output_tokens"]),
                runtime_max_tokens=int(trt_cfg["max_num_tokens"]),
            )
            engine_contract_stage = {
                "name": "trtllm_engine_contract",
                "status": "passed" if not contract_errors else "failed",
                "input_tokens": int(runtime_inputs["trtllm_input_tokens"]),
                "output_tokens": int(experiment["output_tokens"]),
                "artifact": relative_label(paths["trtllm_engine_dir"] / "config.json", repo_root),
            }
            if contract_errors:
                engine_contract_stage["errors"] = contract_errors
        except (KeyError, TypeError, ValueError) as exc:
            engine_contract_stage = {
                "name": "trtllm_engine_contract",
                "status": "failed",
                "error": str(exc),
            }
    else:
        engine_contract_stage = {
            "name": "trtllm_engine_contract",
            "status": "blocked",
            "error": "runtime input preparation failed",
        }
    stages.append(engine_contract_stage)

    native_stage = run_stage(
        "native_mechanisms",
        native_command,
        repo_root=repo_root,
        log_dir=log_dir,
        timeout_seconds=float(native_cfg["timeout_seconds"]) * 20 + 30,
    )
    if native_stage["status"] == "passed":
        try:
            native_payload = read_json_object(raw_dir / "native_probe_run.json")
            counts = native_payload.get("counts", {})
            if not isinstance(counts, dict) or counts.get("passed") != 20 or any(
                counts.get(key, 0) for key in ("failed", "partial", "skipped")
            ):
                native_stage["status"] = "failed"
                native_stage["error"] = "native probe coverage did not finish at 20/20"
        except ValueError as exc:
            native_stage["status"] = "failed"
            native_stage["error"] = str(exc)
    native_stage["artifact"] = "raw/native_probe_run.json"
    stages.append(native_stage)

    llama_stage = run_stage(
        "llama_cpp_edge",
        llama_command,
        repo_root=repo_root,
        log_dir=log_dir,
        timeout_seconds=float(llama_cfg["timeout_seconds"]) + 15,
        sensitive_contents=(str(experiment["prompt"]),),
    )
    enforce_artifact_status(llama_stage, raw_dir / "llama_cpp_benchmark.json", ("ok",), repo_root)
    stages.append(llama_stage)

    vllm_env = dict(common_env)
    vllm_env["VLLM_USE_V2_MODEL_RUNNER"] = "1" if vllm_cfg["use_v2_model_runner"] else "0"
    vllm_env["VLLM_NO_USAGE_STATS"] = "1"
    service: subprocess.Popen[bytes] | None = None
    service_log: Any = None
    startup_stage: Dict[str, Any]
    try:
        service, service_log, startup_stage = start_vllm_service(
            config,
            paths,
            repo_root=repo_root,
            log_dir=log_dir,
            env=vllm_env,
        )
        stages.append(startup_stage)
        if startup_stage["status"] == "passed":
            vllm_stage = run_stage(
                "vllm_cloud",
                vllm_benchmark_command,
                repo_root=repo_root,
                log_dir=log_dir,
                timeout_seconds=float(vllm_cfg["request_timeout_seconds"]) + 30,
                env=vllm_env,
            )
            enforce_artifact_status(vllm_stage, raw_dir / "vllm_serving_benchmark.json", ("ok",), repo_root)
        else:
            vllm_stage = {
                "name": "vllm_cloud",
                "status": "blocked",
                "error": "vLLM startup did not become healthy",
                "artifact": "raw/vllm_serving_benchmark.json",
            }
        stages.append(vllm_stage)
    finally:
        stages.append(stop_vllm_service(service, service_log, config))

    if input_stage["status"] == "passed" and engine_contract_stage["status"] == "passed":
        try:
            runtime_inputs = read_json_object(raw_dir / "runtime_inputs.json")
            input_tokens = int(runtime_inputs["trtllm_input_tokens"])
            input_index = trt_wrapper_command.index("--input-tokens") + 1
            trt_wrapper_command[input_index] = str(input_tokens)
        except (ValueError, KeyError, TypeError):
            input_tokens = 0
        trt_env = dict(common_env)
        trt_env[f"OMPI_MCA_btl_tcp_if_include"] = str(trt_cfg["mpi_interface"])
        trt_env[f"OMPI_MCA_oob_tcp_if_include"] = str(trt_cfg["mpi_interface"])
        trt_stage = run_stage(
            "trtllm_cloud",
            trt_wrapper_command,
            repo_root=repo_root,
            log_dir=log_dir,
            timeout_seconds=float(trt_cfg["timeout_seconds"]) + 30,
            env=trt_env,
        )
        enforce_artifact_status(trt_stage, raw_dir / "trtllm_benchmark.json", ("ok",), repo_root)
        trt_stage["input_tokens"] = input_tokens
    else:
        trt_stage = {
            "name": "trtllm_cloud",
            "status": "blocked",
            "error": "runtime input or TensorRT-LLM engine contract failed",
            "artifact": "raw/trtllm_benchmark.json",
        }
    stages.append(trt_stage)

    compare_stage = run_stage(
        "backend_comparison",
        compare_command,
        repo_root=repo_root,
        log_dir=log_dir,
        timeout_seconds=60,
    )
    comparison_path = run_dir / "backend_comparison.json"
    if not comparison_path.is_file() and compare_stage["status"] == "passed":
        compare_stage["status"] = "failed"
        compare_stage["error"] = "backend comparison artifact is missing"
    compare_stage["artifact"] = "backend_comparison.json"
    stages.append(compare_stage)
    manifest["status"] = "passed" if all(stage.get("status") == "passed" for stage in stages) else "failed"
    finalize_run(
        manifest,
        run_dir,
        repo_root,
        experiment_id=str(experiment["id"]),
        update_stable=manifest["status"] == "passed",
    )
    print(f"showcase {manifest['status']}: {relative_label(run_dir, repo_root)}", flush=True)
    return 0 if manifest["status"] == "passed" else 1


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--config", default=DEFAULT_CONFIG)
    parser.add_argument("--output-root", default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--run-id", default="")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--prepare-inputs-only", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--run-dir", default="", help=argparse.SUPPRESS)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        repo_root = Path(args.repo_root).expanduser().resolve()
        config = validate_config(read_json_object(expand_config_path(repo_root, args.config)))
        if args.prepare_inputs_only:
            if not args.run_dir:
                raise ValueError("--run-dir is required with --prepare-inputs-only")
            return prepare_inputs(config, repo_root, Path(args.run_dir).expanduser().resolve())
        return run_showcase(args)
    except (OSError, ValueError) as exc:
        print(f"showcase error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
