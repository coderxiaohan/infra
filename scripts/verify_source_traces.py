#!/usr/bin/env python3
"""Verify InferLab source notes against pinned upstream checkouts."""

from __future__ import annotations

import argparse
import json
import re
from email.parser import Parser
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from bounded_process import run_bounded_process
from command_redaction import artifact_path_payload as path_payload, redact_path_error, redacted_path_label as path_label
from path_status import safe_exists as safe_path_exists
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


DEFAULT_CHECKS = {
    "transformers": [
        {"path": "src/transformers/generation/utils.py", "patterns": ["generate", "past_key_values"]},
        {"path": "src/transformers/cache_utils.py", "patterns": ["Cache", "update"]},
    ],
    "vllm": [
        {"path": "vllm/engine/llm_engine.py", "patterns": ["add_request", "step"]},
        {"path": "vllm/core/scheduler.py", "patterns": ["Scheduler", "waiting"]},
    ],
    "tensorrt_llm": [
        {"path": "tensorrt_llm/runtime/session.py", "patterns": ["Session"]},
        {"path": "tensorrt_llm/builder.py", "patterns": ["Builder"]},
    ],
    "llama_cpp": [
        {"path": "src/llama.cpp", "patterns": ["llama_decode", "llama_model_load"]},
        {"path": "ggml/src/ggml.c", "patterns": ["ggml_graph"]},
    ],
}


