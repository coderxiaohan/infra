# Scheduler/Batching Deep Dive Implementation Report

## Scope

This report describes a native C++ evidence bridge for scheduler and batching mechanics. It does not claim a production vLLM, SGLang, Dynamo, KServe/llm-d, or Ray Serve benchmark run.

## Product Capabilities

Continuous batching, chunked prefill, prefill/decode disaggregation, tenant-aware routing, autoscaling, and control plane behavior are mature product capabilities in systems such as vLLM, SGLang, Dynamo, KServe/llm-d, and Ray Serve.

## InferLab Integration

InferLab connects product logs and replay artifacts to local scheduler traces through product profiles, replay evidence, benchmark matrix entries, artifact contracts, summary wiring, Prometheus export wiring, and final report cataloging.

## InferLab Native Slice

Implemented native entry:

- C++ source: `native_core/src/scheduler_batching_deep_dive_main.cpp`
- Shared typed model: `native_core/src/scheduler_deep_dive_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/scheduler_deep_dive_view.hpp/.cpp`
- Shared CLI owner: `native_core/src/scheduler_deep_dive_cli.hpp/.cpp`
- CMake target: `inferlab_scheduler_batching_deep_dive`
- Target artifact: `results/raw/native_scheduler_batching_deep_dive.json`
- Matrix refresh command: `./build/inferlab_scheduler_batching_deep_dive --repo-root . --output results/raw/native_scheduler_batching_deep_dive.json`

The artifact records TTFT, TPOT, P99 latency, queue wait, decode stall, prefill chunk timeline, tenant fairness, SLO violation risk, admission score, and admission decision.

Scheduler/Batching and Scheduler Root Cause now consume one strict typed
deep-dive model. Chunk geometry, first-chunk timing, decode-stall and
mechanism-level scheduling facts are composed from the existing
`estimate_chunked_prefill_scheduler` owner; queue/SLO/admission scoring,
ranking and lifecycle policy are not recomputed in either main or view.
Scenario identity, resources, finite ranges, pressure ratios, weight totals
and zero-work state fail closed before that canonical mechanism is called.

## Optional Native Trace Context

The shared scheduler CLI accepts `--trace-context-file <jsonl>`. Each bounded
row binds one canonical `scenario_id` through `record_key` and supplies W3C
trace/span context, a supported clock domain, and `timestamp_ns`; an optional
parent span may also be supplied. The shared projection emits anonymous,
domain-separated, non-cryptographic join tokens and aggregate presence facts,
but never raw trace/span ids or timestamp values.

The same CLI optionally accepts `--trace-clock-calibration-file <jsonl>` and
uses the shared calibration owner to expose a bounded trace-relative point and
uncertainty when an exact trace/scope/domain row exists. Absolute timestamp,
anchor, raw identity, and calibration rows remain suppressed.

No sidecar is the explicit `not_provided` state. Calibration is explicit
input, not observed clock synchronization. These inputs do not prove product
replay, same-request matching, span lineage, ordering, causality, observed
latency, or execute evidence.

## Claim Boundary

This is a compiled analytical scheduler trace and strategy explanation slice.
Its TTFT/TPOT/P99, fairness, admission, score and ranking values are modeled
fixture facts, not observed product metrics. It is not a mature production
scheduler and it is not real online concurrency evidence on the current
server.

## Deferred Validation

Not run per current user request. Future validation should run CMake build, native execution, product replay/execute, summary export, Prometheus export, final report rendering, and claim/freshness gates on the target machine.
