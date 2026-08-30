#!/usr/bin/env python3
"""Validate generated InferLab result files."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Collection, Dict, List, Mapping, Sequence, Tuple

from artifact_json import is_non_success_artifact_status
from command_redaction import redact_path_error, redact_sensitive_text, redacted_path_label as path_label
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root


AllowedNonSuccessStatuses = Mapping[str, Collection[str]]


def normalize_artifact_rule_path(value: str) -> str:
    raw = value.strip()
    path = Path(raw)
    if (
        not raw
        or "\\" in raw
        or path.is_absolute()
        or any(part == ".." for part in path.parts)
    ):
        raise ValueError("artifact path must be a relative POSIX path without '..'")
    normalized = path.as_posix()
    if normalized in {"", "."}:
        raise ValueError("artifact path must name a JSON file")
    return normalized


def parse_allowed_status_rules(values: Sequence[str]) -> Dict[str, Collection[str]]:
    rules: Dict[str, set[str]] = {}
    for value in values:
        if "=" not in value:
            raise ValueError("rule must use FILE=STATUS syntax")
        artifact, status = value.rsplit("=", 1)
        artifact = normalize_artifact_rule_path(artifact)
        normalized_status = status.strip().lower()
        if not is_non_success_artifact_status(normalized_status):
            raise ValueError("rule status must be a recognized non-success status")
        rules.setdefault(artifact, set()).add(normalized_status)
    return rules


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


def validate_json(
    path: Path,
    *,
    check_status: bool = True,
    allowed_non_success_statuses: Collection[str] = (),
) -> List[str]:
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
        elif check_status and is_non_success_artifact_status(payload.get("status")):
            status = str(payload.get("status")).strip().lower()
            allowed = {str(item).strip().lower() for item in allowed_non_success_statuses}
            if status not in allowed:
                errors.append(
                    f"{path_label(path)}: non-success artifact status: "
                    f"{redact_sensitive_text(payload.get('status'))}"
                )
    return errors


def validate_results(
    root: Path,
    *,
    check_status: bool = True,
    allowed_non_success_statuses: AllowedNonSuccessStatuses | None = None,
) -> List[str]:
    policy = {
        normalize_artifact_rule_path(path): {
            str(status).strip().lower() for status in statuses
        }
        for path, statuses in (allowed_non_success_statuses or {}).items()
    }
    paths, errors = iter_json_files(root)
    for path in paths:
        relative_path = path.relative_to(root).as_posix()
        errors.extend(
            validate_json(
                path,
                check_status=check_status,
                allowed_non_success_statuses=policy.get(relative_path, ()),
            )
        )
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument(
        "--allow-non-ok-status",
        action="store_true",
        help="Validate JSON structure without failing on artifact status.",
    )
    parser.add_argument(
        "--allow-status",
        action="append",
        default=[],
        metavar="FILE=STATUS",
        help=(
            "Allow one exact non-success status for one results-relative JSON "
            "file. Repeat for additional exceptions."
        ),
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.allow_non_ok_status and args.allow_status:
        raise SystemExit(
            "--allow-status cannot be combined with --allow-non-ok-status"
        )
    try:
        allowed_statuses = parse_allowed_status_rules(args.allow_status)
    except ValueError as exc:
        raise SystemExit(f"invalid --allow-status rule: {exc}") from exc
    repo_root = resolved_repo_root(args.repo_root)
    results = resolve_repo_path(repo_root, args.results)
    errors = validate_results(
        results,
        check_status=not args.allow_non_ok_status,
        allowed_non_success_statuses=allowed_statuses,
    )
    if errors:
        for error in errors:
            print(error)
        raise SystemExit(1)
    print(f"validated JSON files under {path_label(results)}")


if __name__ == "__main__":
    main()
