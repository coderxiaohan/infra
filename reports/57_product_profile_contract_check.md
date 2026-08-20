# Product Profile Contract Check

This chapter documents the C++ native artifact checker for mature product profile evidence.

InferLab uses vLLM, SGLang, TensorRT-LLM, LMDeploy, Dynamo, LMCache, Mooncake, Ray Serve, KServe/llm-d, FlashInfer, KTransformers, and TGI as external mature products. This checker does not launch those products and does not reimplement their schedulers, kernels, KV connectors, runtimes, storage engines, or control planes.

## Native Entrypoint

- Binary: `build/inferlab_product_profile_contract_check`
- Sources:
  - `native_core/src/mature_product_profile_contract.hpp/.cpp`
  - `native_core/src/product_profile_artifact_contract.hpp/.cpp`
  - `native_core/src/product_profile_contract_check_model.hpp/.cpp`
  - `native_core/src/product_profile_contract_check_view.hpp/.cpp`
  - `native_core/src/product_profile_contract_check_main.cpp`
- Artifact: `results/raw/native_product_profile_contract_check.json`

```bash
./build/inferlab_product_profile_contract_check \
  --results results/raw \
  --output results/raw/native_product_profile_contract_check.json \
  --format json
```

Use `--strict` when a release gate should fail if any expected product profile artifact is absent.

The CLI follows the shared evidence/release contract: it accepts JSON-only `--format json`, rejects unsupported formats or positional arguments, and emits redacted `cli_context` metadata for repo/results/output paths.

## What It Checks

| Area | Check |
|---|---|
| Artifact state | Reads every canonical profile path once. Only a genuinely missing file may be pending when not strict; path-probe errors, directories, unreadable/empty files, malformed JSON, and decoded duplicate keys fail closed. |
| Schema contract | Validates all 21 canonical top-level fields against their registered JSON types, plus exact `product_profile.v1` schema name/version. |
| Product identity | Requires exact canonical product id, display name, category, adapter module, script kind, and redaction contract from the mature-product registry. |
| Product boundary | Requires exact adapter boundary, nonempty official sources, `not_implemented_by_inferlab`, local optimization scope, external-product claim role, and external-core flag. |
| Mode contract | Validates `dry_run`/`execute`/`replay` against `status`, `executed`, `replayed`, claim-scope mode, target-environment requirement, and optional replay evidence. |
| Evidence gate | Requires canonical dry-run/execute/replay command surfaces and a typed raw-metrics contract. Structural contract readiness remains separate from execute/replay evidence eligibility. |
| Report wiring | Emits summary fields consumed by `summarize_benchmarks.py`, Prometheus textfile export, and the final report. |

## Artifact Summary Fields

- `present_artifacts`
- `missing_artifacts`
- `passed_records`
- `failed_records`
- `pending_records`
- `required_field_pass_rate`
- `boundary_pass_count`
- `execute_gate_pass_count`
- `read_issue_count`
- `malformed_or_invalid_artifact_count`
- `contract_coverage`
- `mode_flag_pass_count`
- `claim_scope_pass_count`
- `product_evidence_eligible_count`
- `performance_claim_eligible_count`
- `physical_artifact_read_count`

## Acceptance

- Dry-run development machines may produce `pending_missing_artifact` records without starting product servers.
- A real environment should first generate product profile artifacts through the adapter scripts, or export logs/metrics/profiler files that can be normalized with `--replay`, then run this checker.
- Execute mode may establish that the product adapter ran, but the current product-profile payload contains probe/count metadata rather than benchmark latency or throughput values. Performance conclusions therefore still require explicit measured rows in a source artifact; this checker only marks replay profiles claim-eligible when normalized metric rows and positive parsed-metric evidence are present.
- Missing or malformed evidence is reported as pending/failure according to the explicit missing-file policy, never silently treated as success. Unreadable and non-regular paths always fail regardless of `--strict`.
- A structurally valid dry-run artifact may pass this checker but cannot become product-performance evidence. Execute/replay eligibility additionally requires `status=ok`, consistent mode flags, raw metrics, and the exact claim-scope contract.

The shared semantic evaluator is also consumed by Mature Product Release Readiness. That target no longer owns a second, weaker product-profile field/mode interpretation.
