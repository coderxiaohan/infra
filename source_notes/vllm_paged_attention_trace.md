# vLLM PagedAttention Trace

## Goal

Connect vLLM's KV-cache block management to InferLab's native Paged KV manager,
fragmentation probes, locality probes, and scheduler pressure sweeps.

## Entry Points To Verify

Verify exact source paths against the target vLLM checkout.

```text
block manager
KV cache block allocation
block table per sequence/request
prefix cache / shared blocks
paged attention backend or kernel dispatch
cache eviction or recompute policy when enabled
```

## Mechanism Trace

```text
request admitted
  prompt tokens require KV blocks
  block manager allocates physical blocks
  scheduler tracks logical token positions
  block table maps logical positions to physical blocks
  attention backend reads K/V through block table
  decode appends one token and may allocate another block
  finished request decrements references and releases blocks
```

## Evidence To Attach

- block size and total block count
- allocated/free blocks per scheduler step
- prefix sharing or refcount events
- rejected or delayed requests caused by KV pressure
- PagedAttention backend selected by runtime config
- throughput/latency change as sequence length and concurrency change

## Bottleneck Questions

- Which workload benefits most from paging: many short requests, long-tail
  prompts, or shared-prefix workloads?
- When do block-table reads and non-contiguous K/V access become visible?
- How does block size trade fragmentation against metadata overhead?
- How does KV pressure change admission policy and tail latency?

## InferLab Link

- `native_core/include/inferlab/paged_kv.hpp` implements the toy block table.
- `inferlab_kv_fragmentation_probe` models release and reuse behavior.
- `inferlab_paged_kv_locality_probe` models fragmented versus contiguous reads.
- Scheduler sweeps expose rejection, preemption, recompute, backpressure, and
  fairness trade-offs.
