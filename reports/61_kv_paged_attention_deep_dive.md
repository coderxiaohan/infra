# KV/PagedAttention Deep Dive Implementation Report

## Scope

This report describes a native C++ evidence bridge for KV/PagedAttention mechanics. It does not claim a vLLM, SGLang, TensorRT-LLM, LMCache, or Mooncake benchmark run.

## Product Capabilities

PagedAttention, production KV block management, prefix cache policy, and remote KV transfer are mature product capabilities in systems such as vLLM, SGLang, TensorRT-LLM, LMCache, and Mooncake.

## InferLab Integration

InferLab connects those products through product profiles, replay evidence, benchmark matrix entries, artifact contracts, summary wiring, Prometheus export wiring, and final report cataloging.

## InferLab Native Slice

Implemented native entry:

- Typed composition model: `native_core/src/kv_paged_attention_deep_dive_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/kv_paged_attention_deep_dive_view.hpp/.cpp`
- Thin CLI: `native_core/src/kv_paged_attention_deep_dive_main.cpp`
- CMake target: `inferlab_kv_paged_attention_deep_dive`
- Target artifact: `results/raw/native_kv_paged_attention_deep_dive.json`
- Matrix refresh command: `./build/inferlab_kv_paged_attention_deep_dive --repo-root . --output results/raw/native_kv_paged_attention_deep_dive.json`

The typed model reuses the existing native owners instead of maintaining a
second Paged KV implementation:

- `inferlab::PagedKVCacheManager` owns prefix/private block allocation and
  `locate_token()` owns logical-token-to-physical-block resolution.
- `inferlab::analyze_block_table_locality()` owns touched-block capacity and
  logical-read amplification.
- `runtime_communication_internal::transfer_milliseconds()` owns the shared
  analytical KV transfer serialization conversion.

The artifact records canonical logical-to-physical samples, resident and
transfer-payload byte scopes, block-table entries, dense and paged logical
reads, block-capacity amplification, prefix sharing, internal and modeled
free-tail fragmentation, and transfer-versus-prefill decisions. Prefix reuse
is not presented as eliminating the logical KV reads required by decode.

## Optional Native Trace Context

The existing CLI accepts `--trace-context-file <jsonl>`. Each bounded JSONL
row binds one compiled scenario name through `record_key` and requires a W3C
`trace_id`, `(trace_id, span_id)` context, `clock_domain`, and `timestamp_ns`;
`parent_span_id` is optional. The artifact emits only domain-separated,
non-cryptographic trace/span join tokens, presence/count facts, and a redacted
source label. Raw identity and timestamp values are never rendered.

The same CLI optionally accepts `--trace-clock-calibration-file <jsonl>`.
When one strict calibration row matches the trace, native domain, and clock
domain, the shared projection emits only a trace-relative nanosecond point,
uncertainty, usage counts, and a non-cryptographic full-contract fingerprint.
It never emits the raw trace id, absolute timestamp, affine anchor, or row.

Omitting the sidecar produces the explicit `not_provided` state. Calibration
is explicit input, not inferred clock synchronization. Neither sidecar turns
this dry-run artifact into replay/execute evidence, performs a same-request
join, or proves span parentage, ordering, or causality.

## Source Corrections

The previous mixed entrypoint silently clamped invalid token/page/resource
inputs, manufactured positive work through denominator floors, derived block
ids with `logical_token / page_size`, mixed block metadata with token reads,
and left `live_tokens`/`kv_live_bytes` scope ambiguous. The current model fails
closed on invalid identity, evidence path, count, alignment, resource, finite,
weight, and duplicate contracts. Score weights, thresholds, fixture limits,
free-tail semantics, and transfer-payload scope are explicit.

## Claim Boundary

This is a compiled native mechanism slice and artifact schema. It is not a
production PagedAttention runtime, imported replay, executed CUDA kernel,
measured GPU/product result, or release-readiness proof on the current server.

## Deferred Validation

Not run per current user request. Future validation should run CMake build,
canonical allocation/locality edge matrices, exact JSON parity, native
execution, product replay/execute, summary export, Prometheus export, final
report rendering, and claim/freshness gates on the target machine.
