# Radix Prefix Cache

This chapter documents the retained C++ prefix-cache mechanisms and their
evidence boundary.

## Scope

- `PagedKVCacheManager` owns prefix registration, block sharing and release.
- Prefix traffic profiling compares shared and unshared block pressure.
- KV Lifecycle Deep Dive composes prefix reuse with allocation, transport and
  lifecycle evidence.
- `inferlab_radix_prefix_cache_plan` estimates radix metadata, KV sharing,
  fragmentation, eviction cost and adapter-scoped isolation.

## Command

`./build/inferlab_radix_prefix_cache_plan --repo-root . --output results/raw/native_radix_prefix_cache_plan.json`

## Why This Matters

Radix prefix caching is deeper than a cache-hit counter. The retained C++
surfaces keep the important serving questions explicit:

- how many KV blocks become reusable;
- whether lookup and metadata overhead eat the TTFT savings;
- how sharing affects fragmentation and eviction cost;
- which requests should be routed to a worker that already owns the matching KV
  cache blocks.

## Native Release Isolation Binding

The native `inferlab_radix_prefix_cache_plan` artifact contains one marked
release fixture and publishes `release_lora_prefix_isolation_profile`. The
fixture declares the canonical adapter-scoped prefix key, a residency limit of
12 adapters, and disabled cross-adapter prefix sharing. Release Pipeline
compares that typed declaration with the retained LoRA Adapter Cache profile
through the shared alignment contract. This proves compiled fixture
consistency only, not live cache namespace enforcement or tenant isolation.

## Next Enhancements

- Compare native estimates with live prefix-cache metrics from vLLM or SGLang
  when product validation is restored.
- Feed measured cache hits and eviction events into KV Lifecycle evidence.
