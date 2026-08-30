#!/usr/bin/env python3
"""Plan or execute Android/ARM llama.cpp edge checks through adb."""

from __future__ import annotations

import argparse
import json
import shlex
import shutil
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Sequence

from android_device_evidence import (
    build_device_evidence,
    execute_probe_records,
    footprint_probe_spec,
    identity_probe_specs,
    parse_device_identity,
    planned_probe_records,
    probe_command_entries,
    resource_probe_specs,
    validate_android_abi,
    validate_android_package_name,
)
from cli_validation import require_int_range
from android_benchmark_output import (
    aggregate_android_runtime_observations,
    aggregate_android_benchmark_status,
    artifact_command_parts,
    bounded_probe_stdout,
    command_projection,
    decoded_process_output,
    llama_diagnostic_output,
    parse_llama_output,
    path_label,
    project_android_scenario_observation,
    public_command_results,
    simpleperf_placeholder,
    utc_now,
    validate_output_path,
)
from bounded_process import command_result_succeeded as command_ok, run_bounded_process
from text_output import try_write_text_file as write_text_file

from command_redaction import (
    REDACTED_VALUE,
    artifact_path_payload as path_payload,
    redact_artifact_output,
    redact_path_error,
    redact_value,
    redacted_path_label,
)
from path_status import safe_is_file
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root

DEFAULT_THREADS = [1, 2, 4, 8]
ANDROID_COMMAND_STREAM_LIMIT_BYTES = 16 * 1024 * 1024
ANDROID_PROBE_STREAM_LIMIT_BYTES = 64 * 1024


def output_tail(text: str, command: Sequence[str], cwd: Path | None = None, *, paths: Sequence[Any] = (), contents: Sequence[Any] = ()) -> str:
    tail = "\n".join(str(text).splitlines()[-120:])
    return redact_artifact_output(tail, paths=[cwd, *paths, *command], contents=contents)


def adb_command(args: argparse.Namespace, shell: str) -> List[str]:
    command = [args.adb]
    if args.serial:
        command.extend(["-s", args.serial])
    command.extend(["shell", shell])
    return command


def adb_base_command(args: argparse.Namespace) -> List[str]:
    command = [args.adb]
    if args.serial:
        command.extend(["-s", args.serial])
    return command


def run(
    command: Sequence[str],
    timeout: int,
    cwd: Path | None = None,
    *,
    paths: Sequence[Any] = (),
    contents: Sequence[Any] = (),
    output_observer: Callable[[str], Dict[str, Any]] | None = None,
    artifact_output_filter: Callable[[str], str] | None = None,
    observer_uses_stdout_only: bool = False,
    observer_uses_stderr_only: bool = False,
    artifact_filter_uses_stderr_only: bool = False,
    stream_limit_bytes: int = ANDROID_COMMAND_STREAM_LIMIT_BYTES,
) -> Dict[str, Any]:
    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=float(timeout),
        stdout_limit_bytes=stream_limit_bytes,
        stderr_limit_bytes=stream_limit_bytes,
        launch_error_formatter=lambda exc: (
            "missing_executable"
            if isinstance(exc, FileNotFoundError)
            else redact_path_error(exc, cwd, *command, *paths)
        ),
    )
    stdout = decoded_process_output(capture.stdout)
    stderr = decoded_process_output(capture.stderr)
    output = stdout + stderr
    filter_input = stderr if artifact_filter_uses_stderr_only else output
    artifact_output = artifact_output_filter(filter_input) if artifact_output_filter else filter_input
    result = {
        "command": artifact_command_parts(command),
        **path_payload(cwd or "", "cwd"),
        "process_status": capture.status,
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_bytes": capture.stdout_bytes,
        "stderr_bytes": capture.stderr_bytes,
        "stream_limit_bytes": stream_limit_bytes,
        "output_tail": output_tail(artifact_output, command, cwd, paths=paths, contents=contents),
        "timed_out": capture.timed_out,
        "output_limit_exceeded": capture.output_limit_exceeded,
    }
    if capture.status not in {"ok", "nonzero_exit"}:
        result["error"] = capture.error or capture.status
    if output_observer is not None:
        observer_input = stdout if observer_uses_stdout_only else stderr if observer_uses_stderr_only else output
        result["output_observation"] = output_observer(observer_input)
    return result


def remote_llama_shell(args: argparse.Namespace, device_model: str, threads: int) -> str:
    parts = [
        shlex.quote(args.device_llama_cli),
        "-m",
        shlex.quote(device_model),
        "-p",
        shlex.quote(args.prompt),
        "-n",
        str(args.n_predict),
        "-c",
        str(args.ctx_size),
        "-b",
        str(args.batch_size),
        "-t",
        str(threads),
    ]
    if args.ngl is not None:
        parts.extend(["-ngl", str(args.ngl)])
    command = " ".join(parts)
    if args.simpleperf:
        command = f"simpleperf stat {command}"
    return command


