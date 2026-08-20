"""Compare cached decode with full-prefix recompute for Mini Transformer traces."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Sequence

from .cli_io import require_float_range, require_int_range, write_text_file
from .transformer import MiniTransformerConfig, run_generation_trace


def _parse_prompt(raw_tokens: Sequence[str]) -> list[int]:
    if len(raw_tokens) == 1 and "," in raw_tokens[0]:
        raw_tokens = [piece for piece in raw_tokens[0].split(",") if piece]
    try:
        tokens = [int(token) for token in raw_tokens]
    except ValueError as exc:
        raise ValueError("prompt token ids must be integers") from exc
    if not tokens:
        raise ValueError("prompt must contain at least one token id")
    return tokens


def _ratio(numerator: int | float, denominator: int | float) -> float | None:
    if denominator == 0:
        return None
    return float(numerator) / float(denominator)


def _attention_score_elements(
    config: MiniTransformerConfig,
    record: dict[str, Any],
    *,
    use_cache: bool,
) -> int:
    phase = record.get("phase")
    if phase == "prefill":
        query_tokens = len(record.get("input_ids", []))
        key_tokens = query_tokens
    elif use_cache:
        query_tokens = len(record.get("input_ids", []))
        key_tokens = len(record.get("generated_ids", []))
    else:
        query_tokens = len(record.get("generated_ids", []))
        key_tokens = query_tokens
    return int(config.num_heads * query_tokens * key_tokens)


def _total_input_tokens(records: Sequence[dict[str, Any]]) -> int:
    return sum(len(record.get("input_ids", [])) for record in records)


def _max_kv_bytes(records: Sequence[dict[str, Any]]) -> int:
    return max((int(record.get("kv_cache_total_bytes", 0)) for record in records), default=0)


def compare_cache_strategies(
    config: MiniTransformerConfig | None,
    prompt_ids: Sequence[int],
    *,
    max_new_tokens: int,
    seed: int = 0,
) -> dict[str, Any]:
    """Run cached and no-cache traces and summarize the expected decode savings."""

    config = config or MiniTransformerConfig()
    cached = run_generation_trace(
        config,
        prompt_ids,
        max_new_tokens=max_new_tokens,
        seed=seed,
        use_cache=True,
    )
    recompute = run_generation_trace(
        config,
        prompt_ids,
        max_new_tokens=max_new_tokens,
        seed=seed,
        use_cache=False,
    )
    cached_records = cached["records"]
    recompute_records = recompute["records"]

    steps: list[dict[str, Any]] = []
    cached_total_scores = 0
    recompute_total_scores = 0
    for cached_record, recompute_record in zip(cached_records, recompute_records):
        cached_scores = _attention_score_elements(config, cached_record, use_cache=True)
        recompute_scores = _attention_score_elements(config, recompute_record, use_cache=False)
        cached_total_scores += cached_scores
        recompute_total_scores += recompute_scores
        savings = recompute_scores - cached_scores
        cached_input_tokens = len(cached_record.get("input_ids", []))
        recompute_input_tokens = len(recompute_record.get("input_ids", []))
        steps.append(
            {
                "phase": cached_record.get("phase"),
                "step": cached_record.get("step"),
                "generated_ids_match": cached_record.get("generated_ids")
                == recompute_record.get("generated_ids"),
                "cached_input_tokens": cached_input_tokens,
                "recompute_input_tokens": recompute_input_tokens,
                "input_token_savings": recompute_input_tokens - cached_input_tokens,
                "cached_attention_score_elements": cached_scores,
                "recompute_attention_score_elements": recompute_scores,
                "attention_score_element_savings": savings,
                "attention_score_element_savings_ratio": _ratio(savings, recompute_scores),
                "cached_kv_cache_total_bytes": cached_record.get("kv_cache_total_bytes", 0),
                "cached_kv_cache_lengths": cached_record.get("kv_cache_lengths", []),
                "recompute_kv_cache_total_bytes": recompute_record.get("kv_cache_total_bytes", 0),
                "cached_argmax_next_token": cached_record.get("argmax_next_token"),
                "recompute_argmax_next_token": recompute_record.get("argmax_next_token"),
            }
        )

    cached_total_input = _total_input_tokens(cached_records)
    recompute_total_input = _total_input_tokens(recompute_records)
    input_savings = recompute_total_input - cached_total_input
    score_savings = recompute_total_scores - cached_total_scores
    generated_match = cached["generated_ids"] == recompute["generated_ids"]

    return {
        "schema_version": 1,
        "kind": "mini_transformer_cache_compare",
        "seed": seed,
        "config": config.to_dict(),
        "prompt_ids": list(prompt_ids),
        "max_new_tokens": max_new_tokens,
        "generated_ids_match": generated_match,
        "cached_generated_ids": cached["generated_ids"],
        "recompute_generated_ids": recompute["generated_ids"],
        "totals": {
            "cached_input_tokens": cached_total_input,
            "recompute_input_tokens": recompute_total_input,
            "input_token_savings": input_savings,
            "input_token_savings_ratio": _ratio(input_savings, recompute_total_input),
            "cached_attention_score_elements": cached_total_scores,
            "recompute_attention_score_elements": recompute_total_scores,
            "attention_score_element_savings": score_savings,
            "attention_score_element_savings_ratio": _ratio(score_savings, recompute_total_scores),
            "cached_max_kv_cache_total_bytes": _max_kv_bytes(cached_records),
            "recompute_max_kv_cache_total_bytes": _max_kv_bytes(recompute_records),
        },
        "steps": steps,
        "notes": [
            "Cached decode pays KV storage to avoid full-prefix recompute at each generated token.",
            "Attention score elements are a compact proxy for decode attention work, not wall-clock timing.",
            "Generated IDs should match when both traces use the same seed and deterministic CPU settings.",
        ],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prompt", nargs="+", help="Token ids, either space-separated or comma-separated")
    parser.add_argument("--max-new-tokens", type=int, default=4)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--vocab-size", type=int, default=256)
    parser.add_argument("--max-seq-len", type=int, default=256)
    parser.add_argument("--hidden-size", type=int, default=64)
    parser.add_argument("--num-layers", type=int, default=2)
    parser.add_argument("--num-heads", type=int, default=4)
    parser.add_argument("--ffn-hidden-size", type=int, default=256)
    parser.add_argument("--rope-theta", type=float, default=10000.0)
    parser.add_argument("--dtype", default="float32")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--output", type=Path, help="Write JSON comparison to this path instead of stdout")
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
        comparison = compare_cache_strategies(
            config,
            _parse_prompt(args.prompt),
            max_new_tokens=require_int_range("--max-new-tokens", args.max_new_tokens, 0, 10_000_000),
            seed=args.seed,
        )
    except ValueError as exc:
        parser.error(str(exc))
    payload = json.dumps(comparison, indent=2, sort_keys=True)
    if args.output is not None:
        write_text_file(args.output, payload + "\n")
    else:
        print(payload)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
