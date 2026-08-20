#!/usr/bin/env python3
"""Dependency-free toy quantizer covering symmetric/asymmetric per-group modes."""

from __future__ import annotations

import argparse
import json
import math
import sys
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from artifact_json import path_label, write_json_file as write_json_artifact_file


def require_int_range(name: str, value: int, minimum: int, maximum: int) -> None:
    if isinstance(value, bool) or value < minimum or value > maximum:
        raise SystemExit(f"{name} must be an integer in [{minimum}, {maximum}], got {value!r}")


def parse_values(text: str) -> list[float]:
    values: list[float] = []
    for index, item in enumerate(text.split(","), start=1):
        stripped = item.strip()
        if not stripped:
            continue
        try:
            value = float(stripped)
        except ValueError as exc:
            raise SystemExit(f"--values item {index} must be a number, got {stripped!r}") from exc
        if not math.isfinite(value):
            raise SystemExit(f"--values item {index} must be finite, got {stripped!r}")
        values.append(value)
    if not values:
        raise SystemExit("--values must contain at least one finite number")
    return values


def write_json_file(path: Path, payload: dict) -> None:
    write_json_artifact_file(path, payload)


def chunks(values: list[float], size: int):
    for start in range(0, len(values), size):
        yield start, values[start:start + size]


def quantize_group(group: list[float], bits: int, symmetric: bool) -> dict:
    if not group:
        raise ValueError("quantize_group requires a non-empty group")
    if symmetric:
        qmax = (1 << (bits - 1)) - 1
        max_abs = max(abs(v) for v in group) or 1.0
        scale = max_abs / qmax
        zero_point = 0
        q = [max(-qmax - 1, min(qmax, round(v / scale))) for v in group]
        dq = [x * scale for x in q]
    else:
        qmin, qmax = 0, (1 << bits) - 1
        mn, mx = min(group), max(group)
        scale = (mx - mn) / max(1, qmax - qmin) or 1.0
        zero_point = round(qmin - mn / scale)
        q = [max(qmin, min(qmax, round(v / scale + zero_point))) for v in group]
        dq = [(x - zero_point) * scale for x in q]
    errors = [abs(a - b) for a, b in zip(group, dq)]
    return {"scale": scale, "zero_point": zero_point, "quantized": q, "dequantized": dq, "max_abs_error": max(errors), "mean_abs_error": mean(errors)}


def run(values: list[float], bits: int, group_size: int, symmetric: bool) -> dict:
    require_int_range("bits", bits, 2, 16)
    require_int_range("group_size", group_size, 1, 1_000_000)
    if not values:
        raise SystemExit("values must not be empty")
    groups = []
    restored: list[float] = []
    for start, group in chunks(values, group_size):
        result = quantize_group(group, bits, symmetric)
        result["start"] = start
        groups.append(result)
        restored.extend(result["dequantized"])
    errors = [abs(a - b) for a, b in zip(values, restored)]
    return {
        "run_id": "toy_quantizer",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "backend": "python-reference",
        "model": "synthetic-weight-vector",
        "precision": f"int{bits}",
        "prompt_tokens": 0,
        "output_tokens": 0,
        "config": {"bits": bits, "group_size": group_size, "symmetric": symmetric},
        "values": values,
        "groups": groups,
        "summary": {"max_abs_error": max(errors), "mean_abs_error": mean(errors), "num_values": len(values)},
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bits", type=int, default=4)
    parser.add_argument("--group-size", type=int, default=8)
    parser.add_argument("--mode", choices=["symmetric", "asymmetric"], default="symmetric")
    parser.add_argument("--values", default="-1.5,-0.7,-0.2,0.0,0.3,0.8,1.2,2.0,-2.5,3.0,0.5,-0.4")
    parser.add_argument("--output", default="results/raw/toy_quantizer.json")
    args = parser.parse_args()
    values = parse_values(args.values)
    payload = run(values, args.bits, args.group_size, args.mode == "symmetric")
    path = Path(args.output)
    write_json_file(path, payload)
    print(f"wrote {path_label(path)}")


if __name__ == "__main__":
    main()
