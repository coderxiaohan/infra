"""Small CLI I/O helpers for Mini LLM tools."""

from __future__ import annotations

import math
import numbers
import sys
from pathlib import Path
from typing import Sequence

try:
    from scripts.text_output import write_text_file
except ModuleNotFoundError:
    scripts_dir = Path(__file__).resolve().parents[1] / "scripts"
    if str(scripts_dir) not in sys.path:
        sys.path.insert(0, str(scripts_dir))
    from text_output import write_text_file  # type: ignore


def require_int_range(name: str, value: int, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum or value > maximum:
        raise ValueError(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}")
    return value


def require_positive_int(name: str, value: int) -> int:
    return require_int_range(name, value, 1, sys.maxsize)


def require_non_negative_int(name: str, value: int) -> int:
    return require_int_range(name, value, 0, sys.maxsize)


def require_float_range(name: str, value: float, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        raise ValueError(f"{name} must be a finite number in [{minimum}, {maximum}], got {value!r}")
    parsed = float(value)
    if not math.isfinite(parsed) or parsed < minimum or parsed > maximum:
        raise ValueError(f"{name} must be a finite number in [{minimum}, {maximum}], got {value!r}")
    return parsed


def require_positive_float(name: str, value: float) -> float:
    parsed = require_float_range(name, value, 0.0, math.inf)
    if parsed <= 0.0:
        raise ValueError(f"{name} must be a positive finite number, got {value!r}")
    return parsed


def require_non_negative_float(name: str, value: float) -> float:
    return require_float_range(name, value, 0.0, math.inf)


def parse_prompt_token_ids(raw_tokens: Sequence[str]) -> list[int]:
    if len(raw_tokens) == 1 and "," in raw_tokens[0]:
        raw_tokens = [piece for piece in raw_tokens[0].split(",") if piece]
    try:
        tokens = [int(token) for token in raw_tokens]
    except ValueError as exc:
        raise ValueError("prompt token ids must be integers") from exc
    if not tokens:
        raise ValueError("prompt must contain at least one token id")
    return tokens
