# Quantization Trade-Off Notes

## Scope

This report records the first native C++ weight-only quantization slice. It is
not GPTQ/AWQ yet. The goal is to reproduce the low-level mechanics before using
production quantizers:

- symmetric per-group scale
- INT8 quantize/dequantize
- signed INT4 quantize/dequantize
- INT4 two-values-per-byte packing
- weight-only matvec reference path
- fused dequant + matvec reference path that avoids materializing the full FP32
  weight matrix
- error and storage accounting

The code lives in:

```text
native_core/include/inferlab/quantization.hpp
native_core/src/quantization.cpp
native_core/src/quantization_probe_main.cpp
native_core/src/quantization_sweep_main.cpp
native_core/src/quantization_channel_compare_main.cpp
native_core/tests/quantization_test.cpp
```

## Mechanics

For every row and group:

```text
scale = max(abs(weight[group])) / qmax
q = round(weight / scale)
dequant = q * scale
```

Current `qmax` values:

| dtype | qmax | range |
|---|---:|---|
| INT8 | 127 | -127 to 127 |
| INT4 | 7 | -8 to 7 |

INT4 values are stored as signed 4-bit nibbles:

```text
byte = low_nibble | (high_nibble << 4)
```

## Probe

Run:

```bash
./build/inferlab_quantization_probe --repo-root . --output results/raw/quantization_probe.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default config:

| setting | value |
|---|---:|
| rows | 4 |
| cols | 16 |
| group size | 8 |
| fp32 bytes | 256 |

Observed output:

| dtype | storage bytes | ratio vs FP32 | max abs error | mean abs error |
|---|---:|---:|---:|---:|
| INT8 | 96 | 0.375 | 0.003937 | 0.001784 |
| INT4 | 64 | 0.25 | 0.071429 | 0.031696 |

The metadata scales matter. For this tiny matrix, scale metadata is a large part
of storage, so INT8 is not exactly 25% of FP32. On larger matrices with larger
groups, the packed weights dominate and the ratio approaches the expected
weight-only storage.

The probe also compares materialized dequantization with fused dequant + matvec:

| path comparison | max abs error | mean abs error |
|---|---:|---:|
| INT4 fused vs materialized | 0.0 | 0.0 |

This is still a CPU reference path, but it has the same key shape as a CUDA
decode-stage kernel: read quantized weight, read scale, dequantize in registers,
and immediately multiply by the activation vector.

## Group Size Sweep

Run:

```bash
./build/inferlab_quantization_sweep --repo-root . --output results/raw/quantization_sweep.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default INT4 sweep:

| group size | scale count | storage bytes | ratio vs FP32 | max abs error | mean abs error |
|---:|---:|---:|---:|---:|---:|
| 2 | 32 | 160 | 0.625 | 0.059524 | 0.012649 |
| 4 | 16 | 96 | 0.375 | 0.059524 | 0.019531 |
| 8 | 8 | 64 | 0.25 | 0.071429 | 0.024368 |
| 16 | 4 | 48 | 0.1875 | 0.071429 | 0.035156 |

The trend is the expected trade-off: smaller groups preserve local scale
information and reduce error, but scale metadata grows quickly. Larger groups
are cheaper to store but make unrelated channels share one scale, increasing
mean error.

## Per-Channel vs Per-Group

Run:

```bash
./build/inferlab_quantization_channel_compare --repo-root . --output results/raw/quantization_channel_compare.json
python3 scripts/validate_outputs.py --repo-root . --results results/raw
```

Default comparison:

| mode | group size | scale count | storage bytes | ratio vs FP32 | max abs error | mean abs error |
|---|---:|---:|---:|---:|---:|---:|
| per-channel | 16 | 4 | 48 | 0.1875 | 0.142857 | 0.061607 |
| per-group | 4 | 16 | 96 | 0.375 | 0.142857 | 0.032701 |

In this toy matrix, the first half of every row has much smaller magnitude than
the second half. Per-channel quantization forces both halves to share one row
scale, so the small values lose more precision. Per-group quantization uses more
scale metadata but lowers mean error.

## Correctness Checks

Native tests cover:

- signed INT4 pack/unpack roundtrip
- per-group scale metadata count
- INT8 dequantization error bound
- INT4 dequantization error bound
- weight-only matvec matches a dequantized reference path
- fused dequant + matvec matches the materialized reference

Run:

```bash
ctest --test-dir build --output-on-failure
```

## Next Bottom-Level Steps

1. Add CUDA dequant + GEMV kernel for decode-stage experiments.
2. Compare toy quantization against AWQ/GPTQ outputs once model dependencies are
   introduced.
