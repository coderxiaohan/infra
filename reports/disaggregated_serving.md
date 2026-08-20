# Disaggregated Prefill/Decode Serving

This report path is kept for the P0 disaggregated-serving item in `深层次开发.md`.

| Item | Value |
|---|---|
| Core implementation | `native_core/src/disaggregated_serving_main.cpp` |
| C++ target | `inferlab_disaggregated_serving` |
| Native artifact | `results/raw/native_disaggregated_serving.json` |
| Aggregated report | `reports/16_advanced_serving_research.md` |

Acceptance coverage: aggregated baseline, prefill/decode worker ratios, KV transfer cost, TTFT, TPOT, worker utilization, and failure cases when transfer overhead dominates.
