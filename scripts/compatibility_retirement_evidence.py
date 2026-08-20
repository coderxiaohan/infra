#!/usr/bin/env python3
"""Strict document contract for compatibility retirement attestations."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_sha256_list,
        require_compatibility_evidence_token,
        require_compatibility_evidence_token_list,
        require_compatibility_evidence_utc_second,
        validate_compatibility_evidence_disjoint_id_groups,
        validate_compatibility_evidence_reference_tuples,
        validate_compatibility_evidence_token_tuple,
    )
    from .strict_json_document import (
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_sha256_list,
        require_compatibility_evidence_token,
        require_compatibility_evidence_token_list,
        require_compatibility_evidence_utc_second,
        validate_compatibility_evidence_disjoint_id_groups,
        validate_compatibility_evidence_reference_tuples,
        validate_compatibility_evidence_token_tuple,
    )
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )


@dataclass(frozen=True)
class CompatibilityRetirementEvidence:
    schema_version: int
    artifact_type: str
    evidence_id: str
    entry_id: str
    repository_runtime_callers_migrated: bool
    external_caller_audit_status: str
    unresolved_external_callers: Tuple[str, ...]
    external_caller_evidence_ids: Tuple[str, ...]
    external_caller_evidence_sha256: Tuple[str, ...]
    canonical_command_parity_status: str
    canonical_artifact_parity_status: str
    parity_evidence_ids: Tuple[str, ...]
    parity_evidence_sha256: Tuple[str, ...]
    review_authority_role: str
    review_decision: str
    reviewed_at_utc: str
    target_release: str
    source_size_bytes: int
    source_sha256: str
    _validation_token: object = field(repr=False, compare=False)


COMPATIBILITY_RETIREMENT_EVIDENCE_SCHEMA_VERSION = 3
_ARTIFACT_TYPE = "inferlab_compatibility_retirement_evidence"
_MAX_INPUT_BYTES = 64 * 1024
_FIELDS = frozenset(
    {
        "schema_version",
        "artifact_type",
        "evidence_id",
        "entry_id",
        "repository_runtime_callers_migrated",
        "external_caller_audit_status",
        "unresolved_external_callers",
        "external_caller_evidence_ids",
        "external_caller_evidence_sha256",
        "canonical_command_parity_status",
        "canonical_artifact_parity_status",
        "parity_evidence_ids",
        "parity_evidence_sha256",
        "review_authority_role",
        "review_decision",
        "reviewed_at_utc",
        "target_release",
    }
)
_COMPLETE_EXTERNAL_CALLER_AUDIT_STATUSES = frozenset(
    {"complete_no_callers", "complete_all_migrated"}
)
_VALIDATION_TOKEN = object()


def _require_string_fields(payload: Dict[str, Any], fields: Tuple[str, ...]) -> None:
    for field_name in fields:
        if not isinstance(payload[field_name], str):
            raise CompatibilityRetirementEvidenceError(
                "compatibility retirement evidence field "
                f"{field_name} must be a string"
            )


def _require_loader_origin(evidence: CompatibilityRetirementEvidence) -> None:
    if (
        not isinstance(evidence, CompatibilityRetirementEvidence)
        or evidence._validation_token is not _VALIDATION_TOKEN
    ):
        raise CompatibilityRetirementEvidenceError(
            "retirement evidence must originate from the strict document loader"
        )


def validate_compatibility_retirement_evidence(
    evidence: CompatibilityRetirementEvidence,
    *,
    expected_entry_id: str,
    expected_target_release: str,
    repository_runtime_callers: Tuple[str, ...],
) -> None:
    _require_loader_origin(evidence)
    if (
        type(evidence.schema_version) is not int
        or evidence.schema_version != COMPATIBILITY_RETIREMENT_EVIDENCE_SCHEMA_VERSION
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility retirement evidence schema_version must equal "
            f"{COMPATIBILITY_RETIREMENT_EVIDENCE_SCHEMA_VERSION}"
        )
    if evidence.artifact_type != _ARTIFACT_TYPE:
        raise CompatibilityRetirementEvidenceError(
            "compatibility retirement evidence artifact_type is not canonical"
        )
    require_compatibility_evidence_token("evidence_id", evidence.evidence_id)
    if evidence.entry_id != expected_entry_id:
        raise CompatibilityRetirementEvidenceError(
            "compatibility retirement evidence entry_id does not match the requested entry"
        )
    if (
        type(evidence.repository_runtime_callers_migrated) is not bool
        or not evidence.repository_runtime_callers_migrated
    ):
        raise CompatibilityRetirementEvidenceError(
            "repository runtime callers must be declared migrated"
        )
    if repository_runtime_callers:
        raise CompatibilityRetirementEvidenceError(
            "the canonical registry still contains repository runtime callers"
        )
    if not isinstance(evidence.external_caller_audit_status, str) or (
        evidence.external_caller_audit_status
        not in _COMPLETE_EXTERNAL_CALLER_AUDIT_STATUSES
    ):
        raise CompatibilityRetirementEvidenceError(
            "external caller audit must be complete before candidate promotion"
        )
    if evidence.unresolved_external_callers:
        raise CompatibilityRetirementEvidenceError(
            "unresolved external callers must be empty before candidate promotion"
        )
    validate_compatibility_evidence_token_tuple(
        "unresolved_external_callers",
        evidence.unresolved_external_callers,
        allow_empty=True,
    )
    validate_compatibility_evidence_reference_tuples(
        "external_caller_evidence",
        evidence.external_caller_evidence_ids,
        evidence.external_caller_evidence_sha256,
    )
    validate_compatibility_evidence_reference_tuples(
        "parity_evidence",
        evidence.parity_evidence_ids,
        evidence.parity_evidence_sha256,
    )
    validate_compatibility_evidence_disjoint_id_groups(
        (
            ("retirement", (evidence.evidence_id,)),
            ("external_caller", evidence.external_caller_evidence_ids),
            ("parity", evidence.parity_evidence_ids),
        )
    )
    if (
        not isinstance(evidence.canonical_command_parity_status, str)
        or evidence.canonical_command_parity_status != "verified"
    ):
        raise CompatibilityRetirementEvidenceError(
            "canonical command parity must be verified before candidate promotion"
        )
    if (
        not isinstance(evidence.canonical_artifact_parity_status, str)
        or evidence.canonical_artifact_parity_status != "verified"
    ):
        raise CompatibilityRetirementEvidenceError(
            "canonical artifact parity must be verified before candidate promotion"
        )
    require_compatibility_evidence_token(
        "review_authority_role", evidence.review_authority_role
    )
    if (
        not isinstance(evidence.review_decision, str)
        or evidence.review_decision != "approved_for_removal_candidate"
    ):
        raise CompatibilityRetirementEvidenceError(
            "review decision must approve removal-candidate promotion"
        )
    require_compatibility_evidence_utc_second(
        "reviewed_at_utc", evidence.reviewed_at_utc
    )
    if evidence.target_release != expected_target_release:
        raise CompatibilityRetirementEvidenceError(
            "retirement evidence target_release does not match the lifecycle registry"
        )
    if type(evidence.source_size_bytes) is not int or not (
        0 < evidence.source_size_bytes <= _MAX_INPUT_BYTES
    ):
        raise CompatibilityRetirementEvidenceError(
            "retirement evidence source size is outside the bounded contract"
        )
    require_compatibility_evidence_sha256(
        "retirement evidence source fingerprint", evidence.source_sha256
    )


def load_compatibility_retirement_evidence_document(
    evidence_path: str | Path,
    *,
    expected_entry_id: str,
    expected_target_release: str,
    repository_runtime_callers: Tuple[str, ...],
) -> CompatibilityRetirementEvidence:
    try:
        document = load_strict_json_object_document(
            evidence_path,
            document_label="compatibility retirement evidence",
            maximum_bytes=_MAX_INPUT_BYTES,
        )
    except StrictJsonDocumentError as exc:
        raise CompatibilityRetirementEvidenceError(
            str(exc)
        ) from exc
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
        raise CompatibilityRetirementEvidenceError(
            "compatibility retirement evidence fields do not match the contract: "
            + "; ".join(details)
        )
    if type(payload["schema_version"]) is not int:
        raise CompatibilityRetirementEvidenceError(
            "compatibility retirement evidence schema_version must be an integer"
        )
    if type(payload["repository_runtime_callers_migrated"]) is not bool:
        raise CompatibilityRetirementEvidenceError(
            "repository_runtime_callers_migrated must be a boolean"
        )
    _require_string_fields(
        payload,
        (
            "artifact_type",
            "entry_id",
            "external_caller_audit_status",
            "canonical_command_parity_status",
            "canonical_artifact_parity_status",
            "review_decision",
            "reviewed_at_utc",
            "target_release",
        ),
    )
    evidence = CompatibilityRetirementEvidence(
        schema_version=payload["schema_version"],
        artifact_type=payload["artifact_type"],
        evidence_id=require_compatibility_evidence_token(
            "evidence_id", payload["evidence_id"]
        ),
        entry_id=payload["entry_id"],
        repository_runtime_callers_migrated=payload[
            "repository_runtime_callers_migrated"
        ],
        external_caller_audit_status=payload["external_caller_audit_status"],
        unresolved_external_callers=require_compatibility_evidence_token_list(
            "unresolved_external_callers",
            payload["unresolved_external_callers"],
            allow_empty=True,
        ),
        external_caller_evidence_ids=require_compatibility_evidence_token_list(
            "external_caller_evidence_ids",
            payload["external_caller_evidence_ids"],
            allow_empty=False,
        ),
        external_caller_evidence_sha256=(
            require_compatibility_evidence_sha256_list(
                "external_caller_evidence_sha256",
                payload["external_caller_evidence_sha256"],
            )
        ),
        canonical_command_parity_status=payload[
            "canonical_command_parity_status"
        ],
        canonical_artifact_parity_status=payload[
            "canonical_artifact_parity_status"
        ],
        parity_evidence_ids=require_compatibility_evidence_token_list(
            "parity_evidence_ids",
            payload["parity_evidence_ids"],
            allow_empty=False,
        ),
        parity_evidence_sha256=require_compatibility_evidence_sha256_list(
            "parity_evidence_sha256",
            payload["parity_evidence_sha256"],
        ),
        review_authority_role=require_compatibility_evidence_token(
            "review_authority_role", payload["review_authority_role"]
        ),
        review_decision=payload["review_decision"],
        reviewed_at_utc=payload["reviewed_at_utc"],
        target_release=payload["target_release"],
        source_size_bytes=document.source_size_bytes,
        source_sha256=document.source_sha256,
        _validation_token=_VALIDATION_TOKEN,
    )
    validate_compatibility_retirement_evidence(
        evidence,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        repository_runtime_callers=repository_runtime_callers,
    )
    return evidence


def compatibility_retirement_evidence_payload(
    evidence: CompatibilityRetirementEvidence,
) -> Dict[str, object]:
    _require_loader_origin(evidence)
    return {
        "status": "validated_removal_candidate_attestation",
        "schema_version": evidence.schema_version,
        "artifact_type": evidence.artifact_type,
        "evidence_id": evidence.evidence_id,
        "entry_id": evidence.entry_id,
        "repository_runtime_callers_migrated": (
            evidence.repository_runtime_callers_migrated
        ),
        "external_caller_audit_status": evidence.external_caller_audit_status,
        "unresolved_external_callers": list(evidence.unresolved_external_callers),
        "external_caller_evidence_ids": list(
            evidence.external_caller_evidence_ids
        ),
        "external_caller_evidence_sha256": list(
            evidence.external_caller_evidence_sha256
        ),
        "canonical_command_parity_status": (
            evidence.canonical_command_parity_status
        ),
        "canonical_artifact_parity_status": (
            evidence.canonical_artifact_parity_status
        ),
        "parity_evidence_ids": list(evidence.parity_evidence_ids),
        "parity_evidence_sha256": list(evidence.parity_evidence_sha256),
        "review_authority_role": evidence.review_authority_role,
        "review_decision": evidence.review_decision,
        "reviewed_at_utc": evidence.reviewed_at_utc,
        "target_release": evidence.target_release,
        "source_size_bytes": evidence.source_size_bytes,
        "source_sha256": evidence.source_sha256,
        "source_path_disclosed": False,
        "claim_boundary": (
            "strict attestation plus ordered external-caller and parity-document "
            "fingerprint binding only; does not authenticate the reviewer, "
            "execute parity, delete the wrapper, or prove benchmark performance"
        ),
    }


def missing_compatibility_retirement_evidence_payload(
    *,
    required_for_candidate_promotion: bool,
) -> Dict[str, object]:
    return {
        "status": "not_provided",
        "required_schema_version": COMPATIBILITY_RETIREMENT_EVIDENCE_SCHEMA_VERSION,
        "maximum_input_bytes": _MAX_INPUT_BYTES,
        "required_for_candidate_promotion": required_for_candidate_promotion,
        "source_path_disclosed": False,
    }
