#!/usr/bin/env python3
"""Tiny dependency-free GGUF binary header and metadata parser."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from artifact_json import path_label, path_payload, write_json_file as write_json_artifact_file
from command_redaction import redact_path_error

TYPE_NAMES = {0: "uint8", 1: "int8", 2: "uint16", 3: "int16", 4: "uint32", 5: "int32", 6: "float32", 7: "bool", 8: "string", 9: "array", 10: "uint64", 11: "int64", 12: "float64"}
MAX_DIMS = 16


class Reader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.offset = 0

    def take(self, n: int) -> bytes:
        if self.offset + n > len(self.data):
            raise ValueError("unexpected end of file")
        chunk = self.data[self.offset:self.offset + n]
        self.offset += n
        return chunk

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def u64(self) -> int:
        return struct.unpack("<Q", self.take(8))[0]

    def string(self) -> str:
        size = self.u64()
        return self.take(size).decode("utf-8", errors="replace")


def skip_value(reader: Reader, value_type: int, max_array_items: int):
    if value_type == 4:
        return reader.u32()
    if value_type == 8:
        return reader.string()
    if value_type == 9:
        item_type = reader.u32()
        count = reader.u64()
        if count > max_array_items:
            raise ValueError(f"metadata array count {count} exceeds --max-array-items {max_array_items}")
        values = [skip_value(reader, item_type, max_array_items) for _ in range(min(count, 8))]
        for _ in range(max(0, count - 8)):
            skip_value(reader, item_type, max_array_items)
        return {"item_type": TYPE_NAMES.get(item_type, item_type), "count": count, "preview": values}
    if value_type in (0, 1, 7):
        return list(reader.take(1))[0]
    if value_type in (2, 3):
        return int.from_bytes(reader.take(2), "little", signed=value_type == 3)
    if value_type == 5:
        return int.from_bytes(reader.take(4), "little", signed=True)
    if value_type in (10, 11):
        return int.from_bytes(reader.take(8), "little", signed=value_type == 11)
    if value_type == 6:
        return struct.unpack("<f", reader.take(4))[0]
    if value_type == 12:
        return struct.unpack("<d", reader.take(8))[0]
    raise ValueError(f"unsupported metadata type {value_type}")


def parse(path: Path, max_metadata: int = 8192, max_tensors: int = 100000, max_array_items: int = 8192) -> dict:
    if max_metadata <= 0:
        raise ValueError("max_metadata must be positive")
    if max_tensors <= 0:
        raise ValueError("max_tensors must be positive")
    if max_array_items <= 0:
        raise ValueError("max_array_items must be positive")
    data = path.read_bytes()
    reader = Reader(data)
    magic = reader.take(4)
    if magic != b"GGUF":
        raise ValueError(f"not a GGUF file: magic={magic!r}")
    version = reader.u32()
    tensor_count = reader.u64()
    metadata_count = reader.u64()
    if metadata_count > max_metadata:
        raise ValueError(f"metadata_count {metadata_count} exceeds --max-metadata {max_metadata}")
    if tensor_count > max_tensors:
        raise ValueError(f"tensor_count {tensor_count} exceeds --max-tensors {max_tensors}")
    metadata = []
    for _ in range(metadata_count):
        key = reader.string()
        value_type = reader.u32()
        metadata.append({"key": key, "type": TYPE_NAMES.get(value_type, value_type), "value": skip_value(reader, value_type, max_array_items)})
    tensors = []
    for _ in range(tensor_count):
        name = reader.string()
        n_dims = reader.u32()
        if n_dims > MAX_DIMS:
            raise ValueError(f"tensor {name!r} has too many dimensions: {n_dims}")
        dims = [reader.u64() for _ in range(n_dims)]
        tensor_type = reader.u32()
        relative_offset = reader.u64()
        tensors.append({"name": name, "dims": dims, "tensor_type": tensor_type, "relative_offset": relative_offset})
    return {
        "run_id": "gguf_binary_parser",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "backend": "python-binary-parser",
        "model": path.name,
        "precision": "mixed-gguf",
        "prompt_tokens": 0,
        "output_tokens": 0,
        **path_payload(path),
        "file_size_bytes": len(data),
        "limits": {"max_metadata": max_metadata, "max_tensors": max_tensors, "max_array_items": max_array_items},
        "header": {"magic": magic.decode("ascii"), "version": version, "tensor_count": tensor_count, "metadata_count": metadata_count},
        "metadata": metadata,
        "tensors": tensors,
        "tensor_info_end_offset": reader.offset,
    }


def write_json_file(path: Path, payload: dict) -> None:
    write_json_artifact_file(path, payload)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture")
    parser.add_argument("--output", default="results/raw/gguf_binary_parser.json")
    parser.add_argument("--max-metadata", type=int, default=8192)
    parser.add_argument("--max-tensors", type=int, default=100000)
    parser.add_argument("--max-array-items", type=int, default=8192)
    args = parser.parse_args()
    fixture_path = Path(args.fixture)
    try:
        payload = parse(fixture_path, args.max_metadata, args.max_tensors, args.max_array_items)
    except (OSError, ValueError, struct.error) as exc:
        raise SystemExit(f"failed to parse {path_label(fixture_path)}: {redact_path_error(exc, fixture_path)}") from exc
    path = Path(args.output)
    write_json_file(path, payload)
    print(f"wrote {path_label(path)}")


if __name__ == "__main__":
    main()
