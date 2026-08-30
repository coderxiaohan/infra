#!/usr/bin/env python3
"""Probe CUDA availability without requiring CUDA to be installed."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path
from typing import Dict, List, Optional

try:
    from scripts.bounded_process import run_bounded_process
except ModuleNotFoundError:
    from bounded_process import run_bounded_process
from command_redaction import redact_artifact_output, redact_path_error, redacted_path_label
from cli_validation import require_float_range
from repo_paths import default_repo_root as default_project_root
from text_output import first_text_line as first_line, write_text_file
from time_utils import utc_now_precise

DEFAULT_COMMAND_TIMEOUT_SECONDS = 15.0
DEFAULT_CMAKE_TIMEOUT_SECONDS = 120.0
COMMAND_OUTPUT_LIMIT_BYTES = 4 * 1024 * 1024


def resolve_project_path(project_root: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else project_root / path


def run(
    command: List[str],
    cwd: Optional[Path] = None,
    timeout: float = DEFAULT_COMMAND_TIMEOUT_SECONDS,
) -> Dict[str, object]:
    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=timeout,
        stdout_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        merge_stderr_into_stdout=True,
        launch_error_formatter=lambda exc: (
            "missing" if isinstance(exc, FileNotFoundError) else redact_path_error(exc, cwd, *command)
        ),
    )
    output = capture.stdout.decode("utf-8", errors="replace").strip()
    result: Dict[str, object] = {
        "returncode": capture.returncode if capture.status in {"ok", "nonzero_exit"} else None,
        "output": redact_artifact_output(output, paths=[cwd, *command]),
        "timed_out": capture.timed_out,
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "captured_output_bytes": capture.stdout_bytes,
    }
    if capture.status == "launch_failed" and capture.error == "missing":
        result["output"] = "missing"
    if capture.timed_out:
        result["error"] = f"timed out after {timeout}s"
    elif capture.output_limit_exceeded:
        result["error"] = f"command output exceeded {COMMAND_OUTPUT_LIMIT_BYTES} bytes"
    elif capture.error:
        result["error"] = capture.error
    return result


def collect(
    project_root: Path,
    try_cmake: bool,
    command_timeout: float = DEFAULT_COMMAND_TIMEOUT_SECONDS,
    cmake_timeout: float = DEFAULT_CMAKE_TIMEOUT_SECONDS,
) -> Dict[str, object]:
    nvcc_path = shutil.which("nvcc")
    nvidia_smi_path = shutil.which("nvidia-smi")
    cmake_path = shutil.which("cmake")

    nvcc = run(["nvcc", "--version"], timeout=command_timeout) if nvcc_path else {"returncode": None, "output": "missing", "timed_out": False}
    nvidia_smi = (
        run(["nvidia-smi", "--query-gpu=name,memory.total,driver_version", "--format=csv,noheader"], timeout=command_timeout)
        if nvidia_smi_path
        else {"returncode": None, "output": "missing", "timed_out": False}
    )

    cmake_configure: Dict[str, object] = {"attempted": False}
    if try_cmake and cmake_path:
        result = run(
            [
                "cmake",
                "-B",
                "build-cuda-preflight",
                "-DCMAKE_BUILD_TYPE=Debug",
                "-DINFERLAB_ENABLE_CUDA=ON",
            ],
            cwd=project_root,
            timeout=cmake_timeout,
        )
        cmake_configure = {
            "attempted": True,
            "returncode": result["returncode"],
            "summary": first_line(str(result["output"])),
            "output_tail": redact_artifact_output(
                "\n".join(str(result["output"]).splitlines()[-12:]),
                paths=[project_root, "build-cuda-preflight"],
            ),
            "timed_out": bool(result.get("timed_out")),
            "capture_status": result.get("capture_status"),
            "output_limit_exceeded": bool(result.get("output_limit_exceeded")),
        }
        if result.get("error"):
            cmake_configure["error"] = result["error"]

    return {
        "timestamp_utc": utc_now_precise(),
        "tools": {
            "cmake": redacted_path_label(cmake_path or "missing") or "missing",
            "nvcc": redacted_path_label(nvcc_path or "missing") or "missing",
            "nvidia_smi": redacted_path_label(nvidia_smi_path or "missing") or "missing",
        },
        "tools_path_redacted": {
            "cmake": bool(cmake_path and redacted_path_label(cmake_path) != cmake_path),
            "nvcc": bool(nvcc_path and redacted_path_label(nvcc_path) != nvcc_path),
            "nvidia_smi": bool(nvidia_smi_path and redacted_path_label(nvidia_smi_path) != nvidia_smi_path),
        },
        "nvcc": nvcc,
        "nvidia_smi": nvidia_smi,
        "cuda_available": nvcc_path is not None and nvidia_smi_path is not None,
        "cmake_cuda_configure": cmake_configure,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", help="Optional JSON output path")
    parser.add_argument(
        "--try-cmake",
        action="store_true",
        help="Attempt CUDA-enabled CMake configure and record the result.",
    )
    parser.add_argument("--project-root", default=str(default_project_root()))
    parser.add_argument(
        "--command-timeout-seconds",
        type=float,
        default=DEFAULT_COMMAND_TIMEOUT_SECONDS,
    )
    parser.add_argument(
        "--cmake-timeout-seconds",
        type=float,
        default=DEFAULT_CMAKE_TIMEOUT_SECONDS,
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    require_float_range("--command-timeout-seconds", args.command_timeout_seconds, 0.001, 86_400.0)
    require_float_range("--cmake-timeout-seconds", args.cmake_timeout_seconds, 0.001, 86_400.0)
    project_root = Path(args.project_root).resolve()
    payload = json.dumps(
        collect(project_root, args.try_cmake, args.command_timeout_seconds, args.cmake_timeout_seconds),
        indent=2,
        sort_keys=True,
    )
    if args.output:
        path = resolve_project_path(project_root, args.output)
        write_text_file(path, payload + "\n")
    else:
        print(payload)


if __name__ == "__main__":
    main()
