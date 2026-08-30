#!/usr/bin/env python3
"""Shared JSON artifact loading helpers for InferLab Python glue scripts."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, Optional, Sequence

if __package__:
    from .command_redaction import (
        artifact_path_fields,
        redact_path_error,
        redacted_path_label as path_label,
    )
    from .text_output import atomic_write_text
else:
    from command_redaction import (  # type: ignore
        artifact_path_fields,
        redact_path_error,
        redacted_path_label as path_label,
    )
    from text_output import atomic_write_text  # type: ignore


NON_SUCCESS_ARTIFACT_STATUSES = {
    "blocked",
    "error",
    "failed",
    "incomplete",
    "invalid_artifact",
    "invalid_encoding",
    "invalid_json",
    "mismatch",
    "missing",
    "observed_only",
    "path_probe_failed",
    "planned",
    "skipped",
    "unavailable",
    "unreadable_json",
}
NON_SUCCESS_ARTIFACT_STATUS_PREFIXES = (
    "blocked_",
    "error_",
    "failed_",
    "invalid_",
    "missing_",
    "partial_",
    "planned_",
    "skipped_",
    "unavailable_",
    "unreadable_",
)


def is_non_success_artifact_status(value: Any) -> bool:
    if not isinstance(value, str) or not value.strip():
        return False
    status = value.strip().lower()
    return (
        status in NON_SUCCESS_ARTIFACT_STATUSES
        or status == "partial"
        or status.endswith("_failed")
        or status.endswith("_planned")
        or status.startswith(NON_SUCCESS_ARTIFACT_STATUS_PREFIXES)
    )


def artifact_status_exit_code(
    payload: Dict[str, Any],
    *,
    accepted_statuses: Sequence[str] = ("ok",),
    allow_non_ok: bool = False,
) -> int:
    if allow_non_ok:
        return 0
    return 0 if payload.get("status") in set(accepted_statuses) else 1


def path_payload(path: Path) -> Dict[str, Any]:
    return artifact_path_fields(path.as_posix())


def write_json_file(path: Path, payload: Dict[str, Any]) -> None:
    try:
        atomic_write_text(path, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    except OSError as exc:
        raise SystemExit(f"failed to write {path_label(path)}: {redact_path_error(exc, path)}") from exc


def try_write_json_file(path: Path, payload: Dict[str, Any]) -> Dict[str, Any]:
    """Write an artifact and return a redacted status instead of exiting."""
    try:
        atomic_write_text(path, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    except OSError as exc:
        return {
            "status": "write_failed",
            **path_payload(path),
            "error": redact_path_error(exc, path),
        }
    return {"status": "ok", **path_payload(path)}


def invalid_json_payload(path: Path, exc: json.JSONDecodeError) -> Dict[str, Any]:
    return {
        "status": "invalid_json",
        "error": redact_path_error(exc, path),
        **path_payload(path),
    }


def unreadable_json_payload(path: Path, exc: OSError) -> Dict[str, Any]:
    return {
        "status": "unreadable_json",
        "error": redact_path_error(exc, path),
        **path_payload(path),
    }


def path_probe_failed_payload(path: Path, exc: OSError) -> Dict[str, Any]:
    return {
        "status": "path_probe_failed",
        "error": redact_path_error(exc, path),
        **path_payload(path),
    }


def invalid_encoding_payload(path: Path, exc: UnicodeDecodeError) -> Dict[str, Any]:
    return {
        "status": "invalid_encoding",
        "error": redact_path_error(exc, path),
        **path_payload(path),
    }


def invalid_artifact_payload(path: Path, payload: Any) -> Dict[str, Any]:
    return {
        "status": "invalid_artifact",
        "error": "JSON artifact root must be an object",
        **path_payload(path),
        "value_type": type(payload).__name__,
    }


def load_json_dict(
    path: Path,
    *,
    missing: Optional[Dict[str, Any]] = None,
) -> Optional[Dict[str, Any]]:
    try:
        exists = path.exists()
    except OSError as exc:
        return path_probe_failed_payload(path, exc)
    if not exists:
        return missing
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        return unreadable_json_payload(path, exc)
    except UnicodeDecodeError as exc:
        return invalid_encoding_payload(path, exc)
    except json.JSONDecodeError as exc:
        return invalid_json_payload(path, exc)
    return payload if isinstance(payload, dict) else invalid_artifact_payload(path, payload)


def load_json_dict_or_empty(path: Path) -> Dict[str, Any]:
    """Load an object artifact while preserving an empty object for absence."""
    return load_json_dict(path, missing={}) or {}


def load_json_object_list(path: Path, key: str) -> list[Dict[str, Any]]:
    """Load a list field and retain only object records."""
    payload = load_json_dict_or_empty(path)
    values = payload.get(key)
    if not isinstance(values, list):
        return []
    return [value for value in values if isinstance(value, dict)]
