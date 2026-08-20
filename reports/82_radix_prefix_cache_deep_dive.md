# Radix Prefix Cache Deep Dive Implementation Report

## Scope

This report describes a native C++ radix/prefix-cache explainer for prefix sharing, physical block reuse, hit rate, eviction cost, and fragmentation risk. It does not reimplement SGLang RadixAttention or vLLM prefix caching.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_borrowed_gap_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_borrowed_gap_artifacts`
- Target artifact: `results/raw/native_radix_prefix_cache_deep_dive.json`
- Refresh command: `./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_radix_prefix_cache_deep_dive --output results/raw/native_radix_prefix_cache_deep_dive.json`

The artifact records prefix tokens, shared physical blocks, cache hit rate, eviction cost, fragmentation score, and cache policy.

## Claim Boundary

This is a native explanation artifact. Mature product cache engines remain external, and product cache-hit claims require product replay or execute artifacts.

## Deferred Validation

Not run per current user request.
