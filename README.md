quick_brief:
  purpose: InferLab project entrypoint and current bootstrap status
  current_state: M0 scaffold plus native M1 KV/cache, quantization, decoder, GGUF, speculative decoding model, runtime benchmark harnesses, source-trace notes, Docker runtime manifests, final-report renderer, Prometheus/Grafana observability assets, Android/ARM deployment scaffold, and PyTorch Mini Transformer reference slices
  must_read:
    - 大模型推理优化综合项目方案.md
    - spec.md
    - AGENTS.md
  next_step: Add CUDA correctness targets on a CUDA-enabled machine or expand serving benchmarks after dependency checks
  last_verified: CPU torch dependency import checked; Mini Transformer validation intentionally skipped by request

# InferLab

InferLab is a cloud + edge LLM inference engineering lab. It is not a chatbot demo or a replacement for vLLM, SGLang, TensorRT-LLM, Dynamo, LMCache, Mooncake, Ray Serve, KServe/llm-d, FlashInfer, KTransformers, TGI, llama.cpp, or LMDeploy. The goal is to integrate mature inference products, run migration-friendly benchmark/profile plans, collect evidence artifacts, render reports, and use small C++/CUDA mechanism slices to explain prefill/decode, KV cache, PagedAttention-style layouts, scheduling pressure, quantization, GGUF, CUDA kernels, and edge deployment trade-offs.

The project follows the local AI harness rules:

- Plan in `../spec.md`.
- Implement one small verifiable item at a time.
- Record real command evidence in `../verification.md` and `../evaluation.md`.
- Keep long-running state in `../ai_snapshot.json`, `../todo.md`, and `../session-state.md`.

## Implementation Direction

InferLab is a mixed project, but the implementation center is **C++/CUDA**:

- C++17 native core: inference metadata, KV-cache layout, schedulers, GGUF parsing, CPU-side quantization utilities.
- CUDA C++: GEMM/GEMV, softmax, attention, dequantization, profiling experiments.
- Python: reproducibility scripts, small reference checks, plotting, benchmark orchestration, and reports.

The current `mini_llm/` Python package is a dependency-light reference and bootstrap layer. The production learning path should not stop there; bottom-up implementation starts in `native_core/` and later CUDA milestones will extend the same native build.

Product integrations are adapter/profile/report surfaces. InferLab may inspect, configure, benchmark, and explain mature products, but it does not reimplement their production schedulers, KV connectors, distributed control planes, storage engines, or kernel libraries.

## Current Scope

This first scaffold covers M0 and the start of M1:

- unified result schema and example benchmark config
- environment collection script
- output validation script
- basic no-dependency plotting/report script
- KV cache shape and memory estimator
- shape probe CLI for prefill/decode
- C++17 native KV-cache estimator and shape trace CLI
- C++17 Paged KV Cache Manager with block table allocation, append, release, reuse, and utilization stats
- C++17 Paged KV block-table gather helper for attention K/V reads
- C++17 Paged KV fragmentation/churn probe for release/reuse behavior
- C++17 Paged KV block-table locality/read amplification probe
- C++17 toy continuous batching scheduler backed by the paged cache
- C++17 per-group INT8/INT4 weight-only quantization toy module
- C++17 CPU GEMM/GEMV, softmax, causal attention, RMSNorm/LayerNorm, RoPE
- C++17 minimal decoder block wiring RMSNorm, QKV/O matmul, RoPE, causal attention, SwiGLU FFN, and residuals
- C++17 decoder prefill/decode reference with K/V cache values and Paged KV metadata trace
- C++17 decoder KV decode path that gathers K/V through Paged KV block-table metadata
- PyTorch Mini Transformer reference with RMSNorm, RoPE, causal attention, SwiGLU, KV cache, and prefill/decode JSON traces
- Mini Transformer KV-cache-vs-recompute comparison CLI and benchmark-summary metrics
- Mini Transformer static workload profile for FLOPs, activation bytes, attention score elements, and persistent KV bytes
- Mini Transformer roofline-style bottleneck advisor for compute/memory limiter ranking and optimization guidance
- Mini Transformer scenario sweep for short-chat, decode-heavy, prefill-heavy, and long-context optimization planning
- Mini Transformer Markdown report renderer for cache/workload/bottleneck/scenario planning outputs
- Mini Transformer CSV/JSON table exporter for downstream charts, notebooks, and reports
- dependency-free speculative decoding cost model for draft/target acceptance rate, verify-window, TPOT, fallback cost, and speedup planning
- vLLM serving benchmark harness with GPU/dependency preflight and OpenAI-compatible endpoint timing mode
- GGUF legal-fixture comparison harness for user-provided real model fixtures and expected probe fields
- GGUF IQ2_XXS reference dequant helper with upstream GGML sign/grid tables, 256-value reconstruction, group sums, and probe/summary reporting
- llama.cpp edge benchmark harness with binary/model preflight, GGUF prompt/eval timing log parsing, and benchmark-summary ingestion
- TensorRT-LLM preflight/benchmark harness with TensorRT/CUDA/tool checks, configurable command mode, best-effort `trtllm-bench` invocation, and benchmark-summary ingestion
- Docker runtime scaffold with CPU dev image, CUDA/vLLM image, compose files, env/plan generator, and serving notes
- Final-report renderer that assembles benchmark summaries, module coverage, source notes, missing evidence, cost records, and bottleneck migration analysis
- Source-trace notes and index generator for Transformers, vLLM request lifecycle, vLLM PagedAttention, TensorRT-LLM engine/runtime, and llama.cpp/ggml
- Prometheus textfile metrics exporter plus Prometheus/Grafana monitoring assets for benchmark and serving observability
- Android/ARM deployment scaffold plus a pinned arm64-v8a/API-24 AAR module over the canonical JNI edge-runtime target, caller-owned async lifecycle, consumer rules, license/ABI packaging gates, llama.cpp runner notes, deployment-plan generator, source trace notes, and edge report template
- optional CUDA kernel skeleton guarded by `INFERLAB_ENABLE_CUDA`
- optional CUDA vector-add runtime wrapper and CUDA-only correctness target guarded by `INFERLAB_ENABLE_CUDA`
- optional CUDA naive matvec/matmul/row-softmax/RMSNorm skeletons guarded by `INFERLAB_ENABLE_CUDA`
- optional CUDA matvec/matmul/row-softmax runtime profiling APIs with CUDA event elapsed time
- optional CUDA shared-reduction GEMV, tiled GEMM, block row-softmax, and denominator-shared attention variants with profiling APIs
- optional CUDA naive RoPE/attention and INT4 dequant+matvec skeletons guarded by `INFERLAB_ENABLE_CUDA`
- optional CUDA RMSNorm/RoPE/attention/INT4/Paged-KV runtime profiling APIs with CUDA event elapsed time
- CUDA availability preflight script that emits `results/raw/cuda_environment.json`
- native probe runner that refreshes C++ probe JSON files when `build/` exists
- benchmark summary CLI that aggregates raw probe JSON into Markdown/JSON
- smoke tests that run without PyTorch or CUDA

The PyTorch CPU reference path is now available for model-level traceability.
Live CUDA execution, live vLLM serving runs, TensorRT-LLM engine benchmarks,
and llama.cpp model runs remain later milestones for machines with the required
GPU/runtime environment.

## Quick Start

```bash
python3 -m mini_llm.shape_probe --batch-size 1 --seq-len 128 --decode-steps 16
python3 scripts/collect_system_info.py --output results/raw/system_info.json
python3 scripts/check_cuda_environment.py --output results/raw/cuda_environment.json
python3 scripts/validate_outputs.py --results results/raw
python3 -m unittest discover -s tests -p "test_*.py"
```

Run the optional PyTorch Mini Transformer reference from the project venv:

```bash
source .venv/bin/activate
python3 -m mini_llm.generate 1 2 3 --max-new-tokens 4 --output results/raw/mini_transformer_generate.json
python3 -m mini_llm.cache_compare 1 2 3 --max-new-tokens 4 --output results/raw/mini_transformer_cache_compare.json
python3 -m mini_llm.workload_profile --prompt-tokens 128 --max-new-tokens 16 --output results/raw/mini_transformer_workload_profile.json
python3 -m mini_llm.bottleneck_advisor --prompt-tokens 128 --max-new-tokens 16 --peak-tflops 10 --bandwidth-gbps 300 --output results/raw/mini_transformer_bottleneck_advisor.json
python3 -m mini_llm.scenario_sweep --output results/raw/mini_transformer_scenario_sweep.json
python3 scripts/render_mini_transformer_report.py --results results/raw --output results/tables/mini_transformer_report.md
python3 scripts/export_mini_transformer_tables.py --results results/raw --output-dir results/tables
./build/inferlab_speculative_decode_plan --repo-root . --output results/raw/native_speculative_decode_plan.json
python3 scripts/run_vllm_benchmark.py --output results/raw/vllm_serving_benchmark.json
python3 scripts/run_llama_cpp_benchmark.py --output results/raw/llama_cpp_benchmark.json
python3 scripts/run_trtllm_benchmark.py --output results/raw/trtllm_benchmark.json
python3 scripts/export_prometheus_metrics.py --results results/raw --output results/tables/inferlab_metrics.prom
python3 scripts/plan_runtime_deploy.py --output reports/runtime_deploy_plan.md --json-output results/raw/runtime_deploy_plan.json
python3 scripts/plan_android_deploy.py --output reports/android_deploy_plan.md --json-output results/raw/android_deploy_plan.json
python3 scripts/render_final_report.py --output reports/final_report.md --json-output results/raw/final_report_index.json
python3 scripts/render_source_trace_index.py --output reports/source_trace_index.md --json-output results/raw/source_trace_index.json
python3 scripts/compare_gguf_fixture.py --fixture /path/to/legal/model.gguf --output results/raw/gguf_fixture_comparison.json
```

