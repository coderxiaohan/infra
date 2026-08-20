"""Scenario sweep for Mini Transformer inference optimization planning."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

from .bottleneck_advisor import (
    DEFAULT_BANDWIDTH_GBPS,
    DEFAULT_PEAK_TFLOPS,
    build_bottleneck_report,
)
from .cli_io import (
    require_float_range,
    require_int_range,
    require_non_negative_int,
    require_positive_float,
    require_positive_int,
    write_text_file,
)
from .transformer import MiniTransformerConfig


DEFAULT_SCENARIOS = (
    ("short_chat", 64, 16),
    ("balanced_chat", 128, 64),
    ("decode_heavy", 32, 160),
    ("prefill_heavy", 192, 16),
    ("long_context", 224, 24),
)


@dataclass(frozen=True)
class Scenario:
    name: str
    prompt_tokens: int
    max_new_tokens: int

    def validate(self, max_seq_len: int) -> None:
        if not isinstance(self.name, str) or not self.name.strip():
            raise ValueError("scenario name must not be empty")
        prompt_tokens = require_positive_int(f"{self.name}: prompt_tokens", self.prompt_tokens)
        max_new_tokens = require_non_negative_int(f"{self.name}: max_new_tokens", self.max_new_tokens)
        sequence_limit = require_positive_int("max_seq_len", max_seq_len)
        if prompt_tokens + max_new_tokens > sequence_limit:
            raise ValueError(
                f"{self.name}: prompt_tokens + max_new_tokens exceeds max_seq_len "
                f"({prompt_tokens} + {max_new_tokens} > {sequence_limit})"
            )

    def to_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "prompt_tokens": self.prompt_tokens,
            "max_new_tokens": self.max_new_tokens,
            "total_tokens": self.prompt_tokens + self.max_new_tokens,
        }


def parse_scenario(value: str) -> Scenario:
    parts = value.split(":")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("scenario must be formatted as name:prompt_tokens:max_new_tokens")
    name, prompt_raw, new_raw = parts
    try:
        prompt_tokens = int(prompt_raw)
        max_new_tokens = int(new_raw)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("scenario token counts must be integers") from exc
    return Scenario(name=name, prompt_tokens=prompt_tokens, max_new_tokens=max_new_tokens)


def default_scenarios() -> list[Scenario]:
    return [
        Scenario(name=name, prompt_tokens=prompt, max_new_tokens=new)
        for name, prompt, new in DEFAULT_SCENARIOS
    ]


def _top_group(report: dict[str, Any], strategy: str) -> dict[str, Any]:
    rankings = report.get(strategy, {}).get("group_rankings", [])
    if isinstance(rankings, list) and rankings:
        top = rankings[0]
        return top if isinstance(top, dict) else {}
    return {}


def _scenario_record(
    config: MiniTransformerConfig,
    scenario: Scenario,
    *,
    peak_tflops: float,
    bandwidth_gbps: float,
) -> dict[str, Any]:
    scenario.validate(config.max_seq_len)
    report = build_bottleneck_report(
        config,
        prompt_tokens=scenario.prompt_tokens,
        max_new_tokens=scenario.max_new_tokens,
        peak_tflops=peak_tflops,
        bandwidth_gbps=bandwidth_gbps,
    )
    comparison = report["workload_comparison"]
    cached_top = _top_group(report, "cached_decode")
    recompute_top = _top_group(report, "full_recompute")
    cached_time = cached_top.get("estimated_roofline_time_ms")
    recompute_time = recompute_top.get("estimated_roofline_time_ms")
    return {
        **scenario.to_dict(),
        "cached_top_bottleneck_group": report["cached_decode"].get("top_bottleneck_group"),
        "cached_top_roofline_limiter": cached_top.get("roofline_limiter"),
        "cached_top_estimated_roofline_time_ms": cached_time,
        "full_recompute_top_bottleneck_group": report["full_recompute"].get("top_bottleneck_group"),
        "full_recompute_top_roofline_limiter": recompute_top.get("roofline_limiter"),
        "full_recompute_top_estimated_roofline_time_ms": recompute_time,
        "input_tokens_saved": comparison.get("input_tokens_saved"),
        "input_tokens_saved_ratio": comparison.get("input_tokens_saved_ratio"),
        "attention_score_elements_saved": comparison.get("attention_score_elements_saved"),
        "attention_score_elements_saved_ratio": comparison.get("attention_score_elements_saved_ratio"),
        "flops_saved": comparison.get("flops_saved"),
        "flops_saved_ratio": comparison.get("flops_saved_ratio"),
        "bytes_saved": comparison.get("bytes_saved"),
        "bytes_saved_ratio": comparison.get("bytes_saved_ratio"),
        "cached_persistent_kv_cache_bytes": comparison.get("cached_persistent_kv_cache_bytes"),
        "report": report,
    }


def _rank(records: Sequence[dict[str, Any]], key: str) -> list[dict[str, Any]]:
    ranked = sorted(
        records,
        key=lambda item: (
            float(item.get(key) or 0.0),
            int(item.get("total_tokens", 0)),
            str(item.get("name", "")),
        ),
        reverse=True,
    )
    return [
        {
            "rank": index + 1,
            "name": item.get("name"),
            key: item.get(key),
            "prompt_tokens": item.get("prompt_tokens"),
            "max_new_tokens": item.get("max_new_tokens"),
            "cached_top_bottleneck_group": item.get("cached_top_bottleneck_group"),
        }
        for index, item in enumerate(ranked)
    ]


def build_scenario_sweep(
    config: MiniTransformerConfig | None,
    scenarios: Sequence[Scenario] | None = None,
    *,
    peak_tflops: float = DEFAULT_PEAK_TFLOPS,
    bandwidth_gbps: float = DEFAULT_BANDWIDTH_GBPS,
) -> dict[str, Any]:
    peak_tflops = require_positive_float("peak_tflops", peak_tflops)
    bandwidth_gbps = require_positive_float("bandwidth_gbps", bandwidth_gbps)
    config = config or MiniTransformerConfig()
    selected = list(scenarios) if scenarios is not None else default_scenarios()
    if not selected:
        raise ValueError("at least one scenario is required")
    records = [
        _scenario_record(
            config,
            scenario,
            peak_tflops=peak_tflops,
            bandwidth_gbps=bandwidth_gbps,
        )
        for scenario in selected
    ]
    compact_records = [
        {key: value for key, value in record.items() if key != "report"} for record in records
    ]
    return {
        "schema_version": 1,
        "kind": "mini_transformer_scenario_sweep",
        "config": config.to_dict(),
        "hardware_model": {
            "peak_tflops": peak_tflops,
            "bandwidth_gbps": bandwidth_gbps,
        },
        "scenario_count": len(records),
        "scenarios": compact_records,
        "rankings": {
            "by_flops_saved_ratio": _rank(compact_records, "flops_saved_ratio"),
            "by_bytes_saved_ratio": _rank(compact_records, "bytes_saved_ratio"),
            "by_cached_persistent_kv_cache_bytes": _rank(
                compact_records,
                "cached_persistent_kv_cache_bytes",
            ),
            "by_attention_score_elements_saved_ratio": _rank(
                compact_records,
                "attention_score_elements_saved_ratio",
            ),
        },
        "recommendations": [
            "Use decode-heavy scenarios to stress KV-cache savings and scheduler TPOT behavior.",
            "Use prefill-heavy scenarios to stress dense projection, FFN, and attention-score materialization.",
            "Use long-context scenarios to stress persistent KV memory, Paged KV locality, and block-table gather.",
            "Treat the top bottleneck group per scenario as a planning hint before running hardware measurements.",
        ],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scenario",
        action="append",
        type=parse_scenario,
        help="Scenario formatted as name:prompt_tokens:max_new_tokens; may be repeated",
    )
    parser.add_argument("--vocab-size", type=int, default=256)
    parser.add_argument("--max-seq-len", type=int, default=256)
    parser.add_argument("--hidden-size", type=int, default=64)
    parser.add_argument("--num-layers", type=int, default=2)
    parser.add_argument("--num-heads", type=int, default=4)
    parser.add_argument("--ffn-hidden-size", type=int, default=256)
    parser.add_argument("--rope-theta", type=float, default=10000.0)
    parser.add_argument("--dtype", default="float32")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--peak-tflops", type=float, default=DEFAULT_PEAK_TFLOPS)
    parser.add_argument("--bandwidth-gbps", type=float, default=DEFAULT_BANDWIDTH_GBPS)
    parser.add_argument("--output", type=Path, help="Write JSON scenario sweep to this path")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        config = MiniTransformerConfig(
            vocab_size=require_int_range("--vocab-size", args.vocab_size, 1, 10_000_000),
            max_seq_len=require_int_range("--max-seq-len", args.max_seq_len, 1, 10_000_000),
            hidden_size=require_int_range("--hidden-size", args.hidden_size, 1, 1_000_000),
            num_layers=require_int_range("--num-layers", args.num_layers, 1, 100_000),
            num_heads=require_int_range("--num-heads", args.num_heads, 1, 100_000),
            ffn_hidden_size=require_int_range("--ffn-hidden-size", args.ffn_hidden_size, 1, 10_000_000),
            rope_theta=require_float_range("--rope-theta", args.rope_theta, 0.001, 1.0e12),
            dtype=args.dtype,
            device=args.device,
        )
        report = build_scenario_sweep(
            config,
            args.scenario,
            peak_tflops=require_float_range("--peak-tflops", args.peak_tflops, 0.001, 1.0e9),
            bandwidth_gbps=require_float_range("--bandwidth-gbps", args.bandwidth_gbps, 0.001, 1.0e9),
        )
    except ValueError as exc:
        parser.error(str(exc))
    payload = json.dumps(report, indent=2, sort_keys=True)
    if args.output is not None:
        write_text_file(args.output, payload + "\n")
    else:
        print(payload)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
