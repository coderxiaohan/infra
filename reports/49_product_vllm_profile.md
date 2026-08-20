# vLLM Product Profile

- Generated: `2026-08-09T11:06:51Z`
- Mode: `dry_run`
- Status: `planned`

## Boundary

InferLab uses this product through adapters, configuration, artifact contracts, and reports. It does not reimplement the product core.

## Summary

| Field | Value |
|---|---|
| Product | vLLM |
| Mode | dry_run |
| Status | planned |
| Artifact | path_c372c7773529fd4c.json |
| Config | path_83dfe832abbafaa3.yaml |
| Missing preflight | ["module:vllm"] |
| Expected metrics | ["ttft_ms", "tpot_ms", "throughput_tokens_per_s", "p99_latency_ms", "kv_prefix_hit_rate", "error_rate"] |
| Collected metrics | {} |
| Replay evidence files | None |
| Replay parsed metrics | None |

## Product Capabilities

- DBO
- data parallel
- expert parallel
- KV connectors
- LoRA
- structured outputs
- production stack

## InferLab Does Not Implement

- vLLM scheduler
- PagedAttention backend
- DBO implementation
- KV connector implementation
- expert parallel runtime

## Local Optimization / Integration Scope

- profile normalization
- capacity/SLO explanation
- artifact contract validation
- source-level bottleneck notes

## Official Sources

- https://docs.vllm.ai/en/latest/

## Commands

```bash
python3 -m scripts.product_profiles.profile_vllm --repo-root path_cdb4ee2aea69cc6a --config path_83dfe832abbafaa3.yaml --output path_c372c7773529fd4c.json --report path_5985dd7885bb515d.md --dry-run
python3 -m scripts.product_profiles.profile_vllm --repo-root path_cdb4ee2aea69cc6a --config path_83dfe832abbafaa3.yaml --output path_c372c7773529fd4c.json --report path_5985dd7885bb515d.md --execute --endpoint http://127.0.0.1:8000
python3 -m scripts.product_profiles.profile_vllm --repo-root path_cdb4ee2aea69cc6a --config path_83dfe832abbafaa3.yaml --output path_c372c7773529fd4c.json --report path_5985dd7885bb515d.md --replay
```
