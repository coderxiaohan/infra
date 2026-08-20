# CUDA/Quant Microkernel Deep Dive Implementation Report

## Scope

This report describes a native C++ evidence bridge for CUDA/quant microkernel metadata, fallback, and correctness contracts. It does not claim a FlashInfer, TensorRT-LLM, CUTLASS, Marlin, vLLM, or SGLang benchmark run.

## Product Capabilities

Production decode attention, Paged KV gather, low-bit GEMM/dequant, CUDA graph capture, and kernel autotuning are mature product/kernel-library capabilities in systems such as FlashInfer, TensorRT-LLM, CUTLASS, Marlin, vLLM, and SGLang.

## InferLab Integration

InferLab connects product kernel profile/replay artifacts to native CPU references and CUDA skeleton metadata through benchmark matrix entries, artifact contracts, summary wiring, Prometheus export wiring, and final report cataloging.

## InferLab Native Slice

Implemented native entry:

- Canonical operator registry: `native_core/src/high_value_artifact_catalog.hpp::microbench_operators`
- Typed analytical model: `native_core/src/cuda_quant_microkernel_deep_dive_model.hpp/.cpp`
- Projection-only artifact view: `native_core/src/cuda_quant_microkernel_deep_dive_view.hpp/.cpp`
- Thin CLI: `native_core/src/cuda_quant_microkernel_deep_dive_main.cpp`
- CMake target: `inferlab_cuda_quant_microkernel_deep_dive`
- Target artifact: `results/raw/native_cuda_quant_microkernel_deep_dive.json`
- Matrix refresh command: `./build/inferlab_cuda_quant_microkernel_deep_dive --repo-root . --output results/raw/native_cuda_quant_microkernel_deep_dive.json`

The validated canonical registry owns the six operator identities, families,
shape matrices, dtype/layouts, backend candidates, correctness oracles,
benchmark cases, NVTX ranges, and Nsight commands. The deep-dive profile only
adds one modeled shape, reference artifact, fallback, estimated FLOPs/bytes,
and correctness tolerance per canonical operator. A missing, duplicate,
unknown, incomplete, non-finite, or non-positive profile fails closed.

The artifact records RMSNorm, RoPE, row softmax, INT4 dequant+matvec, Paged KV
gather, and decode attention metadata: canonical operator contract, modeled
shape, estimated FLOPs/bytes, modeled arithmetic intensity, explicit
bottleneck thresholds, correctness and fallback contracts, profiling metadata,
and target-machine commands.

`microkernel_readiness_score` remains as a downstream compatibility field,
but is explicitly an alias of `analytical_priority_score`. It combines
checked static contract coverage with a bounded modeled-intensity priority; it
is not runtime readiness, CUDA availability, correctness, timing, occupancy,
or product support. Cross-operator highest fields are scoped to the compiled
fixture records and are not global kernel rankings.

## Claim Boundary

This is a microkernel metadata and evidence-bridge slice. Arithmetic intensity
is computed from compiled estimated FLOPs/bytes, not observed hardware
counters. Every operator requires target GPU validation. This artifact is not
a CUDA timing or correctness result, does not prove a product backend is
supported, and does not replace FlashInfer, TensorRT-LLM, CUTLASS, Marlin,
vLLM, or SGLang.

The separate `inferlab_kernel_microbench_harness` owns target-machine
correctness/timing/profile manifest and execution semantics. The deep-dive
artifact must not become a second microbenchmark runner.

## Optional Native Trace Context

The existing CLI accepts `--trace-context-file <jsonl>`. Each bounded row uses
the canonical microbenchmark operator name as `record_key` and supplies W3C
trace/span context, `clock_domain`, and `timestamp_ns`; `parent_span_id` is
optional. The artifact stores only domain-separated, non-cryptographic join
tokens and counts. It suppresses raw trace/span ids and timestamp values.

The same CLI optionally accepts `--trace-clock-calibration-file <jsonl>`.
An exact trace/scope/domain entry may explicitly map the CUDA-event point into
trace-relative nanoseconds with uncertainty. The artifact emits usage and a
non-cryptographic calibration fingerprint, never the raw trace id, absolute
timestamp, anchor, or calibration row.

This optional projection prepares later Product Replay/native correlation. It
does not execute a kernel, import Nsight output, infer CUDA-event clock sync,
prove same request/parentage/ordering/causality, or create correctness, timing,
replay, execute, performance, or release evidence.

## Deferred Validation

Not run per current user request. Future validation should run CMake build, native execution, CUDA correctness/timing, product replay/execute, summary export, Prometheus export, final report rendering, and claim/freshness gates on the target machine.
