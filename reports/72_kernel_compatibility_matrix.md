# Kernel Compatibility Matrix Implementation Report

## Scope

This report describes a native C++ compatibility contract for mapping operators, shapes, dtypes, layouts, backend candidates, fallback rules, correctness contracts, and profiling metadata. It does not claim FlashInfer, TensorRT-LLM, CUTLASS, Marlin, vLLM, SGLang, KTransformers, or llama.cpp benchmark execution.

## Product Capabilities

FlashInfer, TensorRT-LLM, CUTLASS, Marlin, vLLM, SGLang, KTransformers, and llama.cpp own mature kernel/runtime behavior, backend selection, quantized kernels, engine metadata, and product fallback paths.

## InferLab Integration

InferLab connects product kernel/profile/replay artifacts to a native compatibility matrix so reports can explain which backend is expected, which fallback is allowed, and which correctness/profiling evidence must exist before claims are made.

## InferLab Native Slice

Implemented native entry:

- Typed model: `native_core/src/kernel_compatibility_matrix_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/kernel_compatibility_matrix_view.hpp/.cpp`
- Thin CLI: `native_core/src/kernel_compatibility_matrix_main.cpp`
- CMake target: `inferlab_kernel_compatibility_matrix`
- Target artifact: `results/raw/native_kernel_compatibility_matrix.json`
- Matrix refresh command: `./build/inferlab_kernel_compatibility_matrix --repo-root . --output results/raw/native_kernel_compatibility_matrix.json`

The artifact records operator, shape signature, dtype, layout, backend candidates, selected backend, fallback backend, fallback reason, analytical arithmetic intensity, typed correctness contract, profiling metadata required, migration command, and release gate.

RMSNorm, RoPE, row softmax, INT4 dequant+matvec, Paged KV gather, and decode attention are foreign-key joins to the validated `inferlab_high_value::microbench_operators()` registry. Their family, canonical shape matrix, dtype/layout, backend inventory, correctness oracle, benchmark case, NVTX range, and Nsight commands are borrowed rather than copied into a second registry.

Prefill attention and MoE grouped GEMM remain explicit compatibility-only extensions. They are non-rankable and cannot enter the target-machine profile queue until their canonical registry, benchmark case, correctness implementation, and harness lifecycle are added together. This preserves the existing eight-record compatibility surface without pretending that the six-operator execution harness already supports eight operators.

Candidate and compatibility weights, the target-profile threshold, modeled FLOPs/bytes, tolerance, registry membership, ranking scope, and target-machine validation requirement are explicit typed model inputs. Invalid, duplicate, non-finite, out-of-range, missing-candidate, missing-canonical-profile, and ambiguous extension inputs fail closed; the old positive floors and silent score clamps are removed.

## Claim Boundary

This is a compiled compatibility fixture and migration checklist. Candidate selection and compatibility scores describe only the declared shape/dtype/risk/correctness metadata. Arithmetic intensity is modeled FLOPs divided by modeled bytes. Neither value proves backend support, CUDA correctness, measured timing, observed hardware counters, product behavior, benchmark performance, or release readiness, and the implementation does not replace external kernel libraries.

## Deferred Validation

Not run per current user request. Future validation should run CMake build, native execution, CUDA correctness/timing, product replay/execute, summary export, Prometheus export, final report rendering, and claim/freshness gates on the target machine.
