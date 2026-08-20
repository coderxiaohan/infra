#!/usr/bin/env python3
"""Strict loader for the CMake-derived compatibility entrypoint manifest."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional

if __package__:
    from .strict_json_document import (
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
else:
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )


_MAX_MANIFEST_BYTES = 256 * 1024
_MAX_ENTRIES = 128
_ROOT_FIELDS = frozenset({"schema_version", "entries"})
_ENTRY_FIELDS = frozenset(
    {
        "entry_id",
        "compatibility_target",
        "canonical_target",
        "path",
        "replacement",
        "target_release",
    }
)


class CMakeCompatibilityManifestError(ValueError):
    """Raised when a generated CMake compatibility manifest is invalid."""


@dataclass(frozen=True)
class CMakeCompatibilityEntrypoint:
    entry_id: str
    compatibility_target: str
    canonical_target: str
    path: str
    replacement: str
    target_release: str
    role: str = "compatibility_wrapper"
    delegate_override_option: Optional[str] = None


def _token(name: str, value: Any) -> str:
    if (
        not isinstance(value, str)
        or not value
        or len(value) > 256
        or any(character not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-" for character in value)
    ):
        raise CMakeCompatibilityManifestError(f"{name} is not a bounded token")
    return value


def _entrypoint_path(name: str, value: Any) -> str:
    if not isinstance(value, str) or not value or len(value) > 4096 or "\x00" in value:
        raise CMakeCompatibilityManifestError(f"{name} is not a valid entrypoint path")
    return value


def load_cmake_compatibility_entrypoint(
    manifest_path: Path, entry_id: str
) -> CMakeCompatibilityEntrypoint:
    try:
        root = load_strict_json_object_document(
            manifest_path,
            document_label="CMake compatibility manifest",
            maximum_bytes=_MAX_MANIFEST_BYTES,
        ).payload
    except StrictJsonDocumentError as exc:
        raise CMakeCompatibilityManifestError(str(exc)) from exc
    if not isinstance(root, dict) or frozenset(root) != _ROOT_FIELDS:
        raise CMakeCompatibilityManifestError(
            "CMake compatibility manifest root schema is not exact"
        )
    if type(root["schema_version"]) is not int or root["schema_version"] != 3:
        raise CMakeCompatibilityManifestError(
            "CMake compatibility manifest schema version is unsupported"
        )
    entries = root["entries"]
    if not isinstance(entries, list) or not (1 <= len(entries) <= _MAX_ENTRIES):
        raise CMakeCompatibilityManifestError(
            "CMake compatibility manifest entry count is outside its bound"
        )
    selected: Optional[CMakeCompatibilityEntrypoint] = None
    seen_ids: set[str] = set()
    seen_targets: set[str] = set()
    for index, value in enumerate(entries):
        if not isinstance(value, dict) or frozenset(value) != _ENTRY_FIELDS:
            raise CMakeCompatibilityManifestError(
                f"CMake compatibility manifest entry {index} schema is not exact"
            )
        current_id = _token("entry_id", value["entry_id"])
        compatibility_target = _token(
            "compatibility_target", value["compatibility_target"]
        )
        canonical_target = _token("canonical_target", value["canonical_target"])
        if current_id != f"cmake.{compatibility_target}":
            raise CMakeCompatibilityManifestError(
                "CMake compatibility entry id does not match its target"
            )
        if current_id in seen_ids or compatibility_target in seen_targets:
            raise CMakeCompatibilityManifestError(
                "CMake compatibility manifest contains a duplicate entry"
            )
        seen_ids.add(current_id)
        seen_targets.add(compatibility_target)
        entry = CMakeCompatibilityEntrypoint(
            entry_id=current_id,
            compatibility_target=compatibility_target,
            canonical_target=canonical_target,
            path=_entrypoint_path("path", value["path"]),
            replacement=_entrypoint_path("replacement", value["replacement"]),
            target_release=_token("target_release", value["target_release"]),
        )
        if current_id == entry_id:
            selected = entry
    if selected is None:
        raise KeyError(entry_id)
    return selected