Build and test the C++ native core:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/inferlab_shape_probe --output results/raw/native_shape_probe.json
./build/inferlab_kv_fragmentation_probe --output results/raw/kv_fragmentation_probe.json
./build/inferlab_paged_kv_locality_probe --output results/raw/paged_kv_locality_probe.json
./build/inferlab_decoder_probe --output results/raw/decoder_probe.json
./build/inferlab_decoder_kv_probe --output results/raw/decoder_kv_probe.json
python3 scripts/run_native_probes.py --build-dir build --results results/raw
python3 scripts/summarize_benchmarks.py --results results/raw
```

Optional embedded ARM Linux runtime adapters are disabled by default. They use
existing, version-pinned third-party source/SDK directories and never download
those dependencies during InferLab configuration. Source worktrees must be
clean, and their current HEAD must equal the declared pin after both are
resolved to complete commits:

```bash
git -C /path/to/executorch-0b0e2c5 rev-parse HEAD \
  > /path/to/executorch-v1.2.0-install/source-revision.txt
cmake -B build-edge-linux-aarch64 \
  -DINFERLAB_ENABLE_EDGE_LLAMA_CPP=ON \
  -DINFERLAB_LLAMA_CPP_SOURCE_DIR=/path/to/llama.cpp-e9fa078 \
  -DINFERLAB_ENABLE_EDGE_EXECUTORCH=ON \
  -DINFERLAB_EXECUTORCH_SOURCE_DIR=/path/to/executorch-0b0e2c5 \
  -DINFERLAB_EXECUTORCH_INSTALL_DIR=/path/to/executorch-v1.2.0-install \
  -DINFERLAB_EXECUTORCH_BUILD_REVISION_FILE=/path/to/executorch-v1.2.0-install/source-revision.txt \
  -DINFERLAB_ENABLE_ARM_NEON=ON
cmake --build build-edge-linux-aarch64 --target inferlab_edge_runtime_matrix
./build-edge-linux-aarch64/inferlab_edge_runtime_matrix \
  --repo-root . \
  --config configs/edge_runtime_matrix.example.json \
  --mode execute \
  --output results/raw/native_edge_runtime_matrix.json
```

When configuring Linux AArch64 from a non-AArch64 host, add an explicit
`-DCMAKE_TOOLCHAIN_FILE=/path/to/linux-aarch64-toolchain.cmake`. Enabling NEON
alone does not select an AArch64 cross compiler.

The current embedded contract carries one prompt per request, so every runtime
row uses `batch_size: 1`. That field is request cardinality, not llama.cpp's
token submission capacity; the adapter derives and bounds `n_batch/n_ubatch`
from the pinned runtime defaults. The registry retains cross-target product
capabilities for planning. A target-architecture projection validates each plan
row, while the current-build projection reuses the same filter for execute. An
x86 host can therefore plan the example AArch64 row, an x86 target plan cannot
accept `cpu-neon`, and only an AArch64 binary can execute that backend.

The ExecuTorch SDK must export its LLM Runner and XNNPACK targets. Its revision
file must contain the complete source commit used for that SDK build and must
match the verified source checkout. This local metadata catches accidental SDK
drift; it is not a cryptographic supply-chain attestation. A requested option,
missing dependency, plan row, or null metric is not execute evidence; see
`reports/112_edge_runtime_matrix.md` for the exact claim boundary.
The matrix also emits checked model/KV/workspace/reserve budget deltas and
per-session thread/mmap guidance. Buffered model residency and per-session
workspace are scaled by concurrency; supported requested mmap remains an
analytical shareable-residency estimate. It cannot select a runtime in plan mode; a
runtime/backend/model-format performance candidate requires complete,
identity-aligned execute observations for every configured row.
Cross-runtime ranking also requires comparable observed thread-control modes;
runtime-managed thread counts remain an explicit alignment gap.

Optional request-before-execution edge/cloud routing is exposed through one
C++17 target. It reuses the embedded runtime lifecycle and mature product
identity registry. libcurl is system-provided, disabled by default, and never
downloaded by InferLab:

```bash
cmake -B build-edge-cloud -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_EDGE_CLOUD_CURL=ON
cmake --build build-edge-cloud --target inferlab_edge_cloud_routing
./build-edge-cloud/inferlab_edge_cloud_routing \
  --repo-root . \
  --config configs/edge_cloud_routing.example.json \
  --mode plan \
  --output results/raw/native_edge_cloud_routing.json \
  --format json
