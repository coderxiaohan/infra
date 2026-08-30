"""Roofline-style bottleneck advice for Mini Transformer workload profiles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Iterable, Sequence

from .cli_io import (
    require_float_range,
    require_int_range,
    require_positive_float,
    write_text_file,
)
from .transformer import MiniTransformerConfig
from .workload_profile import build_workload_report, safe_ratio


DEFAULT_PEAK_TFLOPS = 10.0
DEFAULT_BANDWIDTH_GBPS = 300.0


GROUP_COMPONENTS = {
    "qkv_projection": {
        "flops": ("qkv_projection",),
        "bytes": ("qkv_activation_bytes",),
        "advice": (
            "Use fused QKV projection, tensor-core-friendly layouts, and weight-only quantization "
            "to reduce projection bandwidth and improve matmul occupancy."
        ),
    },
    "rope": {
        "flops": ("rope_qk",),
        "bytes": (),
        "advice": (
            "Fuse RoPE with Q/K staging or attention input preparation so rotary work does not "
            "become a separate memory round trip."
        ),
    },
    "attention_scores": {
        "flops": ("attention_scores",),
        "bytes": ("attention_score_bytes",),
        "advice": (
            "Prefer tiled/FlashAttention-style score computation to avoid materializing large "
            "score matrices and to improve K-cache locality."
        ),
    },
    "attention_values": {
        "flops": ("attention_values",),
        "bytes": ("attention_probability_bytes",),
        "advice": (
            "Fuse probability application with value accumulation and keep V reads coalesced "
            "through block-table-aware KV layouts."
        ),
    },
    "attention_output_projection": {
        "flops": ("attention_output_projection",),
        "bytes": (),
        "advice": (
            "Keep attention output projection in the same fused scheduling region where possible "
            "and preserve tensor-core-aligned hidden dimensions."
        ),
    },
    "ffn": {
        "flops": ("ffn_gate_up_projection", "ffn_swiglu", "ffn_down_projection"),
        "bytes": (),
        "advice": (
            "Use fused SwiGLU kernels, tensor-core matmuls, and weight-only quantization because "
            "FFN projections usually dominate decoder-layer FLOPs."
        ),
    },
    "rmsnorm": {
        "flops": ("rmsnorm_attention", "rmsnorm_ffn", "final_rmsnorm"),
        "bytes": (),
        "advice": (
            "Fuse RMSNorm with adjacent projection input preparation to reduce hidden-state "
            "read/write traffic."
        ),
    },
    "lm_head": {
        "flops": ("lm_head",),
        "bytes": ("logits_bytes",),
        "advice": (
            "For large vocabularies, use tensor-parallel or sampled/top-k-aware output paths; "
            "for this tiny model it is mainly a schema placeholder."
        ),
    },
    "hidden_state_io": {
        "flops": (),
        "bytes": ("hidden_read_write_bytes",),
        "advice": (
            "Treat hidden-state traffic as a shared pipeline cost and fuse adjacent normalization, "
            "projection, and residual operations to remove full-tensor round trips."
        ),
    },
    "persistent_kv_cache": {
        "flops": (),
        "bytes": ("persistent_kv_cache_bytes", "kv_write_bytes", "kv_read_bytes"),
        "advice": (
            "Use paged KV blocks, prefix sharing, quantized KV experiments, and contiguous gather "
            "paths to reduce long-context decode bandwidth pressure."
        ),
    },
}


def _sum_named(items: Iterable[dict[str, Any]], field: str, names: Sequence[str]) -> int:
    total = 0
    for step in items:
        values = step.get(field)
        if not isinstance(values, dict):
            continue
        for name in names:
            total += int(values.get(name, 0))
    return total


def _classify(
    *,
    flops: int,
    bytes_moved: int,
    peak_flops_per_second: float,
    bandwidth_bytes_per_second: float,
) -> dict[str, Any]:
    arithmetic_intensity = safe_ratio(flops, bytes_moved)
    ridge_point = peak_flops_per_second / bandwidth_bytes_per_second
    if flops <= 0 and bytes_moved > 0:
        limiter = "memory"
        bound_flops_per_second = 0.0
        estimated_time_ms = bytes_moved / bandwidth_bytes_per_second * 1000.0
    elif arithmetic_intensity is None:
        limiter = "unknown"
        bound_flops_per_second = None
        estimated_time_ms = None
    else:
        limiter = "compute" if arithmetic_intensity >= ridge_point else "memory"
        bound_flops_per_second = min(
            peak_flops_per_second,
            arithmetic_intensity * bandwidth_bytes_per_second,
        )
        estimated_time_ms = (
            flops / bound_flops_per_second * 1000.0 if bound_flops_per_second > 0 else None
        )
    return {
        "arithmetic_intensity_flops_per_byte": arithmetic_intensity,
        "ridge_point_flops_per_byte": ridge_point,
        "roofline_limiter": limiter,
        "roofline_bound_flops_per_second": bound_flops_per_second,
        "estimated_roofline_time_ms": estimated_time_ms,
    }


def _group_records(
    strategy: dict[str, Any],
    *,
    peak_flops_per_second: float,
    bandwidth_bytes_per_second: float,
) -> list[dict[str, Any]]:
    steps = strategy.get("steps")
    if not isinstance(steps, list):
        steps = []
    total_flops = int(strategy.get("total_flops", 0))
    total_bytes = int(strategy.get("total_bytes", 0))
    records: list[dict[str, Any]] = []
    for group_name, group in GROUP_COMPONENTS.items():
        flops = _sum_named(steps, "component_flops", group["flops"])
        bytes_moved = _sum_named(steps, "component_bytes", group["bytes"])
        classification = _classify(
            flops=flops,
            bytes_moved=bytes_moved,
            peak_flops_per_second=peak_flops_per_second,
            bandwidth_bytes_per_second=bandwidth_bytes_per_second,
        )
        records.append(
            {
                "group": group_name,
                "flops": flops,
                "bytes": bytes_moved,
                "flops_share": safe_ratio(flops, total_flops),
                "bytes_share": safe_ratio(bytes_moved, total_bytes),
                "optimization_advice": group["advice"],
                **classification,
            }
        )
    records.sort(
        key=lambda item: (
            float(item.get("estimated_roofline_time_ms") or 0.0),
            int(item.get("flops", 0)),
            int(item.get("bytes", 0)),
        ),
        reverse=True,
    )
    return records


def _attribution_summary(
    records: Sequence[dict[str, Any]],
    *,
    total_flops: int,
    total_bytes: int,
) -> dict[str, Any]:
    attributed_flops = sum(int(item.get("flops", 0)) for item in records)
    attributed_bytes = sum(int(item.get("bytes", 0)) for item in records)
    return {
        "mode": "exclusive_component_buckets",
        "attributed_flops": attributed_flops,
        "attributed_bytes": attributed_bytes,
        "unattributed_flops": total_flops - attributed_flops,
        "unattributed_bytes": total_bytes - attributed_bytes,
        "flops_share_sum": safe_ratio(attributed_flops, total_flops),
        "bytes_share_sum": safe_ratio(attributed_bytes, total_bytes),
    }


def build_bottleneck_report(
    config: MiniTransformerConfig | None,
    *,
    prompt_tokens: int,
    max_new_tokens: int,
    peak_tflops: float = DEFAULT_PEAK_TFLOPS,
    bandwidth_gbps: float = DEFAULT_BANDWIDTH_GBPS,
) -> dict[str, Any]:
    peak_tflops = require_positive_float("peak_tflops", peak_tflops)
    bandwidth_gbps = require_positive_float("bandwidth_gbps", bandwidth_gbps)
    config = config or MiniTransformerConfig()
    workload = build_workload_report(
        config,
        prompt_tokens=prompt_tokens,
        max_new_tokens=max_new_tokens,
    )
    peak_flops_per_second = peak_tflops * 1_000_000_000_000.0
    bandwidth_bytes_per_second = bandwidth_gbps * 1_000_000_000.0
    hardware_model = {
        "peak_tflops": peak_tflops,
        "bandwidth_gbps": bandwidth_gbps,
        "peak_flops_per_second": peak_flops_per_second,
        "bandwidth_bytes_per_second": bandwidth_bytes_per_second,
        "ridge_point_flops_per_byte": peak_flops_per_second / bandwidth_bytes_per_second,
    }
    cached_groups = _group_records(
        workload["cached_decode"],
        peak_flops_per_second=peak_flops_per_second,
        bandwidth_bytes_per_second=bandwidth_bytes_per_second,
    )
    recompute_groups = _group_records(
        workload["full_recompute"],
        peak_flops_per_second=peak_flops_per_second,
        bandwidth_bytes_per_second=bandwidth_bytes_per_second,
    )
    top_cached = cached_groups[0] if cached_groups else {}
    top_recompute = recompute_groups[0] if recompute_groups else {}
    cached_attribution = _attribution_summary(
        cached_groups,
        total_flops=int(workload["cached_decode"]["total_flops"]),
        total_bytes=int(workload["cached_decode"]["total_bytes"]),
    )
    recompute_attribution = _attribution_summary(
        recompute_groups,
        total_flops=int(workload["full_recompute"]["total_flops"]),
        total_bytes=int(workload["full_recompute"]["total_bytes"]),
    )
    return {
        "schema_version": 1,
        "kind": "mini_transformer_bottleneck_advisor",
        "config": config.to_dict(),
        "prompt_tokens": prompt_tokens,
        "max_new_tokens": max_new_tokens,
        "hardware_model": hardware_model,
        "workload_comparison": workload["comparison"],
        "cached_decode": {
            "strategy": workload["cached_decode"]["strategy"],
            "top_bottleneck_group": top_cached.get("group"),
            "attribution": cached_attribution,
            "group_rankings": cached_groups,
        },
        "full_recompute": {
            "strategy": workload["full_recompute"]["strategy"],
            "top_bottleneck_group": top_recompute.get("group"),
            "attribution": recompute_attribution,
            "group_rankings": recompute_groups,
        },
        "recommendations": [
            "Use KV cache and paged KV layout first when decode recompute dominates token work.",
            "Use FlashAttention-style tiling when attention score/probability bytes dominate memory traffic.",
            "Use tensor-core-friendly FFN/QKV matmul layouts and weight-only quantization when projections dominate FLOPs.",
            "Fuse RMSNorm, RoPE, and adjacent projection staging to avoid extra hidden-state memory passes.",
            "Treat this as a planning model; real bottleneck ranking must be validated on target GPU hardware.",
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
    parser.add_argument("--peak-tflops", type=float, default=DEFAULT_PEAK_TFLOPS)
    parser.add_argument("--bandwidth-gbps", type=float, default=DEFAULT_BANDWIDTH_GBPS)
    parser.add_argument("--output", type=Path, help="Write JSON bottleneck report to this path")
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
        report = build_bottleneck_report(
            config,
            prompt_tokens=require_int_range("--prompt-tokens", args.prompt_tokens, 1, 10_000_000),
            max_new_tokens=require_int_range("--max-new-tokens", args.max_new_tokens, 0, 10_000_000),
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
