# KV Cache And Paged Attention Notes

## Scope

This report records the first native C++ slice for KV-cache management. It is a
small mechanism reproduction, not a vLLM replacement:

- `PagedKVCacheManager` models fixed-size physical KV blocks.
- Each request owns a logical `block_table` mapping logical token blocks to
  physical block ids.
- `append_tokens` grows a request as decode progresses.
- `release_request` returns physical blocks to the free list for reuse.
- Shared prefix blocks are reference-counted so multiple requests can reuse the
  same immutable prefix KV blocks.
- `ContinuousBatchScheduler` drives a toy continuous batching loop on top of the
  paged cache.

The current goal is correctness and explainability. CUDA kernels and real
attention reads from the block table will come later.

## Why Static KV Allocation Wastes Memory

A static allocator reserves `max_seq_len` KV space for every request. If
`max_seq_len = 4096` and a request only uses 300 prompt+decode tokens, most of
the reserved memory is idle. This is especially painful in online serving where
request lengths are heterogeneous.

Paged allocation changes the unit of allocation from "whole max sequence" to
"fixed-size KV blocks". With a block size of 16 tokens, a 17-token request uses
2 blocks instead of reserving the whole max context.

## Current Native Model

The native model is under `native_core/`:

```text
include/inferlab/paged_kv_cache.hpp
include/inferlab/static_kv_cache.hpp
include/inferlab/continuous_batch_scheduler.hpp
src/paged_kv_cache.cpp
include/inferlab/paged_attention.hpp
include/inferlab/paged_kv_locality.hpp
src/paged_attention.cpp
src/paged_kv_locality.cpp
src/static_kv_cache.cpp
src/continuous_batch_scheduler.cpp
src/scheduler_trace_main.cpp
src/kv_allocation_compare_main.cpp
src/prefix_cache_compare_main.cpp
src/kv_fragmentation_probe_main.cpp
src/paged_kv_locality_probe_main.cpp
tests/paged_kv_cache_test.cpp
tests/paged_attention_test.cpp
tests/paged_kv_locality_test.cpp
tests/static_kv_cache_test.cpp
tests/continuous_batch_scheduler_test.cpp
```

`PagedKVCacheManager` exposes:

- `add_request(request_id, initial_tokens)`
- `append_tokens(request_id, new_tokens)`
- `release_request(request_id)`
- `request_blocks(request_id)`
- `stats()`

`AllocationStats` records:

- total/free/used blocks
- active request count
- logical used bytes
- reserved block bytes
- utilization = used bytes / reserved bytes

## Scheduler Trace

Run:

```bash
cmake --build build -j$(nproc)
./build/inferlab_scheduler_trace --repo-root . --output results/raw/scheduler_trace.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

The built-in workload currently submits three requests:

| request | prompt tokens | max decode tokens |
|---|---:|---:|
| 1 | 6 | 3 |
| 2 | 9 | 2 |
| 3 | 4 | 4 |

The trace emits one row per scheduler step:

- admitted requests
- decoded tokens
- finished requests
- rejected requests
- cache block stats after the step
- request lifecycle metrics after the run:
  `queue_wait_steps`, `time_to_first_token_steps`, `total_latency_steps`, and
  `decode_steps_per_token`

The lifecycle fields are discrete scheduler-step analogues of online serving
metrics. `time_to_first_token_steps` models TTFT, while
`decode_steps_per_token` is the toy scheduler equivalent of TPOT. The values are
not wall-clock latency claims; they let the project test scheduling behavior and
report aggregation before GPU timing is available.

The trace also emits a `pressure_profile` object that aggregates scheduler
throughput, cache pressure, and queueing behavior from the same run. This makes
the toy scheduler useful as a bottom-level serving model: it can show how active
batch size and Paged KV block pressure move together before real GPU kernels are
plugged in.

Current built-in workload summary:

| metric | value |
|---|---:|
| requests | 3 |
| finished requests | 3 |
| average TTFT steps | 1.666667 |
| max queue wait steps | 3 |
| average decode steps/token | 1.0 |

Current pressure profile summary:

| metric | value |
|---|---:|
| steps | 6 |
| total decoded tokens | 9 |
| tokens per step | 1.5 |
| peak active requests | 2 |
| average active requests | 1.0 |
| peak used KV blocks | 3 |
| average used KV blocks | 1.166667 |
| peak reserved bytes | 3072 |
| peak total logical tokens | 17 |
| peak cache utilization | 1.0 |
| average cache utilization | 0.659722 |
| rejected requests | 0 |

## Scheduler Pressure Sweep

Run:

```bash
./build/inferlab_scheduler_pressure_sweep --repo-root . --output results/raw/scheduler_pressure_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

