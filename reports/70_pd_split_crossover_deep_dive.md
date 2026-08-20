# PD Split Crossover Deep Dive Implementation Report

## Scope

This report describes a compiled native C++ analytical bridge for deciding when prefill/decode disaggregation should be compared with a monolithic serving pool. It does not claim a Dynamo, Mooncake, llm-d, TensorRT-LLM, vLLM, or SGLang distributed-serving benchmark run.

## Product Capabilities

Prefill/decode disaggregation, KV transfer overlap, routing, and worker-pool control are mature product capabilities. InferLab borrows product identity and evidence wiring, but does not present those runtimes as InferLab-owned implementations.

## Canonical Owners

- Public contract: `native_core/include/inferlab/advanced_serving_runtime_communication.hpp`
- Canonical transfer timeline: `native_core/src/advanced_serving_pd_transfer_overlap.cpp`
- Crossover validation, queue/concurrency/SLO analysis, score, decision, summary, and ranking: `native_core/src/advanced_serving_pd_split_crossover.cpp`
- Five compiled scenarios and mature-product profile foreign keys: `native_core/src/advanced_serving_pd_split_crossover_fixture.cpp`
- Scenario-record to optional native trace/clock/lifecycle composition: `native_core/src/pd_split_crossover_deep_dive_model.hpp/.cpp`
- Shared trace domain, sidecar parser, binding, lifecycle and privacy contract: `native_core/src/native_trace_context.hpp/.cpp`
- Projection-only artifact view: `native_core/src/pd_split_crossover_deep_dive_view.hpp/.cpp`
- CLI/request/output shell: `native_core/src/pd_split_crossover_deep_dive_main.cpp`

The crossover model composes `estimate_pd_transfer_overlap()` rather than maintaining a second KV transfer formula or timeline. Product display names are derived from the mature-product profile registry.

## Existing Surface

- CMake target: `inferlab_pd_split_crossover_deep_dive`
- Target artifact: `results/raw/native_pd_split_crossover_deep_dive.json`
- Matrix refresh command: `./build/inferlab_pd_split_crossover_deep_dive --repo-root . --output results/raw/native_pd_split_crossover_deep_dive.json`
- Optional trace refresh: append `--trace-context-file <bounded-jsonl>` and, when cross-clock relative timestamps are needed, `--trace-clock-calibration-file <bounded-jsonl>`.
- Configuration input: none; current scenarios are compiled fixtures and the CLI does not claim to consume a config file.

No dependency, target, artifact, schema, compatibility entry, fallback, product adapter, live scheduler, or second runtime/communication owner was added.

## Model And Artifact Contract

The typed result separates canonical prefill/decode compute, prompt KV payload, transfer service, hidden transfer, exposed transfer tail, full-prompt-KV readiness, queue relief, TPOT relief, TTFT risk, uncapped and bounded scores, SLO state, bottleneck, decision, recommendation, and stable ranking.

Legacy `decode_blocked_ms` now means the canonical exposed transfer tail after prefill completion. `transfer_setup_ms` is a compatibility projection of canonical per-chunk overhead; its value is derived by the model, not recalculated by the renderer. The artifact also exposes canonical transfer stages and explicitly labels product/evidence inputs as declared follow-up sources that are not read by this compiled fixture.

Invalid identities, product foreign keys, counts, topology, finite/range values, weight sums, active work without concurrency, no-work records with queue pressure, arithmetic overflow, or plan reconciliation drift fail closed. Positive floors and silent input clamps are not retained.

The optional native trace sidecar binds exactly one canonical scenario name to one `serving.pd_split_crossover` span record. Missing sidecar data remains `not_provided`; partial lifecycle bundles, unknown record keys, duplicate trace/span identities, invalid calibration, and record-order drift fail closed in the shared trace owner. The artifact emits only anonymous tokens and relative calibrated intervals, never raw trace/span ids, absolute timestamps, or calibration anchors. Evidence Query discovers this source from Catalog required fields and metrics rather than from a second artifact list.

## Claim Boundary

All TTFT, TPOT, queue, transfer, overlap, pressure, score, SLO, decision, recommendation, and ranking fields are compiled analytical fixture facts. Supplied trace rows establish only the explicit sidecar contract and may participate in downstream candidate lineage/temporal checks; they do not prove inferred causality, live KV movement, scheduler behavior, product replay/execute, distributed-serving performance, benchmark timing, or release readiness.

## Deferred Validation

Not run per current user request. CMake, compiler/header/linker/static initialization, no-sidecar/valid/partial/duplicate/unknown-key/calibration/lifecycle matrices, exact JSON/numerical parity, native execution, artifact refresh, Evidence Query refresh, Summary, Prometheus, Final Report, product replay/execute, benchmark, and GPU/network validation remain deferred.
