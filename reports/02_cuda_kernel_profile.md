# CUDA Kernel Milestone Notes

## Current Environment

This machine does not currently expose `nvcc` or `nvidia-smi`, so CUDA kernels
are added as an optional milestone rather than a mandatory build target.

Run:

```bash
python3 scripts/check_cuda_environment.py --output results/raw/cuda_environment.json --try-cmake
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Current preflight output:

| field | value |
|---|---|
| nvcc | missing |
| nvidia-smi | missing |
| CUDA available | false |
| CUDA CMake configure | failed at CUDA toolkit discovery |

The native build still includes a CPU vector-add reference:

```text
native_core/include/inferlab/vector_add.hpp
native_core/src/vector_add.cpp
native_core/tests/vector_add_test.cpp
native_core/include/inferlab/matrix_ops.hpp
native_core/src/matrix_ops.cpp
native_core/tests/matrix_ops_test.cpp
native_core/include/inferlab/attention.hpp
native_core/src/attention.cpp
native_core/src/attention_probe_main.cpp
native_core/tests/attention_test.cpp
native_core/include/inferlab/normalization.hpp
native_core/src/normalization.cpp
native_core/tests/normalization_test.cpp
native_core/include/inferlab/rope.hpp
native_core/src/rope.cpp
native_core/tests/rope_test.cpp
```

The CUDA skeleton is:

```text
cuda_kernels/vector_add.cu
cuda_kernels/vector_add_runtime.hpp
cuda_kernels/vector_add_runtime.cu
cuda_kernels/vector_add_cuda_test.cpp
cuda_kernels/matrix_ops_runtime.hpp
cuda_kernels/matrix_ops_runtime.cu
cuda_kernels/matrix_ops_cuda_test.cpp
cuda_kernels/normalization_runtime.hpp
cuda_kernels/normalization_runtime.cu
cuda_kernels/normalization_cuda_test.cpp
cuda_kernels/rope_runtime.hpp
cuda_kernels/rope_runtime.cu
cuda_kernels/rope_cuda_test.cpp
cuda_kernels/attention_runtime.hpp
cuda_kernels/attention_runtime.cu
cuda_kernels/attention_cuda_test.cpp
cuda_kernels/quantization_runtime.hpp
cuda_kernels/quantization_runtime.cu
cuda_kernels/quantization_cuda_test.cpp
```

When CUDA is available and `INFERLAB_ENABLE_CUDA=ON`, the runtime wrapper:

- allocates device buffers with RAII
- copies host vectors to device
- launches `inferlab_vector_add_cuda`
- records kernel elapsed time with CUDA events through
  `vector_add_cuda_runtime_profiled`
- checks launch and synchronization errors
- copies the result back to host

`vector_add_cuda_test` compares the CUDA runtime wrapper against
`vector_add_cpu`. It is only registered with CTest inside the CUDA-enabled CMake
branch.

The CUDA branch also includes naive matvec, matmul, and row-softmax runtime
wrappers. `matrix_ops_cuda_test` compares them against `matvec_cpu`,
`matmul_cpu`, and `row_softmax_cpu`. These are baseline correctness kernels,
not optimized GEMM or softmax implementations.

`normalization_cuda_test` compares the naive CUDA RMSNorm runtime wrapper
against `rms_norm_cpu`.

`rope_cuda_test` compares the naive CUDA RoPE runtime wrapper against
`apply_rope_cpu`, including an absolute position offset for decode-style use.

`attention_cuda_test` compares the naive CUDA scaled dot-product attention
runtime wrapper against `scaled_dot_product_attention_cpu`, including a causal
mask case.

`quantization_cuda_test` compares the CUDA INT4 fused dequant + matvec runtime
wrapper against `fused_dequant_matvec`.

## CPU GEMM/Softmax Profile

Run:

```bash
./build/inferlab_matrix_ops_profile --repo-root . --output results/raw/matrix_ops_profile.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default GEMM/GEMV/softmax analytical profile:

