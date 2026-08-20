# GGUF Parser Reference Slice

## Goal

This report records the first CPU-only GGUF parser slice for InferLab. The goal
is not to replace llama.cpp's loader. The goal is to make the binary container
layout concrete enough to explain how a GGUF file describes metadata, tensor
shapes, tensor types, and aligned tensor-data offsets before a runtime maps or
loads the weights.

Primary format reference: `https://github.com/ggml-org/ggml/blob/master/docs/gguf.md`.
K-quant layout/arithmetic references:
`https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/ggml-common.h`
and `https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/ggml-quants.c`.
F32/F64/I64/I32/I16/I8/F16/BF16 scalar conversion reference:
`https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/ggml.c`.

## Implemented Slice

The C++ reader in `native_core/include/inferlab/gguf.hpp` and
`native_core/src/gguf.cpp` parses:

- magic bytes: `GGUF`
- GGUF version 3
- tensor count and metadata KV count
- metadata keys and scalar/string/array values
- `general.alignment` with a default of 32 bytes
- tensor name, dimensions, type id, relative offset, and absolute offset
- aligned tensor-data start offset
- GGML tensor type names for supported type ids
- block size, type-size bytes, row bytes, tensor data bytes, and data end offsets
- tensor data span checks for known non-deprecated GGML types
- layout segment annotations for header, metadata, tensor infos, tensor-data
  padding, and tensor data
- hex and ASCII prefixes for each layout segment
- selected quantized payload previews for IQ2_XXS, Q1_0, Q2_K, Q3_K, Q4_K, Q5_K, Q6_K, Q8_0, Q8_1, Q8_K, IQ4_NL, IQ4_XS, MXFP4, NVFP4, TQ1_0, and TQ2_0 blocks
- F32 scalar tensor raw-value previews and little-endian float32 reconstruction
- F64 scalar tensor raw-value previews and little-endian float64 reconstruction
- I64 scalar tensor raw-value previews and signed int64 reconstruction
- I32 scalar tensor raw-value previews and signed int32 reconstruction
- I16 scalar tensor raw-value previews and signed int16 reconstruction
- I8 scalar tensor raw-value previews and signed int8 reconstruction
- F16 scalar tensor raw-value previews and fp16-to-float32 reconstruction
- BF16 scalar tensor raw-value previews and `uint32_t(raw) << 16` float32 reconstruction
- fp16 scale conversion for selected Q4_K and Q8_0 block headers
- Q4_K packed-byte unpacking into 4-bit nibble prefixes
- Q4_K packed scale/min pair decoding into effective scale/min values
- selected Q4_K arithmetic dequant previews using scale/min pairs
- full synthetic Q4_K first-block reference stats and sub-block sums
- reusable native Q2_K reference dequant helper covered by `gguf_test`
- Q2_K generated tensor payload preview and first-block reference stats
- reusable native Q3_K reference dequant helper covered by `gguf_test`
- Q3_K generated tensor payload preview and first-block reference stats
- reusable native Q4_K reference dequant helper covered by `gguf_test`
- reusable native Q5_K reference dequant helper covered by `gguf_test`
- Q5_K generated tensor payload preview and first-block reference stats
- reusable native Q6_K reference dequant helper covered by `gguf_test`
- Q6_K generated tensor payload preview and first-block reference stats
- Q8_0 dequantized sample values using `int8_quant * fp16(d)`
- reusable native Q8_0 reference dequant helper covered by `gguf_test`
- reusable native Q8_1 reference dequant helper covered by `gguf_test`
- Q8_1 generated tensor payload preview and first-block reference stats
- reusable native Q8_K reference dequant helper entry in `gguf_test`
- Q8_K generated tensor payload preview and first-block reference stats
- reusable native Q1_0 reference dequant helper entry in `gguf_test`
- Q1_0 generated tensor payload preview and first-block reference stats
- reusable native Q4_0/Q4_1 reference dequant helpers covered by `gguf_test`
- Q4_0/Q4_1 generated tensor payload previews and first-block reference stats
- reusable native Q5_0/Q5_1 reference dequant helpers covered by `gguf_test`
- Q5_0/Q5_1 generated tensor payload previews and first-block reference stats
- reusable native IQ4_NL reference dequant helper entry in `gguf_test`
- IQ4_NL generated tensor payload preview and first-block reference stats
- reusable native IQ4_XS reference dequant helper entry in `gguf_test`
- IQ4_XS generated tensor payload preview and first-block reference stats
- reusable native MXFP4 reference dequant helper entry in `gguf_test`
- MXFP4 generated tensor payload preview and first-block reference stats
- reusable native NVFP4 reference dequant helper entry in `gguf_test`
- NVFP4 generated tensor payload preview and first-block reference stats
- F32 generated scalar tensor payload preview and raw-to-fp32 reference stats
- F64 generated scalar tensor payload preview and raw-to-fp64 reference stats
- I64 generated scalar tensor payload preview and raw signed-int64 reference stats
- I32 generated scalar tensor payload preview and raw signed-int32 reference stats
- I16 generated scalar tensor payload preview and raw signed-int16 reference stats
- I8 generated scalar tensor payload preview and raw signed-int8 reference stats
- F16 generated scalar tensor payload preview and raw-to-fp32 reference stats
- BF16 generated scalar tensor payload preview and raw-to-fp32 reference stats
- IQ2_XXS structural payload preview for fp16 `d`, packed `qs` words, grid indices, sign codes, and scale nibbles
- reusable native TQ1_0 reference dequant helper entry in `gguf_test`
- TQ1_0 generated tensor payload preview and first-block reference stats
- reusable native TQ2_0 reference dequant helper entry in `gguf_test`
- TQ2_0 generated tensor payload preview and first-block reference stats
- `--input` parsing for a generated `.gguf` fixture artifact on disk

Q2_K structure and arithmetic are aligned with the upstream ggml layout:
`block_q2_K` stores 16 scale/min bytes, 64 packed 2-bit quant bytes, and fp16
`d`/`dmin` super-block scales. The helper reconstructs 16 groups of 16 values
with `fp16(d) * scale * q - fp16(dmin) * min`, matching the two 128-value half
and 2-bit shift order used by ggml.

