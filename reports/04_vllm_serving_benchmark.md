# vLLM Serving Benchmark

The vLLM module is implemented as a portable harness plus source trace notes:

- `serving/vllm_server.md` documents serving boundaries and OpenAI-compatible API usage.
- `scripts/run_vllm_benchmark.py` performs GPU/dependency preflight and can benchmark an existing endpoint with `--base-url`.
- `benchmarks/bench_vllm_offline.py` and `benchmarks/bench_vllm_online.py` preserve the original project-plan paths and delegate to the shared harness.
- `source_notes/vllm_request_lifecycle.md` and `source_notes/vllm_paged_attention_trace.md` record scheduler/KV-cache call-chain questions.
- `monitoring/prometheus.yml` and `monitoring/grafana_dashboard.json` provide observability scaffolding.

On this CPU-only server, the benchmark records readiness and missing GPU/runtime evidence rather than claiming throughput. Real TTFT, TPOT, P50/P99, and tokens/s should be appended after running against a live vLLM service.
