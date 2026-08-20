#!/usr/bin/env python3
"""Shared nested object/list traversal for InferLab JSON projections."""

from __future__ import annotations

from typing import Any, Mapping, Sequence


def get_path(
    data: Mapping[str, Any],
    path: Sequence[str | int],
    default: Any = None,
) -> Any:
    current: Any = data
    for item in path:
        if isinstance(item, int):
            if not isinstance(current, list):
                return default
            try:
                current = current[item]
            except IndexError:
                return default
            continue
        if not isinstance(current, dict) or item not in current:
            return default
        current = current[item]
    return current
