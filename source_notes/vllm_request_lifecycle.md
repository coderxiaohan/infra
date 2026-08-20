# vLLM Request Lifecycle Trace

## Goal

Trace one OpenAI-compatible request from HTTP ingress to streamed tokens, while
separating queue wait, prefill, decode, scheduler decisions, worker execution,
and response streaming.

## Entry Points To Verify

Verify exact filenames and class names against the vLLM checkout used on the
target machine.

```text
OpenAI-compatible API server
request parsing and sampling params
AsyncLLMEngine / LLMEngine
Scheduler waiting and running queues
worker execute_model
model runner prefill/decode
stream response generator
metrics exporter
```

## Runtime Path

```text
HTTP /v1/completions or /v1/chat/completions
  request validation
  sampling params and prompt tokenization
  engine.add_request
  scheduler queues request
  prefill batch selected
  worker/model runner executes prefill
  first token becomes available
  decode iterations join continuous batches
  streamed chunks return to client
  request finished and KV blocks are released
```

## Evidence To Attach

- request id, arrival timestamp, scheduling timestamp, first token timestamp
- TTFT, TPOT, total latency, output token count
- running batch size and queued request count per engine step
- prompt length, generated length, max model length
- vLLM config: `max_num_batched_tokens`, `max_model_len`, dtype, tensor
  parallel size, attention backend

## Bottleneck Questions

- Does TTFT come from tokenization, queue wait, prefill compute, or GPU memory?
- Does TPOT improve because decode steps batch across requests?
- When does continuous batching hurt P99 or fairness?
- Which request shapes dominate GPU time: long prefill or long decode?

## InferLab Link

- `scripts/run_vllm_benchmark.py` records OpenAI-compatible endpoint timing.
- Native scheduler probes model TTFT, queue wait, fairness, and runtime policy
  trade-offs without requiring vLLM.
- `monitoring/grafana_dashboard.json` exposes latency and throughput metrics
  after `inferlab_metrics.prom` is scraped.