Q4_K structure and arithmetic are aligned with the upstream ggml layout:
`block_q4_K` stores `d`, `dmin`, 12 bytes of packed scale/min values, and 128
packed 4-bit quant bytes; the reference dequant path applies
`fp16(d) * scale * q - fp16(dmin) * min`.

Q5_K reuses the same 12-byte packed scale/min table and adds a 32-byte `qh`
high-bit plane before the 128-byte `qs` low-nibble payload. The native helper
reconstructs each 5-bit value from `qh` and `qs`, then applies the same affine
dequant shape: `fp16(d) * scale * q - fp16(dmin) * min`.

Q6_K uses 16 signed 8-bit scale groups over a 256-value super-block. It stores
128 bytes of low 4-bit quants in `ql`, 64 bytes of high 2-bit planes in `qh`,
16 signed scales, and one fp16 super-block scale `d`; the helper reconstructs
`q = low4(ql) + high2(qh) * 16 - 32`, then applies `fp16(d) * scale * q`.

Q3_K uses 16 signed scale groups packed into 12 bytes, 64 bytes of low 2-bit
quants in `qs`, and a 32-byte `hmask` bit-plane. The helper unpacks each
6-bit scale, subtracts 32 to recover the signed scale, then reconstructs
`q = low2(qs) - (hmask_bit ? 0 : 4)`.

Legacy Q5_0/Q5_1 use 32-value blocks with a 4-byte `qh` high-bit plane and a
16-byte packed-nibble `qs` payload. Q5_0 stores one fp16 scale `d` and applies
`fp16(d) * (q5 - 16)`. Q5_1 stores fp16 `d` plus fp16 minimum `m` and applies
`fp16(d) * q5 + fp16(m)`.

IQ4_NL uses a 32-value block with one fp16 scale and 16 packed 4-bit indices.
Unlike linear Q4 formats, each nibble is a codebook index into the nonlinear
table `[-127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69,
89, 113]`; the helper expands the low nibbles into values 0..15 and high
nibbles into values 16..31 before multiplying by `fp16(d)`.

MXFP4 uses a 32-value microscaling block with one E8M0 exponent byte and 16
packed 4-bit E2M1 indices. The helper matches ggml's `block_mxfp4` layout by
expanding low nibbles into values 0..15 and high nibbles into values 16..31,
looking up the official doubled E2M1 table
`[0, 1, 2, 3, 4, 6, 8, 12, 0, -1, -2, -3, -4, -6, -8, -12]`, then applying
`ggml_e8m0_to_fp32_half(e) * codebook[q4_index]`.

NVFP4 extends the same doubled E2M1 codebook to a 64-value block split into
four 16-value sub-blocks. Each sub-block has its own UE4M3 scale byte; the
helper matches ggml by decoding UE4M3 to the half-scale convention used with
`kvalues_mxfp4`, expanding low nibbles into the first 8 lanes of each sub-block
and high nibbles into the last 8 lanes, and collecting per-sub-block sums.

F32 is the baseline scalar GGML tensor type. The fixture now writes a readable
float32 prefix into `token_embd.weight`, and the helper keeps both the raw
little-endian 32-bit words and decoded values visible. This gives the GGUF
parser a scalar oracle before quantized or mixed-precision tensors are loaded.

F64 is a scalar GGML tensor type used less often for model weights, but it is
part of the format's type table and exercises 8-byte alignment, little-endian
64-bit raw words, and double-precision reconstruction. The fixture exposes it
as `blk.0.f64.weight` with both raw words and decoded double values.

I64 is the signed 64-bit integer scalar path. The fixture exposes it as
`blk.0.i64.weight`, keeps the raw little-endian 64-bit words visible, then
reconstructs signed `int64_t` values and compact stats. This locks the
eight-byte integer sign path separately from the F64 floating-point path.

I32 is a scalar GGML tensor type for signed integer payloads. The fixture
exposes it as `blk.0.i32.weight`, keeps the raw little-endian 32-bit words
visible, then reconstructs signed `int32_t` values and compact stats. This
exercises the non-floating scalar path without adding runtime dependencies.

I16 is the narrower signed integer scalar path. The fixture exposes it as
`blk.0.i16.weight`, keeps raw little-endian 16-bit words visible, then
reconstructs signed `int16_t` values and compact stats. This catches sign
extension and two-byte scalar byte-order handling separately from fp16.

I8 is the one-byte signed integer scalar path. The fixture exposes it as
`blk.0.i8.weight`, keeping the raw byte values visible before interpreting the
same bytes as signed `int8_t` values. This makes two's-complement sign handling
explicit without any endianness ambiguity.

F16 is the canonical half-precision scalar GGML tensor type. The fixture writes
readable fp16 words into `blk.0.f16.weight`, and the helper routes each raw
little-endian 16-bit value through `gguf_fp16_to_float32(raw)`. This separates
ordinary fp16 half conversion from BF16's high-word reinterpretation path.

BF16 is a scalar GGML tensor type rather than a quantized block layout. The
helper keeps the raw 16-bit words visible, reconstructs each value by placing
those bits in the high half of a float32 word (`uint32_t(raw) << 16`), and
reports the resulting prefix plus checksum/min/max/mean. This lets the parser
surface raw BF16 storage without materializing or depending on a model runtime.

IQ2_XXS is an importance-quantized 2-bit GGML block with one fp16 scale and
32 packed uint16 words for a 256-value super-block. The current InferLab preview
stops at structural unpacking: it exposes the per-32-value scale nibble,
effective scale `d * (0.5 + scale_nibble) * 0.25`, four grid indices, and four
sign-code selectors per group. Full numeric dequantization still needs the large
upstream `iq2xxs_grid`/`ksigns_iq2xs` lookup tables, so this slice keeps the
parser honest without vendoring those tables yet.

