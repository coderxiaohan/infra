#!/usr/bin/env python3
"""Shared bounded subprocess capture for InferLab Python glue commands."""

from __future__ import annotations

import math
import os
import signal
import subprocess
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Mapping, Sequence


HARD_STREAM_LIMIT_BYTES = 64 * 1024 * 1024
HARD_TIMEOUT_SECONDS = 86_400.0


@dataclass
class _StreamCapture:
    data: bytearray = field(default_factory=bytearray)
    total_bytes: int = 0
    overflowed: bool = False
    read_failed: bool = False


@dataclass(frozen=True)
class BoundedProcessCapture:
    status: str
    returncode: int | None
    elapsed_ms: float
    stdout: bytes = field(repr=False)
    stderr: bytes = field(repr=False)
    stdout_bytes: int = 0
    stderr_bytes: int = 0
    timed_out: bool = False
    output_limit_exceeded: bool = False
    error: str = ""


def command_result_succeeded(result: Mapping[str, Any]) -> bool:
    """Return whether a projected command result completed without an error."""
    return (
        result.get("returncode") == 0
        and not result.get("timed_out")
        and "error" not in result
    )


def _drain_stream(
    stream: Any,
    limit_bytes: int,
    capture: _StreamCapture,
    overflow_event: threading.Event,
    capture_failure_event: threading.Event,
) -> None:
    try:
        while True:
            chunk = stream.read(64 * 1024)
            if not chunk:
                break
            capture.total_bytes += len(chunk)
            remaining = max(0, limit_bytes - len(capture.data))
            if remaining:
                capture.data.extend(chunk[:remaining])
            if capture.total_bytes > limit_bytes:
                capture.overflowed = True
                overflow_event.set()
    except (OSError, ValueError):
        capture.read_failed = True
        capture_failure_event.set()
    finally:
        try:
            stream.close()
        except OSError:
            pass


def _signal_process_group(process: subprocess.Popen[bytes], sig: int) -> None:
    try:
        if os.name == "posix":
            os.killpg(process.pid, sig)
        elif sig == signal.SIGTERM:
            process.terminate()
        else:
            process.kill()
    except OSError:
        pass


def _stop_process(process: subprocess.Popen[bytes]) -> None:
    _signal_process_group(process, signal.SIGTERM)
    try:
        process.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        _signal_process_group(process, getattr(signal, "SIGKILL", signal.SIGTERM))
        try:
            process.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            pass


def _close_process_streams(process: subprocess.Popen[bytes]) -> None:
    for stream in (process.stdout, process.stderr):
        if stream is None:
            continue
        try:
            stream.close()
        except OSError:
            pass


def _invalid_capture(message: str) -> BoundedProcessCapture:
    return BoundedProcessCapture(
        status="invalid_arguments",
        returncode=None,
        elapsed_ms=0.0,
        stdout=b"",
        stderr=b"",
        error=message,
    )