The sweep runs one serving-style workload through several scheduler/cache
settings. It uses the same pressure profile fields as the single trace, so the
trade-off is explicit: wider batching can improve decoded tokens per step and
TTFT, but it also increases peak KV block pressure; tight cache capacity can
look fast only because requests are rejected.

Current sweep summary:

| scenario | max active | KV blocks | steps | tokens/step | avg TTFT steps | peak used blocks | peak cache util | rejected |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| serial_latency | 1 | 8 | 14 | 1.0 | 6.6 | 2 | 1.0 | 0 |
| balanced_batch | 2 | 8 | 8 | 1.75 | 3.0 | 3 | 1.0 | 0 |
| wide_batch | 3 | 8 | 6 | 2.333333 | 2.0 | 4 | 0.875 | 0 |
| cache_constrained | 3 | 3 | 3 | 1.666667 | 1.0 | 3 | 1.0 | 3 |

The trade-off summary picks `wide_batch` as the best healthy scenario for this
workload: it has the highest throughput at `2.333333` tokens/step and the
lowest average TTFT among no-rejection scenarios at `2.0` steps. The
`cache_constrained` scenario is kept as a failure-mode sample: it reaches full
cache utilization and rejects three requests.

## Scheduler Workload Sweep

Run:

```bash
./build/inferlab_scheduler_workload_sweep --repo-root . --output results/raw/scheduler_workload_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

The workload sweep keeps the scheduler fixed at `max_active_requests=3` with
10 KV blocks of 8 tokens each, then changes the request mix. This separates
"what scheduler setting did we choose?" from "what request shape arrived?".

Current workload summary:

| workload | steps | tokens/step | avg TTFT steps | avg queue wait | peak used blocks | peak cache util | rejected |
|---|---:|---:|---:|---:|---:|---:|---:|
| short_chat | 3 | 2.666667 | 1.6 | 1.6 | 2 | 1.0 | 0 |
| decode_heavy | 11 | 2.727273 | 3.6 | 3.6 | 6 | 1.0 | 0 |
| prefill_heavy | 2 | 3.0 | 1.4 | 1.4 | 3 | 0.708333 | 0 |
| long_tail | 10 | 1.4 | 1.4 | 1.4 | 2 | 1.0 | 0 |
| cache_stress | 5 | 2.4 | 2.0 | 1.8 | 10 | 0.84375 | 1 |

For this toy decode-step model, `prefill_heavy` has the highest decoded tokens
per scheduler step at `3.0` and the lowest no-rejection TTFT at `1.4` steps.
This is not a prefill compute timing claim: prompt work is represented here by
KV allocation pressure, not by GPU prefill latency. The useful signals are that
`decode_heavy` creates the highest queue wait at `3.6` steps, `long_tail` drags
throughput down to `1.4` tokens/step, and `cache_stress` reaches 10 used KV
blocks and rejects one request.

## Scheduler Capacity Sweep

Run:

```bash
./build/inferlab_scheduler_capacity_sweep --repo-root . --output results/raw/scheduler_capacity_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

The capacity sweep fixes `max_active_requests=3`, block size 8 tokens, and a
cache-stress workload, then varies physical KV block capacity. It answers a
serving-planning question directly: how much KV capacity is required before this
workload stops rejecting requests?

Current capacity summary:

| KV blocks | steps | tokens/step | avg TTFT steps | peak used blocks | peak cache util | rejected |
|---:|---:|---:|---:|---:|---:|---:|
| 3 | 1 | 0.0 | 0.0 | 0 | 0.0 | 5 |
| 4 | 2 | 1.0 | 1.0 | 2 | 0.5625 | 4 |
| 5 | 4 | 1.0 | 1.0 | 3 | 0.791667 | 4 |
| 6 | 4 | 1.0 | 1.0 | 4 | 0.84375 | 4 |
| 8 | 4 | 2.0 | 1.0 | 7 | 0.821429 | 3 |
| 10 | 5 | 2.4 | 2.0 | 10 | 0.84375 | 1 |
| 11 | 6 | 2.666667 | 2.6 | 11 | 0.829545 | 0 |
| 12 | 6 | 2.666667 | 2.6 | 11 | 0.829545 | 0 |

