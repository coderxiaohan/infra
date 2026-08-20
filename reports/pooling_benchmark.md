# Pooling / Embedding / Rerank Benchmark

This report tracks the retained endpoint-backed Pooling / Embedding / Rerank runner. The duplicate native analytical plans and synthetic RAG cache estimator have been retired.

| Item | Value |
|---|---|
| Execution glue | `advanced_serving/pooling_benchmark.py` |
| Runtime path | OpenAI-compatible `/embeddings` or `/rerank` endpoint with `--execute` |
| Config | `configs/pooling_benchmark.example.json`, `configs/rag_pairs.example.jsonl` |
| Artifact | `results/raw/pooling_benchmark.json` |
| Aggregated report | `reports/16_advanced_serving_research.md` |

The runner records endpoint response status. Recall@k, nDCG, latency, throughput, and quality conclusions require labeled datasets and target-product execute evidence; planned commands are not benchmark evidence.
