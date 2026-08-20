# Long Context Matrix

This report path is kept for the P0 long-context and chunked-prefill item in `深层次开发.md`.

| Item | Value |
|---|---|
| Core implementation | `native_core/src/long_context_plan_main.cpp` |
| C++ target | `inferlab_long_context_plan` |
| Native artifact | `results/raw/native_long_context_matrix.json` |
| Aggregated report | `reports/16_advanced_serving_research.md` |

Acceptance coverage: 2k/8k/32k/128k prompt plans, chunk size sweep, prefix cache interaction, KV cache MB, first chunk latency, TTFT, P99 and short-request side effects.
