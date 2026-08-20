# Edge / Cloud Routing

## Scope

This chapter covers InferLab's C++17 request-before-execution edge/cloud routing slice. It composes the existing `IEdgeRuntime` lifecycle with an optional OpenAI-compatible cloud transport; it does not replace llama.cpp, ExecuTorch, vLLM, SGLang, TensorRT-LLM, or any mature runtime core.

Canonical implementation surfaces:

- `native_core/include/inferlab/edge_cloud.hpp`
- `native_core/src/edge_cloud.cpp`
- `native_core/src/edge_cloud_budget_policy.cpp`
- `native_core/src/edge_cloud_session_recovery_policy.cpp`
- `native_core/src/edge_cloud_transport.cpp`
- `native_core/src/edge_cloud_routing_*.cpp`
- target: `inferlab_edge_cloud_routing`
- config: `configs/edge_cloud_routing.example.json`
- artifact: `results/raw/native_edge_cloud_routing.json`

## Implemented Behavior

- Selects `local`, `cloud`, `local_then_cloud`, or `reject` before generation.
- Reuses the canonical edge target-platform registry and blocks execute when the configured Linux architecture does not match the compiled binary.
- Forbids remote processing for `device_only` data.
- Requires policy permission, declared network availability, linked transport, configured credential environment, and sufficient deadline before cloud placement.
- Optionally requires a fresh caller-supplied RTT, jitter, packet-loss, and downlink observation to satisfy bounded policy thresholds; this is not an active probe or learned predictor.
- Ages the same observation across a local attempt and reassesses it before fallback, so a stale network snapshot cannot authorize later cloud I/O.
- Optionally gates local placement with a caller-supplied battery observation and local-energy estimate, and gates cloud placement with a caller-supplied per-request cloud-cost estimate.
- Treats the budget inputs as one bounded, freshness-checked estimate snapshot and ages that snapshot before fallback; missing, stale, or over-budget evidence blocks only the affected placement side.
- Reuses the existing edge powercap lifecycle and cloud cost-accounting domains as upstream evidence owners; the router does not read battery/power sensors, recompute serving cost, or ingest a billing ledger.
- Admits caller-requested session recovery only when a durable transcript has already been reconstructed into the current prompt and its session/request identities and freshness pass the bounded contract.
- Allows a recovered request after a completed prior turn, or restarts an incomplete prior request only when it delivered zero output chunks.
- Rejects partial-output continuation; it does not splice generated text, restore sampler state, or transfer KV/hidden state between edge and cloud.
- Allows local-to-cloud fallback only after a retryable local failure with zero delivered output and no cancellation.
- Reuses the canonical local `IEdgeRuntime` request, lifecycle, status, cancellation, and result validation contract.
- Uses optional libcurl 8.5+ for OpenAI-compatible `/v1/chat/completions` SSE execution when explicitly enabled.
- Requires TLS peer and host verification, disables redirects, and limits plaintext HTTP to explicit loopback development configuration.
- Keeps prompt text, streamed text, API key value, and raw endpoint out of the artifact.
- Suppresses the non-cryptographic prompt fingerprint for `sensitive` and `device_only` requests.
- Separates SSE text chunk counts from tokenizer token counts. Cloud token counts are recorded only when supplied by API usage metadata.

## Artifact Interpretation

`plan` mode may report dependency and policy readiness, but does not load a model or call a cloud endpoint. `execute` mode may produce local or cloud attempt observations. A successful single request is not production availability, capacity, cost accounting, model-quality equivalence, or release evidence.

The artifact must preserve these boundaries:

- `executed=false` for plan-only or rejected requests with no generation attempt.
- `replayed=false`; this target is not a product-log replay adapter.
- `network_available` is a configuration declaration, not an active network probe.
- Network-quality provenance is a bounded stable label; endpoint URLs and probe targets are not emitted.
- Budget provenance is a bounded stable label; battery percentage, local-energy estimate, and cloud request-cost estimate remain caller declarations rather than device-exclusive measurements or billing records.
- Session ID, previous request ID, and transcript revision are checked in memory but omitted from the artifact; only bounded recovery state, strategy, provenance, and blockers are projected.
- Recovery prompts also suppress the non-cryptographic prompt fingerprint, avoiding a stable diagnostic fingerprint over reconstructed conversation history.
- Local runtime token observations and cloud-reported usage tokens retain distinct provenance.
- No fallback is legal after any output was delivered.
- Low-cardinality decision, attempt, outcome, fallback, privacy, network, network-quality, budget, session-recovery, resource, and dependency counts are derived once by the C++ model for downstream catalog consumers.

## Refresh Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_EDGE_CLOUD_CURL=ON
cmake --build build --target inferlab_edge_cloud_routing
./build/inferlab_edge_cloud_routing \
  --repo-root . \
  --config configs/edge_cloud_routing.example.json \
  --mode plan \
  --output results/raw/native_edge_cloud_routing.json \
  --format json
```

Use `--mode execute` only on an authorized target machine after the local runtime/model or cloud endpoint, network policy, and API-key environment are prepared.

## Current Validation Status

Validation was restored on the current x86_64 server. A fresh Release configure/build of `inferlab_edge_cloud_routing` succeeded with the repository JSON integration and libcurl 8.5.0 transport enabled. The formal plan producer refreshed `results/raw/native_edge_cloud_routing.json`; it correctly remains `blocked` because the example target is Linux AArch64, llama.cpp is not linked, and a current network-quality observation is absent.

Eight bounded session-recovery scenarios were executed with temporary inputs: disabled, completed turn, incomplete zero-output, missing transcript, stale state, identity mismatch, reused request identity, and partial-output rejection. Their decisions matched the documented admission boundary. A one-shot loopback OpenAI-compatible SSE endpoint also exercised the real libcurl execute path, stream accounting, redaction, and completion reconciliation. That temporary loopback result is transport/orchestrator validation only; it is not a mature-product endpoint, production-network evidence, benchmark timing, or release evidence and was not copied into `results/raw`.

Catalog, Schema Registry, Artifact Contract, Claim Scope, Freshness, Evidence Warehouse, Root Cause, Evidence Query, Release Pipeline, Review Pack, Summary, Prometheus, Final Report, and JSON output validation were refreshed successfully. Native Audit manifest drift was also repaired: all 1,199 historical overrides now apply, while 43 newly added files in the current 1,242-file scope remain explicitly pending. The release pipeline remains `blocked` by those pending reviews and independent target-environment gaps, including product replay/execute evidence, AArch64 runtime execution, and mature-product readiness. CTest, Python unittest, GPU/CUDA, AArch64/Android, real-model local generation, and authorized vLLM/SGLang/TensorRT-LLM/TGI endpoint execution were not run in this validation slice.

## Explicit Non-Goals

- Mid-generation migration or continuation after partial output.
- KV cache transfer between edge and cloud.
- Multi-cloud failover, billing settlement, or a second cost-accounting implementation.
- Active battery/power sensor collection or device-exclusive energy attribution inside the router.
- Partial-output resume, transcript persistence, sampler-state restoration, or hidden-state recovery.
- Active network probing, learned quality prediction, or claims that configured observations represent production network behavior.
- Reimplementation of mature serving engines or mobile inference runtimes.
- Claims that plan readiness or dependency linkage is real performance evidence.