def prepare_model_push(
    args: argparse.Namespace,
    repo_root: Path,
    host_model: Path,
    device_model: str,
    required_identity_gap_count: int,
) -> tuple[Dict[str, Any], Dict[str, Any], int, Dict[str, Any] | None]:
    model_probe: Dict[str, Any] = {
        "requested": bool(args.push_model),
        **path_payload(args.model, "model", "model_path_redacted"),
        **path_payload(host_model if args.model else "", "resolved_model"),
    }
    push_result: Dict[str, Any] = {"requested": bool(args.push_model), "attempted": False, "status": "not_requested"}
    setup_failures = 0
    push_command_entry = None
    if args.push_model and args.model:
        model_exists, model_error = safe_is_file(host_model)
        model_probe.update({"exists": model_exists, "status": "present" if model_exists else "missing"})
        if model_error:
            model_probe.update({"status": "path_probe_failed", "error": redact_path_error(model_error, host_model)})
        push_command = adb_base_command(args)
        push_command.extend(["push", str(host_model), device_model])
        push_command_entry = {"stage": "push_model", **command_projection(push_command)}
        push_result = {"requested": True, "attempted": False, "status": "planned"}
        if args.execute:
            if model_error:
                push_result = {"requested": True, "attempted": False, "status": "model_path_probe_failed", "error": redact_path_error(model_error, host_model)}
                setup_failures += 1
            elif not model_exists:
                push_result = {"requested": True, "attempted": False, "status": "model_missing", **path_payload(host_model)}
                setup_failures += 1
            elif required_identity_gap_count:
                push_result = {"requested": True, "attempted": False, "status": "skipped_required_device_identity"}
            else:
                result = run(push_command, args.timeout_seconds, cwd=repo_root, paths=[host_model])
                push_result = {"requested": True, "attempted": True, "status": "ok" if command_ok(result) else "failed", "result": result}
                setup_failures += int(not command_ok(result))
    elif args.push_model:
        model_probe["status"] = "missing_model_argument"
        push_result = {"requested": True, "attempted": False, "status": "missing_model_argument"}
        setup_failures += 1
    return model_probe, push_result, setup_failures, push_command_entry


