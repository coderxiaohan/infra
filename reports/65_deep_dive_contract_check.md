# Deep Dive Contract Check Report

## Scope

This report describes a native C++ read-only contract checker for existing
InferLab deep-dive and evidence-governance artifacts. It does not run artifact
producers, native binaries, benchmarks, product replay/execute, CUDA work, or
report refresh commands.

## Canonical Inputs

The checker no longer owns a parallel artifact directory:

- Catalog-managed identity, artifact path, producer target, required fields,
  required metrics, refresh command, evidence level, and claim boundary come
  from `catalog_derived_views.hpp::contract_views()`.
- Catalog metric families come from `catalog_required_metrics.cpp` and its
  bounded domain appenders.
- Contract extensions come from
  `artifact_contract_fixture.cpp::contract_extension_registry()`. Nineteen
  mature-product rows borrow canonical identity and wiring from
  `mature_product_artifact_dispatch.hpp`; ten bounded Deep-Dive rows borrow
  canonical identity and wiring from `artifact_wiring_specs()`. No selected
  General Contract fallback remains in this checker.
- KV/PagedAttention, Scheduler/Batching, and CUDA/Quant are catalog-managed;
  their analytical metadata remains owned by the bounded core registry and is
  borrowed by Catalog rather than copied into a second identity table.
- JSON/schema/status/mode/executed/replayed/claim/summary validation composes
  `artifact_evidence_contract.hpp/.cpp` over one `EvidenceSnapshot` read per
  physical artifact.

At the current source snapshot this produces 64 current targets: 35 discovered
from the managed catalog, 19 canonical mature-product dispatch targets, and 10
canonical Deep-Dive wiring targets. The core-extension and legacy-fallback
compatibility counters are fixed at zero.
Evidence Bundle is the single explicit
post-generation consumer because it packages the current Contract Check
output; its older artifact is not read back into the current contract gate.
Review Pack is a normal current contract target and must therefore be generated
before Contract Check; Contract Check then precedes Evidence Bundle.
Future catalog additions are discovered without editing this checker.

## InferLab Native Slice

- Thin CLI: `native_core/src/deep_dive_contract_check_main.cpp`
- Typed model: `native_core/src/deep_dive_contract_check_model.hpp/.cpp`
- Projection-only view: `native_core/src/deep_dive_contract_check_view.hpp/.cpp`
- CMake target: `inferlab_deep_dive_contract_check`
- Artifact: `results/raw/native_deep_dive_contract_check.json`
- Refresh command: `./build/inferlab_deep_dive_contract_check --repo-root . --results results/raw --output results/raw/native_deep_dive_contract_check.json --format json`

The model validates target identity and duplicate surfaces, bounded field and
metric contracts, strict top-level fields, semantic metric-key presence,
required top-level collections, dry-run execute separation, checked count and
score reconciliation, missing/read/parse states, and stable typed decisions.
`--strict-missing` controls whether an absent artifact is pending or blocking;
path-probe, unreadable, malformed, and explicit execute evidence always fail
closed.

The artifact retains the legacy `contract_score`, pass/warn/fail/pending
counts, gate decision, target records, redacted path metadata, and per-record
recommendation fields. It additionally exposes source ownership, canonical
wiring/fallback state, observed metadata, base contract coverage, blockers,
repair commands, `post_generation_consumer_count`, and the typed Bundle
post-generation lifecycle record.

## Claim Boundary

A passing record proves only that the inspected JSON snapshot satisfies the
registered structural and required-key contract. It does not prove semantic
consistency between artifacts, producer correctness, freshness, measured
performance, replay, execute, mature-product behavior, or release readiness.

## Validation Boundary

The full configured CMake build, CTest 16/16, Python unittest 32 passed with 4
environment skips, this checker, and JSON parsing of the refreshed artifact
completed on the current CPU server. The current artifact has 64 targets, all
64 with canonical wiring, zero core-extension registrations, and zero legacy
fallback registrations. Some existing physical artifacts still fail their
declared owning-section metric contract; these are explicit contract blockers,
not catalog fallback debt. Malformed,
duplicate, and extreme matrices; Summary, Prometheus, Final Report, product
replay/execute, benchmark, CUDA timing, GPU, and network validation remain
pending.
