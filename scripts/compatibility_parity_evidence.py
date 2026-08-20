#!/usr/bin/env python3
"""Strict verified parity evidence import for compatibility retirement review."""

from __future__ import annotations

import math
import re
import shlex
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Mapping, Tuple

if __package__:
    from .compatibility_parity_contract import (
        HARD_ARTIFACT_LIMIT_BYTES,
        HARD_OUTPUT_LIMIT_BYTES,
        MAX_COMPATIBILITY_ENTRYPOINT_BYTES,
        CompatibilityParityContractError,
        lifecycle_entrypoint_content_snapshot,
        lifecycle_entrypoint_identity,
        validate_ignore_pointers,
    )
    from .compatibility_evidence_primitives import (
        compatibility_parity_evidence_error as _error,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_token,
        require_compatibility_evidence_utc_second,
    )
    from .strict_json_document import (
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
else:
    from compatibility_parity_contract import (  # type: ignore
        HARD_ARTIFACT_LIMIT_BYTES,
        HARD_OUTPUT_LIMIT_BYTES,
        MAX_COMPATIBILITY_ENTRYPOINT_BYTES,
        CompatibilityParityContractError,
        lifecycle_entrypoint_content_snapshot,
        lifecycle_entrypoint_identity,
        validate_ignore_pointers,
    )
    from compatibility_evidence_primitives import (  # type: ignore
        compatibility_parity_evidence_error as _error,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_token,
        require_compatibility_evidence_utc_second,
    )
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )


COMPATIBILITY_PARITY_ARTIFACT_TYPE = "inferlab_compatibility_parity_evidence"
COMPATIBILITY_PARITY_SCHEMA_VERSION = 2
COMPATIBILITY_PARITY_CLAIM_SCOPE = (
    "explicit local command exit/stdout/stderr and normalized JSON artifact "
    "parity for lifecycle-bound command entrypoints and compatibility delegate; "
    "entrypoint content matched at pre-canonical, post-canonical and "
    "post-compatibility checkpoints and still matches the importing repository; "
    "ignored pointers are excluded from the claim"
)
COMPATIBILITY_PARITY_CLAIM_BOUNDARY = (
    "does not authenticate the reviewer, inventory external callers, prove "
    "performance, authorize removal, execute fallback retirement, or create "
    "product/GPU/release evidence"
)
COMPATIBILITY_PARITY_VERIFIED_NEXT_ACTION = (
    "reference this evidence_id from a separately reviewed retirement "
    "attestation only after external caller inventory is complete"
)

_MAX_INPUT_BYTES = 16 * 1024 * 1024
_PROJECT_ROOT = Path(__file__).resolve().parents[1]
_PATH_LABEL_RE = re.compile(r"^path_[0-9a-f]{16}(?:\.[^/\\\s]{1,15})?$")
_VALIDATION_TOKEN = object()
_TOP_LEVEL_FIELDS = frozenset(
    {
        "schema_version",
        "artifact_type",
        "timestamp_utc",
        "evidence_id",
        "entry_id",
        "target_release",
        "mode",
        "executed",
        "replayed",
        "status",
        "canonical_command_parity_status",
        "canonical_artifact_parity_status",
        "command_binding",
        "command_parity",
        "artifact_parity",
        "runs",
        "limits",
        "claim_scope",
        "claim_boundary",
        "removal_allowed",
        "automatic_removal",
        "recommended_next_action",
    }
)


@dataclass(frozen=True)
class CompatibilityParityEvidence:
    schema_version: int
    artifact_type: str
    timestamp_utc: str
    evidence_id: str
    entry_id: str
    target_release: str
    canonical_entrypoint_identity_sha256: str
    compatibility_entrypoint_identity_sha256: str
    content_snapshot_checkpoint_count: int
    canonical_entrypoint_size_bytes: int
    canonical_entrypoint_sha256: str
    compatibility_entrypoint_size_bytes: int
    compatibility_entrypoint_sha256: str
    compatibility_delegate_binding_status: str
    compatibility_delegate_override_option: str
    compatibility_delegate_override_present: bool
    compatibility_delegate_identity_sha256: str
    ignored_json_pointers: Tuple[str, ...]
    canonical_normalized_sha256: str
    compatibility_normalized_sha256: str
    source_size_bytes: int
    source_sha256: str
    _validation_token: object = field(repr=False, compare=False)


