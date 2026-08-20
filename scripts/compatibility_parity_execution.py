#!/usr/bin/env python3
"""Bounded process and artifact refresh execution for compatibility parity."""

from __future__ import annotations

import shlex
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

if __package__:
    from .artifact_json import path_payload
    from .bounded_process import BoundedProcessCapture as CommandCapture, run_bounded_process
    from .command_redaction import artifact_command_parts, redact_path_error
    from .path_status import safe_is_file, safe_mtime_ns
    from .compatibility_removal_authorization_auth import (
        compatibility_removal_authorization_child_environment,
    )
    from .strict_json_document import (
        StrictJsonDocumentError,
        StrictJsonObjectDocument,
        load_strict_json_object_document,
    )
else:
    from artifact_json import path_payload  # type: ignore
    from bounded_process import (  # type: ignore
        BoundedProcessCapture as CommandCapture,
        run_bounded_process,
    )
    from command_redaction import (  # type: ignore
        artifact_command_parts,
        redact_path_error,
    )
    from path_status import safe_is_file, safe_mtime_ns  # type: ignore
    from compatibility_removal_authorization_auth import (  # type: ignore
        compatibility_removal_authorization_child_environment,
    )
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        StrictJsonObjectDocument,
        load_strict_json_object_document,
    )


_PATH_OPTIONS = (
    "--config",
    "--engine-dir",
    "--model",
    "--output",
    "--prompt-file",
    "--repo-root",
    "--report",
    "--tokenizer",
)
_CONTENT_OPTIONS = ("--prompt", "--query", "--text")


@dataclass(frozen=True)
class SideExecution:
    label: str
    command: Tuple[str, ...]
    artifact_path: Path
    status: str
    command_capture: Optional[CommandCapture] = None
    document: Optional[StrictJsonObjectDocument] = field(
        default=None,
        repr=False,
    )
    artifact_preexisting: bool = False
    artifact_refresh_observed: bool = False
    error: str = ""


def execute_side(
    label: str,
    command: Tuple[str, ...],
    artifact_path: Path,
    *,
    repo_root: Path,
    timeout_seconds: float,
    output_limit_bytes: int,
    artifact_limit_bytes: int,
) -> SideExecution:
    before_exists, before_mtime_ns, before_probe_error = safe_mtime_ns(artifact_path)
    before_is_file, before_file_error = safe_is_file(artifact_path)
    if before_probe_error or before_file_error:
        return SideExecution(
            label,
            command,
            artifact_path,
            "pre_run_probe_failed",
            artifact_preexisting=before_exists,
        )
    if before_exists and not before_is_file:
        return SideExecution(
            label,
            command,
            artifact_path,
            "preexisting_non_file",
            artifact_preexisting=True,
        )

    capture = run_bounded_process(
        command,
        cwd=repo_root,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=output_limit_bytes,
        stderr_limit_bytes=output_limit_bytes,
        env=compatibility_removal_authorization_child_environment(),
        launch_error_formatter=lambda exc: redact_path_error(exc, repo_root, *command),
    )
    if capture.status != "ok":
        return SideExecution(
            label,
            command,
            artifact_path,
            "command_failed",
            command_capture=capture,
            artifact_preexisting=before_exists,
        )

    after_exists, after_mtime_ns, after_probe_error = safe_mtime_ns(artifact_path)
    after_is_file, after_file_error = safe_is_file(artifact_path)
    if after_probe_error or after_file_error:
        status = "post_run_probe_failed"
    elif not after_exists or not after_is_file:
        status = "missing_after_success"
    elif before_exists and before_mtime_ns == after_mtime_ns:
        status = "refresh_not_observed"
    else:
        status = "artifact_refreshed"
    refresh_observed = status == "artifact_refreshed"
    if not refresh_observed:
        return SideExecution(
            label,
            command,
            artifact_path,
            status,
            command_capture=capture,
            artifact_preexisting=before_exists,
        )

    try:
        document = load_strict_json_object_document(
            artifact_path,
            document_label=f"{label} parity artifact",
            maximum_bytes=artifact_limit_bytes,
        )
    except StrictJsonDocumentError as exc:
        return SideExecution(
            label,
            command,
            artifact_path,
            "invalid_artifact",
            command_capture=capture,
            artifact_preexisting=before_exists,
            artifact_refresh_observed=True,
            error=str(exc),
        )
    return SideExecution(
        label,
        command,
        artifact_path,
        "ready",
        command_capture=capture,
        document=document,
        artifact_preexisting=before_exists,
        artifact_refresh_observed=True,
    )


def skipped_side(
    label: str,
    command: Tuple[str, ...],
    path: Path,
) -> SideExecution:
    return SideExecution(label, command, path, "skipped_due_to_canonical_failure")


def planned_side(
    label: str,
    command: Tuple[str, ...],
    path: Path,
) -> SideExecution:
    return SideExecution(label, command, path, "planned")


def _command_payload(command: Tuple[str, ...]) -> Dict[str, Any]:
    parts = artifact_command_parts(
        command,
        path_options=_PATH_OPTIONS,
        content_options=_CONTENT_OPTIONS,
    )
    return {
        "parts": parts,
        "shell": shlex.join(parts),
        "redacted": parts != list(command),
    }


def side_payload(side: SideExecution) -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "label": side.label,
        "status": side.status,
        "command": _command_payload(side.command),
        **path_payload(side.artifact_path),
        "artifact_preexisting": side.artifact_preexisting,
        "artifact_refresh_observed": side.artifact_refresh_observed,
        "artifact_content_disclosed": False,
    }
    if side.command_capture is not None:
        capture = side.command_capture
        payload["command_result"] = {
            "status": capture.status,
            "returncode": capture.returncode,
            "elapsed_ms": capture.elapsed_ms,
            "stdout_bytes": capture.stdout_bytes,
            "stderr_bytes": capture.stderr_bytes,
            "stdout_stderr_content_disclosed": False,
            "timed_out": capture.timed_out,
            "output_limit_exceeded": capture.output_limit_exceeded,
            "error": capture.error,
        }
    if side.document is not None:
        payload["artifact_document"] = {
            "source_size_bytes": side.document.source_size_bytes,
            "source_sha256": side.document.source_sha256,
        }
    if side.error:
        payload["error"] = side.error
    return payload