For this workload, the minimum no-rejection capacity is 11 KV blocks. Adding a
12th block does not improve decoded tokens per scheduler step because peak
demand is already 11 blocks. The useful threshold signal is the transition from
10 blocks, which still rejects one request, to 11 blocks, which completes all
requests.

## Scheduler Admission Policy Sweep

Run:

```bash
./build/inferlab_scheduler_admission_policy_sweep --repo-root . --output results/raw/scheduler_admission_policy_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

This sweep fixes the same mixed workload, `max_active_requests=3`, block size 8
tokens, and 8 KV blocks, then changes the order in which requests enter the
waiting queue. It models admission-policy choices without changing the core
scheduler implementation.

Current admission policy summary:

| policy | order | finished | rejected | tokens/step | avg TTFT steps | avg queue wait | peak used blocks |
|---|---|---:|---:|---:|---:|---:|---:|
| fcfs | 1,2,3,4,5,6 | 5 | 1 | 2.3 | 2.2 | 2.2 | 7 |
| short_total_first | 2,5,3,6,4,1 | 5 | 1 | 2.75 | 2.5 | 2.5 | 8 |
| short_prompt_first | 2,5,3,6,4,1 | 5 | 1 | 2.75 | 2.5 | 2.5 | 8 |
| short_decode_first | 4,2,5,1,6,3 | 6 | 0 | 2.181818 | 2.0 | 2.0 | 8 |
| long_total_first | 1,4,6,3,5,2 | 5 | 1 | 2.222222 | 2.2 | 2.0 | 6 |

For this workload, `short_decode_first` is the only policy that finishes all
six requests without rejection and also has the lowest average TTFT and queue
wait at `2.0` steps. `short_total_first` has the highest decoded tokens per
step at `2.75`, but it rejects one request, so it is a throughput-biased choice
rather than the best serving policy.

## Scheduler Runtime Policy Sweep

Run:

```bash
./build/inferlab_scheduler_runtime_policy_sweep --repo-root . --output results/raw/scheduler_runtime_policy_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

This sweep moves beyond waiting-queue ordering and models runtime KV pressure
control directly with the Paged KV manager. It fixes 6 KV blocks, block size 4
tokens, and a mixed workload, then compares reject-on-pressure, one-block and
two-block admission backpressure, preempt-longest-on-pressure, and a hybrid
backpressure plus preemption policy. The preemption path releases a running
request's KV blocks, requeues it, and records the cache tokens that would need
to be restored or recomputed on resume.

Current runtime policy summary:

| policy | finished | rejected | tokens/step | avg TTFT steps | preemptions | backpressure deferrals | recompute tokens | append pressure |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| reject_on_pressure | 3 | 3 | 1.857143 | 1.333333 | 0 | 0 | 0 | 1 |
| one_block_backpressure | 5 | 1 | 1.125 | 7.2 | 0 | 14 | 0 | 1 |
| two_block_backpressure | 6 | 0 | 1.2 | 8.5 | 0 | 14 | 0 | 0 |
| preempt_longest_on_pressure | 2 | 4 | 1.333333 | 1.0 | 1 | 0 | 8 | 1 |
| hybrid_backpressure_preempt | 6 | 0 | 1.263158 | 7.666667 | 1 | 17 | 17 | 1 |

The useful signal is the trade-off between fast but lossy admission and
conservative KV-aware admission. `reject_on_pressure` decodes the most tokens
per step but rejects half the workload. `two_block_backpressure` finishes all
requests with no preemption or recompute cost, while `hybrid_backpressure_preempt`
also finishes all requests and improves throughput/TTFT slightly at the cost of
one preemption and 17 cache tokens of restore/recompute work.

## Scheduler Fairness And Starvation Sweep

Run:

```bash
./build/inferlab_scheduler_fairness_sweep --repo-root . --output results/raw/scheduler_fairness_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

This sweep models a long-request plus short-request arrival stream under a
two-slot continuous batching scheduler. It compares FIFO, shortest-decode-first,
longest-decode-first, oldest-wait-first, and a starvation-guarded shortest policy.
The guard keeps the short-request TTFT benefit until a waiting request crosses a
5-step guard threshold, then admits the oldest waiting request. Starvation is
reported separately at an 8-step queue-wait threshold.

Current fairness summary:

| policy | tokens/step | avg TTFT steps | avg queue wait | max queue wait | starved requests | Jain queue fairness | worst request |
|---|---:|---:|---:|---:|---:|---:|---:|
| fcfs | 1.9 | 9.4 | 9.4 | 12 | 8 | 0.832015 | 10 |
| shortest_decode_first | 1.9 | 2.2 | 2.2 | 9 | 2 | 0.291566 | 2 |
| longest_decode_first | 1.9 | 9.4 | 9.4 | 12 | 8 | 0.832015 | 10 |
| oldest_wait_first | 1.9 | 9.4 | 9.4 | 12 | 8 | 0.832015 | 10 |
| starvation_guarded_shortest | 1.9 | 5.8 | 5.8 | 12 | 4 | 0.578007 | 10 |

The short-first policy gives the lowest average TTFT (`2.2` steps) but makes
queue wait highly uneven (`0.291566` Jain fairness) because the long requests are
pushed back. FIFO/oldest policies are more even but make many short requests wait
behind long decode work. With a `0.5` Jain fairness floor, the balanced policy is
`starvation_guarded_shortest`: it keeps average TTFT/queue wait below FIFO while
raising queue fairness above the pure short-first policy.

The benchmark summary pipeline also records native probe wall-clock timing in
`results/raw/native_probe_run.json`. These timings are end-to-end process
elapsed times for small CLI probes, not kernel timings. They are still useful as
a reproducibility smoke signal and as a placeholder for later GPU timing
reports.

## Static vs Paged Allocation Baseline

Current source hardening for the four probes below is implementation-only and
has not refreshed the observed artifacts in this report. The entrypoints now
emit common native-reference execute/replay/claim metadata, enforce a bounded
Paged KV fixture capacity before allocation, use canonical allocation/config/
block-table views, and distinguish scalar modeling from product/GPU evidence.
Allocation comparison validates its fixed workload; prefix comparison uses the
maximum strategy capacity instead of summing both capacities; fragmentation
handles both pressure-allocation outcomes and requires final empty state; and
locality validates checked physical-span arithmetic and metric consistency.
The existing observed tables remain prior `results/raw/*.json` evidence and do
not prove that this source revision has been compiled or executed.

Run:

```bash
./build/inferlab_kv_allocation_compare --repo-root . --output results/raw/kv_allocation_compare.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Built-in workload:

| request | tokens |
|---|---:|
| 1 | 7 |
| 2 | 23 |
| 3 | 5 |
| 4 | 31 |

Default allocator config:

| allocator | config |
|---|---|
| static | `max_requests=4`, `max_seq_len=64`, `bytes_per_token=128` |
| paged | `block_size_tokens=8`, same effective max capacity |

Observed output:

| allocator | used bytes | reserved bytes | utilization |
|---|---:|---:|---:|
| static | 8448 | 32768 | 0.257812 |
| paged | 8448 | 9216 | 0.916667 |

Paged allocation reserves `9216 / 32768 = 0.28125` of the static allocator's
memory for this heterogeneous workload. This small example is intentionally
simple, but it demonstrates the core PagedAttention memory-management idea:
reserve KV memory close to actual sequence length instead of reserving every
request's maximum possible context.

## Prefix Cache Sharing

Run:

```bash
./build/inferlab_prefix_cache_compare --repo-root . --output results/raw/prefix_cache_compare.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default workload:

| setting | value |
|---|---:|
| requests | 4 |
| shared prefix tokens | 16 |
| private suffix tokens per request | 8 |
| block size | 8 |
| bytes per token | 128 |

Observed output:

| mode | physical tokens counted | used blocks | reserved bytes |
|---|---:|---:|---:|
| no prefix cache | 96 | 12 | 12288 |
| shared prefix | 48 | 6 | 6144 |

The shared-prefix run reserves half the KV memory because the 16-token prefix is
stored once and referenced by all requests. This models common serving cases
such as a repeated system prompt, a shared RAG instruction prefix, or a batched
agent template.

The same probe now emits a `traffic_profile` object:

| metric | value |
|---|---:|
| no-prefix prefill write bytes | 12288 |
| shared-prefix prefill write bytes | 6144 |
| prefix write bytes saved | 6144 |
| physical tokens saved | 48 |
| decode attention KV read bytes per step | 12288 |
| shared-prefix decode read bytes per step | 8192 |
| shared prefix entry block-table entries | 2 |

Prefix caching reduces prefill write traffic and reserved KV memory, but decode
attention still reads the shared prefix for each active request unless a later
kernel adds a reuse-aware attention path. The profile makes that distinction
explicit.

## Fragmentation And Reuse Probe

Run:

```bash
./build/inferlab_kv_fragmentation_probe --repo-root . --output results/raw/kv_fragmentation_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

The stress probe allocates a heterogeneous set of requests, releases middle
requests to create holes in the free-block pool, then allocates a larger
request and checks whether released blocks are reused. The current allocator
does not require contiguous physical blocks, so this probe is about churn,
reuse, and full final recovery rather than external fragmentation failure.

Observed output:

| metric | value |
|---|---:|
| total blocks | 12 |
| free blocks after releases | 6 |
| used blocks after reuse allocation | 10 |
| reused released blocks | 4 |
| intentionally failed oversized allocations | 1 |
| final free blocks | 12 |
| final used blocks | 0 |

## Block-Table Locality Probe

Run:

```bash
./build/inferlab_paged_kv_locality_probe --repo-root . --output results/raw/paged_kv_locality_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

This probe compares a contiguous block table with a fragmented block table for
the same logical token count and K/V shape. It reports logical element reads,
touched physical block elements, physical span elements, transition counts, and
span amplification. The numbers are a CPU-side model of the memory locality
problem CUDA PagedAttention kernels must handle.

Observed output:

| metric | contiguous | fragmented |
|---|---:|---:|
| logical blocks | 3 | 3 |
| non-contiguous transitions | 0 | 2 |
| max physical jump | 1 | 4 |
| block element amplification | 1.333333 | 1.333333 |
| span element amplification | 1.333333 | 3.111111 |

The block amplification is identical because both layouts touch three physical
blocks and the final logical block is only partially full. The span
amplification increases for the fragmented table because the same logical K/V
history spans a wider physical block range.

## Correctness Checks

Native tests cover:

- expected block count for a request crossing a block boundary
- append that allocates a new block
- non-destructive capacity failure
- release and deterministic block reuse
- utilization for partially filled blocks
- request admission and finishing in the scheduler
- waiting request admission after active capacity frees
- prefill rejection when the prompt cannot fit
- decode rejection when the next token cannot allocate a block
- scheduler pressure aggregation for active requests, cache blocks,
  utilization, queue wait, TTFT, TPOT, and tokens per step
- scheduler pressure sweep trade-off reporting for serial, balanced, wide, and
  cache-constrained serving scenarios
- scheduler workload sweep reporting for short-chat, decode-heavy,
  prefill-heavy, long-tail, and cache-stress request mixes
- scheduler capacity sweep reporting for KV block threshold, no-rejection
  capacity, utilization, and rejected-request behavior
- scheduler admission policy sweep reporting for FIFO, short-first, decode-first,
  and long-first waiting-queue policies
- scheduler runtime policy sweep reporting for backpressure, preemption,
  recompute tokens, append pressure, and rejected-request behavior
- scheduler fairness/starvation sweep reporting for TTFT, queue wait, Jain
  fairness, starvation threshold, and worst-request behavior
- static allocation utilization and max-sequence rejection
- prefix ref-counting, shared block table entries, and delayed prefix release
- paged K/V gather from physical block-major storage through a request block
  table
- attention output equivalence between contiguous K/V and gathered paged K/V
- block-table locality/read-amplification metrics for contiguous, fragmented,
  and partial-tail cases

Run:

```bash
ctest --test-dir build --output-on-failure
```

## Next Bottom-Level Steps

1. Move from CPU-only simulation to CUDA kernels that read K/V through the block
   table.
