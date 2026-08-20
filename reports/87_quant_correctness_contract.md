# Quant Correctness Contract Implementation Report

## Scope

This report describes a native C++ quantization correctness contract for INT4, INT8, FP8, FP4, NVFP4, scale granularity, range clipping, fallback risk, and reference checks. It does not implement TensorRT-LLM, FlashInfer, CUTLASS, Marlin, or product quant kernels.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_borrowed_gap_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_borrowed_gap_artifacts`
- Target artifact: `results/raw/native_quant_correctness_contract.json`
- Refresh command: `./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_quant_correctness_contract --output results/raw/native_quant_correctness_contract.json`

The artifact records tolerance, scale granularity, range clip rate, fallback risk, dtype, and reference path.

## Claim Boundary

This is a correctness contract. Low-precision performance and accuracy claims require target GPU/model validation.

## Deferred Validation

Not run per current user request.
