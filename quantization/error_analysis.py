#!/usr/bin/env python3
"""Summarize quantization error distribution from toy quantizer output."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from artifact_json import path_label, write_json_file
from command_redaction import redact_path_error
from time_utils import utc_now_precise


def load_json_object(path: Path) -> dict:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise SystemExit(f"failed to read {path_label(path)}: {redact_path_error(exc, path)}") from exc
    except UnicodeDecodeError as exc:
        raise SystemExit(f"invalid UTF-8 in {path_label(path)}: {redact_path_error(exc, path)}") from exc
    except json.JSONDecodeError as exc:
        raise SystemExit(f"invalid JSON in {path_label(path)}: {redact_path_error(exc, path)}") from exc
    if not isinstance(payload, dict):
        raise SystemExit(f"{path_label(path)} must contain a JSON object")
    return payload


def finite_number_list(name: str, values: object) -> list[float]:
    if not isinstance(values, list):
        raise SystemExit(f"{name} must be a list")
    parsed: list[float] = []
    for index, value in enumerate(values, start=1):
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise SystemExit(f"{name}[{index}] must be a number, got {value!r}")
        numeric = float(value)
        if not math.isfinite(numeric):
            raise SystemExit(f"{name}[{index}] must be finite, got {value!r}")
        parsed.append(numeric)
    return parsed


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((pct / 100.0) * (len(ordered) - 1))))
    return ordered[index]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default="results/raw/toy_quantizer.json")
    parser.add_argument("--output", default="results/raw/quantization_error_analysis.json")
    args = parser.parse_args()
    source_path = Path(args.input)
    source = load_json_object(source_path)
    values = finite_number_list("values", source.get("values", []))
    groups = source.get("groups", [])
    if not isinstance(groups, list):
        raise SystemExit("groups must be a list")
    restored = finite_number_list(
        "groups[].dequantized",
        [x for group in groups if isinstance(group, dict) for x in group.get("dequantized", [])],
    )
    if len(values) != len(restored):
        raise SystemExit(f"values/dequantized length mismatch: {len(values)} != {len(restored)}")
    errors = [abs(a - b) for a, b in zip(values, restored)]
    payload = {
        "run_id": "quantization_error_analysis",
        "timestamp": utc_now_precise(),
        "backend": "python-reference",
        "model": source.get("model", "synthetic-weight-vector"),
        "precision": source.get("precision", "unknown"),
        "prompt_tokens": 0,
        "output_tokens": 0,
        "source": path_label(source_path),
        "source_redacted": True,
        "summary": {"count": len(errors), "mean_abs_error": mean(errors) if errors else 0.0, "p50_abs_error": percentile(errors, 50), "p90_abs_error": percentile(errors, 90), "max_abs_error": max(errors) if errors else 0.0},
        "interpretation": "Outlier-heavy groups increase scale and reduce effective resolution for small weights; per-group and per-channel schemes limit that damage compared with per-tensor quantization.",
    }
    path = Path(args.output)
    write_json_file(path, payload)
    print(f"wrote {path_label(path)}")


if __name__ == "__main__":
    main()