def _require_exact_object(
    value: Any,
    *,
    label: str,
    fields: frozenset[str],
) -> Dict[str, Any]:
    if not isinstance(value, dict):
        raise _error(f"{label} must be an object")
    actual = frozenset(value)
    if actual != fields:
        missing = sorted(fields - actual)
        unknown_count = len(actual - fields)
        details = []
        if missing:
            details.append("missing=" + ",".join(missing))
        if unknown_count:
            details.append(f"unknown_count={unknown_count}")
        raise _error(f"{label} fields do not match: " + "; ".join(details))
    return value


def _require_bool(value: Any, label: str, expected: bool) -> None:
    if type(value) is not bool or value is not expected:
        raise _error(f"{label} must equal {str(expected).lower()}")


def _require_string(value: Any, label: str, expected: str | None = None) -> str:
    if not isinstance(value, str):
        raise _error(f"{label} must be a string")
    if expected is not None and value != expected:
        raise _error(f"{label} is not canonical")
    return value


def _require_non_negative_int(value: Any, label: str, maximum: int) -> int:
    if type(value) is not int or not (0 <= value <= maximum):
        raise _error(f"{label} is outside the supported range")
    return value


def _require_sha256(value: Any, label: str) -> str:
    return require_compatibility_evidence_sha256(
        f"compatibility parity evidence {label}", value
    )


def _require_path_label(value: Any, label: str) -> None:
    if not isinstance(value, str) or not _PATH_LABEL_RE.fullmatch(value):
        raise _error(f"{label} must be a redacted path label")


def _validate_command(value: Any, label: str) -> None:
    command = _require_exact_object(
        value,
        label=label,
        fields=frozenset({"parts", "shell", "redacted"}),
    )
    parts = command["parts"]
    if not isinstance(parts, list) or not (1 <= len(parts) <= 256):
        raise _error(f"{label}.parts must contain 1..256 strings")
    if any(
        not isinstance(part, str) or not part or len(part) > 4096
        for part in parts
    ):
        raise _error(f"{label}.parts contains an invalid string")
    shell = _require_string(command["shell"], f"{label}.shell")
    if shell != shlex.join(parts):
        raise _error(f"{label}.shell does not match parts")
    if type(command["redacted"]) is not bool:
        raise _error(f"{label}.redacted must be a boolean")


