#!/usr/bin/env python3
"""Run or dry-run InferLab benchmark-matrix entries.

The matrix runner is an orchestration feature, not a test suite. By default it
does a dry run and writes the commands it would execute. Pass --execute on a
prepared machine to actually run selected entries.
"""

from __future__ import annotations

import argparse
import json
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List

from benchmark_matrix_commands import command_for_entry, load_matrix_entries_with_status
from bounded_process import run_bounded_process
from cli_validation import require_int_range
from command_redaction import (
    artifact_command_parts,
    artifact_output_tail,
    artifact_path_payload as path_payload,
    command_line,
    redact_path_error,
    redact_sensitive_text,
    redact_value,
    redacted_path_label as path_label,
)
from repo_paths import default_repo_root, resolve_repo_path
from text_output import try_write_text_file as write_text_file


EXPERIMENT_MATRIX_STREAM_LIMIT_BYTES = 16 * 1024 * 1024


def artifact_path(repo_root: Path, artifact: str) -> Path | None:
    if not artifact:
        return None
    return resolve_repo_path(repo_root, artifact)


def path_mtime_ns(path: Path) -> tuple[bool, int | None, str]:
    try:
        exists = path.exists()
    except OSError as exc:
        return False, None, redact_path_error(exc, path)
    if not exists:
        return False, None, ""
    try:
        return True, path.stat().st_mtime_ns, ""
    except OSError as exc:
        return True, None, redact_path_error(exc, path)


def artifact_status(
    repo_root: Path,
    artifact: str,
    previous_mtime_ns: int | None = None,
) -> str:
    path = artifact_path(repo_root, artifact)
    if path is None:
        return "not_declared"
    exists, current_mtime_ns, probe_error = path_mtime_ns(path)
    if probe_error:
        return "unreadable_json"
    if not exists:
        return "missing_artifact"
    if previous_mtime_ns is not None and current_mtime_ns is not None and current_mtime_ns <= previous_mtime_ns:
        return "stale_artifact"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError:
        return "unreadable_json"
    except UnicodeDecodeError:
        return "invalid_encoding"
    except json.JSONDecodeError:
        return "invalid_json"
    if not isinstance(payload, dict):
        return "invalid_artifact"
    status = payload.get("status")
    return str(status) if isinstance(status, str) and status else "present_json"


def command_status(returncode: Any, artifact_state: str) -> str:
    if returncode != 0:
        return "failed"
    if artifact_state in {"not_declared", "ok", "pass", "passed", "success", "ready", "present_json"}:
        return "passed"
    return f"partial_artifact_{artifact_state}"


INVALID_COMMAND_SOURCES = {"invalid_command", "invalid_replay_command"}


