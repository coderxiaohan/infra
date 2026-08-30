"""Reference-only speculative decoding cost model for InferLab.

This module is a dependency-free simulator. It does not need a draft model or a
target model to be present; instead, it models acceptance rate, verify window,
draft cost, target cost, and fallback behavior so later GPU/vLLM/TensorRT runs
have a clear baseline for what to measure. It is not a formal InferLab artifact
producer; the canonical implementation is the C++ speculative decode plan.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from mini_llm.cli_io import (
    require_float_range,
    require_int_range,
    require_non_negative_float,
    require_positive_float,
    require_positive_int,
    write_text_file,
)
from scripts.command_redaction import redacted_path_label


DEFAULT_SCENARIOS = (
    ("high_accept_short", 0.85, 4),
    ("balanced_accept", 0.65, 4),
    ("low_accept", 0.35, 4),
    ("long_verify_high_accept", 0.8, 8),
    ("short_verify_low_overhead", 0.7, 2),
)


@dataclass(frozen=True)
class SpeculativeScenario:
    name: str
    acceptance_rate: float
    verify_window: int

    def validate(self) -> None:
        if not isinstance(self.name, str) or not self.name.strip():
            raise ValueError("scenario name must not be empty")
        require_float_range(f"{self.name}: acceptance_rate", self.acceptance_rate, 0.0, 1.0)
        require_positive_int(f"{self.name}: verify_window", self.verify_window)

    def to_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "acceptance_rate": self.acceptance_rate,
            "verify_window": self.verify_window,
        }


def parse_scenario(value: str) -> SpeculativeScenario:
    parts = value.split(":")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("scenario must be name:acceptance_rate:verify_window")
    name, acceptance_raw, window_raw = parts
    try:
        acceptance_rate = float(acceptance_raw)
        verify_window = int(window_raw)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("scenario acceptance rate/window are invalid") from exc
    scenario = SpeculativeScenario(name, acceptance_rate, verify_window)
    try:
        scenario.validate()
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc)) from exc
    return scenario


def default_scenarios() -> list[SpeculativeScenario]:
    return [
        SpeculativeScenario(name=name, acceptance_rate=acceptance, verify_window=window)
        for name, acceptance, window in DEFAULT_SCENARIOS
    ]


def _expected_accepted_tokens(acceptance_rate: float, verify_window: int) -> float:
    # Expected accepted prefix length for a truncated geometric sequence.
    return sum(acceptance_rate**position for position in range(1, verify_window + 1))


def _target_only_cost(output_tokens: int, target_ms_per_token: float) -> dict[str, Any]:
    total_ms = output_tokens * target_ms_per_token
    return {
        "target_calls": output_tokens,
        "draft_calls": 0,
        "accepted_tokens": output_tokens,
        "rejected_tokens": 0,
        "total_ms": round(total_ms, 6),
        "tpot_ms": round(total_ms / output_tokens, 6) if output_tokens else None,
        "tokens_per_second": round(output_tokens / (total_ms / 1000.0), 6) if total_ms > 0 else None,
    }


def _speculative_cost(
    *,
    output_tokens: int,
    target_ms_per_token: float,
    draft_ms_per_token: float,
    target_verify_overhead_ms: float,
    acceptance_rate: float,
    verify_window: int,
) -> dict[str, Any]:
    expected_accept = _expected_accepted_tokens(acceptance_rate, verify_window)
    # Every verification round emits the accepted draft prefix plus one target
    # token, including the bonus token when the whole draft window is accepted.
    expected_progress = 1.0 + expected_accept
    rounds = output_tokens / expected_progress if expected_progress > 0 else float(output_tokens)
    draft_tokens = rounds * verify_window
    target_verify_calls = rounds
    accepted_tokens = min(float(output_tokens), rounds * expected_accept)
    rejected_tokens = max(0.0, draft_tokens - accepted_tokens)
    draft_ms = draft_tokens * draft_ms_per_token
    target_ms = target_verify_calls * target_ms_per_token
    verify_overhead_ms = target_verify_calls * target_verify_overhead_ms
    total_ms = draft_ms + target_ms + verify_overhead_ms
    return {
        "target_calls": round(target_verify_calls, 6),
        "draft_calls": round(draft_tokens, 6),
        "accepted_tokens": round(accepted_tokens, 6),
        "rejected_tokens": round(rejected_tokens, 6),
        "expected_progress_per_round": round(expected_progress, 6),
        "rounds": round(rounds, 6),
        "draft_ms": round(draft_ms, 6),
        "target_ms": round(target_ms, 6),
        "verify_overhead_ms": round(verify_overhead_ms, 6),
        "total_ms": round(total_ms, 6),
        "tpot_ms": round(total_ms / output_tokens, 6) if output_tokens else None,
        "tokens_per_second": round(output_tokens / (total_ms / 1000.0), 6) if total_ms > 0 else None,
    }


def scenario_record(
    scenario: SpeculativeScenario,
    *,
    output_tokens: int,
    target_ms_per_token: float,
    draft_ms_per_token: float,
    target_verify_overhead_ms: float,
) -> dict[str, Any]:
    scenario.validate()
    baseline = _target_only_cost(output_tokens, target_ms_per_token)
    speculative = _speculative_cost(
        output_tokens=output_tokens,
        target_ms_per_token=target_ms_per_token,
        draft_ms_per_token=draft_ms_per_token,
        target_verify_overhead_ms=target_verify_overhead_ms,
        acceptance_rate=scenario.acceptance_rate,
        verify_window=scenario.verify_window,
    )
    speedup = (
        baseline["total_ms"] / speculative["total_ms"]
        if speculative["total_ms"] and speculative["total_ms"] > 0
        else None
    )
    tpot_delta_ms = (
        speculative["tpot_ms"] - baseline["tpot_ms"]
        if speculative["tpot_ms"] is not None and baseline["tpot_ms"] is not None
        else None
    )
    return {
        **scenario.to_dict(),
        "baseline": baseline,
        "speculative": speculative,
        "speedup_vs_target_only": round(speedup, 6) if speedup is not None else None,
        "tpot_delta_ms": round(tpot_delta_ms, 6) if tpot_delta_ms is not None else None,
        "profitable": bool(speedup is not None and speedup > 1.0),
    }


def _rank(records: Sequence[dict[str, Any]], key: str, reverse: bool = True) -> list[dict[str, Any]]:
    ranked = sorted(
        records,
        key=lambda item: (
            float(item.get(key) or 0.0),
            float(item.get("acceptance_rate") or 0.0),
            str(item.get("name", "")),
        ),
        reverse=reverse,
    )
    return [
        {
            "rank": index + 1,
            "name": item.get("name"),
            key: item.get(key),
            "acceptance_rate": item.get("acceptance_rate"),
            "verify_window": item.get("verify_window"),
            "profitable": item.get("profitable"),
        }
        for index, item in enumerate(ranked)
    ]


def build_speculative_decode_report(
    scenarios: Sequence[SpeculativeScenario] | None = None,
    *,
    output_tokens: int = 128,
    target_ms_per_token: float = 10.0,
    draft_ms_per_token: float = 1.5,
    target_verify_overhead_ms: float = 0.8,
) -> dict[str, Any]:
    output_tokens = require_positive_int("output_tokens", output_tokens)
    target_ms_per_token = require_positive_float("target_ms_per_token", target_ms_per_token)
    draft_ms_per_token = require_non_negative_float("draft_ms_per_token", draft_ms_per_token)
    target_verify_overhead_ms = require_non_negative_float(
        "target_verify_overhead_ms",
        target_verify_overhead_ms,
    )
    selected = list(scenarios) if scenarios is not None else default_scenarios()
    if not selected:
        raise ValueError("at least one scenario is required")
    records = [
        scenario_record(
            scenario,
            output_tokens=output_tokens,
            target_ms_per_token=target_ms_per_token,
            draft_ms_per_token=draft_ms_per_token,
            target_verify_overhead_ms=target_verify_overhead_ms,
        )
        for scenario in selected
    ]
    best = max(records, key=lambda item: float(item.get("speedup_vs_target_only") or 0.0))
    worst = min(records, key=lambda item: float(item.get("speedup_vs_target_only") or 0.0))
    profitable_count = sum(1 for item in records if item.get("profitable"))
    return {
        "schema_version": 1,
        "kind": "speculative_decode_probe",
        "config": {
            "output_tokens": output_tokens,
            "target_ms_per_token": target_ms_per_token,
            "draft_ms_per_token": draft_ms_per_token,
            "target_verify_overhead_ms": target_verify_overhead_ms,
        },
        "scenario_count": len(records),
        "profitable_count": profitable_count,
        "best_scenario": {
            "name": best["name"],
            "speedup_vs_target_only": best["speedup_vs_target_only"],
            "acceptance_rate": best["acceptance_rate"],
            "verify_window": best["verify_window"],
        },
        "worst_scenario": {
            "name": worst["name"],
            "speedup_vs_target_only": worst["speedup_vs_target_only"],
            "acceptance_rate": worst["acceptance_rate"],
            "verify_window": worst["verify_window"],
        },
        "scenarios": records,
        "rankings": {
            "by_speedup": _rank(records, "speedup_vs_target_only"),
            "by_tpot_delta": _rank(records, "tpot_delta_ms", reverse=False),
        },
        "interpretation": [
            "Speculative decoding is useful only when accepted draft tokens offset draft and verification overhead.",
            "Low acceptance rate can make speculative decoding slower than target-only decode.",
            "Longer verify windows help only when the draft model stays aligned with the target model.",
            "Real runs must attach target/draft model names, sampling settings, output quality checks, and TPOT traces.",
        ],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scenario",
        action="append",
        type=parse_scenario,
        help="Scenario formatted as name:acceptance_rate:verify_window; may be repeated",
    )
    parser.add_argument("--output-tokens", type=int, default=128)
    parser.add_argument("--target-ms-per-token", type=float, default=10.0)
    parser.add_argument("--draft-ms-per-token", type=float, default=1.5)
    parser.add_argument("--target-verify-overhead-ms", type=float, default=0.8)
    parser.add_argument("--output", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        report = build_speculative_decode_report(
            scenarios=args.scenario,
            output_tokens=require_int_range("--output-tokens", args.output_tokens, 1, 10_000_000),
            target_ms_per_token=require_float_range(
                "--target-ms-per-token", args.target_ms_per_token, 0.001, 86_400_000.0
            ),
            draft_ms_per_token=require_float_range(
                "--draft-ms-per-token", args.draft_ms_per_token, 0.0, 86_400_000.0
            ),
            target_verify_overhead_ms=require_float_range(
                "--target-verify-overhead-ms", args.target_verify_overhead_ms, 0.0, 86_400_000.0
            ),
        )
    except ValueError as exc:
        parser.error(str(exc))
    output = Path(args.output)
    write_text_file(output, json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(f"wrote {redacted_path_label(output.as_posix())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