IQ4_XS extends that nonlinear codebook path to a 256-value super-block. It
stores fp16 `d`, a 16-bit `scales_h` plane, 4 bytes of `scales_l`, and 128
packed quant bytes. The helper reconstructs each 32-value sub-block scale as
`scale6 = low4(scales_l) | high2(scales_h) << 4`, uses `scale6 - 32` as the
signed scale, then applies
`fp16(d) * (scale6 - 32) * iq4nl_codebook[q4_index]`.

TQ1_0 is a 256-value ternary-oriented block with 48 `qs` bytes, 4 `qh` bytes,
and one fp16 scale `d` at the end of the block. The helper follows ggml's
packed base-3 extraction: 32 `qs` bytes expand five ternary digits each, the
remaining 16 `qs` bytes expand five ternary digits each, then the 4 `qh` bytes
expand four tail ternary digits each. Each ternary bucket is applied as
`fp16(d) * (q - 1)`.

TQ2_0 is a 256-value ternary-oriented block with 64 packed 2-bit bytes and one
fp16 scale `d` at the end of the block. The helper follows ggml's plane order:
for each 32-byte group it reads 2-bit planes `l=0..3`, expands each quant as
`q = (packed >> (2*l)) & 3`, then applies `fp16(d) * (q - 1)`.

Q8_K uses a 256-value K-quant block with one float32 scale `d`, 256 signed
int8 quant values, and 16 stored int16 block sums over 16-value groups. The
reference helper exposes both the direct dequantization `float32(d) * qs[i]`
and the stored sum side-channel used by dot-product kernels.

Q1_0 uses a 128-value block with one fp16 scale `d` and 16 bytes of one-bit
quants. Bits are read least-significant-bit first within each byte; each bit
maps directly to `bit ? fp16(d) : -fp16(d)`.

The default benchmark path now writes the deterministic synthetic fixture to
`results/raw/inferlab_fixture.gguf` before running the probe. That keeps the CI
path small while still exercising a real file-open and parse path, without
model license, network, or storage blockers.

## Focused Verification

This turn intentionally did not run verification per user request. The focused
verification command remains:

Run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/gguf_test
python3 scripts/generate_gguf_fixture.py --repo-root . --output results/raw/inferlab_fixture.gguf
./build/inferlab_gguf_probe --repo-root . --input results/raw/inferlab_fixture.gguf --output results/raw/gguf_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
python3 scripts/summarize_benchmarks.py --results results/raw --output results/tables/benchmark_summary.md --json-output results/raw/benchmark_summary.json
```

Expected synthetic fixture summary after the current code update:

| field | value |
|---|---:|
| GGUF version | 3 |
| metadata KV count | 7 |
| tensor count | 28 |
| quantized tensor count | 20 |
| fixture source | `file` |
| fixture input path | `results/raw/inferlab_fixture.gguf` |
| fixture file bytes | 5472 |
| known tensor data bytes | 3069 |
| alignment bytes | 32 |
| tensor data start offset | 2016 |
| header bytes | 24 |
| metadata bytes | 338 |
| tensor info bytes | 1638 |
| tensor-data padding bytes | 16 |
| tensor-data segment bytes | 3456 |
| first tensor | `token_embd.weight` |
| first tensor shape | `[16, 8]` |
| first tensor type | `f32` |
| first tensor absolute offset | 2016 |
| F32 raw bytes prefix | `00 00 80 3f 00 00 80 bf 00 00 00 3f 00 00 00 bf 00 00 00 40 00 00 00 c0 00 00 60 40 00 00 80 c0` |
| F32 raw value prefix | `[1065353216, 3212836864, 1056964608, 3204448256, 1073741824, 3221225472, 1080033280, 3229614080]` |
| F32 decode rule | `value = little_endian_float32(raw_4_bytes)` |
| F32 value prefix | `[1, -1, 0.5, -0.5, 2, -2, 3.5, -4]` |
| F32 preview count | 8 |
| F32 checksum | -0.5 |
| F32 min / max / mean | -4 / 3.5 / -0.0625 |
| F64 raw bytes prefix | `00 00 00 00 00 00 f0 3f 00 00 00 00 00 00 f0 bf 00 00 00 00 00 00 d0 3f 00 00 00 00 00 00 d0 bf` |
| F64 raw value prefix | `[4607182418800017408, 13830554455654793216, 4598175219545276416, 13821547256400052224]` |
| F64 decode rule | `value = little_endian_float64(raw_8_bytes)` |
| F64 value prefix | `[1, -1, 0.25, -0.25]` |
| F64 value count | 4 |
| F64 checksum | 0 |
| F64 min / max / mean | -1 / 1 / 0 |
| I64 raw bytes prefix | `01 00 00 00 00 00 00 00 ff ff ff ff ff ff ff ff 15 cd 5b 07 00 00 00 00 eb 32 a4 f8 ff ff ff ff` |
| I64 raw value prefix | `[1, 18446744073709551615, 123456789, 18446744073586094827]` |
| I64 decode rule | `value = int64_t(little_endian_raw_8_bytes)` |
| I64 value prefix | `[1, -1, 123456789, -123456789]` |
| I64 value count | 4 |
| I64 checksum | 0 |
| I64 min / max / mean | -123456789 / 123456789 / 0 |
| I32 raw bytes prefix | `01 00 00 00 ff ff ff ff 40 e2 01 00 c0 1d fe ff` |
| I32 raw value prefix | `[1, 4294967295, 123456, 4294843840]` |
| I32 decode rule | `value = int32_t(little_endian_raw_4_bytes)` |
| I32 value prefix | `[1, -1, 123456, -123456]` |
| I32 value count | 4 |
| I32 checksum | 0 |
| I32 min / max / mean | -123456 / 123456 / 0 |
| I16 raw bytes prefix | `01 00 ff ff d2 04 2e fb` |
| I16 raw value prefix | `[1, 65535, 1234, 64302]` |
| I16 decode rule | `value = int16_t(little_endian_raw_2_bytes)` |
| I16 value prefix | `[1, -1, 1234, -1234]` |
| I16 value count | 4 |
| I16 checksum | 0 |
| I16 min / max / mean | -1234 / 1234 / 0 |
| I8 raw bytes prefix | `01 ff 64 9c` |
| I8 raw value prefix | `[1, 255, 100, 156]` |
| I8 decode rule | `value = int8_t(raw_byte)` |
| I8 value prefix | `[1, -1, 100, -100]` |
| I8 value count | 4 |
| I8 checksum | 0 |
| I8 min / max / mean | -100 / 100 / 0 |
| F16 raw bytes prefix | `00 3c 00 40 00 c1 00 00 00 38 00 b8 00 43 00 c4` |
| F16 raw value prefix | `[15360, 16384, 49408, 0, 14336, 47104, 17152, 50176]` |
| F16 decode rule | `value = gguf_fp16_to_float32(raw_half)` |
| F16 value prefix | `[1, 2, -2.5, 0, 0.5, -0.5, 3.5, -4]` |
| F16 value count | 8 |
| F16 checksum | 0 |
| F16 min / max / mean | -4 / 3.5 / 0 |
| IQ2_XXS raw bytes prefix | `00 3c 01 02 03 04 01 c1 80 10 05 06 07 08 05 c3 01 21` |
| IQ2_XXS `d` as fp32 | 1 |
| IQ2_XXS qs word prefix | `[513, 1027, 49409, 4224, 1541, 2055, 49925, 8449]` |
| IQ2_XXS grid index prefix | `[1, 2, 3, 4, 5, 6, 7, 8]` |
| IQ2_XXS sign code prefix | `[1, 2, 3, 4, 5, 6, 7, 8]` |
| IQ2_XXS scale nibbles | `[1, 2, 3, 4, 5, 6, 7, 8]` |
| IQ2_XXS effective scales | `[0.375, 0.625, 0.875, 1.125, 1.375, 1.625, 1.875, 2.125]` |
| IQ2_XXS structural rule | `d * (0.5 + scale_nibble) * 0.25; grid/sign selectors feed upstream iq2xxs tables` |
| Q4_K tensor data bytes | 576 |
| Q5_K tensor data bytes | 352 |
| Q6_K tensor data bytes | 210 |
| Q3_K tensor data bytes | 110 |
| Q2_K tensor data bytes | 84 |
| Q8_0 tensor data bytes | 272 |
| Q8_1 tensor data bytes | 40 |
| Q8_K tensor data bytes | 292 |
| Q1_0 tensor data bytes | 18 |
| IQ4_XS tensor data bytes | 136 |
| TQ1_0 tensor data bytes | 56 |
| TQ2_0 tensor data bytes | 66 |
| MXFP4 tensor data bytes | 17 |
| NVFP4 tensor data bytes | 36 |
| BF16 tensor data bytes | 16 |
| F16 tensor data bytes | 16 |
| F64 tensor data bytes | 32 |
| I64 tensor data bytes | 32 |
| IQ2_XXS tensor data bytes | 66 |
| I32 tensor data bytes | 16 |
| I16 tensor data bytes | 8 |
| I8 tensor data bytes | 4 |
| Q4_0 tensor data bytes | 18 |
| Q4_1 tensor data bytes | 20 |
| Q5_0 tensor data bytes | 22 |
| Q5_1 tensor data bytes | 24 |
| IQ4_NL tensor data bytes | 18 |
| Q4_K first-block scale bytes | `00 01 02 03 04 05 06 07 08 09 0a 0b` |
| Q4_K first-block packed quant prefix | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q4_K first-block `d` as fp32 | 1 |
| Q4_K first-block `dmin` as fp32 | 0.5 |
| Q4_K first scale/min pair | `scale=0`, `min=4`, `effective_scale=0`, `effective_min=2` |
| Q4_K dequant rule | `value = fp16(d) * scale * q - fp16(dmin) * min` |
| Q4_K sub-block 0 dequant prefix | `[-2, -2, -2, -2, -2, -2, -2, -2]` |
| Q4_K sub-block 1 dequant prefix | `[-2.5 x16, -1.5 x16]` |
| Q4_K first-block dequant count | 256 |
| Q4_K first-block dequant checksum | 8288 |
| Q4_K first-block dequant min / max / mean | -3 / 150 / 32.375 |
| Q4_K first-block sub-block sums | `[-64, -64, 384, 128, 1920, 1296, 2400, 2288]` |
| Q4_K first-block unpacked nibble prefix | `[0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 15, 0]` |
| Q2_K first-block scale/min bytes | `00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff` |
| Q2_K first-block packed quant prefix | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q2_K first-block `d` / `dmin` as fp32 | 1 / 0.5 |
| Q2_K first-block dequant count | 256 |
| Q2_K first-block checksum | 1184 |
| Q2_K first-block min / max / mean | -7.5 / 32.5 / 4.625 |
| Q2_K first-block group sums | `[0, 16, 32, 48, -32, 40, -48, -56, 128, 144, 160, 176, 288, 520, -112, -120]` |
| Q2_K first / last scale-min pair | `scale=0,min=0` / `scale=15,min=15` |
| Q3_K first-block high-mask bytes prefix | `00 ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff` |
| Q3_K first-block packed quant prefix | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q3_K first-block scale bytes | `80 91 a2 b3 c4 d5 e6 f7 aa aa aa aa` |
| Q3_K first-block dequant count | 256 |
| Q3_K first-block checksum | 1920 |
| Q3_K first-block min / max / mean | -56 / 39 / 7.5 |
| Q3_K first-block group sums | `[0, 24, 40, 72, -16, 80, -24, 0, 160, 216, 200, 264, 336, 624, -56, 0]` |
| Q3_K first / last signed scale | 0 / 15 |
| Q5_K first-block scale bytes | `00 01 02 03 04 05 06 07 08 09 0a 0b` |
| Q5_K first-block high-bit bytes prefix | `ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff` |
| Q5_K first-block packed quant prefix | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q5_K first-block dequant count | 256 |
| Q5_K first-block checksum | 30816 |
| Q5_K first-block min / max / mean | -2 / 310 / 120.375 |
| Q5_K first-block sub-block sums | `[-64, 448, 1408, 1664, 6016, 5904, 7520, 7920]` |
| Q5_K first scale/min pair | `scale=0`, `min=4` |
| Q5_K last scale/min pair | `scale=11`, `min=0` |
| Q6_K first-block low-bit bytes prefix | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q6_K first-block high-bit bytes prefix | `ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff` |
| Q6_K first-block scale bytes | `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f` |
| Q6_K first-block dequant count | 256 |
| Q6_K first-block checksum | 41440 |
| Q6_K first-block min / max / mean | 0 / 345 / 161.875 |
| Q6_K first-block group sums | `[0, 376, 752, 1128, 1024, 1360, 1728, 2128, 3008, 3384, 3760, 4136, 3840, 4368, 4928, 5520]` |
| Q6_K first / last signed scale | 0 / 15 |
| Q8_0 first-block quant prefix | `[-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1]` |
| Q8_0 first-block `d` as fp32 | 1 |
| Q8_0 first-block dequant prefix | `[-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1]` |
| Q8_0 first-block quant sum | -16 |
| Q8_0 first-block dequant count | 32 |
| Q8_0 first-block checksum | -16 |
| Q8_0 first-block min / max / mean | -16 / 15 / -0.5 |
| Q8_1 first-block quant prefix | `[-16, -15, -14, -13, -12, -11, -10, -9]` |
| Q8_1 first-block `d` / `s` as fp32 | 0.5 / -8 |
| Q8_1 first-block expected `d * sum(qs)` | -8 |
| Q8_1 first-block dequant prefix | `[-8, -7.5, -7, -6.5, -6, -5.5, -5, -4.5]` |
| Q8_1 first-block quant sum | -16 |
| Q8_1 first-block dequant count | 32 |
| Q8_1 first-block checksum | -8 |
| Q8_1 first-block min / max / mean | -8 / 7.5 / -0.25 |
| Q8_K first-block quant prefix | `[-128, -127, -126, -125, -124, -123, -122, -121]` |
| Q8_K first-block `d` as fp32 | 0.25 |
| Q8_K first-block block sums prefix | `[-1928, -1672, -1416, -1160, -904, -648, -392, -136]` |
| Q8_K first-block block sum bytes prefix | `78 f8 78 f9 78 fa 78 fb 78 fc 78 fd 78 fe 78 ff` |
| Q8_K first-block dequant rule | `value = int8_quant * float32(d); bsums store sum(qs) for each 16-value group` |
| Q8_K first-block dequant prefix | `[-32, -31.75, -31.5, -31.25, -31, -30.75, -30.5, -30.25]` |
| Q8_K first-block quant sum / stored block sum total | -128 / -128 |
| Q8_K first-block dequant count | 256 |
| Q8_K first-block checksum | -32 |
| Q8_K first-block min / max / mean | -32 / 31.75 / -0.125 |
| Q1_0 first-block packed bit bytes prefix | `aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa` |
| Q1_0 first-block bit prefix | `[0, 1, 0, 1, 0, 1, 0, 1]` |
| Q1_0 first-block `d` as fp32 | 1 |
| Q1_0 first-block dequant rule | `value = bit ? fp16(d) : -fp16(d)` |
| Q1_0 first-block dequant prefix | `[-1, 1, -1, 1, -1, 1, -1, 1]` |
| Q1_0 first-block positive / negative bit count | 64 / 64 |
| Q1_0 first-block dequant count | 128 |
| Q1_0 first-block checksum | 0 |
| Q1_0 first-block min / max / mean | -1 / 1 / 0 |
| IQ4_XS first-block scales_h bytes | `aa aa` |
| IQ4_XS first-block scales_l bytes | `21 43 65 87` |
| IQ4_XS first / last raw scale | 33 / 40 |
| IQ4_XS first / last signed scale | 1 / 8 |
| IQ4_XS first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| IQ4_XS first-block codebook prefix | `[-127, -104, -83, -65, -49, -35, -22, -10]` |
| IQ4_XS first-block dequant rule | `value = fp16(d) * (scale6 - 32) * iq4nl_codebook[q4_index]` |
| IQ4_XS sub-block 0 / 1 dequant prefix | `[-127, -104, -83, -65, -49, -35, -22, -10]` / `[-254, -208, -166, -130, -98, -70, -44, -20]` |
| IQ4_XS first-block sub-block sums | `[-188, -376, -564, -752, -940, -1128, -1316, -1504]` |
| IQ4_XS first-block dequant count | 256 |
| IQ4_XS first-block checksum | -6768 |
| IQ4_XS first-block min / max / mean | -1016 / 904 / -26.4375 |
| TQ1_0 first-block packed `qs` prefix | `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00` |
| TQ1_0 first-block packed `qh` prefix | `00 80 ff 80` |
| TQ1_0 first-block `d` as fp32 | 1 |
| TQ1_0 first-block quant prefix | `[0 x16, 1 x16, 0 x16, 1 x16]` |
| TQ1_0 first-block quant counts | `[84, 88, 84]` |
| TQ1_0 first-block dequant rule | `value = fp16(d) * (ternary_quant - 1)` |
| TQ1_0 first-block dequant prefix | `[-1 x16, 0 x16, -1 x16, 0 x16]` |
| TQ1_0 first-block dequant count | 256 |
| TQ1_0 first-block checksum | 0 |
| TQ1_0 first-block min / max / mean | -1 / 1 / 0 |
| TQ2_0 first-block packed quant prefix | `24 24 24 24 24 24 24 24 24 24 24 24 24 24 24 24` |
| TQ2_0 first-block `d` as fp32 | 1 |
| TQ2_0 first-block quant prefix | `[0 x32, 1 x32, 2 x32, 0 x32]` |
| TQ2_0 first-block quant counts | `[128, 64, 64, 0]` |
| TQ2_0 first-block dequant rule | `value = fp16(d) * (two_bit_quant - 1)` |
| TQ2_0 first-block dequant prefix | `[-1 x32, 0 x32]` |
| TQ2_0 first-block dequant count | 256 |
| TQ2_0 first-block checksum | -64 |
| TQ2_0 first-block min / max / mean | -1 / 1 / -0.25 |
| IQ4_NL first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| IQ4_NL first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7]` |
| IQ4_NL first-block codebook prefix | `[-127, -104, -83, -65, -49, -35, -22, -10]` |
| IQ4_NL first-block `d` as fp32 | 0.5 |
| IQ4_NL first-block dequant rule | `value = fp16(d) * iq4nl_codebook[q4_index]` |
| IQ4_NL first-block dequant prefix | `[-63.5, -52, -41.5, -32.5, -24.5, -17.5, -11, -5]` |
| IQ4_NL first-block dequant count | 32 |
| IQ4_NL first-block checksum | -94 |
| IQ4_NL first-block min / max / mean | -63.5 / 56.5 / -2.9375 |
| MXFP4 first-block E8M0 byte | `80` |
| MXFP4 first-block E8M0 half scale | 1 |
| MXFP4 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| MXFP4 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7]` |
| MXFP4 first-block codebook prefix | `[0, 1, 2, 3, 4, 6, 8, 12]` |
| MXFP4 first-block dequant rule | `value = ggml_e8m0_to_fp32_half(e) * kvalues_mxfp4[q4_index]` |
| MXFP4 first-block dequant prefix | `[0, 1, 2, 3, 4, 6, 8, 12]` |
| MXFP4 first-block dequant count | 32 |
| MXFP4 first-block checksum | 0 |
| MXFP4 first-block min / max / mean | -12 / 12 / 0 |
| NVFP4 first-block scale bytes | `38 40 30 48` |
| NVFP4 first / last sub-block scale | 0.5 / 2 |
| NVFP4 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 f0 e1 d2 c3 b4 a5 96 87` |
| NVFP4 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7]` |
| NVFP4 first-block codebook prefix | `[0, 1, 2, 3, 4, 6, 8, 12]` |
| NVFP4 first-block dequant rule | `value = ggml_ue4m3_to_fp32(scale_byte) * kvalues_mxfp4[q4_index]` |
| NVFP4 sub-block 0 / 1 dequant prefix | `[0, 0.5, 1, 1.5, 2, 3, 4, 6]` / `[0, 1, 2, 3, 4, 6, 8, 12]` |
| NVFP4 first-block sub-block sums | `[0, 0, 0, 0]` |
| NVFP4 first-block dequant count | 64 |
| NVFP4 first-block checksum | 0 |
| NVFP4 first-block min / max / mean | -24 / 24 / 0 |
| BF16 raw bytes prefix | `80 3f 00 40 20 c0 00 00 00 3f 00 bf c0 3f 00 c0` |
| BF16 raw value prefix | `[16256, 16384, 49184, 0, 16128, 48896, 16320, 49152]` |
| BF16 decode rule | `value = reinterpret_float32(uint32_t(raw_bf16) << 16)` |
| BF16 value prefix | `[1, 2, -2.5, 0, 0.5, -0.5, 1.5, -2]` |
| BF16 value count | 8 |
| BF16 checksum | 0 |
| BF16 min / max / mean | -2.5 / 2 / 0 |
| Q4_0 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| Q4_0 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]` |
| Q4_0 first-block dequant rule | `value = fp16(d) * (q4 - 8)` |
| Q4_0 first-block dequant count | 32 |
| Q4_0 first-block checksum | -16 |
| Q4_0 first-block min / max / mean | -8 / 7 / -0.5 |
| Q4_1 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| Q4_1 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]` |
| Q4_1 first-block `d` / `m` as fp32 | 1 / 0.5 |
| Q4_1 first-block dequant rule | `value = fp16(d) * q4 + fp16(m)` |
| Q4_1 first-block dequant count | 32 |
| Q4_1 first-block checksum | 256 |
| Q4_1 first-block min / max / mean | 0.5 / 15.5 / 8 |
| Q5_0 first-block high-bit bytes prefix | `00 ff 55 aa` |
| Q5_0 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| Q5_0 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7]` |
| Q5_0 first-block dequant rule | `value = fp16(d) * (q5 - 16)` |
| Q5_0 first-block dequant count | 32 |
| Q5_0 first-block checksum | -16 |
| Q5_0 first-block min / max / mean | -16 / 15 / -0.5 |
| Q5_1 first-block high-bit bytes prefix | `00 ff 55 aa` |
| Q5_1 first-block packed quant prefix | `f0 e1 d2 c3 b4 a5 96 87 78 69 5a 4b 3c 2d 1e 0f` |
| Q5_1 first-block `d` / `m` as fp32 | 1 / 0.5 |
| Q5_1 first-block quant prefix | `[0, 1, 2, 3, 4, 5, 6, 7]` |
| Q5_1 first-block dequant rule | `value = fp16(d) * q5 + fp16(m)` |
| Q5_1 first-block dequant count | 32 |
| Q5_1 first-block checksum | 512 |
| Q5_1 first-block min / max / mean | 0.5 / 31.5 / 16 |

