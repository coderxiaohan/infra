# KV Offload Hierarchy

This chapter models KV cache as a multi-tier memory system rather than a single
GPU-resident buffer. It is intended to connect InferLab's prefix-cache and
KV-aware routing work with production designs such as LMCache, AIBrix KVCache,
KServe KV offloading, llm-d hierarchical KV, and disaggregated prefill/decode
serving.

## Scope

- Estimate prompt and generated-token KV from request count, token counts, and
  KV bytes per token.
- Separate reusable prompt KV from session-output KV; only placed prompt reuse
  earns modeled prefill savings.
- Place reusable KV across HBM, CPU DRAM, NVMe, and remote KV tiers.
- Compare LRU, S3FIFO, priority retention, and session-pinning policies.
- Estimate prefill time saved, transfer time, lookup/access latency, net TTFT
  savings, HBM pressure, HBM saved, cross-engine reusable KV, and monthly cache
  storage cost.

## C++ Entry Point

```bash
cd InferLab
./build/inferlab_kv_offload_hierarchy_plan \
  --repo-root . \
  --output results/raw/native_kv_offload_hierarchy_plan.json \
  --hbm-gb 24 \
  --dram-gb 256 \
  --nvme-gb 2048 \
  --remote-gb 8192
```

The canonical implementation chain is:

```text
native_core/include/inferlab/advanced_serving_kv_tier.hpp
native_core/src/advanced_serving_kv_offload_hierarchy.cpp
native_core/src/kv_offload_hierarchy_plan_view.cpp
native_core/src/kv_offload_hierarchy_plan_main.cpp
```

The main is only the CLI/output shell. Policy profiles, storage accounting,
placement, transfer/access/cost aggregation, recommendation, and ranking belong
to the typed model; artifact JSON belongs to the projection-only view.

## Why This Is C++ Core Work

KV offload decisions live on the serving hot path: each request may need a
placement decision, cache lookup, transfer decision, and eviction update before
decode begins. A production implementation would put the tight data structures,
metadata updates, and transfer scheduling close to the C++ runtime, while Python
remains useful for launching experiments and rendering reports.

## Metrics To Report

- `reusable_kv_gb`: legacy binary-GiB alias for reusable prompt plus retained
  session-output KV.
- `cross_engine_reusable_gb`: legacy alias for the cross-engine-eligible KV
  candidate; it does not prove that the bytes are resident or transferred.
- `transfer_ms`: estimated DRAM/NVMe/remote transfer cost.
- `net_ttft_saving_ms`: prefill time saved minus transfer and access overhead.
- `hbm_saved_gb`: reusable KV actually placed outside HBM; unplaced KV is
  excluded.
- `reuse_efficiency`: ratio of net TTFT gain to placement-backed prefill
  saving.
- `monthly_cache_cost`: rough capacity cost outside GPU memory.

Additional explicit fields distinguish prompt/output storage, placed/unplaced
reusable KV, usable HBM after the compiled reservation assumption, and ideal
versus placement-backed prefill savings. Unplaced KV is not counted as HBM
saved.

## Evidence Boundary

This artifact is a compiled analytical fixture. It does not ingest a live
trace, allocate or evict runtime KV, execute transfers, observe cache hits, run
an external product, or establish benchmark/release evidence. The example
config mirrors the compiled defaults but is not consumed by the current CLI.
The existing `_gb` fields are retained as binary-GiB compatibility aliases.
Within this fixture, `session_pin_rate` supplies both the session-output reuse
candidate and the disclosed HBM reservation signal; it is not an observed hit
rate or proof that a runtime pin operation occurred.

## Next Enhancements

- Add real product trace ingestion through the canonical replay/evidence path.
- Add eviction simulation over time instead of static placement.
- Add network topology parameters for NVLink, PCIe, RDMA, and TCP.
