# llama.cpp and ggml Trace

## Goal

Trace edge inference through llama.cpp and ggml: GGUF loading, tensor mapping,
graph build, backend dispatch, sampling, prompt eval, decode eval, and optional
partial offload.

## Entry Points To Verify

Verify exact source paths against the llama.cpp checkout used on the target
machine.

```text
main CLI entry
llama_backend_init
llama_model_load_from_file
GGUF metadata and tensor loading
llama_new_context_with_model
llama_tokenize
llama_decode
ggml graph construction
backend dispatch / CPU kernels / GPU offload
sampler accept and sample
llama_perf_context_print
```

## Runtime Path

```text
llama-cli arguments
  backend init
  GGUF file open and metadata parse
  tensor map or load
  context allocation
  tokenize prompt
  prompt eval through llama_decode
  build ggml graph
  dispatch graph to selected backend
  sample next token
  repeated decode steps
  performance summary printed
```

## Evidence To Attach

- llama.cpp commit, compiler, backend flags, CPU model, thread count
- model file, quantization type, file size, mmap setting
- prompt eval time, prompt tokens/s, decode eval time, decode tokens/s
- RSS or memory footprint if available
- perf counters: cycles, instructions, cache misses, page faults, context
  switches
- offload settings such as GPU layers when relevant

## Bottleneck Questions

- Is prompt eval compute-bound while decode is memory-bandwidth-bound?
- Does mmap reduce load time or only change memory pressure?
- Does a lower-bit quantization reduce bandwidth enough to offset dequant work?
- Does partial offload help or lose to transfer/synchronization overhead?
- Does thread count improve throughput or cause cache contention?

## InferLab Link

- `scripts/run_llama_cpp_benchmark.py` records binary/model preflight and parses
  prompt/eval timing logs.
- GGUF parser reports tensor metadata, offsets, types, and dequant previews.
- Android deployment notes extend this trace to `arm64-v8a` and simpleperf.
