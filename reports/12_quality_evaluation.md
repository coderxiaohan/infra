# Quality Evaluation

InferLab keeps performance optimization tied to correctness and output quality.
The lightweight quality evaluator is designed for small fixed prompt sets and
cross-backend output comparison after real vLLM, TensorRT-LLM, or llama.cpp runs.

## Command

```bash
python3 scripts/evaluate_backend_quality.py \
  --repo-root . \
  --results results/raw \
  --prompt-file configs/eval_prompts.jsonl \
  --checks configs/quality_checks.example.json \
  --output results/raw/backend_quality_eval.json \
  --report reports/quality_evaluation.md
```

## What It Records

- Per-backend request status, text availability, token counts, latency when present.
- Optional `expected_contains`, `expected_regex`, `numeric_answer`, and `reference_answer` checks.
- Pairwise token-Jaccard and length deltas for matching prompt IDs across backends.
- Explicit unchecked rows when a backend only emitted preflight data.

This is intentionally a small-sample guardrail. It should not be presented as a
full benchmark-suite accuracy result unless paired with a larger dataset and
task-specific metrics.
