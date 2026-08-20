# KV-Aware Router

This report path is kept for the P0 KV-aware routing item in `深层次开发.md`.
The native artifact is a deterministic local C++ simulation for routing diagnostics;
it is not a live vLLM, SGLang, Dynamo, or KServe/llm-d router execution.

| Item | Value |
|---|---|
| Routing mechanism owner | `native_core/src/advanced_serving_routing.cpp` |
| Artifact composition owner | `native_core/src/advanced_kv_router_artifact_model.hpp/.cpp` |
| Projection-only view | `native_core/src/advanced_kv_router_artifact_view.hpp/.cpp` |
| CLI entrypoint | `native_core/src/advanced_kv_router_main.cpp` |
| C++ target | `inferlab_advanced_kv_router` |
| Reference config | `configs/kv_router_workload.example.json` (not consumed by the current CLI) |
| Native artifact | `results/raw/native_advanced_kv_router.json` |
| Aggregated report | `reports/16_advanced_serving_research.md` |
| Execution kind | `synthetic_simulation` |
| Catalog identity | `high_value_artifact_catalog.hpp::advanced_kv_router` |
| Trace role | `kv_aware_routing -> routing.kv_aware` |
| Claim scope | Compiled request-level routing model for prefix-hit, recompute, and worker-load explanation; optional supplied trace context is not live telemetry |
| Requires real backend for performance claims | Yes |

The artifact covers round-robin, least-loaded, prefix-hit-aware and hybrid
policies; prefix-hit rate, reused/recomputed tokens, per-decision worker choice,
worker-load history, peak load, and final imbalance. It does not produce TTFT,
queue-pressure, or KV-pressure measurements.

Request and prefix identities are projected as stable request ordinals and
artifact-local anonymous prefix classes. Raw request ids and prefix strings are
not serialized. The optional sidecar uses one privacy-safe record key per
`policy + request_ordinal` and the shared trace/clock/lifecycle contract:

```bash
./build/inferlab_advanced_kv_router --repo-root . --workers 3 --trace-context-file <bounded-jsonl> --trace-clock-calibration-file <bounded-jsonl> --output results/raw/native_advanced_kv_router.json
```

Without a sidecar, `native_trace_context` remains explicitly `not_provided`.
The catalog-derived General Artifact Contract, freshness, summary, Prometheus,
final report, and Evidence Query native trace projection discover the same
artifact identity. Neither the compiled simulation nor a supplied sidecar
proves product routing behavior, observed latency, causality, performance,
execute evidence, or release readiness.
