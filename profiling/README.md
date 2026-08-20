# InferLab Profiling

This directory holds profiler command plans and collected runtime artifacts. The
current CPU server can generate runbooks, but CUDA/Nsight/vLLM/TensorRT-LLM,
llama.cpp model, and Android/simpleperf evidence should be collected on capable
machines.

Generate the full runbook:

```bash
python3 scripts/plan_profiling_runs.py \
  --output reports/profiling_plan.md \
  --json-output results/raw/profiling_plan.json
```

Expected artifact families:

- `profiling/nsight_compute/*.ncu-rep`: kernel-level CUDA metrics.
- `profiling/vllm/*.nsys-rep` and `profiling/vllm/prometheus_metrics.txt`: serving timeline and counters.
- `profiling/trtllm/*.nsys-rep`: TensorRT-LLM engine/runtime timeline.
- `profiling/llama_cpp/perf_stat.txt`: CPU/edge counters.
