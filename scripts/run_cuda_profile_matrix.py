#!/usr/bin/env python3
"""Plan or execute CUDA correctness and Nsight profiling targets.

The default mode is planning-only. On a CUDA machine, pass --execute to
configure/build CUDA targets and optionally wrap them with ncu/nsys.
"""

from __future__ import annotations

import argparse
import json
import shlex
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Sequence

from bounded_process import command_result_succeeded as command_ok, run_bounded_process
from cli_validation import require_int_range
from command_redaction import (
    artifact_command_parts,
    artifact_output_tail,
    artifact_path_payload as path_payload,
    command_line,
    redact_path_error,
    redact_sensitive_text,
    redacted_path_label as path_label,
)
from path_status import safe_is_file, tool_availability as tool
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import try_write_text_file as write_text_file


DEFAULT_TARGETS = [
    {"name": "matrix_ops", "target": "matrix_ops_cuda_test", "binary": "matrix_ops_cuda_test", "kind": "gemm_gemv_softmax"},
    {"name": "normalization", "target": "normalization_cuda_test", "binary": "normalization_cuda_test", "kind": "rmsnorm"},
    {"name": "rope", "target": "rope_cuda_test", "binary": "rope_cuda_test", "kind": "rotary_embedding"},
    {"name": "attention", "target": "attention_cuda_test", "binary": "attention_cuda_test", "kind": "scaled_dot_product_attention"},
    {"name": "int4_dequant_matvec", "target": "quantization_cuda_test", "binary": "quantization_cuda_test", "kind": "weight_only_int4"},
    {"name": "paged_kv_gather", "target": "paged_kv_cuda_test", "binary": "paged_kv_cuda_test", "kind": "paged_kv"},
]
CUDA_COMMAND_STREAM_LIMIT_BYTES = 16 * 1024 * 1024


def run(command: Sequence[str], timeout: int, cwd: Path | None = None) -> Dict[str, Any]:
    command_parts = [str(part) for part in command]
    redacted_command = artifact_command_parts(command_parts)
    capture = run_bounded_process(
        command_parts,
        cwd=cwd,
        timeout_seconds=float(timeout),
        stdout_limit_bytes=CUDA_COMMAND_STREAM_LIMIT_BYTES,
        stderr_limit_bytes=CUDA_COMMAND_STREAM_LIMIT_BYTES,
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
        "stdout_tail": artifact_output_tail(stdout, command_parts, cwd, 80),
        "stderr_tail": artifact_output_tail(stderr, command_parts, cwd, 80),
        "timed_out": capture.timed_out,
    }
    if capture.status in {"ok", "nonzero_exit"}:
        return result

    result["returncode"] = None
    if capture.status == "launch_failed" and capture.error == "missing_executable":
        result["elapsed_ms"] = 0
        result["stderr_tail"] = "missing executable"
        result["error"] = "missing_executable"
    elif capture.status == "launch_failed":
        result["stderr_tail"] = capture.error
        result["error"] = capture.error
    elif capture.status == "timed_out":
        result["error"] = redact_sensitive_text(f"timed out after {timeout}s")
    else:
        result["error"] = redact_sensitive_text(capture.error or capture.status)
    return result


def binary_status(path: Path) -> Dict[str, Any]:
    exists, error = safe_is_file(path)
    status: Dict[str, Any] = {**path_payload(path), "exists": exists}
    if error:
        status["status"] = "path_probe_failed"
        status["error"] = error
    else:
        status["status"] = "present" if exists else "missing"
    return status


def selected_targets(args: argparse.Namespace) -> tuple[List[Dict[str, str]], List[str]]:
    if not args.target:
        return DEFAULT_TARGETS, []
    wanted = set(args.target)
    selected = [item for item in DEFAULT_TARGETS if item["name"] in wanted or item["target"] in wanted]
    known = {item["name"] for item in DEFAULT_TARGETS} | {item["target"] for item in DEFAULT_TARGETS}
    unknown = [item for item in args.target if item not in known]
    return selected, unknown