## What This Proves

- The parser is reading from raw bytes, not from a pre-decoded JSON fixture.
- The default native probe path reads those bytes from a generated `.gguf` file
  artifact instead of only constructing bytes inside the probe binary.
- Header counts drive the metadata and tensor-info loops.
- Metadata value types are interpreted from the binary enum.
- String-array metadata such as tokenizer token lists can be represented.
- Tensor relative offsets are checked against the file alignment.
- Tensor absolute offsets are computed from the aligned tensor-data section.
- GGML tensor type ids can be mapped to names, quantization flags, block sizes,
  and block byte sizes.
- Tensor byte spans are computed from the leading dimension, block size, row
  count, and GGML type-size bytes.
- Invalid quantized tensor leading dimensions are rejected before a runtime
  would walk an impossible block layout.
- The probe now makes the binary file structure inspectable directly: the
  header segment starts with `47 47 55 46`, which is the `GGUF` magic, followed
  by little-endian version and count fields.
- Segment offsets expose the padding inserted between tensor info records and
  the aligned tensor-data region.
- Q4_K preview exposes the first super-block scale, minimum scale, 12-byte
  scale table, packed 4-bit quant payload prefix, fp16 scale values converted
  to fp32, and selected unpacked 4-bit nibble values.
- Q4_K preview now decodes the 8 packed scale/min pairs, converts them into
  effective floating-point scale/min values, and applies the same affine
  dequant shape used by ggml reference code to selected sub-block values.
