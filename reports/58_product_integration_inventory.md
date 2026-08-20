# Product Integration Inventory

This chapter documents the C++ native inventory gate for mature product integration surfaces.

InferLab uses vLLM, SGLang, TensorRT-LLM, LMDeploy, Dynamo, LMCache, Mooncake, Ray Serve, KServe/llm-d, FlashInfer, KTransformers, and TGI as external products. The inventory gate does not run those products and does not reimplement their schedulers, kernels, KV systems, runtimes, controllers, or storage engines.

## Native Entrypoint

- Binary: `build/inferlab_product_integration_inventory`
- Scoped source contract: `native_core/src/python_source_contract.hpp/.cpp`
- Model: `native_core/src/product_integration_inventory_model.hpp/.cpp`
- View: `native_core/src/product_integration_inventory_view.hpp/.cpp`
- CLI: `native_core/src/product_integration_inventory_main.cpp`
- Artifact: `results/raw/native_product_integration_inventory.json`

```bash
./build/inferlab_product_integration_inventory \
  --repo-root . \
  --output results/raw/native_product_integration_inventory.json
```

## What It Checks

| Area | Check |
|---|---|
| Adapter surface | Each profile runner is a regular readable file and binds exactly once to its canonical product id. |
| Config surface | Each example config is a strict JSON object whose top-level product and report section match the canonical profile. |
| Report surface | Shared report chapters are physically read once and contain the exact artifact code span plus `not_implemented_by_inferlab` boundary. |
| Registry surface | The scoped `PRODUCTS` entry in `scripts/product_profiles/common.py` matches canonical name, category, artifact, report and config fields and has nonempty boundary lists. |
| Catalog glue | Every profile artifact occurs exactly once in `COMMON_LEGACY_EVIDENCE_SOURCES` while the compatibility fallback remains active. |
| Benchmark matrix | The uniquely named product row exactly matches script, module, backend, config, output, report, dry-run command and replay command. |
| Summary wiring | The Summary consumer uses catalog glue and has one product-profile projection call bound to the exact artifact. |
| Prometheus wiring | The exporter consumes `compose_legacy_sources` and `resolve_catalog_sources`; product discovery comes from the canonical glue list rather than repeated per-product literals. |
| Final report wiring | The scoped `Mature Product Profiles` module lists the exact product artifact/report and the current source-contract/model/view/CLI source files. |

The source-contract helper performs bounded lexical inspection of the repository's known Python declarations and calls. It is not a Python AST, does not import modules, and does not execute profile or report code.

## Artifact Summary Fields

- `product_count`
- `passed_records`
- `failed_records`
- `passed_checks`
- `total_checks`
- `integration_score`
- `unique_report_file_count`
- `physical_text_read_count`
- `total_read_issue_count`

## Acceptance

- A product integration is complete only when all scoped source contracts pass with no duplicate registry/matrix/list entry and no file read issue.
- This gate is static and environment-independent; missing GPUs, Kubernetes, models, endpoints, or installed products do not affect it.
- Real performance evidence still requires product profile artifacts under `results/raw/product_*.json` and explicit `--execute` runs on target machines.
- `status=ok` means repository integration wiring is ready for profile generation only; it is not product replay, execute, benchmark, freshness, or release evidence.
