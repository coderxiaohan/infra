#!/usr/bin/env python3
"""Small filesystem status helpers for InferLab Python glue scripts."""

from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

try:
    from scripts.command_redaction import artifact_path_payload, redact_path_error
except ModuleNotFoundError:
    from command_redaction import artifact_path_payload, redact_path_error


def tool_availability(name: str) -> Dict[str, Any]:
    path = shutil.which(name)
    return {
        "name": name,
        **artifact_path_payload(path or "missing"),
        "available": bool(path),
    }


def safe_exists(path: Path) -> Tuple[bool, str]:
    try:
        return path.exists(), ""
    except OSError as exc:
        return False, redact_path_error(exc, path)


def safe_is_file(path: Path) -> Tuple[bool, str]:
    try:
        return path.is_file(), ""
    except OSError as exc:
        return False, redact_path_error(exc, path)


def safe_is_dir(path: Path) -> Tuple[bool, str]:
    try:
        return path.is_dir(), ""
    except OSError as exc:
        return False, redact_path_error(exc, path)


def safe_mtime_ns(path: Path) -> Tuple[bool, Optional[int], str]:
    exists, exists_error = safe_exists(path)
    if exists_error:
        return False, None, exists_error
    if not exists:
        return False, None, ""
    try:
        return True, path.stat().st_mtime_ns, ""
    except OSError as exc:
        return True, None, redact_path_error(exc, path)