- The first synthetic Q4_K block is now dequantized across all 256 values for
  a compact reference oracle: prefix values, per-sub-block sums, checksum, min,
  max, and mean.
- The Q4_K reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q4_k_block`) instead of only in the probe JSON formatter,
  so future CPU/CUDA quantized matvec work can call the same oracle.
- The Q2_K reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q2_k_block`), so tests exercise 4-bit scale/min pairs,
  2-bit `qs` extraction across both 128-value halves, 16 group sums, and compact
  stats before any CUDA Q2_K kernel work starts.
- The generated fixture and probe JSON now expose Q2_K as
  `blk.0.ffn_up.weight`, including scale/min bytes, packed `qs` bytes,
  dequantized prefixes, 16 group sums, and compact stats.
- The Q3_K reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q3_k_block`), so tests exercise 6-bit scale unpacking,
  low two-bit quants, `hmask` high-bit handling, 16 group sums, and compact
  stats before any CUDA Q3_K kernel work starts.
- The generated fixture and probe JSON now expose Q3_K as
  `blk.0.attn_o.weight`, including `hmask` high-mask bytes, packed `qs`
  low-two-bit bytes, packed scale bytes, signed scales, dequantized prefixes,
  16 group sums, and compact stats.
- The Q5_K reference path now also lives in `inferlab_native_core`
  (`dequantize_gguf_q5_k_block`), so tests exercise the GGML high-bit plane
  and low-nibble payload before any CUDA Q5_K kernel work starts.
- The generated fixture and probe JSON now expose Q5_K as
  `blk.0.attn_k.weight`, including `qh` high-bit bytes, `qs` low-nibble bytes,
  dequantized prefixes, sub-block sums, and compact stats.
- The Q6_K reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q6_k_block`), so tests exercise signed scales, lower
  nibbles, high two-bit planes, 16 scale groups, and compact stats before any
  CUDA Q6_K kernel work starts.