def run_scenarios(
    args: argparse.Namespace,
    repo_root: Path,
    host_model: Path,
    device_model: str,
    scenario_blocked: bool,
    setup_failures: int,
) -> tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    records: List[Dict[str, Any]] = []
    commands: List[Dict[str, Any]] = []
    for threads in args.threads:
        command = adb_command(args, remote_llama_shell(args, device_model, threads))
        projected_command = command_projection(command)
        record = {
            "threads": threads,
            **projected_command,
            "status": "planned",
            "attempted": False,
            "simpleperf_observation": simpleperf_placeholder(args.simpleperf, "planned"),
        }
        if args.execute and scenario_blocked:
            reason = "push_model_or_setup_failed" if setup_failures else "required_device_identity_missing"
            record.update({
                "status": "skipped_preflight_failed",
                "result": {"returncode": None, "skipped": True, "reason": reason},
                "timings": {},
                "metric_status": "not_attempted",
                "backend_observation": {"status": "not_attempted", "reported_backends": []},
                "simpleperf_observation": simpleperf_placeholder(args.simpleperf, "skipped_preflight"),
            })
        elif args.execute:
            record["attempted"] = True
            result = run(
                command,
                args.timeout_seconds,
                cwd=repo_root,
                paths=[host_model] if args.model else (),
                contents=[args.prompt],
                output_observer=lambda text: parse_llama_output(text, args.simpleperf),
                artifact_output_filter=llama_diagnostic_output,
                observer_uses_stderr_only=True,
                artifact_filter_uses_stderr_only=True,
            )
            output_observation = result.pop("output_observation", {})
            record.update(project_android_scenario_observation(output_observation, command_ok(result), args.simpleperf))
            record["result"] = result
        records.append(record)
        commands.append({"stage": "llama_cpp", "threads": threads, **projected_command})
    return records, commands


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    model_name = Path(args.model).name if args.model else "model.gguf"
    host_model = resolve_repo_path(repo_root, args.model) if args.model else Path()
    device_model = f"{args.device_dir}/{model_name}"
    identity_specs = identity_probe_specs()
    resource_specs = resource_probe_specs()
    footprint_spec = footprint_probe_spec(args.device_llama_cli, device_model, args.package_name)
    command_builder = lambda shell: adb_command(args, shell)
    command_projector = command_projection
    command_executor = lambda command: run(
        command,
        args.timeout_seconds,
        cwd=repo_root,
        output_observer=bounded_probe_stdout,
        observer_uses_stdout_only=True,
        stream_limit_bytes=ANDROID_PROBE_STREAM_LIMIT_BYTES,
    )
    identity_records = planned_probe_records(identity_specs, "device_identity", command_builder, command_projector)
    before_resource_records = planned_probe_records(resource_specs, "resource_before", command_builder, command_projector)
    footprint_records = planned_probe_records([footprint_spec], "deployment_footprint", command_builder, command_projector)
    after_resource_records = planned_probe_records(resource_specs, "resource_after", command_builder, command_projector)
    if args.execute:
        identity_records = execute_probe_records(identity_specs, "device_identity", command_builder, command_projector, command_executor, command_ok)
    identity = parse_device_identity(identity_records, args.required_abi, args.minimum_api_level) if args.execute else {"required_gaps": []}
    required_identity_gap_count = len(identity.get("required_gaps", []))
    model_probe, push_result, setup_failures, push_command_entry = prepare_model_push(
        args, repo_root, host_model, device_model, required_identity_gap_count
    )

    resource_window_started = None
    if args.execute:
        resource_window_started = time.perf_counter()
        before_resource_records = execute_probe_records(resource_specs, "resource_before", command_builder, command_projector, command_executor, command_ok)
        footprint_records = execute_probe_records([footprint_spec], "deployment_footprint", command_builder, command_projector, command_executor, command_ok)
    scenario_blocked = bool(setup_failures or required_identity_gap_count)
    records, scenario_commands = run_scenarios(
        args, repo_root, host_model, device_model, scenario_blocked, setup_failures
    )
    if args.execute:
        after_resource_records = execute_probe_records(resource_specs, "resource_after", command_builder, command_projector, command_executor, command_ok)
    commands = probe_command_entries(identity_records)
    if push_command_entry is not None:
        commands.append(push_command_entry)
    commands.extend(probe_command_entries([*before_resource_records, *footprint_records]))
    commands.extend(scenario_commands)
    commands.extend(probe_command_entries(after_resource_records))
    attempted_count = sum(bool(item.get("attempted")) for item in records)
    resource_window_duration_ms = (
        round((time.perf_counter() - resource_window_started) * 1000.0, 3)
        if resource_window_started is not None and attempted_count
        else None
    )
    preflight_probe_records = [*identity_records, *before_resource_records, *footprint_records]
    failed_preflight = sum(1 for item in preflight_probe_records if item.get("status") == "failed")
    failed_after_probe_count = sum(1 for item in after_resource_records if item.get("status") == "failed")
    device_evidence = build_device_evidence(
        identity_records,
        before_resource_records,
        after_resource_records,
        footprint_records[0] if footprint_records else None,
        resource_window_duration_ms,
        bool(args.package_name),
        bool(args.execute),
        args.required_abi,
        args.minimum_api_level,
    )
    actual_execution = attempted_count > 0
    runtime_observations = aggregate_android_runtime_observations(records, args.ngl, args.simpleperf)
    return {
        "schema_version": 2,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "mode": "execute" if args.execute else "plan",
        "status": aggregate_android_benchmark_status(
            records,
            bool(args.execute),
            setup_failures + required_identity_gap_count,
            str(device_evidence.get("status", "planned")),
        ),
        "execution_requested": bool(args.execute),
        "executed": actual_execution,
        "environment": {
            **path_payload(shutil.which(args.adb) or "missing", "adb", "adb_path_redacted"),
            "serial": redacted_path_label(args.serial) if args.serial else "",
            "serial_redacted": bool(args.serial),
        },
        "config": redact_value({
            **vars(args),
            **path_payload(args.model, "model", "model_path_redacted"),
            "prompt": REDACTED_VALUE if args.prompt else "",
            "prompt_redacted": bool(args.prompt),
            "serial": redacted_path_label(args.serial) if args.serial else "",
            "serial_redacted": bool(args.serial),
            **path_payload(host_model if args.model else "", "resolved_model"),
        }),
        "model_probe": model_probe,
        "commands": commands,
        "preflight_results": (
            public_command_results([*identity_records, *before_resource_records, *footprint_records])
            if args.execute
            else []
        ),
        "resource_after_results": public_command_results(after_resource_records) if args.execute else [],
        "device_evidence": device_evidence,
        "backend_observation": runtime_observations["backend_observation"],
        "push_model_result": push_result,
        "records": records,
        "summary": {
            "scenario_count": len(records),
            "execution_requested": bool(args.execute),
            "executed": actual_execution,
            "executed_count": attempted_count,
            "ok_count": sum(1 for item in records if item["status"] == "ok"),
            "partial_count": sum(1 for item in records if str(item["status"]).startswith("partial")),
            "failed_count": sum(1 for item in records if item["status"] == "failed"),
            "skipped_count": sum(1 for item in records if str(item["status"]).startswith("skipped")),
            "setup_failure_count": setup_failures,
            "preflight_failure_count": failed_preflight,
            "resource_after_failure_count": failed_after_probe_count,
            "device_probe_command_failure_count": failed_preflight + failed_after_probe_count,
            "required_identity_gap_count": required_identity_gap_count,
            "device_evidence_status": device_evidence.get("status"),
            **runtime_observations["simpleperf_summary"],
            "push_model_attempted": bool(push_result.get("attempted")),
            "push_model_status": push_result.get("status"),
        },
        "claim_scope": "external Android llama.cpp CLI timing, optional process-window simpleperf counters and structured ADB device/resource observations; plan, skipped, partial telemetry, backend log names, simpleperf counters and signed battery-counter changes are not JNI, per-operator attribution, delegation, workload-exclusive energy, production or release evidence",
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial", default="")
    parser.add_argument("--device-dir", default="/data/local/tmp/inferlab")
    parser.add_argument("--device-llama-cli", default="/data/local/tmp/inferlab/llama-cli")
    parser.add_argument("--required-abi", default="arm64-v8a")
    parser.add_argument("--minimum-api-level", type=int, default=24)
    parser.add_argument("--package-name", default="")
    parser.add_argument("--model", default="")
    parser.add_argument("--push-model", action="store_true")
    parser.add_argument("--prompt", default="Explain KV cache in one sentence.")
    parser.add_argument("--threads", type=int, action="append", default=None)
    parser.add_argument("--ctx-size", type=int, default=2048)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--n-predict", type=int, default=128)
    parser.add_argument("--ngl", type=int)
    parser.add_argument("--simpleperf", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, default=900)
    parser.add_argument("--output", default="results/raw/android_edge_benchmark.json")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if args.threads is None:
        args.threads = list(DEFAULT_THREADS)
    if not args.threads:
        raise SystemExit("--threads must contain at least one value")
    if len(set(args.threads)) != len(args.threads):
        raise SystemExit("--threads values must be unique")
    for index, threads in enumerate(args.threads, start=1):
        require_int_range(f"--threads[{index}]", threads, 1, 4096)
    require_int_range("--ctx-size", args.ctx_size, 1, 10_000_000)
    require_int_range("--batch-size", args.batch_size, 1, 10_000_000)
    require_int_range("--n-predict", args.n_predict, 1, 10_000_000)
    if args.ngl is not None:
        require_int_range("--ngl", args.ngl, -1, 1_000_000)
    require_int_range("--timeout-seconds", args.timeout_seconds, 1, 86_400)
    require_int_range("--minimum-api-level", args.minimum_api_level, 1, 1000)
    for name, value in (("--device-dir", args.device_dir), ("--device-llama-cli", args.device_llama_cli)):
        if not value.startswith("/") or len(value.encode("utf-8", errors="replace")) > 4096 or any(ord(ch) < 32 for ch in value):
            raise SystemExit(f"{name} must be a nonempty absolute Android path up to 4096 bytes")
    if "\x00" in args.prompt or len(args.prompt.encode("utf-8", errors="replace")) > 1024 * 1024:
        raise SystemExit("--prompt must be valid command text up to 1 MiB without NUL")
    if any(ord(ch) < 32 for ch in args.serial) or len(args.serial.encode("utf-8", errors="replace")) > 256:
        raise SystemExit("--serial must be at most 256 bytes without control characters")
    if args.model and (Path(args.model).name in {"", ".", ".."} or any(ord(ch) < 32 for ch in args.model)):
        raise SystemExit("--model must identify a file name")
    try:
        validate_android_abi(args.required_abi)
    except ValueError as exc:
        raise SystemExit(f"--required-abi: {exc}") from exc
    try:
        validate_android_package_name(args.package_name)
    except ValueError as exc:
        raise SystemExit(f"--package-name: {exc}") from exc


def main() -> int:
    args = parse_args()
    validate_args(args)
    repo_root = resolved_repo_root(args.repo_root)
    output = resolve_repo_path(repo_root, args.output)
    protected = [resolve_repo_path(repo_root, args.model)] if args.model else []
    try:
        validate_output_path(output, protected)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    payload = build_payload(args, repo_root)
    output_status = write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    if output_status["status"] == "ok":
        print(f"wrote {path_label(output)}")
    else:
        print(f"failed to write {path_label(output)}: {output_status.get('error')}")
    return 0 if output_status["status"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
