#!/usr/bin/env python3
"""Compatibility wrapper helpers for legacy benchmark entrypoints."""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Optional, Sequence

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from command_redaction import redact_path_error, redacted_path_label
from scripts.bounded_process import run_bounded_process
from scripts.implementation_lifecycle import lifecycle_for

COMMAND_TIMEOUT_SECONDS = 86_400.0
COMMAND_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024


def _write_captured_output(stream: object, data: bytes) -> None:
    if not data:
        return
    binary_stream = getattr(stream, "buffer", None)
    if binary_stream is not None:
        binary_stream.write(data)
        binary_stream.flush()
        return
    stream.write(data.decode("utf-8", errors="replace"))
    stream.flush()


def run_compat_command(command: Sequence[str], *, cwd: Path = ROOT) -> int:
    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=COMMAND_TIMEOUT_SECONDS,
        stdout_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        stderr_limit_bytes=COMMAND_OUTPUT_LIMIT_BYTES,
        launch_error_formatter=lambda exc: redact_path_error(exc, cwd, *command),
    )
    try:
        _write_captured_output(sys.stdout, capture.stdout)
        _write_captured_output(sys.stderr, capture.stderr)
    except (AttributeError, OSError, TypeError, ValueError) as exc:
        print(f"failed to relay command output: {redact_path_error(exc, cwd, *command)}", file=sys.stderr)
        return 1
    if capture.status in {"ok", "nonzero_exit"} and capture.returncode is not None:
        return int(capture.returncode)
    command_label = redacted_path_label(command[0]) if command else "<missing-command>"
    if capture.timed_out:
        reason = f"timed out after {COMMAND_TIMEOUT_SECONDS}s"
    elif capture.output_limit_exceeded:
        reason = f"output exceeded {COMMAND_OUTPUT_LIMIT_BYTES} bytes per stream"
    else:
        reason = capture.error or capture.status
    print(f"failed to execute {command_label}: {reason}", file=sys.stderr)
    return 127 if capture.status == "launch_failed" else 1


def _exec_compat_command(command: Sequence[str], *, cwd: Path) -> int:
    command_parts = list(command)
    if not command_parts:
        print("failed to execute <missing-command>: command is empty", file=sys.stderr)
        return 127
    try:
        os.chdir(cwd)
        sys.stdout.flush()
        sys.stderr.flush()
        os.execvp(command_parts[0], command_parts)
    except (OSError, ValueError) as exc:
        command_label = redacted_path_label(command[0]) if command else "<missing-command>"
        print(
            f"failed to execute {command_label}: {redact_path_error(exc, cwd, *command)}",
            file=sys.stderr,
        )
        return 127


def run_python_compat_entry(
    entry_id: str,
    argv: Optional[Sequence[str]] = None,
    *,
    cwd: Path = ROOT,
) -> int:
    lifecycle = lifecycle_for(entry_id)
    if lifecycle.role != "compatibility_wrapper":
        raise ValueError(f"lifecycle entry is not a compatibility wrapper: {entry_id}")
    if not lifecycle.replacement:
        raise ValueError(f"compatibility lifecycle has no replacement: {entry_id}")
    forwarded = list(sys.argv[1:] if argv is None else argv)
    command = [sys.executable, str(ROOT / lifecycle.replacement), *forwarded]
    return _exec_compat_command(command, cwd=cwd)