def _validate_command_binding(
    value: Any,
    *,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
) -> Tuple[str, str, int, int, str, int, str, str, str, bool, str]:
    binding = _require_exact_object(
        value,
        label="command_binding",
        fields=frozenset(
            {
                "status",
                "source",
                "canonical_entrypoint_identity_sha256",
                "compatibility_entrypoint_identity_sha256",
                "content_snapshot_status",
                "content_snapshot_checkpoint_count",
                "canonical_entrypoint_size_bytes",
                "canonical_entrypoint_sha256",
                "compatibility_entrypoint_size_bytes",
                "compatibility_entrypoint_sha256",
                "compatibility_delegate_binding_status",
                "compatibility_delegate_override_option",
                "compatibility_delegate_override_present",
                "compatibility_delegate_identity_sha256",
                "compatibility_delegate_path_disclosed",
                "raw_entrypoint_paths_disclosed",
                "entrypoint_content_disclosed",
            }
        ),
    )
    _require_string(
        binding["status"], "command_binding.status", "lifecycle_entrypoints_exact"
    )
    _require_string(
        binding["source"], "command_binding.source", "implementation_lifecycle"
    )
    canonical_identity = _require_sha256(
        binding["canonical_entrypoint_identity_sha256"],
        "command_binding.canonical_entrypoint_identity_sha256",
    )
    compatibility_identity = _require_sha256(
        binding["compatibility_entrypoint_identity_sha256"],
        "command_binding.compatibility_entrypoint_identity_sha256",
    )
    try:
        expected_canonical_identity = lifecycle_entrypoint_identity(
            "canonical", expected_canonical_entrypoint
        )
        expected_compatibility_identity = lifecycle_entrypoint_identity(
            "compatibility", expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        raise _error(str(exc)) from exc
    if canonical_identity != expected_canonical_identity:
        raise _error("canonical command entrypoint does not match the lifecycle")
    if compatibility_identity != expected_compatibility_identity:
        raise _error("compatibility command entrypoint does not match the lifecycle")
    _require_string(
        binding["content_snapshot_status"],
        "command_binding.content_snapshot_status",
        "captured_before_after_canonical_and_after_compatibility_execution_stable",
    )
    checkpoint_count = _require_non_negative_int(
        binding["content_snapshot_checkpoint_count"],
        "command_binding.content_snapshot_checkpoint_count",
        3,
    )
    if checkpoint_count != 3:
        raise _error("command entrypoint checkpoint sequence is incomplete")
    canonical_size = _require_non_negative_int(
        binding["canonical_entrypoint_size_bytes"],
        "command_binding.canonical_entrypoint_size_bytes",
        MAX_COMPATIBILITY_ENTRYPOINT_BYTES,
    )
    compatibility_size = _require_non_negative_int(
        binding["compatibility_entrypoint_size_bytes"],
        "command_binding.compatibility_entrypoint_size_bytes",
        MAX_COMPATIBILITY_ENTRYPOINT_BYTES,
    )
    if canonical_size == 0 or compatibility_size == 0:
        raise _error("command entrypoint snapshots must be non-empty")
    canonical_sha256 = _require_sha256(
        binding["canonical_entrypoint_sha256"],
        "command_binding.canonical_entrypoint_sha256",
    )
    compatibility_sha256 = _require_sha256(
        binding["compatibility_entrypoint_sha256"],
        "command_binding.compatibility_entrypoint_sha256",
    )
    try:
        current_canonical = lifecycle_entrypoint_content_snapshot(
            _PROJECT_ROOT, expected_canonical_entrypoint
        )
        current_compatibility = lifecycle_entrypoint_content_snapshot(
            _PROJECT_ROOT, expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        raise _error(str(exc)) from exc
    if (
        canonical_size != current_canonical.size_bytes
        or canonical_sha256 != current_canonical.sha256
        or compatibility_size != current_compatibility.size_bytes
        or compatibility_sha256 != current_compatibility.sha256
    ):
        raise _error("command entrypoint content does not match the current lifecycle")
    delegate_option = _require_string(
        binding["compatibility_delegate_override_option"],
        "command_binding.compatibility_delegate_override_option",
        expected_compatibility_delegate_override_option or "",
    )
    override_present = binding["compatibility_delegate_override_present"]
    if type(override_present) is not bool:
        raise _error(
            "command_binding.compatibility_delegate_override_present must be a boolean"
        )
    expected_delegate_status = (
        "fixed_lifecycle_replacement"
        if expected_compatibility_delegate_override_option is None
        else "explicit_lifecycle_replacement"
        if override_present
        else "default_lifecycle_replacement"
    )
    delegate_status = _require_string(
        binding["compatibility_delegate_binding_status"],
        "command_binding.compatibility_delegate_binding_status",
        expected_delegate_status,
    )
    if expected_compatibility_delegate_override_option is None and override_present:
        raise _error("fixed compatibility delegate cannot report an override")
    delegate_identity = _require_sha256(
        binding["compatibility_delegate_identity_sha256"],
        "command_binding.compatibility_delegate_identity_sha256",
    )
    if delegate_identity != expected_canonical_identity:
        raise _error("compatibility delegate does not match the lifecycle replacement")
    _require_bool(
        binding["compatibility_delegate_path_disclosed"],
        "command_binding.compatibility_delegate_path_disclosed",
        False,
    )
    _require_bool(
        binding["raw_entrypoint_paths_disclosed"],
        "command_binding.raw_entrypoint_paths_disclosed",
        False,
    )
    _require_bool(
        binding["entrypoint_content_disclosed"],
        "command_binding.entrypoint_content_disclosed",
        False,
    )
    return (
        canonical_identity,
        compatibility_identity,
        checkpoint_count,
        canonical_size,
        canonical_sha256,
        compatibility_size,
        compatibility_sha256,
        delegate_status,
        delegate_option,
        override_present,
        delegate_identity,
    )


def _validate_limits(value: Any) -> Tuple[float, int, int]:
    limits = _require_exact_object(
        value,
        label="limits",
        fields=frozenset(
            {"timeout_seconds", "output_limit_bytes_per_stream", "artifact_limit_bytes"}
        ),
    )
    timeout = limits["timeout_seconds"]
    if (
        isinstance(timeout, bool)
        or not isinstance(timeout, (int, float))
        or not math.isfinite(float(timeout))
        or not (0.0 < float(timeout) <= 86400.0)
    ):
        raise _error("limits.timeout_seconds is outside the supported range")
    output_limit = _require_non_negative_int(
        limits["output_limit_bytes_per_stream"],
        "limits.output_limit_bytes_per_stream",
        HARD_OUTPUT_LIMIT_BYTES,
    )
    artifact_limit = _require_non_negative_int(
        limits["artifact_limit_bytes"],
        "limits.artifact_limit_bytes",
        HARD_ARTIFACT_LIMIT_BYTES,
    )
    if output_limit == 0 or artifact_limit == 0:
        raise _error("limits byte values must be positive")
    return float(timeout), output_limit, artifact_limit


def _validate_ready_run(
    value: Any,
    *,
    expected_label: str,
    output_limit: int,
    artifact_limit: int,
) -> Tuple[str, int, int]:
    run = _require_exact_object(
        value,
        label=f"runs.{expected_label}",
        fields=frozenset(
            {
                "label",
                "status",
                "command",
                "path",
                "path_redacted",
                "artifact_preexisting",
                "artifact_refresh_observed",
                "artifact_content_disclosed",
                "command_result",
                "artifact_document",
            }
        ),
    )
    _require_string(run["label"], f"runs.{expected_label}.label", expected_label)
    _require_string(run["status"], f"runs.{expected_label}.status", "ready")
    _validate_command(run["command"], f"runs.{expected_label}.command")
    path_label = _require_string(run["path"], f"runs.{expected_label}.path")
    _require_path_label(path_label, f"runs.{expected_label}.path")
    _require_bool(run["path_redacted"], f"runs.{expected_label}.path_redacted", True)
    if type(run["artifact_preexisting"]) is not bool:
        raise _error(
            f"runs.{expected_label}.artifact_preexisting must be a boolean"
        )
    _require_bool(
        run["artifact_refresh_observed"],
        f"runs.{expected_label}.artifact_refresh_observed",
        True,
    )
    _require_bool(
        run["artifact_content_disclosed"],
        f"runs.{expected_label}.artifact_content_disclosed",
        False,
    )

    result = _require_exact_object(
        run["command_result"],
        label=f"runs.{expected_label}.command_result",
        fields=frozenset(
            {
                "status",
                "returncode",
                "elapsed_ms",
                "stdout_bytes",
                "stderr_bytes",
                "stdout_stderr_content_disclosed",
                "timed_out",
                "output_limit_exceeded",
                "error",
            }
        ),
    )
    _require_string(
        result["status"], f"runs.{expected_label}.command_result.status", "ok"
    )
    if type(result["returncode"]) is not int or result["returncode"] != 0:
        raise _error(f"runs.{expected_label}.command_result.returncode must equal 0")
    elapsed = result["elapsed_ms"]
    if (
        isinstance(elapsed, bool)
        or not isinstance(elapsed, (int, float))
        or not math.isfinite(float(elapsed))
        or float(elapsed) < 0.0
    ):
        raise _error(f"runs.{expected_label}.command_result.elapsed_ms is invalid")
    stdout_bytes = _require_non_negative_int(
        result["stdout_bytes"],
        f"runs.{expected_label}.command_result.stdout_bytes",
        output_limit,
    )
    stderr_bytes = _require_non_negative_int(
        result["stderr_bytes"],
        f"runs.{expected_label}.command_result.stderr_bytes",
        output_limit,
    )
    _require_bool(
        result["stdout_stderr_content_disclosed"],
        f"runs.{expected_label}.command_result.stdout_stderr_content_disclosed",
        False,
    )
    _require_bool(
        result["timed_out"],
        f"runs.{expected_label}.command_result.timed_out",
        False,
    )
    _require_bool(
        result["output_limit_exceeded"],
        f"runs.{expected_label}.command_result.output_limit_exceeded",
        False,
    )
    _require_string(
        result["error"], f"runs.{expected_label}.command_result.error", ""
    )

    document = _require_exact_object(
        run["artifact_document"],
        label=f"runs.{expected_label}.artifact_document",
        fields=frozenset({"source_size_bytes", "source_sha256"}),
    )
    size = _require_non_negative_int(
        document["source_size_bytes"],
        f"runs.{expected_label}.artifact_document.source_size_bytes",
        artifact_limit,
    )
    if size == 0:
        raise _error(
            f"runs.{expected_label}.artifact_document.source_size_bytes must be positive"
        )
    _require_sha256(
        document["source_sha256"],
        f"runs.{expected_label}.artifact_document.source_sha256",
    )
    return path_label, stdout_bytes, stderr_bytes


def _validated_evidence_from_payload(
    payload: Mapping[str, Any],
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
    source_size_bytes: int,
    source_sha256: str,
) -> CompatibilityParityEvidence:
    root = _require_exact_object(
        payload,
        label="root",
        fields=_TOP_LEVEL_FIELDS,
    )
    if (
        type(root["schema_version"]) is not int
        or root["schema_version"] != COMPATIBILITY_PARITY_SCHEMA_VERSION
    ):
        raise _error(
            "schema_version must equal "
            f"{COMPATIBILITY_PARITY_SCHEMA_VERSION}"
        )
    _require_string(
        root["artifact_type"], "artifact_type", COMPATIBILITY_PARITY_ARTIFACT_TYPE
    )
    timestamp_utc = require_compatibility_evidence_utc_second(
        "compatibility parity evidence timestamp_utc", root["timestamp_utc"]
    )
    evidence_id = require_compatibility_evidence_token(
        "parity_evidence_id", root["evidence_id"]
    )
    entry_id = require_compatibility_evidence_token(
        "parity_entry_id", root["entry_id"]
    )
    if entry_id != expected_entry_id:
        raise _error("entry_id does not match the requested lifecycle entry")
    target_release = require_compatibility_evidence_token(
        "parity_target_release", root["target_release"]
    )
    if target_release != expected_target_release:
        raise _error("target_release does not match the lifecycle registry")
    _require_string(root["mode"], "mode", "execute")
    _require_bool(root["executed"], "executed", True)
    _require_bool(root["replayed"], "replayed", False)
    _require_string(root["status"], "status", "verified")
    _require_string(
        root["canonical_command_parity_status"],
        "canonical_command_parity_status",
        "verified",
    )
    _require_string(
        root["canonical_artifact_parity_status"],
        "canonical_artifact_parity_status",
        "verified",
    )
    (
        canonical_entrypoint_identity,
        compatibility_entrypoint_identity,
        checkpoint_count,
        canonical_entrypoint_size_bytes,
        canonical_entrypoint_sha256,
        compatibility_entrypoint_size_bytes,
        compatibility_entrypoint_sha256,
        compatibility_delegate_binding_status,
        compatibility_delegate_override_option,
        compatibility_delegate_override_present,
        compatibility_delegate_identity_sha256,
    ) = _validate_command_binding(
        root["command_binding"],
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        expected_compatibility_entrypoint=expected_compatibility_entrypoint,
        expected_compatibility_delegate_override_option=(
            expected_compatibility_delegate_override_option
        ),
    )

    command = _require_exact_object(
        root["command_parity"],
        label="command_parity",
        fields=frozenset(
            {
                "comparable",
                "exit_code_equal",
                "stdout_exact_equal",
                "stderr_exact_equal",
                "stdout_stderr_content_disclosed",
            }
        ),
    )
    for field_name in (
        "comparable",
        "exit_code_equal",
        "stdout_exact_equal",
        "stderr_exact_equal",
    ):
        _require_bool(command[field_name], f"command_parity.{field_name}", True)
    _require_bool(
        command["stdout_stderr_content_disclosed"],
        "command_parity.stdout_stderr_content_disclosed",
        False,
    )

    artifact = _require_exact_object(
        root["artifact_parity"],
        label="artifact_parity",
        fields=frozenset(
            {
                "comparable",
                "normalized_exact_equal",
                "ignored_json_pointers",
                "ignored_pointer_count",
                "canonical_normalized_sha256",
                "compatibility_normalized_sha256",
                "artifact_content_disclosed",
                "error",
            }
        ),
    )
    _require_bool(artifact["comparable"], "artifact_parity.comparable", True)
    _require_bool(
        artifact["normalized_exact_equal"],
        "artifact_parity.normalized_exact_equal",
        True,
    )
    raw_pointers = artifact["ignored_json_pointers"]
    if not isinstance(raw_pointers, list) or any(
        not isinstance(pointer, str) for pointer in raw_pointers
    ):
        raise _error("artifact_parity.ignored_json_pointers must be a string list")
    try:
        validate_ignore_pointers(raw_pointers)
    except CompatibilityParityContractError as exc:
        raise _error(str(exc)) from exc
    pointer_count = _require_non_negative_int(
        artifact["ignored_pointer_count"],
        "artifact_parity.ignored_pointer_count",
        64,
    )
    if pointer_count != len(raw_pointers):
        raise _error("artifact_parity ignored pointer count does not reconcile")
    canonical_sha = _require_sha256(
        artifact["canonical_normalized_sha256"],
        "artifact_parity.canonical_normalized_sha256",
    )
    compatibility_sha = _require_sha256(
        artifact["compatibility_normalized_sha256"],
        "artifact_parity.compatibility_normalized_sha256",
    )
    if canonical_sha != compatibility_sha:
        raise _error("normalized artifact fingerprints do not match")
    _require_bool(
        artifact["artifact_content_disclosed"],
        "artifact_parity.artifact_content_disclosed",
        False,
    )
    _require_string(artifact["error"], "artifact_parity.error", "")

    _, output_limit, artifact_limit = _validate_limits(root["limits"])
    runs = _require_exact_object(
        root["runs"],
        label="runs",
        fields=frozenset({"canonical", "compatibility"}),
    )
    canonical_run = _validate_ready_run(
        runs["canonical"],
        expected_label="canonical",
        output_limit=output_limit,
        artifact_limit=artifact_limit,
    )
    compatibility_run = _validate_ready_run(
        runs["compatibility"],
        expected_label="compatibility",
        output_limit=output_limit,
        artifact_limit=artifact_limit,
    )
    canonical_path_label, canonical_stdout_bytes, canonical_stderr_bytes = canonical_run
    (
        compatibility_path_label,
        compatibility_stdout_bytes,
        compatibility_stderr_bytes,
    ) = compatibility_run
    if canonical_path_label == compatibility_path_label:
        raise _error("canonical and compatibility artifact paths must differ")
    if canonical_stdout_bytes != compatibility_stdout_bytes:
        raise _error("exact-equal stdout byte counts do not reconcile")
    if canonical_stderr_bytes != compatibility_stderr_bytes:
        raise _error("exact-equal stderr byte counts do not reconcile")
    _require_string(root["claim_scope"], "claim_scope", COMPATIBILITY_PARITY_CLAIM_SCOPE)
    _require_string(
        root["claim_boundary"],
        "claim_boundary",
        COMPATIBILITY_PARITY_CLAIM_BOUNDARY,
    )
    _require_bool(root["removal_allowed"], "removal_allowed", False)
    _require_bool(root["automatic_removal"], "automatic_removal", False)
    _require_string(
        root["recommended_next_action"],
        "recommended_next_action",
        COMPATIBILITY_PARITY_VERIFIED_NEXT_ACTION,
    )
    if not (0 < source_size_bytes <= _MAX_INPUT_BYTES):
        raise _error("source size is outside the bounded contract")
    _require_sha256(source_sha256, "source_sha256")
    return CompatibilityParityEvidence(
        schema_version=COMPATIBILITY_PARITY_SCHEMA_VERSION,
        artifact_type=COMPATIBILITY_PARITY_ARTIFACT_TYPE,
        timestamp_utc=timestamp_utc,
        evidence_id=evidence_id,
        entry_id=entry_id,
        target_release=target_release,
        canonical_entrypoint_identity_sha256=canonical_entrypoint_identity,
        compatibility_entrypoint_identity_sha256=(
            compatibility_entrypoint_identity
        ),
        content_snapshot_checkpoint_count=checkpoint_count,
        canonical_entrypoint_size_bytes=canonical_entrypoint_size_bytes,
        canonical_entrypoint_sha256=canonical_entrypoint_sha256,
        compatibility_entrypoint_size_bytes=compatibility_entrypoint_size_bytes,
        compatibility_entrypoint_sha256=compatibility_entrypoint_sha256,
        compatibility_delegate_binding_status=(
            compatibility_delegate_binding_status
        ),
        compatibility_delegate_override_option=(
            compatibility_delegate_override_option
        ),
        compatibility_delegate_override_present=(
            compatibility_delegate_override_present
        ),
        compatibility_delegate_identity_sha256=(
            compatibility_delegate_identity_sha256
        ),
        ignored_json_pointers=tuple(raw_pointers),
        canonical_normalized_sha256=canonical_sha,
        compatibility_normalized_sha256=compatibility_sha,
        source_size_bytes=source_size_bytes,
        source_sha256=source_sha256,
        _validation_token=_VALIDATION_TOKEN,
    )


def load_verified_compatibility_parity_evidence(
    evidence_path: str | Path,
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
) -> CompatibilityParityEvidence:
    try:
        document = load_strict_json_object_document(
            evidence_path,
            document_label="compatibility parity evidence",
            maximum_bytes=_MAX_INPUT_BYTES,
        )
    except StrictJsonDocumentError as exc:
        raise _error(str(exc)) from exc
    return _validated_evidence_from_payload(
        document.payload,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        expected_compatibility_entrypoint=expected_compatibility_entrypoint,
        expected_compatibility_delegate_override_option=(
            expected_compatibility_delegate_override_option
        ),
        source_size_bytes=document.source_size_bytes,
        source_sha256=document.source_sha256,
    )


def validate_loaded_compatibility_parity_evidence(
    evidence: CompatibilityParityEvidence,
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
) -> None:
    if (
        not isinstance(evidence, CompatibilityParityEvidence)
        or evidence._validation_token is not _VALIDATION_TOKEN
    ):
        raise _error("document must originate from the strict loader")
    if evidence.entry_id != expected_entry_id:
        raise _error("document entry_id does not match the lifecycle entry")
    if evidence.target_release != expected_target_release:
        raise _error("document target_release does not match the lifecycle registry")
    if evidence.schema_version != COMPATIBILITY_PARITY_SCHEMA_VERSION:
        raise _error("document schema_version is not canonical")
    if evidence.content_snapshot_checkpoint_count != 3:
        raise _error("document entrypoint checkpoint sequence is incomplete")
    try:
        expected_canonical_identity = lifecycle_entrypoint_identity(
            "canonical", expected_canonical_entrypoint
        )
        expected_compatibility_identity = lifecycle_entrypoint_identity(
            "compatibility", expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        raise _error(str(exc)) from exc
    if (
        evidence.canonical_entrypoint_identity_sha256
        != expected_canonical_identity
        or evidence.compatibility_entrypoint_identity_sha256
        != expected_compatibility_identity
    ):
        raise _error("document command entrypoints do not match the lifecycle")
    try:
        current_canonical = lifecycle_entrypoint_content_snapshot(
            _PROJECT_ROOT, expected_canonical_entrypoint
        )
        current_compatibility = lifecycle_entrypoint_content_snapshot(
            _PROJECT_ROOT, expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        raise _error(str(exc)) from exc
    if (
        evidence.canonical_entrypoint_size_bytes != current_canonical.size_bytes
        or evidence.canonical_entrypoint_sha256 != current_canonical.sha256
        or evidence.compatibility_entrypoint_size_bytes
        != current_compatibility.size_bytes
        or evidence.compatibility_entrypoint_sha256
        != current_compatibility.sha256
    ):
        raise _error("document command entrypoint content is stale")
    if evidence.artifact_type != COMPATIBILITY_PARITY_ARTIFACT_TYPE:
        raise _error("document artifact_type is not canonical")
    expected_delegate_status = (
        "fixed_lifecycle_replacement"
        if expected_compatibility_delegate_override_option is None
        else "explicit_lifecycle_replacement"
        if evidence.compatibility_delegate_override_present
        else "default_lifecycle_replacement"
    )
    if (
        evidence.compatibility_delegate_binding_status != expected_delegate_status
        or evidence.compatibility_delegate_override_option
        != (expected_compatibility_delegate_override_option or "")
        or evidence.compatibility_delegate_identity_sha256
        != expected_canonical_identity
        or (
            expected_compatibility_delegate_override_option is None
            and evidence.compatibility_delegate_override_present
        )
    ):
        raise _error("document compatibility delegate binding is stale")
