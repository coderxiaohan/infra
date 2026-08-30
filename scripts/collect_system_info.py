#!/usr/bin/env python3
"""Collect reproducibility metadata as JSON."""

from __future__ import annotations

import argparse
import json
import platform
import shutil
from pathlib import Path
from typing import Dict, List, Optional

try:
    from scripts.bounded_process import run_bounded_process
except ModuleNotFoundError:
    from bounded_process import run_bounded_process
from command_redaction import artifact_path_payload as path_payload, redact_path_error
from cli_validation import require_float_range
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import first_text_line as first_line, write_text_file
from time_utils import utc_now_precise


def run(command: List[str], cwd: Optional[Path] = None, timeout: float = 15.0) -> str:
    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=timeout,
        stdout_limit_bytes=1024 * 1024,
        stderr_limit_bytes=1024 * 1024,
        merge_stderr_into_stdout=True,
        launch_error_formatter=lambda exc: (
            "missing" if isinstance(exc, FileNotFoundError) else redact_path_error(exc, cwd, *command)
        ),
    )
    output = capture.stdout.decode("utf-8", errors="replace").strip()
    if capture.status == "launch_failed" and capture.error == "missing":
        return "missing"
    if capture.timed_out:
        return f"timed out after {timeout}s"
    if capture.output_limit_exceeded:
        return f"error: command output exceeded {1024 * 1024} bytes"
    if capture.status in {"capture_failed", "invalid_arguments", "launch_failed"}:
        return f"error: {capture.error or capture.status}"
    return output


def collect(repo_root: Path, command_timeout: float) -> Dict[str, object]:
    info: Dict[str, object] = {
        "timestamp_utc": utc_now_precise(),
        **path_payload(repo_root, "repo_root"),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "tools": {
            "git": run(["git", "--version"], timeout=command_timeout),
            "cmake": first_line(run(["cmake", "--version"], timeout=command_timeout))
            if shutil.which("cmake")
            else "missing",
            "nvidia_smi": run(["nvidia-smi", "--query-gpu=name,memory.total,driver_version", "--format=csv,noheader"], timeout=command_timeout)
            if shutil.which("nvidia-smi")
            else "missing",
        },
        "git": {
            "commit": run(["git", "rev-parse", "HEAD"], cwd=repo_root, timeout=command_timeout),
            "branch": run(["git", "branch", "--show-current"], cwd=repo_root, timeout=command_timeout),
            "status_short": run(["git", "status", "--short"], cwd=repo_root, timeout=command_timeout),
        },
    }
    return info


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--output", help="Optional JSON output path")
    parser.add_argument("--command-timeout-seconds", type=float, default=15.0)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    require_float_range("--command-timeout-seconds", args.command_timeout_seconds, 0.001, 86_400.0)
    repo_root = resolved_repo_root(args.repo_root)
    payload = json.dumps(collect(repo_root, args.command_timeout_seconds), indent=2, sort_keys=True)
    if args.output:
        path = resolve_repo_path(repo_root, args.output)
        write_text_file(path, payload + "\n")
    else:
        print(payload)


if __name__ == "__main__":
    main()
