# Paged KV Gather Kernel Contract Implementation Report

## Scope

This report describes a native C++ Paged KV gather kernel contract for block-table lookup, estimated bytes, coalescing risk, layout, and fallback. It does not replace FlashInfer or vLLM PagedAttention kernels.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_borrowed_gap_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_borrowed_gap_artifacts`
- Target artifact: `results/raw/native_paged_kv_gather_kernel_contract.json`
- Refresh command: `./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_paged_kv_gather_kernel_contract --output results/raw/native_paged_kv_gather_kernel_contract.json`

The artifact records logical tokens, physical blocks, estimated bytes, coalescing risk, fallback risk, and layout.

## Claim Boundary

This is an eligibility and fallback contract. CUDA timing and kernel ownership remain external until validated on a GPU machine.

## Deferred Validation

Not run per current user request.
