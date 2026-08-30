#!/usr/bin/env python3
"""Canonical UTF-8 text output helpers for InferLab Python glue."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Any, Dict

if __package__:
    from .command_redaction import (
        artifact_path_payload,
        redact_path_error,
        redacted_path_label,
    )
else:
    from command_redaction import (  # type: ignore
        artifact_path_payload,
        redact_path_error,
        redacted_path_label,
    )


def first_text_line(value: str) -> str:
    lines = value.splitlines()
    return lines[0] if lines else ""


def atomic_write_text(path: Path, text: str) -> None:
    """Atomically replace a UTF-8 text file using a same-directory temporary."""
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
    )
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as handle:
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_path, path)
    except BaseException:
        try:
            temporary_path.unlink(missing_ok=True)
        except OSError:
            pass
        raise


def write_text_file(path: Path, text: str) -> None:
    """Write text or terminate with a redacted path-safe error."""
    try:
        atomic_write_text(path, text)
    except OSError as exc:
        raise SystemExit(
            f"failed to write {redacted_path_label(path)}: "
            f"{redact_path_error(exc, path)}"
        ) from exc


def try_write_text_file(path: Path, text: str) -> Dict[str, Any]:
    """Write text and return redacted status metadata instead of exiting."""
    try:
        atomic_write_text(path, text)
    except OSError as exc:
        return {
            "status": "write_failed",
            **artifact_path_payload(path),
            "error": redact_path_error(exc, path),
        }
    return {"status": "ok", **artifact_path_payload(path)}
