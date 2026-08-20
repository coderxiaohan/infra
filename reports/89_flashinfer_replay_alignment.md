# FlashInfer Replay Alignment Implementation Report

## Scope

This report describes a native C++ alignment bridge between FlashInfer profile/replay metadata and InferLab native kernel contracts. It does not execute or copy FlashInfer kernels.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_borrowed_gap_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_borrowed_gap_artifacts`
- Target artifact: `results/raw/native_flashinfer_replay_alignment.json`
- Refresh command: `./build/inferlab_mature_product_borrowed_gap_artifacts --repo-root . --artifact native_flashinfer_replay_alignment --output results/raw/native_flashinfer_replay_alignment.json`

The artifact records shape coverage, dtype coverage, fallback rate, required metadata presence, operator, and replay mode.

## Claim Boundary

This is an alignment artifact. FlashInfer execution and performance claims require FlashInfer profile/replay or execute artifacts.

## Deferred Validation

Not run per current user request.
