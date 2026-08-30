#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

RESULTS_DIR="${INFERLAB_RESULTS_DIR:-results/raw}"
mkdir -p "$RESULTS_DIR"

python3 scripts/collect_system_info.py --output "$RESULTS_DIR/system_info.json"
python3 scripts/check_cuda_environment.py --output "$RESULTS_DIR/cuda_environment.json"
python3 scripts/run_llama_cpp_benchmark.py --allow-non-ok-status --output "$RESULTS_DIR/llama_cpp_benchmark.json"
python3 scripts/run_trtllm_benchmark.py --allow-non-ok-status --output "$RESULTS_DIR/trtllm_benchmark.json"
python3 -m mini_llm.shape_probe \
  --batch-size 1 \
  --seq-len 128 \
  --decode-steps 16 \
  --hidden-size 768 \
  --num-layers 12 \
  --num-heads 12 \
  --head-dim 64 \
  --dtype fp16 \
  --output "$RESULTS_DIR/shape_probe.json"
python3 scripts/run_native_probes.py \
  --build-dir build \
  --results "$RESULTS_DIR" \
  --output "$RESULTS_DIR/native_probe_run.json"
python3 scripts/validate_outputs.py --allow-non-ok-status --results "$RESULTS_DIR"
python3 scripts/summarize_benchmarks.py \
  --results "$RESULTS_DIR" \
  --output "$RESULTS_DIR/benchmark_summary.json" \
  --allow-non-ok-status
python3 scripts/validate_outputs.py \
  --results "$RESULTS_DIR" \
  --allow-status llama_cpp_benchmark.json=skipped \
  --allow-status trtllm_benchmark.json=skipped \
  --allow-status benchmark_summary.json=incomplete
python3 -m unittest discover -s tests -p "test_*.py"