| field | value |
|---|---:|
| GEMM shape | 4 x 8 x 6 |
| GEMM FLOPs | 384 |
| GEMM compulsory bytes | 416 |
| GEMM naive-loop bytes | 1632 |
| GEMM compulsory arithmetic intensity | 0.923077 |
| GEMM naive-loop arithmetic intensity | 0.235294 |
| tiled GEMM tile shape | 2 x 4 x 3 |
| tiled GEMM global bytes | 736 |
| tiled GEMM arithmetic intensity | 0.521739 |
| naive-to-tiled byte reduction | 2.217391 |
| GEMV FLOPs | 64 |
| GEMV naive-loop bytes | 272 |
| row-softmax memory bytes | 480 |
| row-softmax exp ops | 24 |
| row-softmax scalar ops per byte | 0.141667 |

The GEMM profile records both compulsory traffic, which counts unique lhs/rhs
inputs plus output writes, and naive-loop traffic, which models the current
simple nested-loop baseline repeatedly reading lhs/rhs elements. This gives a
small roofline-style baseline before CUDA tiling, shared-memory reuse, or
kernel fusion is attempted.

The tiled GEMM profile walks actual tile boundaries and estimates global memory
loads for lhs/rhs tiles plus output writes. It is still analytical, but it shows
the direction a shared-memory CUDA kernel should move: fewer repeated global
reads and higher arithmetic intensity than the naive loop, while still above
the compulsory traffic lower bound.

The row-softmax profile models the current stable softmax structure: one pass
for row max, one pass for exponentials and row sum, and one normalization pass.
It reports memory bytes, compare/add/div counts, and exp count separately so
future CUDA softmax kernels can be compared against a clear baseline.

## CPU Attention Reference

Run:

```bash
./build/inferlab_attention_probe --repo-root . --output results/raw/attention_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default probe:

| field | value |
|---|---:|
| heads | 2 |
| query length | 4 |
| key length | 6 |
| head dim | 8 |
| score elements | 48 |
| output elements | 64 |
| non-causal dense K/V element reads | 768 |
| score/probability intermediate bytes | 384 |
| causal square mask sparsity | 0.375 |
| causal dense value element reads | 256 |
| causal mask-aware value element reads | 160 |

This reference implements:

```text
scores = QK^T / sqrt(head_dim)
probabilities = row_softmax(scores)
output = probabilities * V
```

The implementation is intentionally simple and materializes the score matrix.
Later CUDA/FlashAttention work should use this as a correctness baseline before
optimizing away intermediate memory traffic.

The reference also supports a decoder-style causal mask for square
`query_length == key_length` attention, where token `q` cannot attend to keys
`k > q`.

`profile_attention_reads` estimates the traffic shape of the simple reference:
QK score pairs, K/V element reads, materialized score/probability bytes, output
writes, and multiply-add counts. The causal profile records both the dense value
read path used by the simple baseline and the mask-aware value read count a
future fused kernel should move toward. These are analytical counters, not
latency or throughput claims.

## CPU Normalization Reference

The native core also includes:

- `layer_norm_cpu`
- `rms_norm_cpu`

These are row-wise references for later CUDA normalization kernels. RMSNorm is
especially relevant to modern LLaMA-style decoder blocks, while LayerNorm is a
useful baseline for older Transformer variants and for testing mean/variance
logic.

## CPU RoPE Reference

The native core includes `apply_rope_cpu` for rotary position embeddings over
`[seq_len, dim]` row-major inputs. Tests cover position-zero identity,
pair-norm preservation, and invalid odd dimensions.

## Build Policy

Default build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

CUDA-enabled machines can configure with:

```bash
cmake -B build-cuda -DCMAKE_BUILD_TYPE=Release -DINFERLAB_ENABLE_CUDA=ON
cmake --build build-cuda -j$(nproc)
ctest --test-dir build-cuda --output-on-failure
```

On this machine, `cmake -B build-cuda-check -DINFERLAB_ENABLE_CUDA=ON` currently
fails while finding `nvcc`, so the CUDA wrapper is added but not executed here.

## Next CUDA Steps

1. Run `vector_add_cuda_test` and `matrix_ops_cuda_test` on a CUDA machine.
2. Run `normalization_cuda_test` and `rope_cuda_test` on a CUDA machine.
3. Run `attention_cuda_test` and `quantization_cuda_test` on a CUDA machine.
4. Add shared-memory tiled GEMM.
5. Record Nsight Compute metrics: memory throughput, SM throughput, occupancy,
   and warp stalls.
6. Move quantization toward CUDA dequant + GEMV for decode-stage experiments.
