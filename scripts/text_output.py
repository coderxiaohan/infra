#!/usr/bin/env python3
"""Canonical UTF-8 text output helpers for InferLab Python glue."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

from command_redaction import (
    artifact_path_payload,
    redact_path_error,
    redacted_path_label,
)


def write_text_file(path: Path, text: str) -> None:
    """Write text or terminate with a redacted path-safe error."""
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    except OSError as exc:
        raise SystemExit(
            f"failed to write {redacted_path_label(path)}: "
            f"{redact_path_error(exc, path)}"
        ) from exc


def try_write_text_file(path: Path, text: str) -> Dict[str, Any]:
    """Write text and return redacted status metadata instead of exiting."""
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    except OSError as exc:
        return {
            "status": "write_failed",
            **artifact_path_payload(path),
            "error": redact_path_error(exc, path),
        }
    return {"status": "ok", **artifact_path_payload(path)}
