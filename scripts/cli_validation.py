#!/usr/bin/env python3
"""Shared scalar argument validation for InferLab Python CLI glue."""

from __future__ import annotations

import math


def require_int_range(name: str, value: int, minimum: int, maximum: int) -> None:
    if isinstance(value, bool) or value < minimum or value > maximum:
        raise SystemExit(
            f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}"
        )


def require_float_range(
    name: str, value: float, minimum: float, maximum: float
) -> None:
    if (
        isinstance(value, bool)
        or not math.isfinite(value)
        or value < minimum
        or value > maximum
    ):
        raise SystemExit(
            f"{name} must be a finite number in [{minimum}, {maximum}], got {value!r}"
        )