```

`execute` mode requires a matching target platform plus an eligible local
runtime/model or authorized OpenAI-compatible endpoint. `device_only` requests
never use remote transport, and local-to-cloud fallback is permitted only when
the local attempt delivered zero callbacks and the request is still eligible.
SSE deltas are recorded as text chunks, not tokenizer tokens. Prompt text,
streamed text, API key values, and raw endpoints are excluded from the artifact;
the same target can apply a fresh caller-supplied RTT/jitter/loss/downlink
quality gate and a caller-estimated battery/local-energy/cloud-cost budget
gate, then re-age both snapshots before local-to-cloud fallback. The same
request contract can admit pre-generation recovery from a caller-reconstructed
durable transcript after a completed turn or an incomplete zero-output request;
partial-output resume and KV/hidden-state restoration remain forbidden. It does
not actively probe the network, persist transcripts, read power sensors,
recompute serving cost, ingest billing records, or claim learned prediction; see
`reports/113_edge_cloud_routing.md` for the exact claim boundary.

The schema-v4 example declares a bounded serial benchmark protocol:
one cold probe, warmup iterations, minimum measurement iterations, minimum
sustained duration, a maximum-iteration guard, and a per-generation cooperative
deadline. Older v1-v3 inputs use a 300000 ms deadline default. Deadline expiry
requests canonical cancellation and blocks execute evidence; it cannot hard-kill
an unresponsive in-process third-party call. Measurement latency and
throughput use medians only after sample identity and metric availability
reconcile; this remains single-session evidence. When Linux powercap root-zone
counters and `max_power_range_uw` bounds are available, execute mode also records a checked `energy_uj` delta
from immediately before runtime load through unload. Parent/child zones are not
double-counted, and ambiguous wrap, reset, inventory drift or malformed sysfs
input makes the observation unavailable. The delta is whole-system interval
evidence, not process/model-exclusive energy, instantaneous power, battery
energy or a runtime ranking input.
`concurrent_sessions` additionally drives a bounded independent-runtime probe:
each session is normalized, released through one C++17 start gate, and
reconciled against the serial primary. Multi-session ranking uses concurrent
median TPOT and aggregate generated-token throughput; it does not claim
continuous batching or shared-context scheduling.

Run the small bootstrap pipeline:

```bash
bash scripts/run_all_benchmarks.sh
```

The pipeline writes `results/raw/native_probe_run.json`,
`results/raw/benchmark_summary.json`, and
`results/tables/benchmark_summary.md`. The native probe runner skips missing
executables, so the Python smoke path remains usable before the C++ build
exists, while a built tree refreshes all native probe outputs. It also records
per-probe wall-clock elapsed time so later reports can separate orchestration
costs from CUDA kernel event timing.

## Repository Layout

```text
configs/       benchmark matrix and fixed eval prompts
android/       Android NDK deployment scaffold and canonical edge-runtime AAR module
deploy/        Docker Compose runtime and observability entrypoints
docker/        CPU dev and CUDA/vLLM image definitions
mini_llm/      M1 inference and KV-cache experiments
native_core/   C++17 core for KV layout, shape tracing, and future schedulers
cuda_kernels/  optional CUDA kernel experiments, disabled by default
results/       generated JSONL/CSV data, figures, and tables
source_notes/  source-reading notes and runtime call-chain traces
scripts/       reproducibility, validation, plotting, and benchmark entrypoints
serving/       vLLM and TensorRT-LLM runtime notes
monitoring/    Prometheus textfile and Grafana dashboard assets
tests/         dependency-light unit tests
```

## Verification Policy

Performance claims are not accepted until correctness is checked first. For every benchmark, record:

- model/backend/precision
- prompt and output token counts
- hardware and software versions
- latency, TTFT, TPOT, throughput, and memory where applicable
- command used to reproduce the result
