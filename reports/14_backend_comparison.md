# Backend Comparison

InferLab compares cloud and edge inference backends through generated artifacts
rather than hand-written claims.

Generate the comparison after collecting backend runs:

```bash
python3 scripts/compare_backends.py \
  --repo-root . \
  --results results/raw \
  --cost-records configs/cost_records.json \
  --output results/raw/backend_comparison.json \
  --report reports/backend_comparison.md
```

The comparison combines:

- vLLM endpoint latency/throughput and serving readiness.
- TensorRT-LLM engine benchmark/preflight metrics.
- llama.cpp prompt/eval timing for GGUF edge runtime.
- Native probe readiness and CPU/CUDA reference signals.
- Optional quality evaluation results.
- Optional profiling artifact imports.
- Optional cloud cost records.

Use it as the final trade-off table once real hardware-backed evidence exists.
