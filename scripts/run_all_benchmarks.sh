#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p results/raw results/tables results/figures

python3 scripts/collect_system_info.py --output results/raw/system_info.json
python3 scripts/check_cuda_environment.py --output results/raw/cuda_environment.json
python3 scripts/run_llama_cpp_benchmark.py --output results/raw/llama_cpp_benchmark.json
python3 scripts/run_trtllm_benchmark.py --output results/raw/trtllm_benchmark.json
python3 -m mini_llm.shape_probe \
  --batch-size 1 \
  --seq-len 128 \
  --decode-steps 16 \
  --hidden-size 768 \
  --num-layers 12 \
  --num-heads 12 \
  --head-dim 64 \
  --dtype fp16 \
  --output results/raw/shape_probe.json
python3 scripts/run_native_probes.py \
  --build-dir build \
  --results results/raw \
  --output results/raw/native_probe_run.json
python3 scripts/validate_outputs.py --results results/raw
python3 scripts/summarize_benchmarks.py \
  --results results/raw \
  --output results/tables/benchmark_summary.md \
  --json-output results/raw/benchmark_summary.json
python3 scripts/export_prometheus_metrics.py \
  --results results/raw \
  --output results/tables/inferlab_metrics.prom
python3 scripts/validate_outputs.py --results results/raw
python3 scripts/plot_results.py --results results/raw --output results/tables/result_index.md
python3 -m unittest discover -s tests -p "test_*.py"
