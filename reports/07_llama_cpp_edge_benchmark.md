# llama.cpp Edge Benchmark

The edge module contains:

- `edge/llama_cpp_build.md`: build and backend command boundaries.
- `edge/run_gguf.sh`: thin runner around `llama-cli`.
- `benchmarks/bench_llama_cpp.py`: compatibility wrapper for `scripts/run_llama_cpp_benchmark.py`.
- `source_notes/llama_cpp_ggml_trace.md`: model load, graph build, backend dispatch, mmap, KV cache, and sampler trace notes.

On a prepared edge machine, collect prompt eval time, eval time, threads, context length, backend, `-ngl`, memory, and perf/simpleperf counters. The current server can still emit a preflight JSON and parse saved llama.cpp logs.
