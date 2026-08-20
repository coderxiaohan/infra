# Artifact Schema Registry

## Purpose

`inferlab_artifact_schema_registry` centralizes the schema expectations for high-value InferLab artifacts so contract checks, release gates, evidence warehouse, and final report wiring can reason from one registry-style artifact.

The registry is a schema and claim-boundary catalog. It does not validate refreshed artifacts on the current server and does not create runtime or benchmark conclusions.

## Artifact

- Target: `inferlab_artifact_schema_registry`
- Output: `results/raw/native_artifact_schema_registry.json`
- Refresh command: `./build/inferlab_artifact_schema_registry --repo-root . --output results/raw/native_artifact_schema_registry.json --format json`

The CLI follows the shared JSON-only native artifact contract: `--format json` is accepted, unsupported formats are rejected, and no validation, benchmark, replay, execute, summary, Prometheus, or final-report command is run by the registry target itself.

## Reported Fields

- `registry_files`: schema/rule files under `configs/schemas/`.
- `artifact_schemas`: artifact name, target, report, required fields, summary metrics, allowed modes, claim boundary, freshness policy, and release decision impact.
- `confidence_levels`: `schema_only`, `dry_run_ready`, `replay_supported`, `execute_verified`, and `stale_or_blocked`.
- `claim_boundary_rules`: dry-run/replay/execute and external-product ownership rules.
- `release_decision_rules`: blocked, report_ready, replay_ready, and execute_ready requirements.
- `summary.registered_schema_file_count`: number of schema/rule files listed by the registry.
- `summary.selector_repair_card_export_schema_registered`: explicit guard that the Evidence Query selector repair-card export schema is part of the registry.

The registry file list includes `configs/schemas/evidence_query_result.json`, `configs/schemas/evidence_query_selector_repair_card_export.json`, and `configs/schemas/native_core_audit_register.json`, so Evidence Query UX, selector repair-card exports, and the native-core audit register are governed by the same schema/report/gate chain as the other high-value evidence artifacts.

The recommended next commands include the selector repair-card export refresh command as a read-only handoff step, alongside schema registry and non-GPU acceptance refreshes. Running those commands remains deferred on the current server per user request.

## Claim Boundary

The registry can support schema governance and release-gate planning. It cannot prove that any artifact was refreshed, that any product was replayed or executed, or that any performance number is valid. Performance numbers must still come from `results/raw/*.json`.

## Validation Status

Current server status: `not run per user request`.

Target-machine validation should refresh the registry artifact before contract checks, evidence warehouse indexing, release root-cause query, release evidence pipeline, summary, Prometheus export, and final report rendering.
