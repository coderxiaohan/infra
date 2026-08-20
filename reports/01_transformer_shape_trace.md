# Mini Transformer Shape Trace

This report is the module-1 handoff for the Mini LLM inference kernel. The runnable artifacts are:

- `mini_llm/transformer.py`: PyTorch decoder-only reference with RMSNorm, RoPE, causal attention, SwiGLU, and KV cache support.
- `mini_llm/generate.py`: prefill/decode trace generator.
- `mini_llm/cache_compare.py`: cached decode versus full-prefix recompute comparison.
- `mini_llm/workload_profile.py`: FLOP/byte/KV-cache estimator.
- `mini_llm/bottleneck_advisor.py`: roofline-style bottleneck ranking.
- `mini_llm/scenario_sweep.py`: short-chat, balanced, decode-heavy, prefill-heavy, and long-context scenario sweep.

## Shape Contract

The project uses `[batch, heads, sequence, head_dim]` for KV cache tensors. Prefill writes the full prompt K/V once per layer. Decode appends one token per request step and reuses prior K/V blocks. Attention scores have shape `[batch, heads, query_tokens, key_tokens]`; for decode, `query_tokens` is normally `1`, so the dominant read is cached K/V history rather than a full quadratic prefill matrix.

## Reproducible Commands

```bash
python3 -m mini_llm.generate 1 2 3 --max-new-tokens 4 --output results/raw/mini_transformer_generate.json
python3 -m mini_llm.cache_compare 1 2 3 --max-new-tokens 4 --output results/raw/mini_transformer_cache_compare.json
python3 -m mini_llm.workload_profile --prompt-tokens 128 --max-new-tokens 16 --output results/raw/mini_transformer_workload_profile.json
python3 -m mini_llm.bottleneck_advisor --prompt-tokens 128 --max-new-tokens 16 --peak-tflops 10 --bandwidth-gbps 300 --output results/raw/mini_transformer_bottleneck_advisor.json
python3 -m mini_llm.scenario_sweep --output results/raw/mini_transformer_scenario_sweep.json
python3 scripts/render_mini_transformer_report.py --repo-root . --results results/raw --output results/tables/mini_transformer_report.md
python3 scripts/export_mini_transformer_tables.py --repo-root . --results results/raw --output-dir results/tables
```

## Bottleneck Reading

KV cache removes repeated projection and attention-score work during decode, but it converts the decode path into a memory-resident workload: every new token reads historical K/V. That is why the later CUDA, Paged KV, and vLLM modules focus on memory layout, block tables, batching, and reducing HBM traffic.
