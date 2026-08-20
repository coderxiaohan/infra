# Artifact Catalog Single Source

## Purpose

`inferlab_artifact_catalog_single_source` turns `native_core/src/high_value_artifact_catalog.hpp` into an explicit registry artifact. The goal is to make artifact, target, report, schema, refresh command, freshness, Prometheus, final report, and claim-boundary metadata discoverable from one source instead of repeating lists across gates.

This is a registry and wiring surface. It does not run producers, validate refreshed artifacts, or create benchmark conclusions.

## Implemented Surface

- C++ target: `inferlab_artifact_catalog_single_source`
- Request model: `native_core/src/artifact_catalog_single_source_model.hpp/.cpp`
- Projection-only views: `native_core/src/artifact_catalog_single_source_views.hpp`, `artifact_catalog_single_source_registry_view.cpp`, `artifact_catalog_single_source_surface_view.cpp`, `artifact_catalog_single_source_fallback_view.cpp`, and `artifact_catalog_single_source_artifact_view.cpp`
- Thin CLI: `native_core/src/artifact_catalog_single_source_main.cpp`
- Catalog input: `native_core/src/high_value_artifact_catalog.hpp`
- Artifact: `results/raw/native_artifact_catalog_single_source.json`
- Benchmark matrix entry: `native_artifact_catalog_single_source_refresh`
- Report kind: `evidence`
- CLI boundary: `--format` is JSON-only (`json`), unsupported formats and unsupported positional forms are rejected by shared CLI validation, and artifact output includes `cli_context` with strict CLI flags plus redacted repo/output path status.

## Artifact Fields

- `summary`: registered artifact count, duplicate registration count, missing wiring policy count, registry decision, single-source repair/parity counts, per-surface migration readiness/action counts, surface-consumer migration-plan counts, explicit metadata policy counts, derived metadata policy counts, and confidence tier.
- `cli_context`: input-contract metadata for the catalog registry command. It records accepted JSON output mode, strict CLI validation flags, supported formats, and redacted path-probe status for repo root and output.
- `artifacts`: artifact id, output file, producer target, report, report kind, summary group, final-report group, Prometheus category, freshness policy, benchmark matrix entry, refresh command, required fields, registry-derived summary metrics, optional fields, claim boundary, metadata policy source review, execute/hardware/replay requirements, and validation order.
- `artifact_entry_required_fields`: machine-readable contract for each `artifacts[]` entry, including `metadata_policy`, report/summary/Prometheus routing, freshness policy, claim boundary, execution requirements, and validation order.
- `wiring_policies`: schema contract, freshness gate, benchmark matrix, summary, Prometheus, and final-report expectations.
- `claim_boundary_rules`: dry-run/replay/execute and external product ownership boundaries.
- `freshness_policies`: max-age and target-machine requirements by artifact class.
- `generated_surfaces`: downstream surfaces that should eventually consume the registry.
- `migration_plan`: read-only registry first, then gate consumption, then dedupe hardcoded lists.
- `registry_sources`: source files and config surfaces that still participate in artifact discovery.
- `derived_*`: registry-derived contract, freshness, summary, Prometheus, final-report, and evidence-bundle entries. Summary, Prometheus, and final-report entries are emitted through dedicated `catalog_derived_views.hpp` view types so downstream consumers have stable surface-specific fields.
- `hardcoded_duplicate_locations`: downstream lists that still exist as explicit fallback registrations.
- `consumer_statuses`: downstream consumer state, including `registry_consumed_with_fallback` for contract and freshness gates.
- `surface_migration_readiness`: per-surface readiness/action records for contract, freshness, summary, Prometheus, final report, and evidence bundle. Each row says whether the surface can consume the registry, whether legacy fallback remains, whether parity refresh is still required, and which current-repo / target-machine action owns cleanup.
- `surface_consumer_migration_plan`: per-surface ordered cleanup plan that names the fallback location, current-repo action, target-machine parity-refresh action, fallback-removal blocker state, and ordered steps required before deleting legacy fallback lists.
- `fallback_removal_queue`: operator-facing queue for retained fallback surfaces. It gives each fallback a queue id, blocker, exact wiring locations, ordered cleanup steps, current-repo action, target-machine parity-refresh action, and queue-level state so release/query tooling can explain why the fallback is still intentionally retained.
- `fallback_removal_shortest_path`: compact first-path view for removing retained fallback wiring. It records state, step counts, current-repo step count, target-machine step count, source-location gap count, primary blocker/environment, first affected surface, first fallback location, first current-repo action, first target-machine action, and bounded cleanup steps.
- `fallback_removal_parity_gate`: conservative deletion gate for retained fallback lists. It records the gate decision, whether fallback removal is allowed, blocker count, blocking-surface count, fallback surface count, parity-refresh requirements, current-repo actions, target-machine actions, source-location gaps, first blocking surface, required evidence, bounded `blocking_surfaces`, and ordered steps before any fallback list can be deleted.
- `consumer_field_coverage_summary`: aggregate field-level coverage for catalog consumer surfaces. It counts catalog-managed surfaces, fallback-retained surfaces, required fields, covered fields, field gaps, parity-refresh requirements, and current-repo / target-machine cleanup actions.
- `consumer_field_coverage`: per-surface field coverage matrix for artifact contract, freshness gate, summary, Prometheus, final report, and evidence bundle. Each row names the derived catalog view, required catalog fields, covered fields, missing fields, fallback status, and claim boundary.
- `downstream_wiring_matrix`: per-artifact, per-surface registry coverage for contract, freshness, summary, Prometheus, final report, and evidence bundle.
- `catalog_drift_review`: aggregate source-registration drift summary, including matrix entry count, missing surface count, and retained fallback count.
- `single_source_parity_review`: structured parity summary for registry-vs-fallback migration, including retained fallback surfaces, duplicate registrations, missing wiring policy count, repair-card count, current-repo action count, and parity decision.
- `single_source_repair_cards`: bounded repair cards for retained fallback surfaces, missing consumer-surface registration, field-consumption gaps, duplicate catalog registrations, or missing wiring policy metadata. These cards name the surface, fallback location, exact wiring locations, current-repo action, and validation action without running commands.
- `metadata_completeness_review`: aggregate review of explicit registry metadata coverage for `claim_boundary`, `freshness_policy`, `summary_group`, `final_report_group`, `prometheus_category`, and `report_kind`, including derived-policy artifact and field counts.
- `metadata_policy_cards`: bounded repair cards for artifacts that still rely on catalog-derived defaults instead of explicit `ManagedArtifact` metadata fields.
- `migration_readiness`: whether each downstream surface can fully consume the registry today.

