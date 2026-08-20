# Third-Party Dependency Governance

## Purpose

This report tracks `results/raw/native_third_party_dependency_gate.json`.

The dependency gate records why a dependency is introduced, which CMake option controls it, whether it is required or optional, how it falls back, what license applies, and whether it affects claim scope.

Role boundary: this artifact is the policy/risk/fallback summary. `native_dependency_sbom_gate.json` is the SBOM-style component inventory.

## Governed Dependencies

- Build and tooling: `nlohmann/json`, SQLite3, `google/benchmark`, CUDA NVTX, and CUDA Toolkit.
- Product runtimes: vLLM, SGLang, TensorRT-LLM, FlashInfer, LMCache, Mooncake, Ray Serve, KServe/llm-d, Hugging Face TGI, and KTransformers.
- Fetched dependencies use fixed revisions. System, toolkit, and product entries must record the resolved target-environment version before execute or performance claims.

## Implemented Surface

- C++ target: `inferlab_third_party_dependency_gate`
- Canonical registry: `native_core/src/high_value_artifact_catalog.hpp::dependency_specs()`
- Shared model/view/runner: `native_core/src/dependency_governance_model.*`, `dependency_governance_view.*`, and `dependency_governance_cli.*`
- Thin entrypoint: `native_core/src/third_party_dependency_gate_main.cpp`
- Artifact sections:
  - `dependencies`
  - `dependency_to_target_map`
  - `dependency_to_claim_map`
  - `license_review`
  - `fallback_matrix`
- Registry validation rejects duplicate dependency names, unknown risk levels, non-exclusive required/optional flags, and duplicate nonempty `used_by` identities.
- The shared model owns metadata-gap detection, license review classification, checked summary counts, blocker indices, fallback policy, product/build classification, and target-machine commands for both dependency artifacts.
- The shared projection preserves the Policy Gate and SBOM Gate as separate artifact roles without maintaining two copies of dependency maps or governance decisions.
- CMake scope: nlohmann/json is an optional PUBLIC usage requirement of `inferlab_native_core` because the shared inline artifact parser must use one capability value across library and executable translation units. Its governed fallback is the shared bounded strict parser, not an independent legacy scanner, so dependency absence is not a required-dependency blocker. SQLite is scoped to Evidence Warehouse; google/benchmark and NVTX are scoped to Kernel Microbenchmark.
- CMake options:
  - `INFERLAB_ENABLE_THIRD_PARTY_JSON`
  - `INFERLAB_ENABLE_EVIDENCE_SQLITE`
  - `INFERLAB_ENABLE_GOOGLE_BENCHMARK`
  - `INFERLAB_ENABLE_NVTX`

## Claim Boundary

The dependency gate documents dependency policy. It does not prove dependency installation, production license approval, build success, benchmark execution, product readiness, or performance on the current server. `documented` means registry metadata is complete; it is not an execute-readiness decision.

## Refresh Command

```bash
./build/inferlab_third_party_dependency_gate --repo-root . --output results/raw/native_third_party_dependency_gate.json --format json
```

Current server status: `not run per user request`.
