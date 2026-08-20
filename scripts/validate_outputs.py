#!/usr/bin/env python3
"""Validate generated InferLab result files."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import List, Tuple

from command_redaction import redact_path_error, redact_sensitive_text, redacted_path_label as path_label
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root


def iter_json_files(root: Path) -> Tuple[List[Path], List[str]]:
    try:
        exists = root.exists()
    except OSError as exc:
        return [], [f"{path_label(root)}: unreadable results root: {redact_path_error(exc, root)}"]
    if not exists:
        return [], []

    paths: List[Path] = []
    errors: List[str] = []
    try:
        for path in root.rglob("*.json"):
            try:
                if path.is_file():
                    paths.append(path)
            except OSError as exc:
                errors.append(f"{path_label(path)}: unreadable JSON file probe: {redact_path_error(exc, path)}")
    except OSError as exc:
        errors.append(f"{path_label(root)}: unreadable results root scan: {redact_path_error(exc, root)}")
    return sorted(paths), errors


def validate_json(path: Path) -> List[str]:
    errors: List[str] = []
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        errors.append(f"{path_label(path)}: unreadable JSON file: {redact_path_error(exc, path)}")
    except UnicodeDecodeError as exc:
        errors.append(f"{path_label(path)}: invalid UTF-8 JSON encoding: {redact_path_error(exc, path)}")
    except json.JSONDecodeError as exc:
        errors.append(f"{path_label(path)}: invalid JSON: {redact_sensitive_text(str(exc))}")
    else:
        if not isinstance(payload, dict):
            errors.append(f"{path_label(path)}: invalid artifact root: expected object, got {type(payload).__name__}")
    return errors


def validate_results(root: Path) -> List[str]:
    paths, errors = iter_json_files(root)
    for path in paths:
        errors.extend(validate_json(path))
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    results = resolve_repo_path(repo_root, args.results)
    errors = validate_results(results)
    if errors:
        for error in errors:
            print(error)
        raise SystemExit(1)
    print(f"validated JSON files under {path_label(results)}")


if __name__ == "__main__":
    main()
