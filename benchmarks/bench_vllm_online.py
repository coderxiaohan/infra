#!/usr/bin/env python3
"""Compatibility wrapper for vLLM online/endpoint benchmark."""

from __future__ import annotations

from _compat import run_python_compat_entry


def main() -> None:
    raise SystemExit(run_python_compat_entry("benchmarks.vllm_online"))


if __name__ == "__main__":
    main()