def profile_commands(args: argparse.Namespace, binary: Path, name: str, profiling_root: Path) -> List[List[str]]:
    commands: List[List[str]] = []
    output_base = profiling_root / name
    if args.profile_tool in {"ncu", "both"}:
        commands.append([
            args.ncu,
            "--set",
            args.ncu_set,
            "--target-processes",
            "all",
            "-o",
            str(output_base.with_suffix(".ncu-rep")),
            str(binary),
        ])
    if args.profile_tool in {"nsys", "both"}:
        commands.append([
            args.nsys,
            "profile",
            "-o",
            str(output_base.with_suffix(".nsys-rep")),
            "--force-overwrite=true",
            str(binary),
        ])
    return commands


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    targets, unknown_targets = selected_targets(args)
    build_dir = resolve_repo_path(repo_root, args.build_dir)
    profiling_root = resolve_repo_path(repo_root, args.profiling_root)
    target_selection = {
        "requested": list(args.target),
        "selected": [item["name"] for item in targets],
        "unknown": unknown_targets,
        "available": [{"name": item["name"], "target": item["target"]} for item in DEFAULT_TARGETS],
    }
    configure = ["cmake", "-S", str(repo_root), "-B", str(build_dir), "-DINFERLAB_ENABLE_CUDA=ON", f"-DCMAKE_BUILD_TYPE={args.build_type}"]
    configure.extend(args.cmake_arg)
    build = ["cmake", "--build", str(build_dir), "-j", str(args.jobs)]
    for item in targets:
        build.extend(["--target", item["target"]])
    runs = []
    commands = [
        {"stage": "configure", "command": artifact_command_parts(configure)},
        {"stage": "build", "command": artifact_command_parts(build)},
    ]
    for item in targets:
        binary = build_dir / item["binary"]
        command = [str(binary)]
        commands.append({"stage": "target", "name": item["name"], "command": artifact_command_parts(command)})
        for profile in profile_commands(args, binary, item["name"], profiling_root):
            commands.append({"stage": "profile", "name": item["name"], "command": artifact_command_parts(profile)})

    executed = []
    setup_errors: List[Dict[str, Any]] = []
    target_selection_invalid = bool(unknown_targets) or (bool(args.target) and not targets)
    if args.execute and not target_selection_invalid:
        if args.profile_tool != "none":
            try:
                profiling_root.mkdir(parents=True, exist_ok=True)
            except OSError as exc:
                setup_errors.append({"stage": "profiling_root", **path_payload(profiling_root), "error": redact_path_error(exc, profiling_root)})
        configure_result = run(configure, args.timeout_seconds, cwd=repo_root)
        executed.append({"stage": "configure", "result": configure_result})
        build_result = (
            run(build, args.timeout_seconds, cwd=repo_root)
            if command_ok(configure_result)
            else {
                "returncode": None,
                "skipped": True,
                "reason": "configure_failed",
            }
        )
        executed.append({"stage": "build", "result": build_result})
        configure_build_ok = command_ok(configure_result) and command_ok(build_result) and not setup_errors
        for item in targets:
            binary = build_dir / item["binary"]
            status = binary_status(binary)
            if not configure_build_ok:
                runs.append({
                    "name": item["name"],
                    "target": item["target"],
                    **path_payload(binary, "binary"),
                    "binary_status": status,
                    "status": "skipped_configure_or_build_failed",
                    "result": {"returncode": None, "skipped": True, "reason": "configure_or_build_failed"},
                    "profiles": [],
                })
                continue
            target_result = run([str(binary)], args.timeout_seconds, cwd=repo_root)
            profile_results = []
            if command_ok(target_result):
                for profile in profile_commands(args, binary, item["name"], profiling_root):
                    profile_results.append(run(profile, args.timeout_seconds, cwd=repo_root))
            runs.append({
                "name": item["name"],
                "target": item["target"],
                **path_payload(binary, "binary"),
                "binary_status": status,
                "status": "ok" if command_ok(target_result) and all(command_ok(result) for result in profile_results) else "failed",
                "result": target_result,
                "profiles": profile_results,
            })

    configure_build_failed = sum(
        1
        for item in executed
        if not item.get("result", {}).get("skipped")
        and not command_ok(item.get("result", {}))
    )
    failed_runs = sum(1 for item in runs if item.get("status") != "ok")
    failed_profiles = sum(1 for item in runs for result in item.get("profiles", []) if not command_ok(result))
    failed = configure_build_failed + failed_runs + len(setup_errors)
    skipped = sum(1 for item in runs if str(item.get("status", "")).startswith("skipped"))
    status = "planned"
    if target_selection_invalid:
        status = "invalid_target_selection"
    elif args.execute:
        status = "ok" if failed == 0 and skipped == 0 else "failed"
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "status": status,
        "execute_requested": bool(args.execute),
        "executed": bool(args.execute and not target_selection_invalid),
        "setup_errors": setup_errors,
        "preflight": {
            "cmake": tool("cmake"),
            "nvcc": tool("nvcc"),
            "nvidia_smi": tool("nvidia-smi"),
            "ncu": tool(args.ncu),
            "nsys": tool(args.nsys),
        },
        "config": {
            **path_payload(args.build_dir, "build_dir", "build_dir_path_redacted"),
            **path_payload(build_dir, "resolved_build_dir"),
            "build_type": args.build_type,
            "profile_tool": args.profile_tool,
            **path_payload(args.profiling_root, "profiling_root", "profiling_root_path_redacted"),
            **path_payload(profiling_root, "resolved_profiling_root"),
        },
        "commands": [{"stage": item["stage"], "name": item.get("name"), "command": list(item["command"]), "shell": command_line(artifact_command_parts(item["command"]))} for item in commands],
        "target_selection": target_selection,
        "targets": targets,
        "configure_build": executed,
        "runs": runs,
        "summary": {
            "target_count": len(targets),
            "requested_target_count": len(args.target),
            "unknown_target_count": len(unknown_targets),
            "run_count": len(runs),
            "configure_build_failed": configure_build_failed,
            "failed_runs": failed_runs,
            "failed_profiles": failed_profiles,
            "setup_error_count": len(setup_errors),
            "skipped_runs": skipped,
            "failure_count": failed,
            "planned_profile_commands": sum(1 for item in commands if item["stage"] == "profile"),
        },
    }




def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--build-dir", default="build-cuda")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--cmake-arg", action="append", default=[])
    parser.add_argument("--profile-tool", choices=["none", "ncu", "nsys", "both"], default="none")
    parser.add_argument("--profiling-root", default="profiling/nsight_compute")
    parser.add_argument("--ncu", default="ncu")
    parser.add_argument("--ncu-set", default="full")
    parser.add_argument("--nsys", default="nsys")
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    parser.add_argument("--output", default="results/raw/cuda_profile_matrix.json")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--jobs", args.jobs, 1, 1024)
    require_int_range("--timeout-seconds", args.timeout_seconds, 1, 86_400)


def main() -> int:
    args = parse_args()
    validate_args(args)
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
