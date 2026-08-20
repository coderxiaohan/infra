# Final Report Guide

`scripts/render_final_report.py` renders the report-facing view of existing
InferLab evidence. It does not run benchmarks, replay products, execute native
binaries, refresh upstream artifacts, or upgrade evidence confidence.

## Canonical Inputs

- `results/raw/benchmark_summary.json`
- `results/raw/native_artifact_catalog_single_source.json`
- `results/raw/native_evidence_query_result.json`
- `results/raw/native_release_evidence_pipeline.json`
- `results/raw/native_core_audit_register.json`
- `reports/*.md`, `source_notes/*.md`, `serving/*.md`, `deploy/*.md`, and
  `monitoring/*.md`
- optional `configs/cost_records.json`

The Catalog owns artifact identity and report wiring. Evidence Query owns
blocker explanation, repair cards, source locations, and readiness paths. The
Release Pipeline owns the release decision. The Final Report only projects
those decisions and links them to their raw artifacts.

## Command

```bash
python3 scripts/render_final_report.py \
  --root . \
  --results results/raw \
  --summary results/raw/benchmark_summary.json \
  --evaluation ../evaluation.md \
  --costs configs/cost_records.json \
  --output reports/final_report.md \
  --json-output results/raw/final_report_index.json
```

## Output Contract

The report and JSON index retain only release-facing information:

- environment, build, runtime, and product evidence inventory;
- benchmark summary metrics originating from raw artifacts;
- explicit `dry_run`, `replay`, and `execute` evidence levels;
- Catalog drift and Native Audit aggregate status;
- one release decision from the Release Pipeline;
- bounded blocker, repair action, source-location, and readiness-path views
  from Evidence Query;
- reproducible commands and artifact/report indexes.

Synthetic fixture results remain developer-validation inputs. They may
exercise report confidence rendering but cannot prove benchmark, CUDA timing,
product replay, or execute behavior.

`top_repair_actions` is a bounded presentation of existing Query/Pipeline
actions. It does not execute commands. `action_source_trace` records which
canonical source supplied each displayed action; it must not recreate blocker
policy or introduce a second release decision.

## Evidence Boundary

- Performance values must originate from `results/raw/*.json` produced by an
  actual benchmark or execute path.
- Missing, stale, malformed, or wrong-mode artifacts remain blocked.
- Fixture, plan, capability, and dry-run evidence must not be rendered as
  replay or execute evidence.
- Report generation does not prove target-machine, GPU, Android, or product
  readiness.

Current slimming work has not restored validation. Regenerate
`reports/final_report.md` only after build/test/artifact/report validation is
explicitly enabled; do not hand-edit generated evidence to mimic a refresh.
