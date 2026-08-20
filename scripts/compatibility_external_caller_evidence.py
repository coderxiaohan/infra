#!/usr/bin/env python3
"""Strict external-caller inventory evidence for compatibility retirement."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_token,
        require_compatibility_evidence_utc_second,
    )
    from .strict_json_document import (
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_token,
        require_compatibility_evidence_utc_second,
    )
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )


COMPATIBILITY_EXTERNAL_CALLER_EVIDENCE_SCHEMA_VERSION = 1
COMPATIBILITY_EXTERNAL_CALLER_ARTIFACT_TYPE = (
    "inferlab_compatibility_external_caller_audit_evidence"
)
COMPATIBILITY_EXTERNAL_CALLER_CLAIM_SCOPE = (
    "reviewed external-caller inventory scope, source snapshot fingerprint, "
    "and migration counts only"
)
COMPATIBILITY_EXTERNAL_CALLER_CLAIM_BOUNDARY = (
    "does not authenticate the reviewer or source system, discover omitted "
    "callers, execute parity, authorize removal, or prove benchmark, product, "
    "GPU, or release behavior"
)

_MAX_INPUT_BYTES = 64 * 1024
_MAX_CALLERS_PER_SCOPE = 1_000_000
_MODE = "external_caller_inventory_attestation"
_STATUS = "reviewed_complete"
_AUDIT_METHODS = frozenset(
    {
        "combined_inventory_review",
        "deployment_inventory_export",
        "owner_registry_review",
        "package_usage_inventory",
    }
)
_AUDIT_STATUSES = frozenset(
    {"complete_no_callers", "complete_all_migrated"}
)
_AUDIT_SCOPE_RE = re.compile(r"^scope_[0-9a-f]{16,64}$")
_FIELDS = frozenset(
    {
        "schema_version",
        "artifact_type",
        "evidence_id",
        "entry_id",
        "target_release",
        "mode",
        "executed",
        "replayed",
        "status",
        "audit_scope_id",
        "audit_method",
        "inventory_snapshot_sha256",
        "external_caller_audit_status",
        "external_caller_count",
        "migrated_external_caller_count",
        "unresolved_external_caller_count",
        "review_authority_role",
        "reviewed_at_utc",
        "raw_caller_identifiers_disclosed",
        "source_path_disclosed",
        "claim_scope",
        "claim_boundary",
    }
)
_VALIDATION_TOKEN = object()


@dataclass(frozen=True)
class CompatibilityExternalCallerEvidence:
    schema_version: int
    artifact_type: str
    evidence_id: str
    entry_id: str
    target_release: str
    audit_scope_id: str
    audit_method: str
    inventory_snapshot_sha256: str
    external_caller_audit_status: str
    external_caller_count: int
    migrated_external_caller_count: int
    unresolved_external_caller_count: int
    review_authority_role: str
    reviewed_at_utc: str
    source_size_bytes: int
    source_sha256: str
    _validation_token: object = field(repr=False, compare=False)


def _error(message: str) -> CompatibilityRetirementEvidenceError:
    return CompatibilityRetirementEvidenceError(
        f"compatibility external caller evidence {message}"
    )


def _require_count(value: Any, field_name: str) -> int:
    if type(value) is not int or not (0 <= value <= _MAX_CALLERS_PER_SCOPE):
        raise _error(f"{field_name} is outside the supported range")
    return value


def _require_string(value: Any, field_name: str) -> str:
    if not isinstance(value, str):
        raise _error(f"{field_name} must be a string")
    return value


def _require_bool(value: Any, field_name: str, expected: bool) -> None:
    if type(value) is not bool or value is not expected:
        raise _error(f"{field_name} must equal {str(expected).lower()}")


def _require_audit_scope_id(value: Any) -> str:
    if not isinstance(value, str) or not _AUDIT_SCOPE_RE.fullmatch(value):
        raise _error("audit_scope_id must be a pseudonymous scope token")
    return value


def _require_loader_origin(evidence: CompatibilityExternalCallerEvidence) -> None:
    if (
        not isinstance(evidence, CompatibilityExternalCallerEvidence)
        or evidence._validation_token is not _VALIDATION_TOKEN
    ):
        raise _error("document must originate from the strict loader")


def validate_loaded_compatibility_external_caller_evidence(
    evidence: CompatibilityExternalCallerEvidence,
    *,
    expected_entry_id: str,
    expected_target_release: str,
) -> None:
    _require_loader_origin(evidence)
    if evidence.schema_version != COMPATIBILITY_EXTERNAL_CALLER_EVIDENCE_SCHEMA_VERSION:
        raise _error(
            "schema_version must equal "
            f"{COMPATIBILITY_EXTERNAL_CALLER_EVIDENCE_SCHEMA_VERSION}"
        )
    if evidence.artifact_type != COMPATIBILITY_EXTERNAL_CALLER_ARTIFACT_TYPE:
        raise _error("artifact_type is not canonical")
    require_compatibility_evidence_token("evidence_id", evidence.evidence_id)
    if evidence.entry_id != expected_entry_id:
        raise _error("entry_id does not match the lifecycle entry")
    if evidence.target_release != expected_target_release:
        raise _error("target_release does not match the lifecycle registry")
    _require_audit_scope_id(evidence.audit_scope_id)
    if evidence.audit_method not in _AUDIT_METHODS:
        raise _error("audit_method is not supported")
    require_compatibility_evidence_sha256(
        "inventory_snapshot_sha256", evidence.inventory_snapshot_sha256
    )
    if evidence.external_caller_audit_status not in _AUDIT_STATUSES:
        raise _error("external_caller_audit_status is not complete")
    caller_count = _require_count(
        evidence.external_caller_count, "external_caller_count"
    )
    migrated_count = _require_count(
        evidence.migrated_external_caller_count,
        "migrated_external_caller_count",
    )
    unresolved_count = _require_count(
        evidence.unresolved_external_caller_count,
        "unresolved_external_caller_count",
    )
    if migrated_count > caller_count or unresolved_count > caller_count:
        raise _error("caller count partitions exceed the audited caller count")
    if evidence.external_caller_audit_status == "complete_no_callers":
        if caller_count or migrated_count or unresolved_count:
            raise _error("complete_no_callers requires zero caller counts")
    elif (
        caller_count == 0
        or migrated_count != caller_count
        or unresolved_count != 0
    ):
        raise _error(
            "complete_all_migrated requires positive fully migrated caller counts"
        )
    require_compatibility_evidence_token(
        "review_authority_role", evidence.review_authority_role
    )
    require_compatibility_evidence_utc_second(
        "reviewed_at_utc", evidence.reviewed_at_utc
    )
    if type(evidence.source_size_bytes) is not int or not (
        0 < evidence.source_size_bytes <= _MAX_INPUT_BYTES
    ):
        raise _error("source size is outside the bounded contract")
    require_compatibility_evidence_sha256(
        "external caller evidence source fingerprint", evidence.source_sha256
    )


def load_compatibility_external_caller_evidence(
    evidence_path: str | Path,
    *,
    expected_entry_id: str,
    expected_target_release: str,
) -> CompatibilityExternalCallerEvidence:
    try:
        document = load_strict_json_object_document(
            evidence_path,
            document_label="compatibility external caller evidence",
            maximum_bytes=_MAX_INPUT_BYTES,
        )
    except StrictJsonDocumentError as exc:
        raise _error(str(exc)) from exc
    payload = document.payload
    actual_fields = frozenset(payload)
    if actual_fields != _FIELDS:
        missing = sorted(_FIELDS - actual_fields)
        unknown_count = len(actual_fields - _FIELDS)
        details = []
        if missing:
            details.append("missing=" + ",".join(missing))
        if unknown_count:
            details.append(f"unknown_count={unknown_count}")
        raise _error("fields do not match the contract: " + "; ".join(details))
    if type(payload["schema_version"]) is not int:
        raise _error("schema_version must be an integer")
    for field_name in (
        "artifact_type",
        "entry_id",
        "target_release",
        "mode",
        "status",
        "audit_method",
        "external_caller_audit_status",
        "claim_scope",
        "claim_boundary",
    ):
        _require_string(payload[field_name], field_name)
    if payload["mode"] != _MODE:
        raise _error("mode is not canonical")
    _require_bool(payload["executed"], "executed", False)
    _require_bool(payload["replayed"], "replayed", False)
    if payload["status"] != _STATUS:
        raise _error("status is not canonical")
    _require_bool(
        payload["raw_caller_identifiers_disclosed"],
        "raw_caller_identifiers_disclosed",
        False,
    )
    _require_bool(payload["source_path_disclosed"], "source_path_disclosed", False)
    if payload["claim_scope"] != COMPATIBILITY_EXTERNAL_CALLER_CLAIM_SCOPE:
        raise _error("claim_scope is not canonical")
    if payload["claim_boundary"] != COMPATIBILITY_EXTERNAL_CALLER_CLAIM_BOUNDARY:
        raise _error("claim_boundary is not canonical")
    evidence = CompatibilityExternalCallerEvidence(
        schema_version=payload["schema_version"],
        artifact_type=payload["artifact_type"],
        evidence_id=require_compatibility_evidence_token(
            "evidence_id", payload["evidence_id"]
        ),
        entry_id=payload["entry_id"],
        target_release=payload["target_release"],
        audit_scope_id=_require_audit_scope_id(payload["audit_scope_id"]),
        audit_method=payload["audit_method"],
        inventory_snapshot_sha256=require_compatibility_evidence_sha256(
            "inventory_snapshot_sha256",
            payload["inventory_snapshot_sha256"],
        ),
        external_caller_audit_status=payload["external_caller_audit_status"],
        external_caller_count=_require_count(
            payload["external_caller_count"], "external_caller_count"
        ),
        migrated_external_caller_count=_require_count(
            payload["migrated_external_caller_count"],
            "migrated_external_caller_count",
        ),
        unresolved_external_caller_count=_require_count(
            payload["unresolved_external_caller_count"],
            "unresolved_external_caller_count",
        ),
        review_authority_role=require_compatibility_evidence_token(
            "review_authority_role", payload["review_authority_role"]
        ),
        reviewed_at_utc=payload["reviewed_at_utc"],
        source_size_bytes=document.source_size_bytes,
        source_sha256=document.source_sha256,
        _validation_token=_VALIDATION_TOKEN,
    )
    validate_loaded_compatibility_external_caller_evidence(
        evidence,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
    )
    return evidence
