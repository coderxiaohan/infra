# 10 Speculative Decoding

## Purpose

Speculative decoding uses a cheaper draft model to propose several tokens, then
uses the target model to verify the proposed span. It can improve TPOT only
when accepted draft tokens outweigh draft compute and verification overhead.

## Canonical Implementation

- `native_core/include/inferlab/advanced_serving_generation_strategy.hpp` and
  `native_core/src/advanced_serving_generation_strategy.cpp` own the C++ model.
- `inferlab_speculative_decode_plan` is the only formal artifact producer.
- It records draft/accepted/rejected tokens, verification rounds, target-only
  and speculative cost, speedup, tokens/s and break-even acceptance.
- `scripts/summarize_benchmarks.py` ingests
  `native_speculative_decode_plan.json`; the Python example is not a release
  or benchmark source.

## How To Use Later

```bash
./build/inferlab_speculative_decode_plan \
  --repo-root . \
  --output results/raw/native_speculative_decode_plan.json
```

The dependency-light teaching model remains outside the formal artifact path:

```bash
PYTHONPATH=. python3 examples/reference/speculative_decode.py \
  --scenario high_accept:0.85:4 \
  --scenario low_accept:0.35:4 \
  --output /tmp/speculative_decode_reference.json
```

## Interpretation Questions

- What acceptance rate is needed before speculative decoding becomes faster?
- Does a longer verify window help, or does it create too many rejected draft
  tokens?
- How much target-model work is saved per accepted token?
- Does the draft model change output quality under the fixed prompt set?
- How does speculative decode interact with continuous batching and KV cache
  pressure in vLLM/TensorRT-LLM?

## Runtime Evidence To Add

| Evidence | Source |
|---|---|
| draft model name and target model name | runtime config |
| acceptance rate by prompt | serving trace |
| target calls per generated token | benchmark logs |
| draft tokens discarded | benchmark logs |
| TPOT before/after | vLLM/TensorRT-LLM benchmark |
| output diff or quality score | fixed eval prompts |
