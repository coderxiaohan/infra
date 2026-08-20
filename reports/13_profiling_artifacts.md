# Profiling Artifact Import

After running CUDA/Nsight, vLLM, TensorRT-LLM, llama.cpp, perf, or Android
simpleperf commands on capable machines, import their artifacts with:

```bash
python3 scripts/parse_profiling_artifacts.py \
  --repo-root . \
  --artifact-root profiling \
  --output results/raw/profiling_artifacts.json \
  --report reports/profiling_artifacts.md
```

The importer records binary profiler artifacts by path and size, and extracts
portable metrics from text logs, Prometheus snapshots, `perf stat` output,
llama.cpp timing logs, and CSV exports.

For detailed Nsight Compute metrics, export a CSV next to the `.ncu-rep` file;
for Nsight Systems, keep the `.nsys-rep` plus any CLI summary text. The final
report can then reference a normalized JSON source without depending on local
GUI tools.