- The generated fixture and probe JSON now expose Q6_K as
  `blk.0.attn_v.weight`, including `ql` low-bit bytes, `qh` high-two-bit
  bytes, signed scale bytes, dequantized prefixes, 16 group sums, and compact
  stats.
- Q8_0 preview exposes the first block scale, signed int8 quant prefix, and the
  dequantization rule `value = int8_quant * fp16(d)`.
- Q8_0 preview now applies the dequantization rule to the synthetic first-block
  prefix, so downstream reports can inspect decoded numeric values without
  guessing from raw bytes.
- The Q8_0 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q8_0_block`), so tests exercise fp16 scale conversion,
  signed int8 quant extraction, 32 values, quant sum, compact stats, and
  short-block rejection before any CUDA Q8_0 path is added.
- The Q8_1 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q8_1_block`), so tests exercise the GGML dot-product
  helper layout: float32 `d`, float32 stored `s = d * sum(qs)`, signed int8
  quant extraction, 32 values, compact stats, short-block rejection, and the
  corrected 40-byte type size.
- The generated fixture and probe JSON now expose Q8_1 as
  `blk.0.dot_q8_1.weight`, including float32 `d`, stored `s`, expected
  `d * sum(qs)`, signed int8 quant prefixes, dequantized prefixes, and compact
  stats.
- The Q8_K reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q8_k_block`), so the native test entry exercises float32
  scale extraction, 256 signed int8 quants, 16 stored int16 block sums, compact
  stats, and short-block rejection.
- The generated fixture and probe JSON now expose Q8_K as
  `blk.0.q8_k.weight`, including float32 `d`, signed int8 quant prefixes,
  stored block sums, dequantized prefixes, quant-sum comparison, and compact
  stats.
- The Q1_0 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_q1_0_block`), so the native test entry exercises fp16 scale
  conversion, least-significant-bit-first bit extraction, 128 direct sign
  values, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose Q1_0 as
  `blk.0.q1_0.weight`, including the fp16 scale, packed bit bytes, bit-value
  prefixes, dequantized prefixes, positive/negative bit counts, and compact
  stats.
- The IQ4_NL reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_iq4_nl_block`), so the native test entry exercises fp16
  scale conversion, nonlinear codebook lookup, low-half/high-half packed
  nibble order, 32 values, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose IQ4_NL as
  `blk.0.iq4_nl.weight`, including the fp16 scale, packed quant bytes,
  nonlinear codebook table, codebook-expanded values, dequantized prefixes,
  and compact stats.
- The MXFP4 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_mxfp4_block`), so the native test entry exercises E8M0
  half-scale conversion, the official doubled E2M1 codebook, low-half/high-half
  nibble expansion order, 32 values, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose MXFP4 as
  `blk.0.mxfp4.weight`, including the raw E8M0 byte, E8M0 half-scale value,
  packed quant bytes, official codebook table, codebook-expanded values,
  dequantized prefixes, and compact stats.
