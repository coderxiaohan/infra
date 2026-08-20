# InferLab Source Trace Index

- Generated: `2026-06-15T02:23:20Z`

## Coverage

| Trace | Framework | Note | Exists | Evidence to attach |
|---|---|---|---:|---|
| Transformers forward | transformers | `source_notes/transformers_forward_trace.md` | True | model config and dtype, prefill/decode tensor shapes, KV cache length and memory |
| vLLM request lifecycle | vllm | `source_notes/vllm_request_lifecycle.md` | True | request arrival time, queue wait, TTFT, TPOT, stream chunk timeline |
| vLLM PagedAttention | vllm | `source_notes/vllm_paged_attention_trace.md` | True | block allocation snapshots, cache utilization, fragmentation or eviction events |
| TensorRT-LLM engine runtime | TensorRT-LLM | `source_notes/trtllm_engine_runtime_trace.md` | True | engine build log, engine artifact size, runtime latency and throughput, CUDA Graph or plugin settings |
| llama.cpp and ggml | llama.cpp | `source_notes/llama_cpp_ggml_trace.md` | True | prompt eval time, decode eval time, thread count, mmap and backend selection, perf counters |

## Verification Commands

```bash
python3 scripts/render_source_trace_index.py --output reports/source_trace_index.md --json-output results/raw/source_trace_index.json
grep -RIn "AsyncLLMEngine\|Scheduler\|block manager" /path/to/vllm
grep -RIn "runtime session\|in-flight\|executor" /path/to/TensorRT-LLM
grep -RIn "llama_decode\|llama_model_load_from_file\|ggml_graph" /path/to/llama.cpp
```
