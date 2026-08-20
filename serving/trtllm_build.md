# TensorRT-LLM Runtime Notes

TensorRT-LLM is the compiled-engine comparison path for InferLab. The current
repository records preflight and benchmark command contracts; actual engine
build and serving execution should happen on a CUDA/TensorRT-LLM machine.

## Build Boundary

Keep these stages separate in reports:

1. Model conversion or checkpoint preparation.
2. Engine build configuration: precision, max batch, max input length, max
   output length, tensor parallelism, and plugin flags.
3. Engine build time and artifact size.
4. Runtime command or serving command.
5. Benchmark command and parsed latency/throughput metrics.

## InferLab Harness

Use the portable benchmark harness when a target command is known:

```bash
python3 scripts/run_trtllm_benchmark.py \
  --mode command \
  --command "trtllm-bench --engine_dir /engines/model --batch_size 1 --input_output_len 128,32" \
  --model model \
  --output results/raw/trtllm_benchmark.json
```

If the installed TensorRT-LLM version supports the expected `trtllm-bench`
flags, use:

```bash
python3 scripts/run_trtllm_benchmark.py \
  --mode bench \
  --engine-dir /engines/model \
  --batch-size 1 \
  --input-tokens 128 \
  --output-tokens 32
```

## Comparison Questions

- How much time is paid during engine build before serving starts?
- Does the compiled engine improve steady-state decode throughput for the fixed
  prompt/output mix?
- Does TensorRT-LLM lose flexibility for dynamic shapes or model variants
  compared with vLLM?
- Which backend is easier to operate under the same monitoring and benchmark
  harness?
