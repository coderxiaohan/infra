# Transformers Forward Trace

## Goal

Use a Hugging Face decoder-only model as the readable reference path for
prefill, decode, attention masks, RoPE, and KV cache semantics before comparing
with vLLM, TensorRT-LLM, and llama.cpp.

## Entry Points To Verify

These names are trace anchors, not version-pinned claims. Verify them against
the exact `transformers` checkout used on the target machine.

```text
AutoModelForCausalLM.from_pretrained
GenerationMixin.generate
model.forward
attention.forward
past_key_values / cache_position
logits processor / sampler
```

## Runtime Path

```text
tokenizer(prompt)
  model.generate(...)
    prepare_inputs_for_generation
    model.forward prefill
      embedding
      decoder layer stack
      attention QKV projection
      RoPE / position handling
      causal attention
      KV cache write
      MLP
      lm_head logits
    sample next token
    model.forward decode step
      read past_key_values
      append new K/V
      one-token logits
```

## Evidence To Attach

- model name, model config, dtype, device map, and attention implementation
- prompt token count and generated token count
- prefill tensor shapes and decode tensor shapes
- KV cache length per layer before and after decode
- tokenizer and sampling settings

## Bottleneck Questions

- Which operations scale with full prompt length during prefill?
- Which operations scale with one token plus cached K/V during decode?
- How does cache layout affect memory reads?
- Which parts are framework overhead versus model math?

## InferLab Link

- `mini_llm/transformer.py` mirrors the core decoder pieces.
- `mini_llm/cache_compare.py` compares cached decode with full-prefix recompute.
- `scripts/render_final_report.py` records missing runtime evidence until this
  trace is validated on a target machine.
