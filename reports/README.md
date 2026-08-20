# InferLab Reports

This directory contains generated or hand-written report chapters. A report is not automatically an implementation; it must point back to code, config, artifact, or target-environment evidence.

## Report Kinds

| Kind | Meaning | Examples |
|---|---|---|
| capability | Explains an implemented local capability or benchmark surface. | KV cache, quantization, GGUF, structured outputs |
| plan | Dry-run, migration, deployment, or target-environment execution plan. | runtime deploy, Android deploy, KServe/llm-d, SLO capacity |
| profile | Mature external product profile and evidence collection entry. | vLLM, SGLang, TensorRT-LLM, FlashInfer |
| contract | Artifact/schema/boundary rules that gate claims. | product profile contract, product profile contract check, product integration inventory, claim scope contract |
| deep-dive | Mechanism-level implementation/evidence bridge that links native C++ slices with product artifacts without claiming benchmark execution. | KV/PagedAttention, KV lifecycle, scheduler/batching, CUDA/quant microkernel, KV tier decision, PD split crossover, scheduler root cause, KV transport, radix prefix cache, chunked prefill, tenant quota, live request migration, kernel compatibility, quant correctness, Paged KV gather contract, FlashInfer/TRT-LLM alignment, gateway/deployment/API readiness, streaming/backpressure, autoscaler cost/SLO, LoRA adapter cache, mature-product grammar/parser compatibility, edge compiler readiness, edge runtime matrix, edge/cloud request routing with network, caller-budget and transcript-recovery gates, product replay semantics, product replay alignment, release evidence pipeline, deep-dive contract check, migration readiness, evidence bundle, trace correlation |
| evidence | Quality, comparison, profiling, readiness, verification, lineage, dependency, schema registry, developer fixture input, native-core audit, query, or release-pipeline evidence. | backend comparison, quality evaluation, evidence warehouse lineage, release evidence pipeline, evidence query, kernel microbench harness, dependency governance, dependency SBOM gate, artifact schema registry, artifact catalog single source, native core audit register |
| benchmark | Benchmark matrix or workload-specific measurement report. | llama.cpp edge benchmark, long context matrix |
| final | Roll-up generated from available artifacts. | `final_report.md` |

## What `*plan*.md` Means

`*plan*.md` files exist because this repository is often edited on machines without GPU, Kubernetes, real models, or installed external products. They describe what should run later, which artifact must be produced, and which claims are still blocked.

They are not a substitute for implementation. A plan report should be backed by at least one of:

- C++/CUDA native code or product adapter code
- config or deployment template
- `results/raw/*.json` artifact contract
- benchmark matrix entry
- summary / Prometheus / final report wiring

## New Report Rule

Do not add a new `*plan*.md` for every small feature. Prefer extending code, artifacts, and existing report chapters. Add a new report only when it has a distinct audience or acceptance surface, and make sure it is discoverable from the final report.
