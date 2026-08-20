#!/usr/bin/env python3
"""Generate the deterministic tiny GGUF fixture used by parser probes."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path
from typing import Any, Iterable

try:
    from command_redaction import redact_path_error, redacted_path_label as path_label
    from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
except ModuleNotFoundError:
    from scripts.command_redaction import redact_path_error, redacted_path_label as path_label
    from scripts.repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root


GGUF_METADATA_TYPE_UINT32 = 4
GGUF_METADATA_TYPE_STRING = 8
GGUF_METADATA_TYPE_ARRAY = 9
GGUF_TENSOR_TYPE_F32 = 0
GGUF_TENSOR_TYPE_F16 = 1
GGUF_TENSOR_TYPE_Q4_0 = 2
GGUF_TENSOR_TYPE_Q4_1 = 3
GGUF_TENSOR_TYPE_Q5_0 = 6
GGUF_TENSOR_TYPE_Q5_1 = 7
GGUF_TENSOR_TYPE_Q8_0 = 8
GGUF_TENSOR_TYPE_Q8_1 = 9
GGUF_TENSOR_TYPE_Q2_K = 10
GGUF_TENSOR_TYPE_Q3_K = 11
GGUF_TENSOR_TYPE_Q4_K = 12
GGUF_TENSOR_TYPE_Q5_K = 13
GGUF_TENSOR_TYPE_Q6_K = 14
GGUF_TENSOR_TYPE_Q8_K = 15
GGUF_TENSOR_TYPE_IQ2_XXS = 16
GGUF_TENSOR_TYPE_IQ4_NL = 20
GGUF_TENSOR_TYPE_IQ4_XS = 23
GGUF_TENSOR_TYPE_TQ1_0 = 34
GGUF_TENSOR_TYPE_TQ2_0 = 35
GGUF_TENSOR_TYPE_MXFP4 = 39
GGUF_TENSOR_TYPE_NVFP4 = 40
GGUF_TENSOR_TYPE_BF16 = 30
GGUF_TENSOR_TYPE_I8 = 24
GGUF_TENSOR_TYPE_I16 = 25
GGUF_TENSOR_TYPE_I32 = 26
GGUF_TENSOR_TYPE_I64 = 27
GGUF_TENSOR_TYPE_F64 = 28
GGUF_TENSOR_TYPE_Q1_0 = 41


def _append_le(out: bytearray, value: int, width: int) -> None:
    out.extend(int(value).to_bytes(width, byteorder="little", signed=False))


def _append_u32(out: bytearray, value: int) -> None:
    _append_le(out, value, 4)


def _append_u64(out: bytearray, value: int) -> None:
    _append_le(out, value, 8)


def _append_string(out: bytearray, value: str) -> None:
    raw = value.encode("utf-8")
    _append_u64(out, len(raw))
    out.extend(raw)


def _append_metadata_string(out: bytearray, key: str, value: str) -> None:
    _append_string(out, key)
    _append_u32(out, GGUF_METADATA_TYPE_STRING)
    _append_string(out, value)


def _append_metadata_u32(out: bytearray, key: str, value: int) -> None:
    _append_string(out, key)
    _append_u32(out, GGUF_METADATA_TYPE_UINT32)
    _append_u32(out, value)


def _append_metadata_string_array(
    out: bytearray, key: str, values: Iterable[str]
) -> None:
    items = list(values)
    _append_string(out, key)
    _append_u32(out, GGUF_METADATA_TYPE_ARRAY)
    _append_u32(out, GGUF_METADATA_TYPE_STRING)
    _append_u64(out, len(items))
    for item in items:
        _append_string(out, item)


def _append_tensor(
    out: bytearray, name: str, dims: Iterable[int], tensor_type: int, relative_offset: int
) -> None:
    dims = list(dims)
    _append_string(out, name)
    _append_u32(out, len(dims))
    for dim in dims:
        _append_u64(out, dim)
    _append_u32(out, tensor_type)
    _append_u64(out, relative_offset)


def _write_u16(out: bytearray, offset: int, value: int) -> None:
    out[offset : offset + 2] = int(value).to_bytes(2, byteorder="little")


def _write_f32(out: bytearray, offset: int, value: float) -> None:
    out[offset : offset + 4] = struct.pack("<f", value)


def _write_f64(out: bytearray, offset: int, value: float) -> None:
    out[offset : offset + 8] = struct.pack("<d", value)


def _write_i16(out: bytearray, offset: int, value: int) -> None:
    out[offset : offset + 2] = int(value).to_bytes(
        2, byteorder="little", signed=True
    )


def _write_i8(out: bytearray, offset: int, value: int) -> None:
    out[offset : offset + 1] = int(value).to_bytes(
        1, byteorder="little", signed=True
    )


def _write_i32(out: bytearray, offset: int, value: int) -> None:
    out[offset : offset + 4] = int(value).to_bytes(
        4, byteorder="little", signed=True
    )


def _write_i64(out: bytearray, offset: int, value: int) -> None:
    out[offset : offset + 8] = int(value).to_bytes(
        8, byteorder="little", signed=True
    )


def _write_iq2_xxs_group(
    out: bytearray, offset: int, grid0: int, scale_nibble: int, sign0: int
) -> None:
    aux0 = grid0 | ((grid0 + 1) << 8) | ((grid0 + 2) << 16) | ((grid0 + 3) << 24)
    aux1 = (
        ((scale_nibble & 0x0F) << 28)
        | (sign0 & 0x7F)
        | (((sign0 + 1) & 0x7F) << 7)
        | (((sign0 + 2) & 0x7F) << 14)
        | (((sign0 + 3) & 0x7F) << 21)
    )
    out[offset : offset + 4] = aux0.to_bytes(4, byteorder="little")
    out[offset + 4 : offset + 8] = aux1.to_bytes(4, byteorder="little")


def _populate_tensor_data(out: bytearray, tensor_data_start: int) -> None:
    f32_start = tensor_data_start
    out[f32_start : f32_start + 512] = b"\x11" * 512
    for i, value in enumerate((1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 3.5, -4.0)):
        _write_f32(out, f32_start + i * 4, value)

    q4_k_start = tensor_data_start + 512
    for row in range(4):
        block_start = q4_k_start + row * 144
        _write_u16(out, block_start + 0, 0x3C00 + row)
        _write_u16(out, block_start + 2, 0x3800 + row)
        for i in range(12):
            out[block_start + 4 + i] = row * 16 + i
        for i in range(128):
            out[block_start + 16 + i] = (i + row) & 0xFF

    q5_k_start = tensor_data_start + 1088
    for row in range(2):
        block_start = q5_k_start + row * 176
        _write_u16(out, block_start + 0, 0x3C00 + row)
        _write_u16(out, block_start + 2, 0x3800 + row)
        for i in range(12):
            out[block_start + 4 + i] = row * 16 + i
        for i in range(32):
            out[block_start + 16 + i] = 0xFF if row == 0 else (0xAA ^ i) & 0xFF
        for i in range(128):
            out[block_start + 48 + i] = (i + row) & 0xFF

    q6_k_start = tensor_data_start + 1440
    block_start = q6_k_start
    for i in range(128):
        out[block_start + i] = i & 0xFF
    for i in range(64):
        out[block_start + 128 + i] = 0xFF
    for i in range(16):
        out[block_start + 192 + i] = i & 0xFF
    _write_u16(out, block_start + 208, 0x3C00)

    q3_k_start = tensor_data_start + 1664
    for i in range(32):
        out[q3_k_start + i] = 0xFF
    out[q3_k_start] = 0x00
    for i in range(64):
        out[q3_k_start + 32 + i] = i & 0xFF
    packed_scales = (0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7, 0xAA, 0xAA, 0xAA, 0xAA)
    for i, value in enumerate(packed_scales):
        out[q3_k_start + 96 + i] = value
    _write_u16(out, q3_k_start + 108, 0x3C00)

    q2_k_start = tensor_data_start + 1792
    for group in range(16):
        out[q2_k_start + group] = group | (group << 4)
    for i in range(64):
        out[q2_k_start + 16 + i] = i & 0xFF
    _write_u16(out, q2_k_start + 80, 0x3C00)
    _write_u16(out, q2_k_start + 82, 0x3800)

    q8_0_start = tensor_data_start + 1920
    for block in range(8):
        block_start = q8_0_start + block * 34
        _write_u16(out, block_start, 0x3C00 + block)
        for i in range(32):
            out[block_start + 2 + i] = (i - 16 + block) & 0xFF

    q4_0_start = tensor_data_start + 2208
    _write_u16(out, q4_0_start, 0x3C00)
    for i in range(16):
        out[q4_0_start + 2 + i] = i | ((15 - i) << 4)

    q4_1_start = tensor_data_start + 2240
    _write_u16(out, q4_1_start, 0x3C00)
    _write_u16(out, q4_1_start + 2, 0x3800)
    for i in range(16):
        out[q4_1_start + 4 + i] = i | ((15 - i) << 4)

    q5_0_start = tensor_data_start + 2272
    _write_u16(out, q5_0_start, 0x3C00)
    for i, value in enumerate((0x00, 0xFF, 0x55, 0xAA)):
        out[q5_0_start + 2 + i] = value
    for i in range(16):
        out[q5_0_start + 6 + i] = i | ((15 - i) << 4)

    q5_1_start = tensor_data_start + 2304
    _write_u16(out, q5_1_start, 0x3C00)
    _write_u16(out, q5_1_start + 2, 0x3800)
    for i, value in enumerate((0x00, 0xFF, 0x55, 0xAA)):
        out[q5_1_start + 4 + i] = value
    for i in range(16):
        out[q5_1_start + 8 + i] = i | ((15 - i) << 4)

    q8_1_start = tensor_data_start + 2336
    _write_f32(out, q8_1_start, 0.5)
    _write_f32(out, q8_1_start + 4, -8.0)
    for i in range(32):
        out[q8_1_start + 8 + i] = (i - 16) & 0xFF

    iq4_nl_start = tensor_data_start + 2400
    _write_u16(out, iq4_nl_start, 0x3800)
    for i in range(16):
        out[iq4_nl_start + 2 + i] = i | ((15 - i) << 4)

    q8_k_start = tensor_data_start + 2432
    _write_f32(out, q8_k_start, 0.25)
    for i in range(256):
        out[q8_k_start + 4 + i] = (i - 128) & 0xFF
    for group in range(16):
        block_sum = sum(group * 16 + lane - 128 for lane in range(16))
        _write_i16(out, q8_k_start + 260 + group * 2, block_sum)

    q1_0_start = tensor_data_start + 2752
    _write_u16(out, q1_0_start, 0x3C00)
    out[q1_0_start + 2 : q1_0_start + 18] = b"\xAA" * 16

    iq4_xs_start = tensor_data_start + 2784
    _write_u16(out, iq4_xs_start, 0x3C00)
    _write_u16(out, iq4_xs_start + 2, 0xAAAA)
    out[iq4_xs_start + 4 : iq4_xs_start + 8] = b"\x21\x43\x65\x87"
    for sub_block in range(8):
        for i in range(16):
            out[iq4_xs_start + 8 + sub_block * 16 + i] = i | ((15 - i) << 4)

    tq2_0_start = tensor_data_start + 2944
    out[tq2_0_start : tq2_0_start + 64] = b"\x24" * 64
    _write_u16(out, tq2_0_start + 64, 0x3C00)

    tq1_0_start = tensor_data_start + 3040
    out[tq1_0_start : tq1_0_start + 16] = b"\x00" * 16
    out[tq1_0_start + 16 : tq1_0_start + 32] = b"\x80" * 16
    out[tq1_0_start + 32 : tq1_0_start + 48] = b"\xff" * 16
    out[tq1_0_start + 48 : tq1_0_start + 52] = b"\x00\x80\xff\x80"
    _write_u16(out, tq1_0_start + 52, 0x3C00)

    mxfp4_start = tensor_data_start + 3104
    out[mxfp4_start] = 0x80
    for i in range(16):
        out[mxfp4_start + 1 + i] = i | ((15 - i) << 4)

    nvfp4_start = tensor_data_start + 3136
    out[nvfp4_start : nvfp4_start + 4] = b"\x38\x40\x30\x48"
    for sub_block in range(4):
        for i in range(8):
            out[nvfp4_start + 4 + sub_block * 8 + i] = i | ((15 - i) << 4)

    bf16_start = tensor_data_start + 3200
    bf16_values = (
        0x3F80,
        0x4000,
        0xC020,
        0x0000,
        0x3F00,
        0xBF00,
        0x3FC0,
        0xC000,
    )
    for i, raw in enumerate(bf16_values):
        _write_u16(out, bf16_start + i * 2, raw)

    f16_start = tensor_data_start + 3232
    f16_values = (
        0x3C00,
        0x4000,
        0xC100,
        0x0000,
        0x3800,
        0xB800,
        0x4300,
        0xC400,
    )
    for i, raw in enumerate(f16_values):
        _write_u16(out, f16_start + i * 2, raw)

    f64_start = tensor_data_start + 3264
    for i, value in enumerate((1.0, -1.0, 0.25, -0.25)):
        _write_f64(out, f64_start + i * 8, value)

    i32_start = tensor_data_start + 3296
    for i, value in enumerate((1, -1, 123456, -123456)):
        _write_i32(out, i32_start + i * 4, value)

    i16_start = tensor_data_start + 3328
    for i, value in enumerate((1, -1, 1234, -1234)):
        _write_i16(out, i16_start + i * 2, value)

    i8_start = tensor_data_start + 3360
    for i, value in enumerate((1, -1, 100, -100)):
        _write_i8(out, i8_start + i, value)

    i64_start = tensor_data_start + 3392
    for i, value in enumerate((1, -1, 123456789, -123456789)):
        _write_i64(out, i64_start + i * 8, value)

    iq2_xxs_start = tensor_data_start + 3424
    _write_u16(out, iq2_xxs_start, 0x3C00)
    for group in range(8):
        _write_iq2_xxs_group(
            out,
            iq2_xxs_start + 2 + group * 8,
            1 + group * 4,
            group + 1,
            1 + group * 4,
        )


def build_synthetic_gguf_fixture() -> bytes:
    out = bytearray()
    out.extend(b"GGUF")
    _append_u32(out, 3)
    _append_u64(out, 28)
    _append_u64(out, 7)
    _append_metadata_string(out, "general.architecture", "llama")
    _append_metadata_string(out, "general.name", "inferlab-gguf-fixture")
    _append_metadata_u32(out, "general.alignment", 32)
    _append_metadata_u32(out, "general.quantization_version", 2)
    _append_metadata_u32(out, "general.file_type", 15)
    _append_metadata_u32(out, "llama.context_length", 128)
    _append_metadata_string_array(
        out, "tokenizer.ggml.tokens", ("<s>", "hello", "world", "</s>")
    )
    _append_tensor(out, "token_embd.weight", (16, 8), GGUF_TENSOR_TYPE_F32, 0)
    _append_tensor(
        out, "blk.0.attn_q.weight", (256, 4), GGUF_TENSOR_TYPE_Q4_K, 512
    )
    _append_tensor(
        out, "blk.0.attn_k.weight", (256, 2), GGUF_TENSOR_TYPE_Q5_K, 1088
    )
    _append_tensor(
        out, "blk.0.attn_v.weight", (256, 1), GGUF_TENSOR_TYPE_Q6_K, 1440
    )
    _append_tensor(
        out, "blk.0.attn_o.weight", (256, 1), GGUF_TENSOR_TYPE_Q3_K, 1664
    )
    _append_tensor(
        out, "blk.0.ffn_up.weight", (256, 1), GGUF_TENSOR_TYPE_Q2_K, 1792
    )
    _append_tensor(out, "output.weight", (32, 8), GGUF_TENSOR_TYPE_Q8_0, 1920)
    _append_tensor(
        out, "blk.0.legacy_q4_0.weight", (32, 1), GGUF_TENSOR_TYPE_Q4_0, 2208
    )
    _append_tensor(
        out, "blk.0.legacy_q4_1.weight", (32, 1), GGUF_TENSOR_TYPE_Q4_1, 2240
    )
    _append_tensor(
        out, "blk.0.legacy_q5_0.weight", (32, 1), GGUF_TENSOR_TYPE_Q5_0, 2272
    )
    _append_tensor(
        out, "blk.0.legacy_q5_1.weight", (32, 1), GGUF_TENSOR_TYPE_Q5_1, 2304
    )
    _append_tensor(
        out, "blk.0.dot_q8_1.weight", (32, 1), GGUF_TENSOR_TYPE_Q8_1, 2336
    )
    _append_tensor(
        out, "blk.0.iq4_nl.weight", (32, 1), GGUF_TENSOR_TYPE_IQ4_NL, 2400
    )
    _append_tensor(
        out, "blk.0.q8_k.weight", (256, 1), GGUF_TENSOR_TYPE_Q8_K, 2432
    )
    _append_tensor(
        out, "blk.0.q1_0.weight", (128, 1), GGUF_TENSOR_TYPE_Q1_0, 2752
    )
    _append_tensor(
        out, "blk.0.iq4_xs.weight", (256, 1), GGUF_TENSOR_TYPE_IQ4_XS, 2784
    )
    _append_tensor(
        out, "blk.0.tq2_0.weight", (256, 1), GGUF_TENSOR_TYPE_TQ2_0, 2944
    )
    _append_tensor(
        out, "blk.0.tq1_0.weight", (256, 1), GGUF_TENSOR_TYPE_TQ1_0, 3040
    )
    _append_tensor(
        out, "blk.0.mxfp4.weight", (32, 1), GGUF_TENSOR_TYPE_MXFP4, 3104
    )
    _append_tensor(
        out, "blk.0.nvfp4.weight", (64, 1), GGUF_TENSOR_TYPE_NVFP4, 3136
    )
    _append_tensor(
        out, "blk.0.bf16.weight", (8, 1), GGUF_TENSOR_TYPE_BF16, 3200
    )
    _append_tensor(
        out, "blk.0.f16.weight", (8, 1), GGUF_TENSOR_TYPE_F16, 3232
    )
    _append_tensor(
        out, "blk.0.f64.weight", (4, 1), GGUF_TENSOR_TYPE_F64, 3264
    )
    _append_tensor(
        out, "blk.0.i32.weight", (4, 1), GGUF_TENSOR_TYPE_I32, 3296
    )
    _append_tensor(
        out, "blk.0.i16.weight", (4, 1), GGUF_TENSOR_TYPE_I16, 3328
    )
    _append_tensor(
        out, "blk.0.i8.weight", (4, 1), GGUF_TENSOR_TYPE_I8, 3360
    )
    _append_tensor(
        out, "blk.0.i64.weight", (4, 1), GGUF_TENSOR_TYPE_I64, 3392
    )
    _append_tensor(
        out, "blk.0.iq2_xxs.weight", (256, 1), GGUF_TENSOR_TYPE_IQ2_XXS, 3424
    )
    while len(out) % 32 != 0:
        out.append(0)
    tensor_data_start = len(out)
    out.extend(b"\x00" * 3520)
    _populate_tensor_data(out, tensor_data_start)
    return bytes(out)


def write_fixture(path: Path) -> int:
    payload = build_synthetic_gguf_fixture()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload)
    except OSError as exc:
        raise SystemExit(f"failed to write {path_label(path)}: {redact_path_error(str(exc), path)}") from exc
    return len(payload)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--output", default="results/raw/inferlab_fixture.gguf")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    output = resolve_repo_path(repo_root, args.output)
    size = write_fixture(output)
    print(f"{path_label(output)} ({size} bytes)")


if __name__ == "__main__":
    main()
