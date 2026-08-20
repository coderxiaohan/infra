#!/usr/bin/env python3
"""Compatibility wrapper for llama.cpp benchmark/preflight."""

from __future__ import annotations

from _compat import run_python_compat_entry


def main() -> None:
    raise SystemExit(run_python_compat_entry("benchmarks.llama_cpp"))


if __name__ == "__main__":
    main()
