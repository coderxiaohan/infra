# Product Replay Semantic Events Implementation Report

## Scope

This report describes a native C++ semantic extraction layer for repository fixtures and explicitly imported product replay JSONL. It normalizes request, prefill, decode, batching, KV, routing, admission, fallback, and error event types. It does not execute products or endpoints.

## Product Capabilities

vLLM, SGLang, LMCache, Mooncake, Dynamo, KServe/llm-d, Ray Serve, TensorRT-LLM, FlashInfer, and TGI own their logs, metrics, runtime behavior, and production event semantics.

## InferLab Integration

InferLab uses semantic event records to connect product replay evidence to native KV tier, PD crossover, scheduler root-cause, kernel compatibility, and the canonical Release Evidence Pipeline replay dimension.

## InferLab Native Slice

Implemented native ownership split:

- Event-field, event-rule, default-fixture, output-identity, and downstream-pipeline identity owner: `native_core/src/product_replay_semantic_events_fixture.hpp/.cpp`
- Bounded JSONL read and strict row-contract owner: `native_core/src/product_replay_semantic_events_sample_parser.hpp/.cpp`
- Shared W3C trace/span-context validation, non-cryptographic set-fingerprint, and anonymous trace/request-span join-token owner: `native_core/src/trace_correlation_contract.hpp/.cpp`
- Shared terminal-status, exact start/terminal cardinality, interval-containment, and temporal-relation vocabulary owner: `native_core/src/trace_span_lifecycle_contract.hpp/.cpp`
- Shared bounded clock-calibration JSONL, affine normalization, usage reconciliation, privacy projection, and non-cryptographic calibration-fingerprint owner: `native_core/src/trace_clock_calibration.hpp/.cpp` and `native_core/src/trace_clock_calibration_view.hpp/.cpp`
- Source/product/request-scoped anonymous lifecycle join, duplicate/order policy, checked reconciliation owner: `native_core/src/product_replay_request_trace_model.cpp`
- Projection-only anonymous request-trace view: `native_core/src/product_replay_request_trace_view.hpp/.cpp`
- Product-profile projection, event aggregation, evidence classification, decision, and reconciliation owner: `native_core/src/product_replay_semantic_events_model.hpp/.cpp`
- Projection-only artifact owner: `native_core/src/product_replay_semantic_events_view.hpp/.cpp`
- Thin CLI/request/output shell: `native_core/src/product_replay_semantic_events_main.cpp`
- CMake target: `inferlab_product_replay_semantic_events`
- Default sample inputs:
  - `configs/product_profiles/vllm_replay_sample.jsonl`
  - `configs/product_profiles/sglang_replay_sample.jsonl`
  - `configs/product_profiles/tensorrtllm_replay_sample.jsonl`
  - `configs/product_profiles/lmcache_replay_sample.jsonl`
  - `configs/product_profiles/mooncake_replay_sample.jsonl`
  - `configs/product_profiles/flashinfer_replay_sample.jsonl`
  - `configs/product_profiles/kserve_llmd_replay_sample.jsonl`
  - `configs/product_profiles/ray_serve_replay_sample.jsonl`
  - `configs/product_profiles/tgi_replay_sample.jsonl`
- Target artifact: `results/raw/product_replay_semantic_events.json`
- Matrix refresh command: `./build/inferlab_product_replay_semantic_events --repo-root . --results results/raw --output results/raw/product_replay_semantic_events.json --format json`

The CLI follows the shared evidence/release contract: it accepts JSON-only `--format json`, rejects unsupported formats or positional arguments, emits redacted `cli_context` metadata for repo/results/output/sample paths, and accepts an optional `--trace-clock-calibration-file` without changing fixture/replay/execute classification.

When `--sample` is omitted, the nine repository JSONL files are always classified as `default_fixture`, even when individual rows happen to contain product-like or claim-like fields. They can establish fixture schema coverage only and never set top-level `replayed=true`.

When one or more explicit `--sample` arguments are supplied, every replay-derived known event row must be a strict JSON object with a canonical non-blank product profile id, unsigned `timestamp_ms`, non-blank `claim_scope`, and the typed non-blank fields required by its event rule. Malformed JSON, decoded duplicate keys, wrong field types, blank or missing canonical identity, unreadable sources, and resource-limit failures fail closed. Unsupported event types are reported but do not become evidence for a known event type.

The importer accepts at most 64 sample files, 4 MiB per file, 100,000 non-empty rows per file, and 64 KiB per row. The shared artifact reader enforces the byte limit during the physical read so a failed size probe or file growth cannot turn the importer into an unbounded read. `request_ids` membership arrays must contain unique non-blank strings. Optional direct `trace_id` values must be nonzero 32-character lowercase W3C trace ids; optional batch `trace_ids` must be a nonempty array aligned one-to-one with `request_ids`. Optional direct `request_span_id` must be a nonzero 16-character lowercase W3C span id paired with direct `request_id` and `trace_id`; optional batch `request_span_ids` must be unique and aligned one-to-one with both `request_ids` and `trace_ids`. Direct and array forms are mutually exclusive. Raw event, request, trace, and span values are not emitted; the artifact contains row location, type/contract state, redacted source metadata, anonymous request ordinals, pseudonymous join tokens, and aggregate evidence only.

`request_terminal` is the only event that activates the explicit request-span lifecycle contract. It requires direct request identity, timestamp, claim scope, and one canonical `status` value from `completed`, `error`, `cancelled`, or `rejected`. One request lifecycle is ready only with exactly one `request_arrival`, exactly one `request_terminal`, complete ordered timestamps, every observed request event inside that interval, and one unique complete request-span binding. Missing terminal data preserves the old additive `not_provided` state; duplicate, reversed, out-of-interval, invalid-status, or binding-conflict input fails closed.

