# KV Lifecycle Deep Dive Implementation Report

## Scope

This report describes a native C++ aggregate evidence bridge for the KV lifecycle across Paged KV mapping, prefix sharing, fragmentation, tier decisions, KV transport, transfer-vs-prefill, and reuse/compression risk. It does not implement vLLM, SGLang, LMCache, Mooncake, Dynamo, TensorRT-LLM, or any production KV cache engine.

## InferLab Native Slice

- Typed composition model: `native_core/src/kv_lifecycle_deep_dive_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/kv_lifecycle_deep_dive_view.hpp/.cpp`
- Thin CLI: `native_core/src/kv_lifecycle_deep_dive_main.cpp`
- CMake target: `inferlab_kv_lifecycle_deep_dive`
- Target artifact: `results/raw/native_kv_lifecycle_deep_dive.json`
- Refresh command: `./build/inferlab_kv_lifecycle_deep_dive --repo-root . --output results/raw/native_kv_lifecycle_deep_dive.json`

The artifact composes existing canonical owners instead of recalculating their
mechanics: `PagedKVCacheManager` and Paged locality provide physical mapping,
the Radix Prefix model provides cache facts, the KV Compression model provides
retained payload and correctness risk, the shared KV Tier model provides the
hit-weighted candidate decision, and KV Transport Replay provides the timeline
and Gbps/GiB serialization. Cross-component resident bytes, transfer payload,
prefill work, serialization, restore latency, selected tier, and summary counts
must reconcile before the artifact can be rendered.

The previous mixed entrypoint derived physical blocks from sample indices,
treated Gbps as bytes per second, silently clamped invalid ratios, used hidden
score weights, and mixed modeled tier choice with connector availability. The
typed model now fails closed, names every score weight and threshold, and marks
all external tier choices as modeled candidates requiring replay or execute
evidence. Conservative tie handling prevents an equal-cost connector from
being described as beating prefill recompute, and every product replay command
is projected from the shared mature-product registry.

## Claim Boundary

This is a compiled native mechanism composition and artifact schema. Product
KV offload, radix attention, distributed KV stores, connector availability,
measured latency, replay, execute, and release readiness remain external.

## Deferred Validation

Not run per current user request.
