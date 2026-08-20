# Structured Output Parser Contract Implementation Report

## Scope

This report describes a native parser contract for tool calling, reasoning spans, JSON schema validation, stream fragment recovery, and grammar fallback. It does not execute models or grammar backends.

## InferLab Native Slice

- C++ source: `native_core/src/mature_product_p2_artifacts_main.cpp`
- CMake target: `inferlab_mature_product_p2_artifacts`
- Target artifact: `results/raw/native_structured_output_parser_contract.json`
- Refresh command: `./build/inferlab_mature_product_p2_artifacts --repo-root . --artifact native_structured_output_parser_contract --output results/raw/native_structured_output_parser_contract.json`

The artifact records JSON parse success, schema validity, required-argument miss rate, reasoning span conflicts, stream fragment recovery, retry rate, and parser policy.

## Claim Boundary

Parser fixture/contract evidence is not model correctness and does not replace vLLM, SGLang, XGrammar, llguidance, or Outlines behavior.

## Deferred Validation

Not run per current user request.