## Current Consumption State

The former 1,937-line mixed entrypoint is now a 50-line CLI over one request-owned projection. One request validates and borrows the managed catalog, obtains each catalog-derived surface/source view once, verifies per-entry identity and policy parity, classifies fallback and field-coverage state once, and owns all counts, decisions, blockers, repair actions, and migration steps. The registry, surface, fallback, and final artifact views perform presentation only and do not reopen the catalog, rescan surfaces, or recompute status.

Static drift now fails closed. An empty surface registry, unconsumed surface, missing catalog field, empty or duplicate raw required field, or source-view identity mismatch cannot be reported as a healthy catalog. `catalog_drift_missing_surface_count`, action counts, readiness, blocker categories, repair cards, and next actions all derive from the same request projection instead of fixed zeroes or fallback-count aliases.

- Artifact contract: consumes registry-derived entries and keeps the legacy explicit contract list as fallback.
- Freshness gate: consumes registry-derived freshness targets and keeps runtime/product-specific targets as fallback.
- Evidence bundle: consumes registry-derived bundle components and keeps explicit deep-dive components as fallback.
- Summary: resolves the generated catalog artifact, keeps the explicit `EXPECTED_SOURCES` list as fallback, and appends catalog-only artifacts as generic evidence-governance sections.
- Prometheus export: resolves the generated catalog artifact, keeps the explicit raw-source list as fallback, and emits catalog source-resolution metrics.
- Final report: resolves catalog-managed governance sources for module coverage and evidence-governance summary filtering while keeping explicit module results as fallback.
- Native core audit register: catalog-managed evidence artifact for `native_core/include` and `native_core/src` source-review status. It is registered as `native_core_audit_register` and remains static audit evidence only until refreshed on a validation machine.
- Evidence query UX: `native_evidence_query_result.json` is registered with `query_ux`, `explain_tree`, `repair_cards`, exact wiring locations, environment-split actions, shortest paths, and native-core audit filter fields as required registry/schema evidence. These fields are explain/repair metadata only and do not create benchmark, replay, or execute claims.

All six catalog consumer surfaces are represented by the validated registry. Catalog no longer exposes a pseudo trace-correlation surface: real request/token/span correlation is owned by the Evidence Query trace projection rather than by a registry mirror over every managed artifact.

`downstream_wiring_matrix` and `catalog_drift_review` make this state reviewable at the artifact level. They explain which surfaces are expected to discover each catalog artifact through the registry, while still marking fallback retention as a pending parity-refresh condition.

`surface_migration_readiness` makes the same state reviewable at the consumer-surface level. It separates current-repo dedupe work from target-machine parity refresh, so release tooling can show exactly which downstream consumer still has fallback wiring and which action must happen before fallback removal.

