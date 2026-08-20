# Tool Calling And Reasoning Parser

This chapter tracks the request-semantics layer that modern OpenAI-compatible
serving stacks expose above raw text generation.

## Scope

- Parse tool calls emitted as Hermes/Qwen-style `<tool_call>` wrappers, raw JSON
  objects, and OpenAI-compatible `recipient_name` payloads.
- Detect reasoning wrappers such as `<think>` and `<reasoning>` so tool-call
  parsing can be evaluated separately from hidden or visible chain-of-thought
  spans.
- Measure JSON parse success, tool-name match rate, argument schema validity,
  retry token budget, and malformed sample taxonomy.
- Keep the current path offline and deterministic. The accepted `--execute`
  flag remains explicitly unsupported and does not contact an endpoint.

## Commands

```bash
cd InferLab
python3 -m advanced_serving.tool_call_parser_benchmark \
  --repo-root . \
  --config configs/tool_call_cases.example.json \
  --output results/raw/tool_call_parser_benchmark.json \
  --report reports/17_tool_call_reasoning_parser.md
```

## Design Notes

Tool calling is not just an API feature. It creates a parser and constraint
problem at the serving boundary:

- strict constrained decoding can guarantee schema shape but may add decode
  overhead;
- raw parser extraction is cheaper but can emit malformed JSON or wrong tool
  names;
- reasoning tags and streaming fragments can split a tool call across multiple
  chunks;
- retries consume extra output tokens and can amplify tail latency.

This runner is retained because it performs actual extraction, JSON parsing,
tool-name matching, and argument-schema validation over supplied samples. The
retired native plan only multiplied assumed success probabilities and therefore
did not implement a parser.

## Next Enhancements

- Add streaming partial-fragment cases after the SSE benchmark lands.
- Add parser-specific repair prompts and compare retry token cost.
- Feed valid and invalid cases into backend quality evaluation so function-call
  correctness appears in the final report.
