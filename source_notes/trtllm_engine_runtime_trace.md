# TensorRT-LLM Engine Runtime Trace

## Goal

Explain the compiled-engine path and compare it with vLLM's dynamic scheduling
path. TensorRT-LLM work should be validated on a CUDA machine with a specific
TensorRT-LLM version and engine build log.

## Entry Points To Verify

Verify exact filenames, command names, and class names against the installed
TensorRT-LLM checkout.

```text
checkpoint conversion
engine build configuration
network / builder / plugin selection
serialized engine artifacts
runtime session or executor
in-flight batching
KV cache manager
serving or benchmark command
```

## Runtime Path

```text
model checkpoint
  conversion or preprocessing
  engine build config
  TensorRT network and plugins
  serialized engine plan
  runtime session loads engine
  request batch enters executor
  prefill/decode kernels execute
  outputs and metrics return to benchmark harness
```

## Evidence To Attach

- TensorRT-LLM version, TensorRT version, CUDA version, driver version
- model, precision, max batch, max input length, max output length
- tensor parallel size and plugin flags
- engine build command, build time, log, and artifact size
- benchmark command, latency, throughput, and GPU memory
- whether CUDA Graph, fused plugins, or in-flight batching were enabled

## Comparison With vLLM

| Dimension | TensorRT-LLM | vLLM |
|---|---|---|
| Optimization style | compiled engine and plugins | dynamic runtime scheduling |
| Startup cost | engine build and load time | model load and cache init |
| Shape flexibility | constrained by engine config | more dynamic request shapes |
| Scheduling focus | executor and in-flight batching | continuous batching and block manager |
| Best evidence | build log plus runtime metrics | request timeline plus scheduler metrics |

## Bottleneck Questions

- Does engine build time make sense for the expected deployment lifetime?
- Does compiled execution improve steady-state decode throughput?
- Does the engine config overfit a narrow sequence/batch shape?
- Is operational complexity higher than the vLLM baseline for the same model?

## InferLab Link

- `scripts/run_trtllm_benchmark.py` records preflight, exact command mode, and
  best-effort `trtllm-bench` mode.
- `serving/trtllm_build.md` records deployment command boundaries.
- `reports/final_report.md` should compare TensorRT-LLM with vLLM only after
  both have target-machine evidence.
