# Product Replay Evidence Alignment Report

## Scope

This report describes a native C++ evidence aligner that maps mature product replay/profile artifacts to InferLab native deep-dive artifacts. It does not execute products, run benchmarks, or claim ownership of product internals.

## Product Capabilities

vLLM, SGLang, LMCache, Mooncake, TensorRT-LLM, FlashInfer, KTransformers, Dynamo, KServe/llm-d, Ray Serve, LMDeploy, and TGI own their production schedulers, KV managers, kernel libraries, runtime backends, and serving control planes.

## InferLab Integration

InferLab imports product evidence through product profile dry-run, replay, or execute artifacts. The replay alignment layer then connects those artifacts to native mechanism explanations:

- vLLM/SGLang to KV/PagedAttention, KV lifecycle, and Scheduler/Batching deep dives
- LMCache/Mooncake to KV lifecycle, KV transfer, and transfer-vs-prefill decisions
- TensorRT-LLM/FlashInfer/KTransformers to CUDA/Quant microkernel metadata
- Dynamo/KServe/Ray Serve to scheduler, queue depth, routing, autoscaling, and SLO explanations
- LMDeploy/TGI to runtime backend, scheduler, and microkernel explanation surfaces

## InferLab Native Slice

Implemented native entry:

- Canonical rule fixture: `native_core/src/product_replay_alignment_fixture.hpp/.cpp`
- Single-read typed model: `native_core/src/product_replay_alignment_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/product_replay_alignment_view.hpp/.cpp`
- Thin CLI source: `native_core/src/product_replay_alignment_main.cpp`
- CMake target: `inferlab_product_replay_alignment`
- Target artifact: `results/raw/native_product_replay_alignment.json`
- Matrix refresh command: `./build/inferlab_product_replay_alignment --repo-root . --results results/raw --output results/raw/native_product_replay_alignment.json --format json`

The CLI follows the shared evidence/release contract: it accepts JSON-only `--format json`, rejects unsupported formats or positional arguments, and emits redacted `cli_context` metadata for repo/results/output paths.

The artifact records canonical product-profile and native-artifact identities, strict structural/semantic contract state, replay/execute/dry-run separation, expected replay signals, native explainer fields, single-read accounting, CLI context, alignment score, claim boundary, and recommended next actions. The score is an evidence-completeness score, not a performance score. `aligned_explanation_ready` requires every rule to have contract-valid native inputs and replay/execute evidence for every product named by that rule; subset coverage remains `partial_alignment_ready`.

Native identity composition has three bounded sources rather than a copied alignment-local directory:

- KV/PagedAttention, Scheduler/Batching, and CUDA/Quant identities come from the deep-dive core registry.
- Twelve bounded deep-dive/gate identities resolve through the canonical `artifact_wiring_specs()` registry and strict `deep_dive_artifact_identity` foreign-key join; they are not fallback identities.
- P2 and borrowed-gap identities compose `mature_product_artifact_dispatch.hpp` with their typed contract extensions; the dispatch owns artifact/target/report/refresh identity while the contract owns required fields, metrics, and claim limits.

Each mature-product artifact is evaluated by the shared `product_profile.v1` semantic contract. Each native input must be a regular, parseable, exact-type artifact satisfying the shared Artifact Evidence Contract plus its canonical top-level required fields and semantic required metrics. Missing inputs remain pending; path-probe failures and unreadable, non-regular, malformed, type-mismatched, or semantically invalid existing inputs fail closed.

The local dependency order is now explicit and one-way:

`product profiles/native explainers -> product replay semantic events -> product replay alignment -> release evidence pipeline replay dimension`

Product Replay Alignment is an upstream replay-evidence producer. The canonical Release Evidence Pipeline consumes release inputs after alignment refresh; Alignment does not read downstream release decisions.

## Claim Boundary

This is an evidence alignment and explanation layer. It is not product execution, not a benchmark, not a product-core reimplementation, not a causal request/trace join, and not proof of runtime performance or release approval by itself.

## Deferred Validation

Not run per current user request. Future validation should refresh native deep-dive artifacts, collect or replay product profile evidence, refresh semantic events, run this aligner, then refresh the Release Evidence Pipeline, summary, Prometheus export, final report rendering, claim-scope contract, and freshness gate on the target machine.
