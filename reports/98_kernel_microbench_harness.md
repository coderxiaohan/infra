# Kernel Microbenchmark Harness

## Purpose

This report tracks `results/raw/native_kernel_microbench_harness.json`.

The harness defines a target-machine microbenchmark and profiling surface for:

- RMSNorm
- RoPE
- row softmax
- INT4 dequant + matvec
- Paged KV gather
- decode attention

## Implemented Surface

- C++ target: `inferlab_kernel_microbench_harness`
- Canonical operator metadata: `high_value_artifact_catalog.hpp::microbench_operators()`
- Typed execution model: `kernel_microbench_harness_model.hpp/.cpp`
- Correctness and timing runner: `kernel_microbench_harness_runner.hpp/.cpp`
- Projection-only artifact view: `kernel_microbench_harness_view.hpp/.cpp`
- Strict request/output shell: `kernel_microbench_harness_cli.hpp/.cpp`; the formal main is five lines
- Optional dependency: pinned `google/benchmark` v1.8.5
- Optional runtime: InferLab CUDA runtime kernels for RMSNorm, RoPE, row softmax, INT4 dequant + matvec, Paged KV gather, and decode attention
- Optional profiling hook: CUDA NVTX, linked through `CUDA::nvtx3` or `CUDA::nvToolsExt` when available
- Dependency scope: google/benchmark, CUDA-build and NVTX compile definitions/linkage are applied only to `inferlab_kernel_microbench_harness`; unrelated high-value targets do not inherit profiling dependencies

The harness owns 18 typed execution cases: every canonical operator has exactly one `smoke`, `serving_like`, and `stress` case. Case dimensions, concrete execution dtype, deterministic input seed, output cardinality, and absolute/relative tolerance are validated before rendering or execution. The execution fixture extends the canonical operator registry by foreign key; it does not copy operator identity, benchmark names, oracle names, NVTX ranges, or product metadata.

Supported flags:

- `--execute`
- `--operator <all|rmsnorm|rope|row_softmax|int4_dequant_matvec|paged_kv_gather|decode_attention>`
- `--case-profile <smoke|serving_like|stress|all>`
- `--backend <cuda_runtime|cpu_reference>`
- `--benchmark-min-time-ms <positive finite value>`
- `--benchmark-repetitions <1..100>`

Unknown operators, profiles, backends, formats, and invalid timing controls fail closed. They no longer silently select every operator.

## Execution Semantics

`--backend cuda_runtime` generates the CPU reference, invokes the selected InferLab CUDA runtime, checks shape and finite output, and applies the typed elementwise absolute-plus-relative tolerance. Google Benchmark timing is allowed only for cases that pass correctness. CUDA runtimes report device-event elapsed time through Google Benchmark manual timing; CPU reference baselines use Google Benchmark real time.

`--backend cpu_reference` executes a CPU baseline and marks it as `cpu_reference_executed`. It is not treated as candidate correctness or target GPU evidence. The artifact emits `target_gpu_execute_verified: true` only when every canonical operator is selected and all selected CUDA cases pass correctness and produce timing. A single-operator success remains valid selected-case evidence but cannot close the project-level GPU microbenchmark gate. Release Root Cause consumes that explicit bit, so CPU-only or partial execution cannot close the blocker.

The custom reporter collects iteration runs, derives P50/P99 latency and output-item throughput, rejects non-finite or non-positive timing, and emits no timing values when the library is unavailable or correctness fails. NVTX ranges are emitted only around CUDA benchmark iterations. Nsight commands are generated for the exact selected case, but external Nsight reports are never claimed as captured by this binary.

## Claim Boundary

Without `--execute`, this remains a typed manifest. Timing, throughput, latency, occupancy, bandwidth, and speedup claims are blocked until target GPU artifacts are generated under `results/raw/*.json`.

CPU baseline execution, CUDA correctness, CUDA timing, and external profile capture remain separate evidence states. A successful selected-case run is not a cross-product speedup, production SLO, cost, product behavior, or release-readiness result.

## Refresh Command

```bash
./build/inferlab_kernel_microbench_harness --repo-root . --output results/raw/native_kernel_microbench_harness.json --format json
./build/inferlab_kernel_microbench_harness --repo-root . --execute --operator all --case-profile smoke --backend cuda_runtime --output results/raw/native_kernel_microbench_harness.json --format json
```

Current server status: `not run per user request`.
