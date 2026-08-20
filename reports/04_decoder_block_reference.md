# Decoder Block Reference Notes

## Scope

This report records the first native C++ decoder-block slice. It is still a CPU
reference, but it wires together the same low-level pieces used by a
LLaMA-style decoder block:

- RMSNorm over `[seq_len, hidden_size]`
- Q/K/V projection with row-major matrix multiplication
- per-head RoPE on Q and K
- causal scaled dot-product attention
- output projection with row-major matrix multiplication
- attention residual add
- second RMSNorm
- SwiGLU feed-forward network with gate/up/down projections
- FFN residual add

The code lives in:

```text
native_core/include/inferlab/decoder_block.hpp
native_core/include/inferlab/decoder_kv.hpp
native_core/src/decoder_block.cpp
native_core/src/decoder_kv.cpp
native_core/src/decoder_probe_main.cpp
native_core/src/decoder_kv_probe_main.cpp
native_core/tests/decoder_block_test.cpp
native_core/tests/decoder_kv_test.cpp
```

The implementation still exposes `decoder_block_attention_cpu` as an
attention-only oracle. The full `decoder_block_cpu` path adds residual wiring
and the FFN/SwiGLU sublayer.

## Data Layout

The public input and output are row-major:

```text
hidden_states[token, hidden]
```

Internally, Q/K/V are reshaped into the attention layout already used by the
native attention reference:

```text
tensor[head, token, head_dim]
```

RoPE is applied independently to every head as a `[seq_len, head_dim]` slice.
The current implementation requires an even `head_dim`, matching the pairwise
rotation used by rotary embeddings.

## KV-Backed Prefill/Decode

The native core now also exposes:

- `decoder_prefill_cpu`
- `decoder_decode_cpu`
- `decoder_prefill_decode_cpu`

Prefill projects and stores K/V for all prompt tokens. Decode accepts one token
at a time, appends only that token's K/V to the cache, applies RoPE with an
absolute position offset, and attends the new query over the full cached
history.

The KV path stores K/V values in physical block-major storage and uses
`gather_paged_kv_cache` plus `RequestBlocks.block_table` to recover logical
token order for the CPU attention oracle. This is intentionally still simple,
but it models the key indirection later CUDA kernels must perform.

## Probe

Run:

```bash
./build/inferlab_decoder_probe --repo-root . --output results/raw/decoder_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default config:

| setting | value |
|---|---:|
| sequence length | 4 |
| hidden size | 16 |
| heads | 4 |
| head dim | 4 |
| FFN intermediate size | 16 |

Observed output:

| field | value |
|---|---:|
| input elements | 64 |
| projected elements | 64 |
| attention score elements | 64 |
| FFN elements | 64 |
| attention-only checksum | -3.253877 |
| FFN checksum | 13.900683 |
| output elements | 64 |
| output checksum | 8.146804 |
| output abs sum | 63.543545 |

## KV Probe

Run:

```bash
./build/inferlab_decoder_kv_probe --repo-root . --output results/raw/decoder_kv_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default config:

| setting | value |
|---|---:|
| prompt tokens | 5 |
| decode steps | 3 |
| final tokens | 8 |
| hidden size | 16 |
| heads | 4 |
| block size | 4 |

Observed output:

| field | value |
|---|---:|
| used blocks | 2 |
| key/value elements | 256 |
| last token block id | 1 |
| last token block offset | 3 |
| used block-table gather | true |
| final logical KV bytes | 1024 |
| final reserved KV bytes | 1024 |
| decode attention KV read elements | 672 |
| Paged gather read elements | 672 |
| Paged gather write elements | 672 |
| block-table lookups | 42 |
| block-table lookup bytes | 168 |
| prefill key checksum | 3.966770 |
| decode key checksum | -0.184927 |
| last token output checksum | -0.025822 |

The `cache_profile` object in `decoder_kv_probe.json` is an analytical traffic
profile, not a latency result. It records how many K/V elements the decode
attention path reads, how much traffic the current Paged KV gather baseline
performs, how many block-table lookups are needed, and how much final KV memory
is logical payload versus reserved block capacity.

## Correctness Checks

Native tests cover:

- output shape and trace metadata
- causal masking through the full decoder attention path
- deterministic repeated execution
- rejection of odd RoPE head dimensions
- attention residual behavior when FFN weights are zero
- FFN/SwiGLU trace metadata and shape validation
- rejection of bad FFN projection shapes
- prefill K/V cache shape and block allocation
- decode-time single-token K/V append
- KV trace metadata for final token physical block location
- decoder KV decode path using block-table gather
- rejection of bad decode token shapes

Run:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Next Bottom-Level Steps

1. Add CUDA kernels one piece at a time while keeping this CPU path as the
   correctness oracle.
