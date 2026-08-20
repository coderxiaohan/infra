#!/usr/bin/env python3
"""Shared quiet-success output policy for artifact-producing CLI glue."""

from __future__ import annotations

import argparse


def add_quiet_success_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress the success message; errors are still reported.",
    )


def emit_success(message: str, *, quiet: bool) -> None:
    if not quiet:
        print(message)
