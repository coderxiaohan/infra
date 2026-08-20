# Grammar Backend Matrix Implementation Report

## Scope

This report describes a native grammar backend matrix for schema coverage, compile cache, token-mask cost, fallback, and supported structured-output surfaces. It does not execute XGrammar, llguidance, Outlines, vLLM, or SGLang.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_p2_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_p2_artifacts`
- Target artifact: `results/raw/native_grammar_backend_matrix.json`
- Refresh command: `./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_grammar_backend_matrix --output results/raw/native_grammar_backend_matrix.json`

The artifact records schema coverage, compile-cache hit rate, mask cost, fallback rate, supported surface, and backend decision.

## Claim Boundary

This is a compatibility and parser contract matrix. Grammar backend performance and model correctness require target product evidence.

## Deferred Validation

Not run per current user request.
