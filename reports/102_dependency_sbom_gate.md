# Dependency SBOM Gate

## Purpose

This report tracks `results/raw/native_dependency_sbom_gate.json`.

The SBOM gate upgrades dependency governance into a release-facing component list. It records library/product runtime names, pinned versions or target-environment version policy, source, license, CMake option or product profile control, fallback, risk level, and claim impact.

Role boundary: this artifact is the SBOM-style component inventory. `native_third_party_dependency_gate.json` is the dependency policy/risk/fallback summary.

## Implemented Surface

- C++ target: `inferlab_dependency_sbom_gate`
- Thin entrypoint: `native_core/src/dependency_sbom_gate_main.cpp`
- Shared catalog: `native_core/src/high_value_artifact_catalog.hpp::dependency_specs()`
- Shared model/view/runner: `native_core/src/dependency_governance_model.*`, `dependency_governance_view.*`, and `dependency_governance_cli.*`
- Artifact: `results/raw/native_dependency_sbom_gate.json`
- Covered dependency families: `nlohmann/json`, SQLite3, google/benchmark, CUDA NVTX, CUDA Toolkit, vLLM, SGLang, TensorRT-LLM, FlashInfer, LMCache, Mooncake, Ray Serve, KServe/llm-d, TGI, KTransformers
- Governance sections: `dependency_to_target_map`, `dependency_to_claim_map`, `license_review`, `fallback_matrix`, `target_machine_validation_commands`
- Policy/SBOM common fields, checked counts, metadata-gap blockers, fallback readiness, license classification, and validation commands are projected once; this target only selects the SBOM role.
- The registry is stable and validated for unique dependency identity, bounded risk vocabulary, exclusive required/optional state, and duplicate `used_by` identity.

## Claim Boundary

This artifact is dependency governance, not dependency validation. It does not prove packages are installed, licenses are approved for production, CUDA is available, product runtimes are installed, or performance improved. Target-machine CMake/dependency validation is required before release claims.

Dependency metadata can explain whether a claim is blocked or optional. It must not create throughput, latency, correctness, or product-readiness claims without corresponding `results/raw/*.json` execution or replay evidence.

## Refresh Command

```bash
./build/inferlab_dependency_sbom_gate --repo-root . --output results/raw/native_dependency_sbom_gate.json --format json
```

Current server status: `not run per user request`.
