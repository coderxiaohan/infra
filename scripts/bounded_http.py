#!/usr/bin/env python3
"""Bounded binary response-body reads for InferLab HTTP glue."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


HARD_HTTP_BODY_LIMIT_BYTES = 64 * 1024 * 1024
READ_CHUNK_BYTES = 64 * 1024


@dataclass(frozen=True)
class BoundedHttpBody:
    status: str
    data: bytes = field(repr=False)
    observed_bytes: int = 0
    output_limit_exceeded: bool = False
    error: str = ""


def read_bounded_http_body(stream: Any, limit_bytes: int) -> BoundedHttpBody:
    """Read at most ``limit_bytes`` and probe one extra byte for completeness."""
    if type(limit_bytes) is not int or not (1 <= limit_bytes <= HARD_HTTP_BODY_LIMIT_BYTES):
        return BoundedHttpBody(
            status="invalid_limit",
            data=b"",
            error="HTTP body limit is outside the supported range",
        )

    data = bytearray()
    observed_bytes = 0
    try:
        while True:
            remaining_with_probe = limit_bytes - len(data) + 1
            read_size = min(READ_CHUNK_BYTES, max(1, remaining_with_probe))
            chunk = stream.read(read_size)
            if chunk == b"":
                break
            if not isinstance(chunk, bytes):
                return BoundedHttpBody(
                    status="read_failed",
                    data=bytes(data),
                    observed_bytes=observed_bytes,
                    error="HTTP body stream returned non-bytes data",
                )
            observed_bytes += len(chunk)
            remaining = max(0, limit_bytes - len(data))
            if remaining:
                data.extend(chunk[:remaining])
            if observed_bytes > limit_bytes:
                return BoundedHttpBody(
                    status="output_limit_exceeded",
                    data=bytes(data),
                    observed_bytes=observed_bytes,
                    output_limit_exceeded=True,
                    error="HTTP response body exceeded the configured byte limit",
                )
    except Exception as exc:  # External stream implementations have varied read exceptions.
        return BoundedHttpBody(
            status="read_failed",
            data=bytes(data),
            observed_bytes=observed_bytes,
            error=str(exc) or type(exc).__name__,
        )

    return BoundedHttpBody(
        status="ok",
        data=bytes(data),
        observed_bytes=observed_bytes,
    )
