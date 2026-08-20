# Runtime Profiling Plan

InferLab separates implementation from environment-specific measurement. The
repository now includes command generators for the measurements that cannot be
truthfully collected on a CPU-only server.

## Main Entry

```bash
python3 scripts/plan_profiling_runs.py \
  --repo-root . \
  --output reports/profiling_plan.md \
  --json-output results/raw/profiling_plan.json
```

## Coverage

- CUDA kernels: configure `INFERLAB_ENABLE_CUDA=ON`, run CUDA correctness/timing
  targets, and collect Nsight Compute reports.
- vLLM: start an OpenAI-compatible server, run endpoint benchmarks, collect
  Prometheus metrics, and optionally wrap the benchmark in Nsight Systems.
- TensorRT-LLM: run benchmark/preflight against a built engine and collect
  runtime timeline evidence.
- llama.cpp: run GGUF prompt/eval timing and collect `perf stat` counters.
- Android/ARM: regenerate deployment commands and collect simpleperf/thermal
  context on device.

After collecting evidence, refresh benchmark summary, Prometheus textfile
metrics, and the final report.
