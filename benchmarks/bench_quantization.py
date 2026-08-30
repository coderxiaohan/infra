#!/usr/bin/env python3
"""Run the dependency-free toy quantization benchmark."""

from __future__ import annotations

import sys
from pathlib import Path

from _compat import run_compat_command


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    q_out = root / "results" / "raw" / "toy_quantizer.json"
    e_out = root / "results" / "raw" / "quantization_error_analysis.json"
    commands = [
        [sys.executable, str(root / "quantization" / "toy_quantizer.py"), "--output", str(q_out), *sys.argv[1:]],
        [sys.executable, str(root / "quantization" / "error_analysis.py"), "--input", str(q_out), "--output", str(e_out)],
    ]
    for command in commands:
        code = run_compat_command(command, cwd=root)
        if code:
            raise SystemExit(code)


if __name__ == "__main__":
    main()
