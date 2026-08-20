# Runtime Evidence Checklist

- Generated: `2026-08-09T11:18:01Z`
- Status: `open`
- Complete: `2/12`

| Module | Artifact | Status | Complete | Next action |
|---|---|---|---:|---|
| Mini Transformer | `mini_transformer_cache_compare.json` | `present` | True | done |
| CUDA kernels | `cuda_profile_matrix.json` | `missing` | False | run on target environment |
| KV/PagedAttention | `scheduler_trace.json` | `present` | True | done |
| vLLM serving | `vllm_serving_benchmark.json` | `skipped` | False | run on target environment |
| TensorRT-LLM | `trtllm_benchmark.json` | `skipped` | False | run on target environment |
| Quantization | `backend_quality_eval.json` | `missing` | False | run on target environment |
| llama.cpp edge | `llama_cpp_matrix.json` | `missing` | False | run on target environment |
| GGUF parser | `gguf_fixture_comparison.json` | `missing` | False | run on target environment |
| Android/ARM | `android_edge_benchmark.json` | `missing` | False | run on target environment |
| Monitoring | `profiling_artifacts.json` | `missing` | False | run on target environment |
| Source traces | `source_trace_verification.json` | `planned` | False | configure pinned upstream checkouts and rerun source verification |
| Backend comparison | `backend_comparison.json` | `missing` | False | refresh aggregate report |
