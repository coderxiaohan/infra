#!/usr/bin/env python3
"""Canonical UTC timestamp formatting for InferLab Python glue."""

from __future__ import annotations

from datetime import datetime, timezone


def utc_now() -> str:
    """Return a second-precision UTC timestamp with the canonical Z suffix."""
    return (
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )


def utc_now_precise() -> str:
    """Return a microsecond-preserving UTC timestamp with an explicit offset."""
    return datetime.now(timezone.utc).isoformat()