The artifact records event type, canonical product profile, source artifact, canonical native destination, required-field contract, fixture/replay counts, replay readiness, profile-contract state, CLI context, unweighted contract coverage, status, claim boundary, blocker, and recommendation. Aggregated event types become `semantic_event_available` only when every valid explicit replay row represented by that event type has a ready canonical product-profile contract; mixed ready/pending profiles remain pending.

New multi-product sections:

- `sample_sources`
- `normalized_events`
- `request_trace_join`
- `product_family_coverage`
- `event_schema_coverage`
- `native_alignment_coverage`
- `release_pipeline_linkage`

## Request Trace Join

The request join is scoped by physical sample source, canonical product profile, and `request_id`; identical request text in different files or products is never merged. Direct `request_id` events and `request_ids` batch-membership events feed the same request-local group. The model reports multi-event versus singleton requests, timestamp completeness, same-record identity/timestamp coverage, source-order timestamp pairs, duplicate event-type/timestamp pairs, fixture/replay provenance, profile-contract readiness, and one checked decision.

Only explicit replay rows with a ready product profile, complete timestamps, at least two joined events, monotonic source-order timestamps, and no duplicate event-type/timestamp pair can produce `request_trace_replay_ready`. Fixture groups remain `fixture_request_trace_schema_ready`; singleton or profile-pending replay groups remain partial; explicit duplicate or out-of-order groups block the semantic artifact.

Trace context is additive: missing trace context does not manufacture a replay blocker, while conflicting trace ids inside one source/product/request group do. Complete and consistent groups contribute one invocation-local trace id to a sorted distinct set. Each eligible anonymous request record now also emits one canonical `trace_join_token`, `trace_join_token_present`, and the shared `fnv1a64x2_length_framed_join_token_v1_noncryptographic` algorithm identity. Summary counts reconcile eligible requests, token count, distinct raw trace-id count, distinct token count, and a zero local collision count. Different invocation-local trace ids collapsing to one token fail closed before output.

The artifact continues to emit no raw request or trace id. The original strings are discarded with the request-group construction state. Per-request tokens are stable pseudonymous equality hints intended for a bounded downstream candidate join; they are not cryptographic digests, privacy guarantees, globally unique request keys, span identities, or native same-request proof. The existing length-framed set fingerprint remains available for set-level snapshot propagation.

Request-span binding is also optional and fail-closed. A request becomes lineage-eligible only when every membership has complete and consistent trace and request-span context and one request span is not reused by another request in the same invocation. Eligible records emit one non-cryptographic `request_span_join_token`; conflicting or reused bindings block request-trace evidence and suppress that token. The summary reconciles complete/consistent/conflicting request-span counts, eligible/token/distinct-token counts, binding scope, and algorithm. This producer does not traverse native spans. Evidence Query's `native_trace_correlation` projection is the sole cross-artifact owner that may combine this explicit root with native `span_join_token` and `parent_span_join_token` fields.

When `request_terminal` is present, the same request-group owner evaluates lifecycle cardinality, start/end ordering, event containment, terminal status, and request-span eligibility before replay readiness. The artifact exposes only aggregate terminal-status partitions and the request's canonical lifecycle status. It does not copy raw timestamps into the lifecycle section or infer a native predecessor edge. Evidence Query's native trace projection remains the sole cross-artifact owner for declared predecessor temporal consistency.

Clock calibration is optional and fail-closed when supplied. Each non-empty calibration JSONL row must contain one valid W3C `trace_id`, canonical `source_scope`, canonical `clock_domain`, matching `timestamp_unit`, unsigned `anchor_timestamp`, `trace_relative_anchor_ns`, and `uncertainty_ns`. Product rows use `source_scope=product_replay:<profile>:source:<index>` and `clock_domain=product_log_ms`. Eligible requests consume exactly two lookups, one for the minimum and one for the maximum event timestamp. The output contains only bounded trace-relative nanoseconds, uncertainty, redacted source metadata, usage counts, and a non-cryptographic full-contract fingerprint; raw trace ids, absolute timestamps, and anchors remain invocation-local.

A request window is present only when both bounds resolve through the same calibration entry. Missing calibration remains an explicit optional state and does not downgrade an otherwise valid replay join. Cross-artifact comparison is deliberately not performed here: the Evidence Query native trace projection requires Product and native sources to report the same complete calibration fingerprint before classifying a native timestamp as window-compatible or window-excluded.

## Claim Boundary

This is a semantic replay bridge. Fixture rows are schema-only. Strict explicit rows can establish supplied, source-scoped Product request-lifecycle consistency, an anonymous trace-context set summary, per-request pseudonymous trace/request-span join inputs, and optionally calibrated relative request windows. This producer does not itself compare another artifact. Replay is not execute; a request-span token only declares an explicit Product request root for a downstream evaluator and is not native same-request proof by itself. Token equality, Product request-lifecycle consistency, or time-window overlap does not prove shared raw identity, native span lifecycle, undeclared cross-span ordering, inferred causality, performance, or release approval. Native alignment destinations are metadata links and are not read by this producer. The Release Evidence Pipeline and Evidence Query are downstream consumers and are never read or closed by this artifact.

## Deferred Validation

Not run per current user request. CMake, compilation, parser execution, malformed/resource/mode/request-terminal/lifecycle matrices, exact JSON parity, native execution, artifact refresh, Summary, Prometheus, Final Report, product replay/execute, benchmark, and GPU/network validation remain unverified. Future validation should first build the native target, then refresh fixture-mode output, import strict real replay samples with and without explicit request-span bindings and `request_terminal`, exercise duplicate/reversed/out-of-interval/invalid-terminal cases, refresh this artifact, and finally refresh Product Replay Alignment, Evidence Query, and the Release Evidence Pipeline.
