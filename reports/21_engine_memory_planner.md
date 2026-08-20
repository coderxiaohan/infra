# Engine Memory Planner

This chapter describes the native compiled-fixture memory planner used to
review model profiles before target-machine execution. It is a C++ mechanism
slice because its capacity facts belong near runtime admission, KV allocation,
CUDA graph workspace, and backend profile selection.

## Scope

- Weight storage uses exact parameter counts, typed precision, packed-byte
  arithmetic, and explicit metadata ratios.
- KV storage reuses the canonical `inferlab::estimate_kv_cache` owner; FP8 and
  INT8 share eight-bit storage, while FP4, INT4, and NVFP4 share four-bit
  packed storage for this analytical model.
- Activation residency, hidden/logits I/O copies, LoRA storage, CUDA graph
  workspace, and fragmentation are explicit assumptions.
- Capacity is evaluated before fragmentation so configured KV is not included
  in fragmentation and then subtracted from its own budget a second time.
- `memory_pressure_over_target` is uncapped. The historical
  `oom_risk_score` remains as a normalized compatibility field capped at one.

All fields ending in `_gb` are retained for artifact compatibility, but the
implemented unit is binary GiB (`2^30` bytes). The artifact also emits explicit
`*_gib` and unit-scope metadata where needed.

## Ownership

```text
native_core/src/engine_memory_planner_model.hpp/.cpp
native_core/src/engine_memory_planner_view.hpp/.cpp
native_core/src/engine_memory_planner_main.cpp
native_core/include/inferlab/kv_cache.hpp
native_core/src/kv_cache.cpp
```

The model owns validation, checked storage geometry, memory budgets,
recommendations, ranking, and the plan decision. The view owns artifact
identity and JSON. The 49-line main owns CLI arguments and output only.

## Refresh Command

```bash
cd InferLab
./build/inferlab_engine_memory_planner \
  --repo-root . \
  --output results/raw/native_engine_memory_planner.json \
  --gpu-memory-gb 24
```

`configs/engine_memory.example.json` is a reference profile document. The
current CLI consumes only `--gpu-memory-gb`; it does not parse that file.

## Artifact Contract

The existing legacy fields remain available:

- `weight_gb`, `kv_cache_gb`, `activation_gb`, `io_tensor_gb`
- `lora_adapter_gb`, `cuda_graph_workspace_gb`, `fragmentation_gb`
- `total_required_gb`, `kv_budget_gb`
- `max_tokens_by_kv_budget`, `max_concurrency_by_memory`
- `memory_utilization`, `oom_risk_score`, `recommendation`

The strengthened artifact also discloses exact parameter count, vocabulary
size, compiled assumptions, aggregate-token capacity scope, uncapped pressure,
reference-config non-consumption, canonical owners, and plan decision.

## Claim Boundary

This target emits a compiled analytical fixture with `executed=false`,
`replayed=false`, and `evidence_confidence=schema_only`. It does not observe a
CUDA allocator, reproduce an OOM, mutate admission control, verify a mature
product backend, execute a benchmark, or establish measured capacity,
performance, replay, execute, or release-readiness evidence.

Build, native execution, exact JSON/numerical parity, artifact refresh,
Summary, Prometheus, Final Report, product replay/execute, benchmark, and GPU
validation remain not run per user request.
