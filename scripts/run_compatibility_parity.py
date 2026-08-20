#!/usr/bin/env python3
"""Capture strict command and JSON artifact parity for compatibility review."""

from __future__ import annotations

import argparse
import hashlib
from time_utils import utc_now as _utc_now
from pathlib import Path
from typing import Any, Dict

if __package__:
    from .artifact_json import write_json_file
    from .compatibility_parity_contract import (
        DEFAULT_ARTIFACT_LIMIT_BYTES,
        DEFAULT_OUTPUT_LIMIT_BYTES,
        HARD_ARTIFACT_LIMIT_BYTES,
        HARD_OUTPUT_LIMIT_BYTES,
        bind_lifecycle_command_entrypoints,
        checkpoint_lifecycle_command_entrypoints_after_canonical,
        finalize_lifecycle_command_entrypoints,
        normalized_document,
        parse_command_json,
        parse_ignore_pointers,
        require_distinct_paths,
        resolve_repo_path,
    )
    from .compatibility_parity_execution import (
        SideExecution,
        execute_side,
        planned_side,
        side_payload,
        skipped_side,
    )
    from .compatibility_parity_evidence import (
        COMPATIBILITY_PARITY_ARTIFACT_TYPE,
        COMPATIBILITY_PARITY_CLAIM_BOUNDARY,
        COMPATIBILITY_PARITY_CLAIM_SCOPE,
        COMPATIBILITY_PARITY_SCHEMA_VERSION,
        COMPATIBILITY_PARITY_VERIFIED_NEXT_ACTION,
    )
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_token,
    )
    from .cmake_compatibility_manifest import (
        CMakeCompatibilityManifestError,
        load_cmake_compatibility_entrypoint,
    )
    from .implementation_lifecycle import lifecycle_for
