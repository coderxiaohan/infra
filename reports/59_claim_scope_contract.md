# Claim Scope Contract

This chapter documents the C++ native claim-scope gate for InferLab result artifacts.

InferLab is a product integration, benchmark, evidence, and report platform. Many artifacts are dry-run plans, static audits, or deterministic local simulations. This gate prevents those artifacts from being treated as real backend performance evidence unless their JSON explicitly carries execution semantics and enough evidence fields.

## Native Entrypoint

- Binary: `build/inferlab_claim_scope_contract`
- Source: `native_core/src/claim_scope_contract_main.cpp`
- Artifact: `results/raw/native_claim_scope_contract.json`

```bash
./build/inferlab_claim_scope_contract \
  --repo-root . \
  --matrix configs/benchmark_matrix.yaml \
  --results results/raw \
  --output results/raw/native_claim_scope_contract.json \
  --format json
```

The CLI follows the shared evidence/release contract: it accepts JSON-only `--format json`, rejects unsupported formats or positional arguments, emits redacted `cli_context` metadata for repo/matrix/results/output paths, and marks the artifact as `mode: dry_run` / `executed: false` because it audits existing artifact declarations instead of running producers.

The implementation is split into bounded owners: `claim_scope_contract_matrix.hpp/.cpp` owns constrained benchmark-matrix inventory, `claim_scope_contract_model.hpp/.cpp` owns one-read contract evaluation and checked summaries, `claim_scope_contract_view.hpp/.cpp` owns projection-only JSON, and `claim_scope_contract_main.cpp` is the CLI/request/output shell.

## What It Checks

| Area | Check |
|---|---|
| Benchmark matrix coverage | Reads every `results/raw/*.json` artifact declared by `configs/benchmark_matrix.yaml`. |
| Matrix source integrity | Missing, unreadable, empty, output-free, malformed, duplicate, unsafe, or resource-limit matrix declarations fail closed. |
| JSON integrity | Requires a valid root object without decoded duplicate keys before inspecting claim fields. |
| Execution semantics | Checks for `executed`, `mode`, `execution_kind`, `audit_type`, or `claim_scope` depending on artifact family. |
| Field types | Required booleans, nonempty strings, summary objects, and replay record arrays must have their declared JSON types. |
| Product boundary | Product profiles must expose `not_implemented_by_inferlab` and command/config evidence. |
| Static audit boundary | Native contract artifacts must declare audit type or claim scope and keep external-product boundaries visible. |
| Deep-dive boundary | Native deep-dive artifacts must declare `executed`, `claim_scope` or `audit_type`, and must not use execute semantics. |
| Simulation boundary | Native plan/simulation artifacts must not claim `executed: true` without command, metrics, config, version, or environment evidence. |
| Release risk | Flags `executed: true` artifacts that lack backend, command, metric, or environment evidence. |

## Artifact Summary Fields

- `artifact_count`
- `present_count`
- `pass_count`
- `warn_count`
- `fail_count`
- `pending_count`
- `executed_true_count`
- `executed_false_count`
- `claim_scope_count`
- `risky_executed_without_evidence_count`
- `claim_contract_score`
- `matrix_read_issue_count`
- `matrix_issue_count`
- `matrix_ignored_output_count`
- `matrix_duplicate_output_count`

## Acceptance

- Dry-run artifacts may be pending on machines without GPU, Kubernetes, models, or installed products.
- Simulation and plan artifacts must keep claim scope explicit before they are used in final reports.
- Real execution claims require command/config, raw metrics, and environment or version evidence.
- Missing artifacts contribute an explicit failed check; zero required checks never produce a perfect score.
- This gate does not run benchmarks, external products, Kubernetes, model downloads, or endpoint probes.
