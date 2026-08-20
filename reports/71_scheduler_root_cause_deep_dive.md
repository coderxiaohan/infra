# Scheduler Root Cause Deep Dive Implementation Report

## Scope

This report describes a native C++ evidence bridge that maps request-level queue, prefill, decode, KV pressure, tenant quota, and SLO signals into scheduler root-cause explanations. It does not claim a vLLM, SGLang, Dynamo, KServe/llm-d, Ray Serve, LMCache, or Mooncake benchmark run.

## Product Capabilities

Continuous batching, chunked prefill, admission control, tenant quota, KV-aware routing, autoscaling, and SLO-aware scheduling are mature product capabilities in serving systems such as vLLM, SGLang, Dynamo, KServe/llm-d, Ray Serve, LMCache, and Mooncake.

## InferLab Integration

InferLab aligns product replay/profile events with native scheduler artifacts, summary wiring, Prometheus export wiring, final report cataloging, contract gates, freshness gates, and trace correlation.

## InferLab Native Slice

Implemented native entry:

- C++ source: `native_core/src/scheduler_root_cause_deep_dive_main.cpp`
- Shared typed model: `native_core/src/scheduler_deep_dive_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/scheduler_deep_dive_view.hpp/.cpp`
- Shared CLI owner: `native_core/src/scheduler_deep_dive_cli.hpp/.cpp`
- CMake target: `inferlab_scheduler_root_cause_deep_dive`
- Target artifact: `results/raw/native_scheduler_root_cause_deep_dive.json`
- Matrix refresh command: `./build/inferlab_scheduler_root_cause_deep_dive --repo-root . --output results/raw/native_scheduler_root_cause_deep_dive.json`

The artifact records request id, tenant id, queue depth, prefill chunk time, decode stall, TTFT, TPOT, P99 risk, fairness score, admission score, admission decision, root cause, and recommended action.

This artifact and Scheduler/Batching share one strict typed model rather than
maintaining parallel queue, prefill, decode-stall, SLO, admission and ranking
formulas. The model composes the existing
`estimate_chunked_prefill_scheduler` mechanism owner, then applies an explicit
root-cause policy profile. Root-cause precedence, score weights, thresholds,
canonical mechanism assumptions and zero-work behavior are serialized with
the artifact. The view only projects typed facts and both executable mains are
thin role selectors.

## Optional Native Trace Context

The shared scheduler CLI accepts `--trace-context-file <jsonl>`. A bounded row
binds one compiled request scenario through `record_key` and requires W3C
trace/span context, `clock_domain`, and `timestamp_ns`; `parent_span_id` is
optional. The artifact emits anonymous non-cryptographic join tokens and
aggregate presence facts while suppressing raw identity and timestamp values.

The same CLI optionally accepts `--trace-clock-calibration-file <jsonl>`.
An exact trace/scope/domain row can produce one bounded trace-relative point,
uncertainty, usage summary, and non-cryptographic calibration fingerprint;
raw trace ids, absolute timestamps, anchors, and calibration rows are omitted.

The default remains `not_provided`. Explicit calibration is not inferred clock
synchronization. Even a complete sidecar does not prove a Product
Replay/native same-request join, span lineage, ordering, or causality, or
promote modeled root cause to observed product behavior.

## Claim Boundary

This is a compiled analytical native scheduler root-cause model and artifact
schema. Its latency, risk, score, decision, root-cause and recommendation
values are modeled fixture facts. It is not a production scheduler and it is
not measured product scheduler evidence on the current server.

## Deferred Validation

Not run per current user request. Future validation should run native refresh, product replay/execute, summary export, Prometheus export, final report rendering, and claim/freshness gates on the target machine.