def git_revision_status(path: Path) -> Dict[str, Any]:
    command = ["git", "-C", str(path), "rev-parse", "HEAD"]
    capture = run_bounded_process(
        command,
        cwd=None,
        timeout_seconds=5.0,
        stdout_limit_bytes=64 * 1024,
        stderr_limit_bytes=64 * 1024,
        launch_error_formatter=lambda exc: (
            "missing executable" if isinstance(exc, FileNotFoundError) else redact_path_error(exc, path)
        ),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    capture_metadata = {
        "capture_status": capture.status,
        "output_limit_exceeded": capture.output_limit_exceeded,
        "stdout_bytes": capture.stdout_bytes,
        "stderr_bytes": capture.stderr_bytes,
    }
    if capture.status == "launch_failed" and capture.error == "missing executable":
        return {
            "available": False,
            "status": "missing_git_executable",
            "revision": "",
            "error": capture.error,
            **capture_metadata,
        }
    if capture.timed_out:
        output = "\n".join(item for item in [stdout, stderr] if item)
        return {
            "available": False,
            "status": "git_probe_timeout",
            "revision": "",
            "error": redact_path_error(output or "timed out after 5s", path),
            **capture_metadata,
        }
    if capture.status not in {"ok", "nonzero_exit"}:
        output = "\n".join(item for item in [stderr.strip(), stdout.strip(), capture.error] if item)
        return {
            "available": False,
            "status": "git_probe_failed",
            "revision": "",
            "error": redact_path_error(output or capture.status, path),
            **capture_metadata,
        }
    revision = stdout.strip() if capture.returncode == 0 else ""
    if revision:
        return {"available": True, "status": "ok", "revision": revision, "error": "", **capture_metadata}
    error = "\n".join(item for item in [stderr.strip(), stdout.strip()] if item)
    return {
        "available": False,
        "status": "git_revision_unavailable",
        "revision": "",
        "returncode": capture.returncode,
        "error": redact_path_error(error, path),
        **capture_metadata,
    }


def safe_read_text(path: Path) -> Tuple[str, str]:
    try:
        return path.read_text(encoding="utf-8", errors="replace"), ""
    except OSError as exc:
        return "", redact_path_error(str(exc), path)


def default_config() -> Dict[str, Any]:
    return {"repositories": {}, "checks": DEFAULT_CHECKS}


def load_config_with_status(path: Path) -> Tuple[Dict[str, Any], Dict[str, Any]]:
    exists, exists_error = safe_path_exists(path)
    if exists_error:
        return default_config(), {"status": "config_probe_failed", "path": path_label(path), "error": exists_error, "fallback_used": True}
    if not exists:
        return default_config(), {"status": "missing_config", "path": path_label(path), "fallback_used": True}
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        return default_config(), {"status": "unreadable_config", "path": path_label(path), "error": redact_path_error(str(exc), path), "fallback_used": True}
    except UnicodeDecodeError as exc:
        return default_config(), {"status": "invalid_config_encoding", "path": path_label(path), "error": redact_path_error(str(exc), path), "fallback_used": True}
    except json.JSONDecodeError as exc:
        return default_config(), {"status": "invalid_config_json", "path": path_label(path), "error": redact_path_error(str(exc), path), "fallback_used": True}
    if not isinstance(payload, dict):
        return default_config(), {"status": "invalid_config_schema", "path": path_label(path), "error": f"expected object, got {type(payload).__name__}", "fallback_used": True}
    return payload, {"status": "ok", "path": path_label(path), "fallback_used": False}


def normalized_distribution_name(value: str) -> str:
    return re.sub(r"[-_.]+", "-", value).lower()


def source_identity_status(
    root: Path,
    root_exists: bool,
    identity: Dict[str, Any],
    git_status: Dict[str, Any],
) -> Dict[str, Any]:
    identity_type = str(identity.get("type", ""))
    if not identity_type:
        return {
            "type": "unconfigured",
            "status": "identity_unconfigured",
            "complete": False,
        }
    if not root_exists:
        return {
            "type": identity_type,
            "status": "root_missing",
            "complete": False,
        }
    if identity_type == "git":
        expected = str(identity.get("revision", "")).lower()
        observed = str(git_status.get("revision", "")).lower()
        expected_valid = bool(re.fullmatch(r"[0-9a-f]{40}", expected))
        complete = (
            expected_valid
            and git_status.get("status") == "ok"
            and observed == expected
        )
        if complete:
            status = "complete"
        elif not expected_valid:
            status = "invalid_expected_revision"
        elif git_status.get("status") != "ok":
            status = str(git_status.get("status", "git_revision_unavailable"))
        else:
            status = "revision_mismatch"
        return {
            "type": identity_type,
            "status": status,
            "complete": complete,
            "expected_revision": expected,
            "observed_revision": observed,
        }
    if identity_type == "python_distribution":
        expected_name = str(identity.get("distribution", ""))
        expected_version = str(identity.get("version", ""))
        metadata_relative = str(identity.get("metadata_path", ""))
        metadata_path = root / metadata_relative
        text, read_error = safe_read_text(metadata_path) if metadata_relative else ("", "missing metadata_path")
        metadata = Parser().parsestr(text, headersonly=True) if text and not read_error else {}
        observed_name = str(metadata.get("Name", ""))
        observed_version = str(metadata.get("Version", ""))
        complete = (
            bool(expected_name)
            and bool(expected_version)
            and not read_error
            and normalized_distribution_name(observed_name)
            == normalized_distribution_name(expected_name)
            and observed_version == expected_version
        )
        return {
            "type": identity_type,
            "status": "complete" if complete else "distribution_identity_mismatch",
            "complete": complete,
            "expected_distribution": expected_name,
            "expected_version": expected_version,
            "observed_distribution": observed_name,
            "observed_version": observed_version,
            **path_payload(metadata_path, "metadata_path", "metadata_path_redacted"),
            "metadata_read_error": read_error,
        }
    return {
        "type": identity_type,
        "status": "unsupported_identity_type",
        "complete": False,
    }


def verify_repo(
    name: str,
    root: Path,
    checks: List[Dict[str, Any]],
    configured: bool = True,
    identity: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    identity = identity or {}
    root_exists, root_probe_error = safe_path_exists(root) if configured else (False, "")
    root_exists = configured and root_exists
    records = []
    for check in checks:
        relative = check["path"]
        path = root / relative if configured else Path()
        path_exists, path_probe_error = safe_path_exists(path) if root_exists else (False, "")
        text, read_error = safe_read_text(path) if path_exists else ("", "")
        pattern_records = []
        for pattern in check.get("patterns", []):
            found = bool(re.search(pattern, text))
            pattern_records.append({"pattern": pattern, "found": found})
        records.append({
            "path": relative,
            "exists": root_exists and path_exists,
            "path_probe_error": path_probe_error,
            "read_error": read_error,
            "patterns": pattern_records,
            "ok": root_exists and path_exists and not read_error and all(item["found"] for item in pattern_records),
        })
    identity_type = str(identity.get("type", ""))
    git_status = (
        git_revision_status(root)
        if root_exists and identity_type == "git"
        else {
            "available": False,
            "status": "not_applicable" if root_exists else "root_missing_or_unconfigured",
            "revision": "",
            "error": root_probe_error,
        }
    )
    source_identity = source_identity_status(root, root_exists, identity, git_status)
    return {
        "name": name,
        **(path_payload(root, "root", "root_path_redacted") if configured else {"root": "", "root_path_redacted": False}),
        "root_configured": configured,
        "root_exists": root_exists,
        "root_probe_error": root_probe_error,
        "git_commit": str(git_status.get("revision", "")),
        "git_revision": git_status,
        "source_identity": source_identity,
        "checks": records,
        "ok": root_exists and source_identity["complete"] and all(item["ok"] for item in records),
    }


def resolve_external_or_repo_path(repo_root: Path, value: str) -> Path:
    return resolve_repo_path(repo_root, value).resolve() if value else Path()


def build_payload(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    config_path = resolve_repo_path(repo_root, args.config)
    config, config_read_status = load_config_with_status(config_path)
    repositories = dict(config.get("repositories", {}))
    for item in args.repo:
        if "=" in item:
            name, path = item.split("=", 1)
            configured = repositories.get(name)
            if isinstance(configured, dict):
                repositories[name] = {**configured, "root": path}
            else:
                repositories[name] = path
    checks = config.get("checks", DEFAULT_CHECKS)
    repos = []
    for name, repo_checks in checks.items():
        repository = repositories.get(name, "")
        if isinstance(repository, dict):
            configured_path = str(repository.get("root", ""))
            identity = repository.get("identity", {})
            identity = identity if isinstance(identity, dict) else {}
        else:
            configured_path = str(repository)
            identity = {}
        root = resolve_external_or_repo_path(repo_root, configured_path)
        repos.append(
            verify_repo(
                name,
                root,
                repo_checks,
                bool(configured_path),
                identity,
            )
        )
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root", "repo_root_path_redacted"),
        "status": "complete" if repos and all(item["ok"] for item in repos) else "planned",
        "config": {
            **path_payload(args.config, "config_path", "config_path_redacted"),
            **path_payload(config_path, "resolved_config_path"),
            "read_status": config_read_status,
        },
        "repositories": repos,
        "summary": {
            "repository_count": len(repos),
            "ready_repositories": sum(1 for item in repos if item["ok"]),
            "check_count": sum(len(item["checks"]) for item in repos),
            "failed_checks": sum(1 for repo in repos for check in repo["checks"] if not check["ok"]),
            "identity_issue_count": sum(1 for repo in repos if not repo.get("source_identity", {}).get("complete")),
            "git_probe_issue_count": sum(
                1
                for repo in repos
                if repo.get("source_identity", {}).get("type") == "git"
                and repo.get("git_revision", {}).get("status") != "ok"
            ),
        },
    }




def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--config", default="configs/source_trace_targets.example.json")
    parser.add_argument("--repo", action="append", default=[], help="Override repository path, e.g. vllm=/src/vllm")
    parser.add_argument("--output", default="results/raw/source_trace_verification.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    payload = build_payload(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