else:
    from artifact_json import write_json_file  # type: ignore
    from compatibility_parity_contract import (  # type: ignore
        DEFAULT_ARTIFACT_LIMIT_BYTES,
        DEFAULT_OUTPUT_LIMIT_BYTES,
        HARD_ARTIFACT_LIMIT_BYTES,
        HARD_OUTPUT_LIMIT_BYTES,
        bind_lifecycle_command_entrypoints,
        checkpoint_lifecycle_command_entrypoints_after_canonical,
        finalize_lifecycle_command_entrypoints,
        normalized_document,
        parse_command_json,
        parse_ignore_pointers,
        require_distinct_paths,
        resolve_repo_path,
    )
    from compatibility_parity_execution import (  # type: ignore
        SideExecution,
        execute_side,
        planned_side,
        side_payload,
        skipped_side,
    )
    from compatibility_parity_evidence import (  # type: ignore
        COMPATIBILITY_PARITY_ARTIFACT_TYPE,
        COMPATIBILITY_PARITY_CLAIM_BOUNDARY,
        COMPATIBILITY_PARITY_CLAIM_SCOPE,
        COMPATIBILITY_PARITY_SCHEMA_VERSION,
        COMPATIBILITY_PARITY_VERIFIED_NEXT_ACTION,
    )
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_token,
    )
    from cmake_compatibility_manifest import (  # type: ignore
        CMakeCompatibilityManifestError,
        load_cmake_compatibility_entrypoint,
    )
    from implementation_lifecycle import lifecycle_for  # type: ignore


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--entry-id", required=True)
    parser.add_argument(
        "--cmake-compatibility-manifest",
        default="",
        help="Optional CMake-derived lifecycle manifest for native compatibility targets.",
    )
    parser.add_argument("--evidence-id", required=True)
    parser.add_argument("--canonical-command-json", required=True)
    parser.add_argument("--compatibility-command-json", required=True)
    parser.add_argument("--canonical-artifact", required=True)
    parser.add_argument("--compatibility-artifact", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--ignore-json-pointer", action="append", default=[])
    parser.add_argument("--timeout-seconds", type=float, default=300.0)
    parser.add_argument(
        "--output-limit-bytes",
        type=int,
        default=DEFAULT_OUTPUT_LIMIT_BYTES,
    )
    parser.add_argument(
        "--artifact-limit-bytes",
        type=int,
        default=DEFAULT_ARTIFACT_LIMIT_BYTES,
    )
    parser.add_argument("--execute", action="store_true")
    return parser


def _validate_scalar_arguments(
    parser: argparse.ArgumentParser,
    args: argparse.Namespace,
) -> tuple[str, str]:
    try:
        entry_id = require_compatibility_evidence_token("entry_id", args.entry_id)
        evidence_id = require_compatibility_evidence_token(
            "evidence_id", args.evidence_id
        )
    except CompatibilityRetirementEvidenceError as exc:
        parser.error(str(exc))
    if not (0.0 < args.timeout_seconds <= 86400.0):
        parser.error("timeout-seconds must be in (0, 86400]")
    if type(args.output_limit_bytes) is not int or not (
        1 <= args.output_limit_bytes <= HARD_OUTPUT_LIMIT_BYTES
    ):
        parser.error("output-limit-bytes is outside the supported range")
    if type(args.artifact_limit_bytes) is not int or not (
        1 <= args.artifact_limit_bytes <= HARD_ARTIFACT_LIMIT_BYTES
    ):
        parser.error("artifact-limit-bytes is outside the supported range")
    return entry_id, evidence_id


def _resolve_repo_root(parser: argparse.ArgumentParser, value: str) -> Path:
    try:
        repo_root = Path(value).expanduser().resolve(strict=True)
        is_directory = repo_root.is_dir()
    except (OSError, RuntimeError, TypeError, ValueError):
        parser.error("repo-root could not be resolved")
    if not is_directory:
        parser.error("repo-root must be a directory")
    return repo_root


def _command_parity(
    canonical: SideExecution,
    compatibility: SideExecution,
) -> Dict[str, bool]:
    canonical_capture = canonical.command_capture
    compatibility_capture = compatibility.command_capture
    comparable = bool(
        canonical_capture is not None
        and compatibility_capture is not None
        and canonical_capture.status == "ok"
        and compatibility_capture.status == "ok"
    )
    return {
        "comparable": comparable,
        "exit_code_equal": bool(
            comparable
            and canonical_capture is not None
            and compatibility_capture is not None
            and canonical_capture.returncode == compatibility_capture.returncode
        ),
        "stdout_exact_equal": bool(
            comparable
            and canonical_capture is not None
            and compatibility_capture is not None
            and canonical_capture.stdout == compatibility_capture.stdout
        ),
        "stderr_exact_equal": bool(
            comparable
            and canonical_capture is not None
            and compatibility_capture is not None
            and canonical_capture.stderr == compatibility_capture.stderr
        ),
    }


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()
    entry_id, evidence_id = _validate_scalar_arguments(parser, args)
    repo_root = _resolve_repo_root(parser, args.repo_root)
    if args.cmake_compatibility_manifest:
        manifest_path = resolve_repo_path(
            parser,
            repo_root,
            "cmake_compatibility_manifest",
            args.cmake_compatibility_manifest,
        )
        try:
            lifecycle = load_cmake_compatibility_entrypoint(
                manifest_path, entry_id
            )
        except KeyError:
            parser.error("entry-id is not registered in the CMake compatibility manifest")
        except CMakeCompatibilityManifestError as exc:
            parser.error(str(exc))
    else:
        try:
            lifecycle = lifecycle_for(entry_id)
        except KeyError:
            parser.error("entry-id is not registered in the implementation lifecycle")
    if lifecycle.role != "compatibility_wrapper":
        parser.error("entry-id does not identify a compatibility wrapper")
    canonical_command = parse_command_json(
        parser, "canonical", args.canonical_command_json
    )
    compatibility_command = parse_command_json(
        parser, "compatibility", args.compatibility_command_json
    )
    if canonical_command == compatibility_command:
        parser.error("canonical and compatibility commands must differ")
    if lifecycle.replacement is None:
        parser.error("compatibility lifecycle has no canonical replacement")
    command_binding = bind_lifecycle_command_entrypoints(
        parser,
        repo_root=repo_root,
        canonical_command=canonical_command,
        compatibility_command=compatibility_command,
        expected_canonical_entrypoint=lifecycle.replacement,
        expected_compatibility_entrypoint=lifecycle.path,
        compatibility_delegate_override_option=(
            lifecycle.delegate_override_option
        ),
        capture_content=bool(args.execute),
    )
    canonical_artifact = resolve_repo_path(
        parser, repo_root, "canonical_artifact", args.canonical_artifact
    )
    compatibility_artifact = resolve_repo_path(
        parser, repo_root, "compatibility_artifact", args.compatibility_artifact
    )
    output_path = resolve_repo_path(parser, repo_root, "output", args.output)
    require_distinct_paths(
        parser,
        (
            ("canonical_artifact", canonical_artifact),
            ("compatibility_artifact", compatibility_artifact),
            ("output", output_path),
        ),
    )
    ignore_pointers = parse_ignore_pointers(parser, args.ignore_json_pointer)

    if args.execute:
        canonical = execute_side(
            "canonical",
            canonical_command,
            canonical_artifact,
            repo_root=repo_root,
            timeout_seconds=args.timeout_seconds,
            output_limit_bytes=args.output_limit_bytes,
            artifact_limit_bytes=args.artifact_limit_bytes,
        )
        command_binding = checkpoint_lifecycle_command_entrypoints_after_canonical(
            parser,
            binding=command_binding,
            repo_root=repo_root,
            expected_canonical_entrypoint=lifecycle.replacement,
            expected_compatibility_entrypoint=lifecycle.path,
        )
        compatibility = (
            execute_side(
                "compatibility",
                compatibility_command,
                compatibility_artifact,
                repo_root=repo_root,
                timeout_seconds=args.timeout_seconds,
                output_limit_bytes=args.output_limit_bytes,
                artifact_limit_bytes=args.artifact_limit_bytes,
            )
            if canonical.status == "ready"
            else skipped_side(
                "compatibility", compatibility_command, compatibility_artifact
            )
        )
        compatibility_executed = compatibility.command_capture is not None
    else:
        compatibility_executed = False
        canonical = planned_side(
            "canonical", canonical_command, canonical_artifact
        )
        compatibility = planned_side(
            "compatibility", compatibility_command, compatibility_artifact
        )

    command_binding = finalize_lifecycle_command_entrypoints(
        parser,
        binding=command_binding,
        repo_root=repo_root,
        expected_canonical_entrypoint=lifecycle.replacement,
        expected_compatibility_entrypoint=lifecycle.path,
        executed=bool(args.execute),
        compatibility_executed=compatibility_executed,
    )

    command = _command_parity(canonical, compatibility)
    command_verified = bool(
        command["comparable"]
        and command["exit_code_equal"]
        and command["stdout_exact_equal"]
        and command["stderr_exact_equal"]
    )
    artifact_comparable = (
        canonical.document is not None and compatibility.document is not None
    )
    artifact_error = ""
    canonical_normalized = b""
    compatibility_normalized = b""
    if artifact_comparable:
        try:
            canonical_normalized = normalized_document(
                canonical.document, ignore_pointers
            )
            compatibility_normalized = normalized_document(
                compatibility.document, ignore_pointers
            )
        except (RecursionError, TypeError, ValueError) as exc:
            artifact_error = str(exc)
            artifact_comparable = False
    artifact_verified = bool(
        artifact_comparable and canonical_normalized == compatibility_normalized
    )
    verified = bool(args.execute and command_verified and artifact_verified)
    status = "verified" if verified else ("failed" if args.execute else "planned")

    payload: Dict[str, Any] = {
        "schema_version": COMPATIBILITY_PARITY_SCHEMA_VERSION,
        "artifact_type": COMPATIBILITY_PARITY_ARTIFACT_TYPE,
        "timestamp_utc": _utc_now(),
        "evidence_id": evidence_id,
        "entry_id": entry_id,
        "target_release": lifecycle.target_release,
        "mode": "execute" if args.execute else "dry_run",
        "executed": bool(args.execute),
        "replayed": False,
        "status": status,
        "canonical_command_parity_status": (
            "verified" if command_verified else ("failed" if args.execute else "not_run")
        ),
        "canonical_artifact_parity_status": (
            "verified" if artifact_verified else ("failed" if args.execute else "not_run")
        ),
        "command_binding": command_binding,
        "command_parity": {
            **command,
            "stdout_stderr_content_disclosed": False,
        },
        "artifact_parity": {
            "comparable": artifact_comparable,
            "normalized_exact_equal": artifact_verified,
            "ignored_json_pointers": list(args.ignore_json_pointer),
            "ignored_pointer_count": len(ignore_pointers),
            "canonical_normalized_sha256": (
                hashlib.sha256(canonical_normalized).hexdigest()
                if artifact_comparable
                else ""
            ),
            "compatibility_normalized_sha256": (
                hashlib.sha256(compatibility_normalized).hexdigest()
                if artifact_comparable
                else ""
            ),
            "artifact_content_disclosed": False,
            "error": artifact_error,
        },
        "runs": {
            "canonical": side_payload(canonical),
            "compatibility": side_payload(compatibility),
        },
        "limits": {
            "timeout_seconds": args.timeout_seconds,
            "output_limit_bytes_per_stream": args.output_limit_bytes,
            "artifact_limit_bytes": args.artifact_limit_bytes,
        },
        "claim_scope": COMPATIBILITY_PARITY_CLAIM_SCOPE,
        "claim_boundary": COMPATIBILITY_PARITY_CLAIM_BOUNDARY,
        "removal_allowed": False,
        "automatic_removal": False,
        "recommended_next_action": (
            COMPATIBILITY_PARITY_VERIFIED_NEXT_ACTION
            if verified
            else (
                "rerun with --execute on an allowed validation machine"
                if not args.execute
                else "resolve the reported command/artifact parity gaps and rerun explicitly"
            )
        ),
    }
    write_json_file(output_path, payload)
    return 0 if not args.execute or verified else 2


if __name__ == "__main__":
    raise SystemExit(main())