def run_command(command: List[str], timeout_seconds: int, cwd: Path) -> Dict[str, Any]:
    launch_error_type = ""

    def format_launch_error(exc: BaseException) -> str:
        nonlocal launch_error_type
        launch_error_type = type(exc).__name__
        return redact_path_error(exc, cwd, *command)

    capture = run_bounded_process(
        command,
        cwd=cwd,
        timeout_seconds=float(timeout_seconds),
        stdout_limit_bytes=EXPERIMENT_MATRIX_STREAM_LIMIT_BYTES,
        stderr_limit_bytes=EXPERIMENT_MATRIX_STREAM_LIMIT_BYTES,
        launch_error_formatter=format_launch_error,
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    result = {
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_tail": artifact_output_tail(stdout, command, cwd, 80),
        "stderr_tail": artifact_output_tail(stderr, command, cwd, 80),
        "timed_out": capture.timed_out,
    }
    if capture.status in {"ok", "nonzero_exit"}:
        return result
    if capture.status == "timed_out":
        result["returncode"] = None
        return result

    result["returncode"] = None
    result["error_type"] = launch_error_type or capture.status
    diagnostic = redact_sensitive_text(capture.error or capture.status)
    if not result["stderr_tail"]:
        result["stderr_tail"] = diagnostic
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--matrix", default="configs/benchmark_matrix.yaml")
    parser.add_argument("--output", default="results/raw/experiment_matrix_run.json")
    parser.add_argument("--execute", action="store_true")
    parser.add_argument(
        "--use-replay-command",
        action="store_true",
        help="Use replay_command when a selected matrix entry provides it.",
    )
    parser.add_argument("--select", action="append", default=[], help="Run only entries with this name. Can be repeated.")
    parser.add_argument("--timeout-seconds", type=int, default=300)
    args = parser.parse_args()
    require_int_range("--timeout-seconds", args.timeout_seconds, 1, 86_400)

    repo_root = Path(args.repo_root).expanduser().resolve()
    matrix_path = resolve_repo_path(repo_root, args.matrix)
    runs, matrix_read_status = load_matrix_entries_with_status(matrix_path)
    selected = set(args.select)
    available_names = [str(run.get("name", "unnamed")) for run in runs]
    available_name_set = set(available_names)
    unmatched_select = [name for name in args.select if name not in available_name_set]
    invalid_selection = bool(unmatched_select)
    records: List[Dict[str, Any]] = []
    for run in runs:
        name = str(run.get("name", "unnamed"))
        if selected and name not in selected:
            continue
        matrix_command = command_for_entry(run, args.use_replay_command)
        redacted_command = (
            command_line(artifact_command_parts(matrix_command.parts))
            if matrix_command.parts
            else redact_sensitive_text(matrix_command.command)
        )
        record: Dict[str, Any] = {
            "name": name,
            "status": "planned",
            "command": redacted_command,
            "command_source": matrix_command.source,
            "artifact": matrix_command.artifact,
            "entry": redact_value(run),
        }
        if invalid_selection:
            record["status"] = "skipped_invalid_selection"
            record["selection_error"] = "one_or_more_requested_entries_not_found"
            records.append(record)
            continue
        if matrix_command.source in INVALID_COMMAND_SOURCES:
            record["status"] = "invalid_command"
            record["returncode"] = None
            record["command_error"] = redact_sensitive_text(matrix_command.command)
            records.append(record)
            continue
        if args.execute:
            if not matrix_command.parts:
                record["status"] = "failed"
                record["returncode"] = None
                record["stderr_tail"] = "no executable command could be derived"
                records.append(record)
                continue
            artifact_target = artifact_path(repo_root, matrix_command.artifact)
            if artifact_target is not None:
                _, previous_mtime_ns, baseline_probe_error = path_mtime_ns(artifact_target)
            else:
                previous_mtime_ns = None
                baseline_probe_error = ""
            result = run_command(matrix_command.parts, args.timeout_seconds, repo_root)
            record.update(result)
            if result.get("timed_out"):
                record["status"] = "timeout"
            else:
                if baseline_probe_error:
                    current_artifact_status = "baseline_probe_failed"
                    record["artifact_baseline_probe_error"] = baseline_probe_error
                else:
                    current_artifact_status = artifact_status(
                        repo_root,
                        matrix_command.artifact,
                        previous_mtime_ns,
                    )
                record["artifact_status"] = current_artifact_status
                record["status"] = command_status(
                    result.get("returncode"),
                    current_artifact_status,
                )
        records.append(record)

    planned_count = sum(1 for item in records if item["status"] == "planned")
    passed_count = sum(1 for item in records if item["status"] == "passed")
    partial_count = sum(1 for item in records if str(item["status"]).startswith("partial"))
    failed_count = sum(1 for item in records if item["status"] == "failed")
    invalid_command_count = sum(1 for item in records if item["status"] == "invalid_command")
    timeout_count = sum(1 for item in records if item["status"] == "timeout")
    skipped_invalid_selection_count = sum(1 for item in records if item["status"] == "skipped_invalid_selection")
    if matrix_read_status.get("status") != "ok":
        overall_status = "matrix_unavailable"
    elif invalid_selection:
        overall_status = "invalid_selection"
    elif invalid_command_count:
        overall_status = "matrix_command_invalid"
    elif args.execute and (failed_count or timeout_count):
        overall_status = "failed"
    elif args.execute and partial_count:
        overall_status = "partial"
    elif args.execute:
        overall_status = "ok"
    else:
        overall_status = "planned"

    payload = {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "status": overall_status,
        **path_payload(repo_root, "repo_root"),
        **path_payload(args.matrix, "matrix", "matrix_path_redacted"),
        **path_payload(matrix_path, "resolved_matrix"),
        "matrix_read_status": matrix_read_status,
        "mode": "execute" if args.execute else "dry_run",
        "command_policy": "prefer_replay_command" if args.use_replay_command else "prefer_command",
        "selection": {
            "requested": list(args.select),
            "matched_count": len(records),
            "unmatched": unmatched_select,
            "available_count": len(available_names),
        },
        "counts": {
            "planned": planned_count,
            "passed": passed_count,
            "partial": partial_count,
            "failed": failed_count,
            "invalid_command": invalid_command_count,
            "timeout": timeout_count,
            "skipped_invalid_selection": skipped_invalid_selection_count,
            "selected_requested": len(args.select),
            "selected_unmatched": len(unmatched_select),
        },
        "runs": records,
    }
    output = resolve_repo_path(repo_root, args.output)
    output_status = write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    if output_status["status"] == "ok":
        print(f"wrote {path_label(output)}")
    else:
        print(f"failed to write {path_label(output)}: {output_status.get('error')}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
