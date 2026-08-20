# KV Tier Decision Deep Dive Implementation Report

## Scope

This report describes a native C++ evidence bridge for KV tier selection across HBM, CPU DRAM, NVMe, remote KV store, and prefill recompute. It does not claim an LMCache, Mooncake, Dynamo, vLLM, or SGLang benchmark run.

## Product Capabilities

KV offload, remote KV reuse, KV transfer, and connector correctness are mature product capabilities in systems such as vLLM, LMCache, Mooncake, Dynamo, and SGLang.

## InferLab Integration

InferLab aligns those product artifacts through product profile/replay evidence, benchmark matrix entries, artifact contracts, summary wiring, Prometheus export wiring, final report cataloging, freshness gates, and deep-dive evidence bundles.

## InferLab Native Slice

Implemented native entry:

- Typed decision model: `native_core/src/kv_tier_decision_deep_dive_model.hpp/.cpp`
- Shared candidate/artifact view: `native_core/src/kv_tier_decision_deep_dive_view.hpp/.cpp`
- Thin CLI: `native_core/src/kv_tier_decision_deep_dive_main.cpp`
- CMake target: `inferlab_kv_tier_decision_deep_dive`
- Target artifact: `results/raw/native_kv_tier_decision_deep_dive.json`
- Matrix refresh command: `./build/inferlab_kv_tier_decision_deep_dive --repo-root . --output results/raw/native_kv_tier_decision_deep_dive.json`

The artifact records checked KV cardinality, explicit GiB units, HBM capacity
fit and shortfall, transfer payload ratio, per-path setup/wait/serialization/
restore latency, hit-weighted expected cost, correctness eligibility, selected
modeled tier, recompute delta, score components, decision, and recommendation.
Transfer serialization reuses the Runtime/Communication owner; product names
and evidence paths come from the mature-product and General Artifact Contract
registries. Bandwidth is explicitly decimal Gbit/s, capacity and payload are
binary GiB, and legacy `*_gb` fields are labeled as GiB compatibility aliases.
Expected-cost ties use an explicit tolerance and conservatively prefer HBM,
then recompute, before any external connector candidate. An external tier can
therefore be selected only when it strictly beats recompute under the declared
expected-cost policy.

The previous mixed entrypoint silently floored invalid inputs, used a
manufactured HBM-pressure denominator, folded correctness risk into latency
without a declared unit, and could rank an external connector candidate without
separating modeled eligibility from execution evidence. It also treated a
binary-GiB payload as if Gbps were bytes per second. Those rules now live in one
strict typed model, transfer serialization accounts for bits explicitly, and
all external candidates retain an explicit replay or execute boundary.

## Claim Boundary

This is a native mechanism slice and artifact schema. It is not a production KV offload runtime and it is not measured GPU/product evidence on the current server.

## Deferred Validation

Not run per current user request. Future validation should run CMake build, native execution, product replay/execute, summary export, Prometheus export, final report rendering, and claim/freshness gates on the target machine.
