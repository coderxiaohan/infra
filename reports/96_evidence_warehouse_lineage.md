# Evidence Warehouse Lineage

## Purpose

This report tracks the high-value evidence warehouse line. It covers:

- `results/raw/native_evidence_warehouse_index.json`
- `results/raw/native_evidence_query_result.json`
- `results/raw/native_release_evidence_pipeline.json`

The warehouse turns raw InferLab JSON files into a queryable evidence graph. Evidence Query consumes the canonical Catalog correlation-rule registry directly and maps mature-product replay semantic events onto current native KV, scheduler, CUDA/quant, and serving artifacts. The release evidence pipeline then consumes the Query result as an explanation surface without introducing a second correlation artifact.

## Implemented Surface

- C++ target: `inferlab_evidence_warehouse_index`
- C++ target: `inferlab_evidence_query`
- C++ target: `inferlab_release_evidence_pipeline`
- Shared metadata reader: `native_core/src/artifact_metadata_reader.hpp`
- Shared catalog: `native_core/src/high_value_artifact_catalog.hpp`
- Warehouse lineage model: `native_core/src/evidence_warehouse_index_model.hpp/.cpp`
- Warehouse projection-only view: `native_core/src/evidence_warehouse_index_view.hpp/.cpp`
- Warehouse CLI: `native_core/src/evidence_warehouse_index_cli.hpp/.cpp`; the legacy main is only a role-neutral entry shell
- SQLite materializer: `native_core/src/evidence_warehouse_index_sqlite.hpp/.cpp`
- Warehouse reader: parses managed `artifacts` and `scanned_raw_artifacts` from `native_evidence_warehouse_index.json`
- Correlation rules: one stable validated Catalog registry owns product event, product source, native artifact, native signal, and claim boundary mappings
- Correlation input authority: Evidence Query captures semantic events, alignment, and every referenced native artifact through the canonical `EvidenceSnapshotRepository`; current direct snapshots remain authoritative
- Warehouse boundary: the warehouse contributes lineage inventory only. It does not replace current artifact contracts or determine semantic-correlation readiness
- Correlation output: Evidence Query emits checked configured/ready/blocked correlation counts and an actionable blocker without SQL templates or a duplicate producer
- Optional backend: SQLite through `INFERLAB_ENABLE_EVIDENCE_SQLITE` and an explicit non-empty `--sqlite-output`; the canonical refresh does not request SQLite
- Fallback: JSON-only warehouse when SQLite is not requested or unavailable; direct current-artifact snapshots remain authoritative whether or not the warehouse is available
- Single lineage owner: Warehouse generation and Evidence Query direct-scan fallback both consume `project_managed_artifact_lineage()`; neither consumer keeps a second artifact/surface substring table
- Repository wiring: producer targets require exactly one declaration with the catalog-owned source in that declaration, and benchmark entries require one exact name/binary/output/command row; Summary, Prometheus, Final Report, Contract, Freshness, Bundle and Trace membership come from validated catalog-derived views
- Artifact input safety: the current warehouse output is deferred instead of reading stale output as its own input; raw scan rejects symlink/non-regular JSON entries and records directory/iteration/read issues separately
- Query acquisition: fixture mode does not read Warehouse; a valid Warehouse does not read repository fallback sources; only the direct-fallback path loads repository text, and source bodies are discarded after typed projection
- SQLite safety: optional materialization uses an atomic transaction, explicit columns, checked bind/step results and JSON-only fallback; JSON and SQLite destinations must differ, and SQLite failure does not manufacture or upgrade lineage evidence
- Dependency scope: SQLite compile definitions and linkage are applied only to `inferlab_evidence_warehouse_index`; unrelated high-value targets do not inherit the optional database backend

## Claim Boundary

This is evidence governance, not a benchmark. Warehouse `lineage_ready` means only that catalog-owned repository wiring is structurally present; missing or malformed producer artifacts remain visible inventory state and are not converted into evidence. Evidence Query correlation readiness means only that canonical semantic/alignment inputs and referenced native artifact contracts are structurally usable. It does not prove product execute behavior, release readiness, correctness, latency, throughput, or performance gains. Those claims require target-environment replay/execute artifacts.

## Refresh Commands

```bash
./build/inferlab_evidence_warehouse_index --repo-root . --results results/raw --output results/raw/native_evidence_warehouse_index.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query confidence_summary --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_release_evidence_pipeline --repo-root . --results results/raw --output results/raw/native_release_evidence_pipeline.json --format json
```

SQLite is an explicit opt-in mirror. A workflow that actually consumes the
database may enable `INFERLAB_ENABLE_EVIDENCE_SQLITE` and add
`--sqlite-output results/raw/inferlab_evidence.sqlite` to the warehouse command.
Without that option, the artifact reports SQLite as `not_requested`; this is not
an unavailable-dependency failure and does not change the canonical JSON lineage.

Current server status: `not run per user request`.
