#!/usr/bin/env python3
"""Shared scalar argument validation for InferLab Python CLI glue."""

from __future__ import annotations

import math
import re
from typing import Any


ENV_VAR_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def normalize_env_var_name(label: str, value: Any) -> str:
    if value is None:
        return ""
    name = str(value).strip()
    if not name:
        return ""
    if not ENV_VAR_NAME_RE.fullmatch(name):
        raise SystemExit(f"{label} must be an environment variable name, not a secret value")
    return name


def require_int_range(name: str, value: Any, minimum: int, maximum: int) -> None:
    if (
        isinstance(value, bool)
        or not isinstance(value, int)
        or value < minimum
        or value > maximum
    ):
        raise SystemExit(
            f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}"
        )


def require_float_range(
    name: str, value: Any, minimum: float, maximum: float
) -> None:
    try:
        numeric = float(value)
    except (TypeError, ValueError) as exc:
        raise SystemExit(
            f"{name} must be a finite number in [{minimum}, {maximum}], got {value!r}"
        ) from exc
    if (
        isinstance(value, bool)
        or not math.isfinite(numeric)
        or numeric < minimum
        or numeric > maximum
    ):
        raise SystemExit(
            f"{name} must be a finite number in [{minimum}, {maximum}], got {value!r}"
        )
