#!/usr/bin/env python3
"""Shared repo-root path helpers for InferLab Python glue scripts."""

from __future__ import annotations

from pathlib import Path


def default_repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def resolved_repo_root(value: str) -> Path:
    return Path(value).expanduser().resolve()


def resolve_repo_path(repo_root: Path, value: str) -> Path:
    path = Path(value).expanduser()
    if path.is_absolute():
        return path
    return repo_root / path