`surface_consumer_migration_plan` turns that readiness state into a concrete cleanup checklist. It keeps fallback removal blocked until the surface has been refreshed, compared against the retained fallback list, and recorded as parity-safe; it does not remove fallback lists or run refresh commands.

`fallback_removal_queue` is the actionable deletion queue built from the same surface state. It is intentionally conservative: every retained fallback remains blocked until downstream parity evidence exists in `results/raw/*.json`, and each queue item splits what can be done in the current repository from what must wait for the validation or target machine.

`fallback_removal_shortest_path` gives release/report consumers a smaller first route through the same queue. It is useful for dashboards and final-report summaries that need to answer the first actionable surface, the ownership split between current repository and target machine, and the shortest blocked path without parsing every retained fallback row.

`fallback_removal_parity_gate` is the single yes/no cleanup gate for fallback deletion. It keeps deletion blocked while any downstream surface still requires a refreshed parity comparison, and it exposes the evidence that must exist before code cleanup can happen. The gate also emits `blocking_surface_count`, `blocking_surface_source_location_gap_count`, and bounded `blocking_surfaces` rows so release/query tooling can show which retained fallback surfaces block deletion without parsing the broader queue. This makes the remaining work explicit without pretending the current non-GPU server refreshed summary, Prometheus, final report, evidence bundle, or product evidence.

`consumer_field_coverage` makes catalog consumption reviewable below the surface level. It shows which fields each downstream consumer can obtain from the registry-derived view, such as required fields for contract checks, freshness policy for freshness gates, summary/final-report groups for report rendering, Prometheus category for export, and claim boundary metadata across all surfaces. In the current implementation, the relevant fields are covered by catalog-derived views, while fallback lists remain intentionally retained until refreshed artifacts prove parity.

`single_source_parity_review` and `single_source_repair_cards` make the same state actionable. They separate current-repo catalog cleanup from later validation-machine parity refresh, so release/query tooling can explain why a fallback list remains without claiming that downstream artifacts were refreshed.

`metadata_completeness_review` and `metadata_policy_cards` make registry maintainability visible. They are not blockers by themselves; they show where the current catalog still relies on derived defaults so future cleanup can promote important artifacts to explicit metadata one batch at a time.

## Claim Boundary

The artifact proves only that the registry shape and wiring policy are declared. A downstream surface is not considered refreshed until its own `results/raw/*.json` artifact exists. Performance, replay, and execute claims remain blocked until target-machine evidence is generated.

`cli_context` is also registry metadata only. It documents accepted command inputs and path status, but it does not refresh artifacts, remove fallback lists, prove parity, run summary/Prometheus/final-report generation, or create benchmark/replay/execute evidence.

Single-source repair cards are operator guidance only. They do not remove fallback lists, refresh summary/Prometheus/final-report outputs, or prove parity by themselves.

Surface migration readiness is also operator guidance only. A surface marked `can_consume_registry` still needs refreshed downstream artifacts before the legacy fallback list can be removed safely.

Surface-consumer migration plans are cleanup guidance only. A plan row can say what to do next, but it does not prove that parity refresh happened, does not delete fallback lists, and does not create release, replay, execute, or benchmark evidence.

Fallback removal queue entries are cleanup guidance only. They explain the deletion order and blockers for retained fallback surfaces, but they do not remove fallback code, refresh downstream artifacts, or create parity, release, replay, execute, or benchmark evidence.

Fallback removal shortest-path fields are cleanup guidance only. They expose the first safe route from retained fallback wiring toward catalog-only consumption, but they do not prove downstream parity, delete fallback lists, refresh artifacts, or create release, replay, execute, or benchmark evidence.

Fallback removal parity-gate blocking-surface fields are also cleanup guidance only. They identify which surfaces still block fallback deletion and split current-repo actions from target-machine parity refresh actions, but they do not run refresh commands, delete fallback wiring, or prove downstream parity.

Fallback removal parity gate fields are cleanup gate metadata only. A blocked gate means retained fallback wiring must stay in place; an allowed gate would still require human review of refreshed parity artifacts before deleting code. The gate does not execute refresh commands, mutate fallback lists, render summary/Prometheus/final-report outputs, or create runtime evidence.

Consumer field coverage is registry metadata only. It can prove that a downstream surface has a catalog-derived field path, but it cannot prove that the surface was refreshed, that fallback output matched, or that the fallback list can already be deleted.

Metadata completeness fields are maintainability evidence only. A missing explicit metadata field means the catalog used a derived default; it does not mean the artifact is invalid, executed, replayed, or benchmarked.

## Refresh Command

```bash
./build/inferlab_artifact_catalog_single_source --repo-root . --output results/raw/native_artifact_catalog_single_source.json --format json
```

Current server status: `not run per user request`.
