#!/usr/bin/env python3
"""Bounded duplicate-key-free JSON object loading for governance inputs."""

from __future__ import annotations

import hashlib
import json
import os
import stat
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict


class StrictJsonDocumentError(ValueError):
    """Raised when an untrusted JSON document violates the physical contract."""


@dataclass(frozen=True)
class StrictJsonObjectDocument:
    payload: Dict[str, Any]
    source_size_bytes: int
    source_sha256: str


_HARD_MAXIMUM_BYTES = 64 * 1024 * 1024
_READ_CHUNK_BYTES = 64 * 1024


def _validate_loader_parameters(document_label: str, maximum_bytes: int) -> None:
    if (
        not isinstance(document_label, str)
        or not document_label
        or len(document_label) > 96
        or any(character in document_label for character in "\r\n")
    ):
        raise ValueError("strict JSON document label is invalid")
    if type(maximum_bytes) is not int or not (
        1 <= maximum_bytes <= _HARD_MAXIMUM_BYTES
    ):
        raise ValueError("strict JSON maximum_bytes is outside the supported range")


def _read_bounded_regular_file(
    source_path: str | Path,
    *,
    document_label: str,
    maximum_bytes: int,
) -> bytes:
    try:
        path = Path(source_path).expanduser()
    except (OSError, RuntimeError, TypeError, ValueError) as exc:
        raise StrictJsonDocumentError(f"{document_label} path is invalid") from exc

    descriptor: int | None = None
    try:
        open_flags = os.O_RDONLY
        open_flags |= getattr(os, "O_CLOEXEC", 0)
        open_flags |= getattr(os, "O_NONBLOCK", 0)
        descriptor = os.open(path, open_flags)
        path_status = os.fstat(descriptor)
        if not stat.S_ISREG(path_status.st_mode):
            raise StrictJsonDocumentError(
                f"{document_label} must be a regular file"
            )
        if path_status.st_size > maximum_bytes:
            raise StrictJsonDocumentError(
                f"{document_label} exceeds the {maximum_bytes}-byte input limit"
            )

        chunks = []
        remaining = maximum_bytes + 1
        while remaining:
            chunk = os.read(descriptor, min(remaining, _READ_CHUNK_BYTES))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        raw = b"".join(chunks)
    except StrictJsonDocumentError:
        raise
    except (OSError, ValueError) as exc:
        raise StrictJsonDocumentError(
            f"{document_label} could not be read"
        ) from exc
    finally:
        if descriptor is not None:
            try:
                os.close(descriptor)
            except OSError:
                pass

    if len(raw) > maximum_bytes:
        raise StrictJsonDocumentError(
            f"{document_label} exceeds the {maximum_bytes}-byte input limit"
        )
    if not raw:
        raise StrictJsonDocumentError(f"{document_label} must not be empty")
    return raw


def load_strict_json_object_document(
    source_path: str | Path,
    *,
    document_label: str,
    maximum_bytes: int,
) -> StrictJsonObjectDocument:
    """Load one bounded strict JSON object without disclosing its source path."""

    _validate_loader_parameters(document_label, maximum_bytes)
    raw = _read_bounded_regular_file(
        source_path,
        document_label=document_label,
        maximum_bytes=maximum_bytes,
    )
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise StrictJsonDocumentError(
            f"{document_label} must be UTF-8"
        ) from exc

    def strict_object(pairs: list[tuple[str, Any]]) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise StrictJsonDocumentError(
                    f"{document_label} contains a duplicate object key"
                )
            result[key] = value
        return result

    def reject_nonstandard_constant(value: str) -> None:
        raise StrictJsonDocumentError(
            f"{document_label} contains non-standard JSON constant {value}"
        )

    try:
        payload = json.loads(
            text,
            object_pairs_hook=strict_object,
            parse_constant=reject_nonstandard_constant,
        )
    except StrictJsonDocumentError:
        raise
    except json.JSONDecodeError as exc:
        raise StrictJsonDocumentError(
            f"{document_label} is invalid JSON at "
            f"line {exc.lineno}, column {exc.colno}"
        ) from exc
    except (RecursionError, ValueError) as exc:
        raise StrictJsonDocumentError(
            f"{document_label} exceeds parser limits"
        ) from exc

    if not isinstance(payload, dict):
        raise StrictJsonDocumentError(f"{document_label} root must be an object")
    return StrictJsonObjectDocument(
        payload=payload,
        source_size_bytes=len(raw),
        source_sha256=hashlib.sha256(raw).hexdigest(),
    )
