# Structured Outputs

InferLab keeps one product-facing structured-output runner instead of a second
compiled analytical producer.

| Item | Value |
|---|---|
| Runner | `advanced_serving/structured_outputs.py` |
| Config | `configs/structured_outputs.example.json` |
| Artifact | `results/raw/structured_outputs.json` |
| Endpoint | OpenAI-compatible `/chat/completions` |

The default mode emits a request plan. With explicit `--execute`, the runner
contacts the configured endpoint and records the returned product evidence.
Planned requests, HTTP success, schema validity, latency, and throughput remain
separate claims; performance values may only be reported from an executed raw
artifact.

The retired native structured-output plan only estimated validity and latency
from compiled constants. Grammar/backend compatibility remains represented by
the mature-product P2 compatibility artifacts, while real constrained-decoding
behavior must come from an authorized product endpoint.

```bash
cd InferLab
python3 -m advanced_serving.structured_outputs \
  --repo-root . \
  --config configs/structured_outputs.example.json \
  --output results/raw/structured_outputs.json \
  --report reports/structured_outputs.md
```

Validation status for the current slimming worktree: `not run per user
request`.
