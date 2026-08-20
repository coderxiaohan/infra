"""Small CLI I/O helpers for Mini LLM tools."""

from __future__ import annotations

import math
import numbers
import sys
from pathlib import Path

try:
    from scripts.command_redaction import redact_path_error, redacted_path_label
except ModuleNotFoundError:
    scripts_dir = Path(__file__).resolve().parents[1] / "scripts"
    if str(scripts_dir) not in sys.path:
        sys.path.insert(0, str(scripts_dir))
    from command_redaction import redact_path_error, redacted_path_label  # type: ignore


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


def write_text_file(path: Path, text: str) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    except OSError as exc:
        raise SystemExit(f"failed to write {redacted_path_label(path.as_posix())}: {redact_path_error(exc, path)}") from exc