- The NVFP4 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_nvfp4_block`), so the native test entry exercises UE4M3
  per-sub-block scale conversion, the official doubled E2M1 codebook,
  low-half/high-half nibble expansion inside each 16-value sub-block, 64 values,
  sub-block sums, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose NVFP4 as
  `blk.0.nvfp4.weight`, including the four UE4M3 scale bytes, reconstructed
  scale records, packed quant bytes, official codebook table,
  codebook-expanded values, sub-block dequant prefixes, sub-block sums, and
  compact stats.
- The generated fixture and probe JSON now expose F32 as
  `token_embd.weight`, including raw little-endian 32-bit words, decoded
  float32 prefixes, and compact stats for the scalar baseline tensor.
- The generated fixture and probe JSON now expose F64 as
  `blk.0.f64.weight`, including raw little-endian 64-bit words,
  little-endian float64 reconstruction, value prefixes, and compact stats.
- The generated fixture and probe JSON now expose I64 as
  `blk.0.i64.weight`, including raw little-endian 64-bit words, signed int64
  reconstruction, value prefixes, and compact stats.
- The generated fixture and probe JSON now expose I32 as
  `blk.0.i32.weight`, including raw little-endian 32-bit words, signed int32
  reconstruction, value prefixes, and compact stats.
- The generated fixture and probe JSON now expose I16 as
  `blk.0.i16.weight`, including raw little-endian 16-bit words, signed int16
  reconstruction, value prefixes, and compact stats.
- The generated fixture and probe JSON now expose I8 as
  `blk.0.i8.weight`, including raw byte values, signed int8 reconstruction,
  value prefixes, and compact stats.
- The generated fixture and probe JSON now expose IQ2_XXS as
  `blk.0.iq2_xxs.weight`, including fp16 `d`, packed `qs` word prefixes,
  grid-index prefixes, sign-code prefixes, scale nibbles, effective scales,
  and the upstream-table structural decode rule.
- The generated fixture and probe JSON now expose F16 as
  `blk.0.f16.weight`, including raw little-endian 16-bit half words,
  `gguf_fp16_to_float32(raw_half)` reconstruction, value prefixes, and
  compact stats.
- The generated fixture and probe JSON now expose BF16 as
  `blk.0.bf16.weight`, including raw little-endian 16-bit words, the
  `uint32_t(raw) << 16` float32 reconstruction rule, value prefixes, and
  compact stats.
- The IQ4_XS reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_iq4_xs_block`), so the native test entry exercises fp16
  scale conversion, `scales_h`/`scales_l` 6-bit scale reconstruction, signed
  scale adjustment, nonlinear codebook lookup, 256 values, sub-block sums,
  compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose IQ4_XS as
  `blk.0.iq4_xs.weight`, including the fp16 scale, high/low scale bytes,
  reconstructed scale records, packed quant bytes, codebook-expanded values,
  dequantized prefixes, sub-block sums, and compact stats.
- The TQ1_0 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_tq1_0_block`), so the native test entry exercises fp16
  scale conversion, packed base-3 ternary extraction from `qs` and `qh`, 256
  values, quant bucket counts, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose TQ1_0 as
  `blk.0.tq1_0.weight`, including packed `qs` bytes, packed `qh` bytes, fp16
  tail scale, expanded ternary prefixes, dequantized prefixes, quant counts,
  and compact stats.
- The TQ2_0 reference path now lives in `inferlab_native_core`
  (`dequantize_gguf_tq2_0_block`), so the native test entry exercises fp16
  scale conversion, 2-bit plane extraction, 256 values, quant bucket counts,
  compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose TQ2_0 as
  `blk.0.tq2_0.weight`, including the packed 2-bit bytes, fp16 tail scale,
  plane-expanded quant prefixes, dequantized prefixes, quant counts, and
  compact stats.
- The legacy Q4_0/Q4_1 reference paths now live in `inferlab_native_core`
  (`dequantize_gguf_q4_0_block` and `dequantize_gguf_q4_1_block`). Tests lock
  the GGML 32-value nibble expansion order: low nibbles fill the first 16
  values and high nibbles fill the second 16 values.
- The generated fixture and probe JSON now expose Q4_0/Q4_1 as
  `blk.0.legacy_q4_0.weight` and `blk.0.legacy_q4_1.weight`, including packed
  nibble bytes, quant prefixes, dequantized prefixes, and compact stats.
- The legacy Q5_0/Q5_1 reference paths now live in `inferlab_native_core`
  (`dequantize_gguf_q5_0_block` and `dequantize_gguf_q5_1_block`). Tests lock
  `qh` high-bit reconstruction, packed low-nibble expansion, 32 dequantized
  values, compact stats, and short-block rejection.
- The generated fixture and probe JSON now expose Q5_0/Q5_1 as
  `blk.0.legacy_q5_0.weight` and `blk.0.legacy_q5_1.weight`, including `qh`
  high-bit bytes, packed nibble bytes, 32 quant values, dequantized prefixes,
  and compact stats.

## Not Yet Proven

- No real GGUF model has been parsed yet.
- Q4_K arithmetic currently covers the synthetic first block and direct helper
  tests, but it has not been compared against llama.cpp on a legal real-model
  fixture yet.
- The parser has not been compared against llama.cpp or gguf-py output.
- No mmap or page-fault behavior has been measured.
- Tensor payload bytes are synthetic and deterministic. F32/F64/I64/I32/I16/I8/F16/BF16/IQ2_XXS/Q1_0/Q4_0/Q4_1/Q5_0/Q5_1/Q2_K/Q3_K/Q4_K/Q5_K/Q6_K/Q8_0/Q8_1/Q8_K/IQ4_NL/IQ4_XS/MXFP4/NVFP4/TQ1_0/TQ2_0 block headers,
  selected unpacked nibbles, scale/min pairs, selected prefixes, and compact
  first-block stats are exposed, but no real-model quantized payload has been
  compared against llama.cpp yet.

## Next Bottom-Level Steps

1. When a small legal GGUF file is available, compare parsed metadata and tensor
   offsets against llama.cpp or gguf-py output.
2. Compare F32, F64, I64, I32, I16, I8, F16, BF16, IQ2_XXS, Q1_0, Q2_K, Q3_K, Q4_K, Q5_K, Q6_K, Q8_0, Q8_1, Q8_K, Q4_0,
   Q4_1, Q5_0, Q5_1, IQ4_NL, IQ4_XS, MXFP4, NVFP4, TQ1_0, and TQ2_0 helper output against llama.cpp/gguf-py on a small legal
   fixture, or extend another GGML reference-dequant family while no legal
   fixture is available.