def run_bounded_process(
    command: Sequence[str],
    *,
    cwd: Path | None,
    timeout_seconds: float,
    stdout_limit_bytes: int,
    stderr_limit_bytes: int,
    env: Mapping[str, str] | None = None,
    launch_error_formatter: Callable[[BaseException], str] | None = None,
    merge_stderr_into_stdout: bool = False,
) -> BoundedProcessCapture:
    """Run argv without a shell and retain bounded output stream prefixes."""
    if isinstance(command, (str, bytes)) or not command or any(
        not isinstance(part, str) or "\x00" in part for part in command
    ):
        return _invalid_capture("command must be a nonempty sequence of NUL-free string argv")
    if (
        isinstance(timeout_seconds, bool)
        or not isinstance(timeout_seconds, (int, float))
        or not math.isfinite(timeout_seconds)
        or not (0.0 < timeout_seconds <= HARD_TIMEOUT_SECONDS)
    ):
        return _invalid_capture("timeout is outside the supported range")
    for name, limit in (("stdout", stdout_limit_bytes), ("stderr", stderr_limit_bytes)):
        if type(limit) is not int or not (1 <= limit <= HARD_STREAM_LIMIT_BYTES):
            return _invalid_capture(f"{name} limit is outside the supported range")
    if type(merge_stderr_into_stdout) is not bool:
        return _invalid_capture("merge_stderr_into_stdout must be a boolean")

    started = time.monotonic()
    try:
        process = subprocess.Popen(
            list(command),
            cwd=cwd,
            env=dict(env) if env is not None else None,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT if merge_stderr_into_stdout else subprocess.PIPE,
            start_new_session=(os.name == "posix"),
        )
    except (OSError, TypeError, ValueError) as exc:
        try:
            formatted_error = (
                launch_error_formatter(exc)
                if launch_error_formatter is not None
                else "process launch failed"
            )
            error = (
                formatted_error
                if isinstance(formatted_error, str) and formatted_error
                else "process launch failed"
            )
        except Exception:
            # A diagnostic callback must not hide the original launch failure.
            error = "process launch failed; error formatting also failed"
        return BoundedProcessCapture(
            status="launch_failed",
            returncode=None,
            elapsed_ms=round((time.monotonic() - started) * 1000.0, 3),
            stdout=b"",
            stderr=b"",
            error=error,
        )

    stdout_capture = _StreamCapture()
    stderr_capture = _StreamCapture()
    overflow_event = threading.Event()
    capture_failure_event = threading.Event()
    assert process.stdout is not None
    stream_specs = [(process.stdout, stdout_limit_bytes, stdout_capture)]
    active_captures = [stdout_capture]
    if not merge_stderr_into_stdout:
        assert process.stderr is not None
        stream_specs.append((process.stderr, stderr_limit_bytes, stderr_capture))
        active_captures.append(stderr_capture)
    threads = tuple(
        threading.Thread(
            target=_drain_stream,
            args=(stream, limit, capture, overflow_event, capture_failure_event),
            daemon=True,
        )
        for stream, limit, capture in stream_specs
    )
    started_threads = []
    try:
        for thread in threads:
            thread.start()
            started_threads.append(thread)
    except RuntimeError:
        _stop_process(process)
        _signal_process_group(process, getattr(signal, "SIGKILL", signal.SIGTERM))
        _close_process_streams(process)
        for thread in started_threads:
            thread.join(timeout=1.0)
        return BoundedProcessCapture(
            status="capture_failed",
            returncode=process.returncode,
            elapsed_ms=round((time.monotonic() - started) * 1000.0, 3),
            stdout=bytes(stdout_capture.data),
            stderr=bytes(stderr_capture.data),
            stdout_bytes=stdout_capture.total_bytes,
            stderr_bytes=stderr_capture.total_bytes,
            error="capture thread could not be started",
        )

    timed_out = False
    output_limit_exceeded = False
    deadline = started + timeout_seconds
    while process.poll() is None:
        if overflow_event.is_set():
            output_limit_exceeded = True
            _stop_process(process)
            break
        if capture_failure_event.is_set():
            _stop_process(process)
            break
        if time.monotonic() >= deadline:
            timed_out = True
            _stop_process(process)
            break
        time.sleep(0.01)
    if process.poll() is None:
        _stop_process(process)
    for thread in threads:
        thread.join(timeout=2.0)

    forced_capture_cleanup = any(thread.is_alive() for thread in threads)
    if forced_capture_cleanup:
        # A descendant may outlive the process leader while retaining a pipe.
        _signal_process_group(process, signal.SIGTERM)
        for thread in threads:
            thread.join(timeout=1.0)
    if any(thread.is_alive() for thread in threads):
        _signal_process_group(process, getattr(signal, "SIGKILL", signal.SIGTERM))
        _close_process_streams(process)
        for thread in threads:
            thread.join(timeout=1.0)

    capture_threads_alive = any(thread.is_alive() for thread in threads)
    capture_read_failed = any(capture.read_failed for capture in active_captures)
    capture_failed = forced_capture_cleanup or capture_read_failed
    output_limit_exceeded = output_limit_exceeded or any(capture.overflowed for capture in active_captures)
    if timed_out:
        status = "timed_out"
    elif output_limit_exceeded:
        status = "output_limit_exceeded"
    elif capture_failed:
        status = "capture_failed"
    elif process.returncode != 0:
        status = "nonzero_exit"
    else:
        status = "ok"
    if capture_read_failed:
        capture_error = "capture stream read failed"
    elif capture_threads_alive:
        capture_error = "capture stream did not close"
    elif forced_capture_cleanup:
        capture_error = "capture stream required forced closure"
    else:
        capture_error = ""
    return BoundedProcessCapture(
        status=status,
        returncode=process.returncode,
        elapsed_ms=round((time.monotonic() - started) * 1000.0, 3),
        stdout=bytes(stdout_capture.data),
        stderr=bytes(stderr_capture.data),
        stdout_bytes=stdout_capture.total_bytes,
        stderr_bytes=stderr_capture.total_bytes,
        timed_out=timed_out,
        output_limit_exceeded=output_limit_exceeded,
        error=capture_error,
    )
