# vLLM vs TensorRT-LLM

| Dimension | vLLM | TensorRT-LLM |
|---|---|---|
| Primary optimization | PagedAttention, continuous batching, serving ergonomics | Engine build, fused kernels, plugins, CUDA Graph, in-flight batching |
| Best fit | Dynamic online serving and fast model iteration | Stable high-throughput deployment with prepared engines |
| Setup cost | Lower; Python service command once dependencies and model are present | Higher; build engine, choose precision/plugins, validate profile bounds |
| Evidence path | `scripts/run_vllm_benchmark.py`, Prometheus, vLLM logs | `scripts/run_trtllm_benchmark.py`, build logs, `trtllm-bench` |

Current repository status: both backends have portable preflight/benchmark harnesses and source-trace notes. This CPU-only host cannot supply real GPU throughput comparisons, so `scripts/render_final_report.py` records that evidence as deferred until a CUDA/TensorRT-LLM machine is available.
