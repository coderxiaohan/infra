#!/usr/bin/env python3
"""Generate lightweight Markdown tables from JSON results.

This intentionally avoids plotting dependencies during M0. Later milestones can
add matplotlib or pandas if the dependency is justified.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List, Tuple

from command_redaction import redact_path_error, redacted_path_label as path_label
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


def iter_json_files(root: Path) -> Tuple[List[Path], List[Dict[str, Any]]]:
    issues: List[Dict[str, Any]] = []
    try:
        exists = root.exists()
    except OSError as exc:
        return [], [{"file": path_label(root), "status": "unreadable_results_root_probe", "error": redact_path_error(str(exc), root)}]
    if not exists:
        return [], [{"file": path_label(root), "status": "missing_results_root", "error": "results root does not exist"}]

    paths: List[Path] = []
    try:
        for path in root.rglob("*.json"):
            try:
                if path.is_file():
                    paths.append(path)
            except OSError as exc:
                issues.append({"file": path_label(path), "status": "unreadable_json_probe", "error": redact_path_error(str(exc), path)})
    except OSError as exc:
        issues.append({"file": path_label(root), "status": "unreadable_results_root_scan", "error": redact_path_error(str(exc), root)})
    return sorted(paths), issues


def load_json_files(root: Path) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    rows: List[Dict[str, Any]] = []
    paths, issues = iter_json_files(root)
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except OSError as exc:
            issues.append({"file": path_label(path), "status": "unreadable_json", "error": redact_path_error(str(exc), path)})
            continue
        except UnicodeDecodeError as exc:
            issues.append({"file": path_label(path), "status": "invalid_encoding", "error": redact_path_error(str(exc), path)})
            continue
        except json.JSONDecodeError as exc:
            issues.append({"file": path_label(path), "status": "invalid_json", "error": redact_path_error(str(exc), path)})
            continue
        if isinstance(data, dict):
            data["_file"] = path_label(path)
            rows.append(data)
        else:
            issues.append({"file": path_label(path), "status": "invalid_artifact", "error": f"expected object, got {type(data).__name__}"})
    return rows, issues


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output", default="results/tables/result_index.md")
    args = parser.parse_args()

    repo_root = resolved_repo_root(args.repo_root)
    rows, issues = load_json_files(resolve_repo_path(repo_root, args.results))
    output = resolve_repo_path(repo_root, args.output)
    lines = ["# Result Index", "", "| file | keys |", "|---|---|"]
    for row in rows:
        keys = ", ".join(sorted(k for k in row.keys() if k != "_file"))
        lines.append(f"| `{row['_file']}` | {keys} |")
    if not rows:
        lines.append("| _none_ | _no JSON result files found_ |")
    if issues:
        lines.extend(["", "## Result File Issues", "", "| file | status | error |", "|---|---|---|"])
        for issue in issues:
            error = str(issue.get("error", "")).replace("|", "\\|")
            lines.append(f"| `{issue.get('file')}` | `{issue.get('status')}` | {error} |")
    write_text_file(output, "\n".join(lines) + "\n")
    print(path_label(output))


if __name__ == "__main__":
    main()
