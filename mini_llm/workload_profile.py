"""Static workload profile for Mini Transformer prefill/decode paths.

The profile is deliberately analytical rather than timed. It estimates the
operator-level work that changes when decode uses a persistent KV cache instead
of recomputing the full prefix on every generated token.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

from .cli_io import (
    require_float_range,
    require_int_range,
    require_non_negative_int,
    require_positive_int,
    write_text_file,
)
from .transformer import MiniTransformerConfig


DTYPE_BYTES = {
    "float32": 4,
    "fp32": 4,
    "float16": 2,
    "fp16": 2,
    "bfloat16": 2,
    "bf16": 2,
}


@dataclass(frozen=True)
class WorkloadStep:
    phase: str
    step: int
    query_tokens: int
    key_tokens: int
    sequence_length: int
    persistent_kv_tokens: int
    component_flops: dict[str, int]
    component_bytes: dict[str, int]

    @property
    def total_flops(self) -> int:
        return sum(self.component_flops.values())

    @property
    def total_bytes(self) -> int:
        return sum(self.component_bytes.values())

    def to_dict(self) -> dict[str, Any]:
        return {
            "phase": self.phase,
            "step": self.step,
            "query_tokens": self.query_tokens,
            "key_tokens": self.key_tokens,
            "sequence_length": self.sequence_length,
            "persistent_kv_tokens": self.persistent_kv_tokens,
            "component_flops": dict(self.component_flops),
            "component_bytes": dict(self.component_bytes),
            "total_flops": self.total_flops,
            "total_bytes": self.total_bytes,
        }


def dtype_size(dtype: str) -> int:
    try:
        return DTYPE_BYTES[dtype.lower()]
    except KeyError as exc:
        raise ValueError(f"Unsupported dtype for workload profile: {dtype}") from exc


def _ratio(numerator: int | float, denominator: int | float) -> float | None:
    if denominator == 0:
        return None
    return float(numerator) / float(denominator)


def _dense_flops(m: int, k: int, n: int, *, batch: int = 1) -> int:
    return int(2 * batch * m * k * n)


def _layer_step(
    config: MiniTransformerConfig,
    *,
    query_tokens: int,
    key_tokens: int,
    sequence_length: int,
    persistent_kv_tokens: int,
    phase: str,
    step: int,
    bytes_per_value: int,
) -> WorkloadStep:
    hidden = config.hidden_size
    ffn = config.ffn_hidden_size
    heads = config.num_heads
    head_dim = config.head_dim
    batch = 1

    per_layer_flops = {
        "qkv_projection": _dense_flops(query_tokens, hidden, 3 * hidden, batch=batch),
        "rope_qk": int(12 * batch * heads * query_tokens * head_dim),
        "attention_scores": int(2 * batch * heads * query_tokens * key_tokens * head_dim),
        "attention_values": int(2 * batch * heads * query_tokens * key_tokens * head_dim),
        "attention_output_projection": _dense_flops(query_tokens, hidden, hidden, batch=batch),
        "rmsnorm_attention": int(5 * batch * query_tokens * hidden),
        "ffn_gate_up_projection": _dense_flops(query_tokens, hidden, 2 * ffn, batch=batch),
        "ffn_swiglu": int(6 * batch * query_tokens * ffn),
        "ffn_down_projection": _dense_flops(query_tokens, ffn, hidden, batch=batch),
        "rmsnorm_ffn": int(5 * batch * query_tokens * hidden),
    }
    component_flops = {
        name: value * config.num_layers for name, value in per_layer_flops.items()
    }
    component_flops["final_rmsnorm"] = int(5 * batch * query_tokens * hidden)
    component_flops["lm_head"] = _dense_flops(query_tokens, hidden, config.vocab_size, batch=batch)

    qkv_activation = batch * query_tokens * 3 * hidden * bytes_per_value
    attention_scores = batch * heads * query_tokens * key_tokens * bytes_per_value
    attention_probs = attention_scores
    kv_read = batch * heads * key_tokens * head_dim * 2 * bytes_per_value
    kv_write = batch * heads * query_tokens * head_dim * 2 * bytes_per_value
    persistent_kv = (
        config.num_layers
        * batch
        * heads
        * persistent_kv_tokens
        * head_dim
        * 2
        * bytes_per_value
    )

    per_layer_bytes = {
        "qkv_activation_bytes": qkv_activation,
        "attention_score_bytes": attention_scores,
        "attention_probability_bytes": attention_probs,
        "kv_read_bytes": kv_read,
        "kv_write_bytes": kv_write,
        "hidden_read_write_bytes": batch * query_tokens * hidden * 4 * bytes_per_value,
    }
    component_bytes = {
        name: value * config.num_layers for name, value in per_layer_bytes.items()
    }
    component_bytes["logits_bytes"] = batch * query_tokens * config.vocab_size * bytes_per_value
    component_bytes["persistent_kv_cache_bytes"] = persistent_kv

    return WorkloadStep(
        phase=phase,
        step=step,
        query_tokens=query_tokens,
        key_tokens=key_tokens,
        sequence_length=sequence_length,
        persistent_kv_tokens=persistent_kv_tokens,
        component_flops=component_flops,
        component_bytes=component_bytes,
    )


def build_strategy_profile(
    config: MiniTransformerConfig,
    *,
    prompt_tokens: int,
    max_new_tokens: int,
    use_cache: bool,
) -> dict[str, Any]:
    config.validate()
    prompt_tokens = require_positive_int("prompt_tokens", prompt_tokens)
    max_new_tokens = require_non_negative_int("max_new_tokens", max_new_tokens)
    if prompt_tokens + max_new_tokens > config.max_seq_len:
        raise ValueError("prompt_tokens + max_new_tokens exceeds max_seq_len")

    bytes_per_value = dtype_size(config.dtype)
    steps: list[WorkloadStep] = [
        _layer_step(
            config,
            query_tokens=prompt_tokens,
            key_tokens=prompt_tokens,
            sequence_length=prompt_tokens,
            persistent_kv_tokens=prompt_tokens if use_cache else 0,
            phase="prefill",
            step=0,
            bytes_per_value=bytes_per_value,
        )
    ]
    for step in range(1, max_new_tokens + 1):
        sequence_length = prompt_tokens + step
        query_tokens = 1 if use_cache else sequence_length
        persistent_kv_tokens = sequence_length if use_cache else 0
        steps.append(
            _layer_step(
                config,
                query_tokens=query_tokens,
                key_tokens=sequence_length,
                sequence_length=sequence_length,
                persistent_kv_tokens=persistent_kv_tokens,
                phase="decode",
                step=step,
                bytes_per_value=bytes_per_value,
            )
        )

    total_flops = sum(step.total_flops for step in steps)
    total_bytes = sum(step.total_bytes for step in steps)
    attention_score_elements = sum(
        step.query_tokens * step.key_tokens * config.num_heads for step in steps
    )
    input_tokens_processed = sum(step.query_tokens for step in steps)
    max_persistent_kv = max(
        step.component_bytes["persistent_kv_cache_bytes"] for step in steps
    )
    return {
        "strategy": "cached_decode" if use_cache else "full_recompute",
        "input_tokens_processed": input_tokens_processed,
        "attention_score_elements": attention_score_elements,
        "total_flops": total_flops,
        "total_bytes": total_bytes,
        "max_persistent_kv_cache_bytes": max_persistent_kv,
        "steps": [step.to_dict() for step in steps],
    }


def build_workload_report(
    config: MiniTransformerConfig | None,
    *,
    prompt_tokens: int,
    max_new_tokens: int,
) -> dict[str, Any]:
    config = config or MiniTransformerConfig()
    cached = build_strategy_profile(
        config,
        prompt_tokens=prompt_tokens,
        max_new_tokens=max_new_tokens,
        use_cache=True,
    )
    recompute = build_strategy_profile(
        config,
        prompt_tokens=prompt_tokens,
        max_new_tokens=max_new_tokens,
        use_cache=False,
    )

    flops_saved = int(recompute["total_flops"]) - int(cached["total_flops"])
    bytes_saved = int(recompute["total_bytes"]) - int(cached["total_bytes"])
    score_saved = int(recompute["attention_score_elements"]) - int(
        cached["attention_score_elements"]
    )
    input_saved = int(recompute["input_tokens_processed"]) - int(
        cached["input_tokens_processed"]
    )
    return {
        "schema_version": 1,
        "kind": "mini_transformer_workload_profile",
        "config": config.to_dict(),
        "prompt_tokens": prompt_tokens,
        "max_new_tokens": max_new_tokens,
        "dtype_bytes": dtype_size(config.dtype),
        "cached_decode": cached,
        "full_recompute": recompute,
        "comparison": {
            "input_tokens_saved": input_saved,
            "input_tokens_saved_ratio": _ratio(input_saved, recompute["input_tokens_processed"]),
            "attention_score_elements_saved": score_saved,
            "attention_score_elements_saved_ratio": _ratio(
                score_saved, recompute["attention_score_elements"]
            ),
            "flops_saved": flops_saved,
            "flops_saved_ratio": _ratio(flops_saved, recompute["total_flops"]),
            "bytes_saved": bytes_saved,
            "bytes_saved_ratio": _ratio(bytes_saved, recompute["total_bytes"]),
            "cached_persistent_kv_cache_bytes": cached["max_persistent_kv_cache_bytes"],
            "recompute_persistent_kv_cache_bytes": recompute["max_persistent_kv_cache_bytes"],
        },
        "notes": [
            "FLOP estimates use multiply-add as two FLOPs for dense projections and attention matmuls.",
            "Byte estimates cover activation-like tensors, attention intermediates, KV reads/writes, logits, and persistent KV cache bytes.",
            "This static profile is for reasoning about bottlenecks; measured latency still requires hardware-specific validation.",
        ],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prompt-tokens", type=int, default=128)
    parser.add_argument("--max-new-tokens", type=int, default=16)
    parser.add_argument("--vocab-size", type=int, default=256)
    parser.add_argument("--max-seq-len", type=int, default=256)
    parser.add_argument("--hidden-size", type=int, default=64)
    parser.add_argument("--num-layers", type=int, default=2)
    parser.add_argument("--num-heads", type=int, default=4)
    parser.add_argument("--ffn-hidden-size", type=int, default=256)
    parser.add_argument("--rope-theta", type=float, default=10000.0)
    parser.add_argument("--dtype", default="float32")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--output", type=Path, help="Write JSON workload profile to this path")
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
        report = build_workload_report(
            config,
            prompt_tokens=require_int_range("--prompt-tokens", args.prompt_tokens, 1, 10_000_000),
            max_new_tokens=require_int_range("--max-new-tokens", args.max_new_tokens, 0, 10_000_000),
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
