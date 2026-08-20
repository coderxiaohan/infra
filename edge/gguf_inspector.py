#!/usr/bin/env python3
"""Human-friendly GGUF inspector wrapper around the binary parser."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

from gguf_binary_parser import parse

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from artifact_json import path_label, write_json_file as write_json_artifact_file
from command_redaction import redact_path_error


def write_json_file(path: Path, payload: dict) -> None:
    write_json_artifact_file(path, payload)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture")
    parser.add_argument("--output", default="results/raw/gguf_inspector.json")
    parser.add_argument("--max-metadata", type=int, default=8192)
    parser.add_argument("--max-tensors", type=int, default=100000)
    parser.add_argument("--max-array-items", type=int, default=8192)
    args = parser.parse_args()
    fixture_path = Path(args.fixture)
    try:
        payload = parse(fixture_path, args.max_metadata, args.max_tensors, args.max_array_items)
    except (OSError, ValueError, struct.error) as exc:
        raise SystemExit(f"failed to parse {path_label(fixture_path)}: {redact_path_error(exc, fixture_path)}") from exc
    payload["tensor_names"] = [tensor["name"] for tensor in payload["tensors"]]
    payload["metadata_keys"] = [item["key"] for item in payload["metadata"]]
    path = Path(args.output)
    write_json_file(path, payload)
    print(f"wrote {path_label(path)}")


if __name__ == "__main__":
    main()
