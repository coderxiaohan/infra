"""CLI for Mini Transformer prefill/decode generation traces."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

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
    parser.add_argument("--no-cache", action="store_true", help="Recompute the full prefix on every decode step")
    parser.add_argument("--output", type=Path, help="Write JSON trace to this path instead of stdout")
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
        trace = run_generation_trace(
            config,
            _parse_prompt(args.prompt),
            max_new_tokens=require_int_range("--max-new-tokens", args.max_new_tokens, 0, 10_000_000),
            seed=args.seed,
            use_cache=not args.no_cache,
        )
    except ValueError as exc:
        parser.error(str(exc))
    payload = json.dumps(trace, indent=2, sort_keys=True)
    if args.output is not None:
        write_text_file(args.output, payload + "\n")
    else:
        print(payload)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
