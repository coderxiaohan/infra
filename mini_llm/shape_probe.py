"""Emit prefill/decode shape and KV-cache memory traces as JSON."""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List

from .cli_io import require_positive_int, write_text_file
from .kv_cache import KVCacheConfig, estimate_kv_cache


@dataclass(frozen=True)
class TransformerShapeConfig:
    batch_size: int
    seq_len: int
    decode_steps: int
    hidden_size: int
    num_layers: int
    num_heads: int
    head_dim: int
    dtype: str

    def validate(self) -> None:
        for name, value in asdict(self).items():
            if name == "dtype":
                continue
            require_positive_int(name, value)
        if self.hidden_size != self.num_heads * self.head_dim:
            raise ValueError(
                "hidden_size must equal num_heads * head_dim "
                f"({self.hidden_size} != {self.num_heads} * {self.head_dim})"
            )


def build_trace(config: TransformerShapeConfig) -> Dict[str, object]:
    config.validate()
    prefill_kv = estimate_kv_cache(
        KVCacheConfig(
            batch_size=config.batch_size,
            sequence_length=config.seq_len,
            num_layers=config.num_layers,
            num_heads=config.num_heads,
            head_dim=config.head_dim,
            dtype=config.dtype,
        )
    )

    decode: List[Dict[str, object]] = []
    for step in range(1, config.decode_steps + 1):
        total_seq = config.seq_len + step
        estimate = estimate_kv_cache(
            KVCacheConfig(
                batch_size=config.batch_size,
                sequence_length=total_seq,
                num_layers=config.num_layers,
                num_heads=config.num_heads,
                head_dim=config.head_dim,
                dtype=config.dtype,
            )
        )
        decode.append(
            {
                "step": step,
                "query_shape": [config.batch_size, 1, config.hidden_size],
                "kv_sequence_length": total_seq,
                "kv_total_bytes": estimate.total_bytes,
                "kv_total_mib": estimate.total_mib,
            }
        )

    return {
        "config": asdict(config),
        "prefill": {
            "input_shape": [config.batch_size, config.seq_len, config.hidden_size],
            "qkv_projection_shape": [
                config.batch_size,
                config.seq_len,
                3 * config.hidden_size,
            ],
            "attention_score_shape": [
                config.batch_size,
                config.num_heads,
                config.seq_len,
                config.seq_len,
            ],
            "kv_cache": prefill_kv.to_dict(),
        },
        "decode": decode,
        "notes": [
            "Prefill attends over the full prompt and exposes [seq, seq] attention work.",
            "Decode adds one token at a time and grows KV cache linearly with sequence length.",
        ],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch-size", type=int, default=1)
    parser.add_argument("--seq-len", type=int, default=128)
    parser.add_argument("--decode-steps", type=int, default=16)
    parser.add_argument("--hidden-size", type=int, default=768)
    parser.add_argument("--num-layers", type=int, default=12)
    parser.add_argument("--num-heads", type=int, default=12)
    parser.add_argument("--head-dim", type=int, default=64)
    parser.add_argument("--dtype", default="fp16")
    parser.add_argument("--output", type=Path, help="Optional JSON output path")
    return parser


def parse_args() -> argparse.Namespace:
    return build_parser().parse_args()


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        trace = build_trace(
            TransformerShapeConfig(
                batch_size=args.batch_size,
                seq_len=args.seq_len,
                decode_steps=args.decode_steps,
                hidden_size=args.hidden_size,
                num_layers=args.num_layers,
                num_heads=args.num_heads,
                head_dim=args.head_dim,
                dtype=args.dtype,
            )
        )
    except ValueError as exc:
        parser.error(str(exc))
    payload = json.dumps(trace, indent=2, sort_keys=True)
    if args.output:
        write_text_file(args.output, payload + "\n")
    else:
        print(payload)


if __name__ == "__main__":
    main()
