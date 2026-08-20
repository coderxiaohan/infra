#!/usr/bin/env python3
"""Bind retirement attestations to strict external-caller audit documents."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        validate_compatibility_evidence_reference_tuples,
    )
    from .compatibility_external_caller_evidence import (
        CompatibilityExternalCallerEvidence,
        validate_loaded_compatibility_external_caller_evidence,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        validate_compatibility_evidence_reference_tuples,
    )
    from compatibility_external_caller_evidence import (  # type: ignore
        CompatibilityExternalCallerEvidence,
        validate_loaded_compatibility_external_caller_evidence,
    )


_VALIDATION_TOKEN = object()


@dataclass(frozen=True)
class CompatibilityExternalCallerEvidenceBinding:
    evidence_documents: Tuple[CompatibilityExternalCallerEvidence, ...]
    aggregate_audit_status: str
    external_caller_count: int
    migrated_external_caller_count: int
    unresolved_external_caller_count: int
    _validation_token: object = field(repr=False, compare=False)


def _error(message: str) -> CompatibilityRetirementEvidenceError:
    return CompatibilityRetirementEvidenceError(
        f"compatibility external caller evidence binding {message}"
    )


def bind_compatibility_external_caller_evidence(
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_audit_status: str,
    expected_evidence_ids: Tuple[str, ...],
    expected_evidence_sha256: Tuple[str, ...],
    evidence_documents: Tuple[CompatibilityExternalCallerEvidence, ...],
) -> CompatibilityExternalCallerEvidenceBinding:
    validate_compatibility_evidence_reference_tuples(
        "external_caller_evidence",
        expected_evidence_ids,
        expected_evidence_sha256,
    )
    if not isinstance(evidence_documents, tuple) or not evidence_documents:
        raise _error("requires canonical documents for every attested evidence id")
    for evidence in evidence_documents:
        validate_loaded_compatibility_external_caller_evidence(
            evidence,
            expected_entry_id=expected_entry_id,
            expected_target_release=expected_target_release,
        )
    actual_ids = tuple(evidence.evidence_id for evidence in evidence_documents)
    actual_sha256 = tuple(
        evidence.source_sha256 for evidence in evidence_documents
    )
    if actual_ids != expected_evidence_ids:
        raise _error("document ids do not exactly match attestation order")
    if actual_sha256 != expected_evidence_sha256:
        raise _error(
            "document source fingerprints do not exactly match attestation order"
        )
    audit_scope_ids = tuple(
        evidence.audit_scope_id for evidence in evidence_documents
    )
    if len(set(audit_scope_ids)) != len(audit_scope_ids):
        raise _error("documents contain duplicate audit scope ids")
    aggregate_status = (
        "complete_all_migrated"
        if any(
            evidence.external_caller_audit_status == "complete_all_migrated"
            for evidence in evidence_documents
        )
        else "complete_no_callers"
    )
    if aggregate_status != expected_audit_status:
        raise _error("aggregate audit status does not match the attestation")
    caller_count = sum(
        evidence.external_caller_count for evidence in evidence_documents
    )
    migrated_count = sum(
        evidence.migrated_external_caller_count
        for evidence in evidence_documents
    )
    unresolved_count = sum(
        evidence.unresolved_external_caller_count
        for evidence in evidence_documents
    )
    if migrated_count != caller_count or unresolved_count != 0:
        raise _error("aggregate caller migration counts are not complete")
    return CompatibilityExternalCallerEvidenceBinding(
        evidence_documents=evidence_documents,
        aggregate_audit_status=aggregate_status,
        external_caller_count=caller_count,
        migrated_external_caller_count=migrated_count,
        unresolved_external_caller_count=unresolved_count,
        _validation_token=_VALIDATION_TOKEN,
    )


def compatibility_external_caller_evidence_binding_payload(
    binding: Optional[CompatibilityExternalCallerEvidenceBinding],
    *,
    required_for_candidate_promotion: bool,
) -> Dict[str, object]:
    if binding is None:
        return {
            "status": "not_provided",
            "binding_key": "ordered_evidence_id_and_source_sha256",
            "evidence_count": 0,
            "source_paths_disclosed": False,
            "raw_caller_identifiers_disclosed": False,
            "required_for_candidate_promotion": required_for_candidate_promotion,
        }
    if (
        not isinstance(binding, CompatibilityExternalCallerEvidenceBinding)
        or binding._validation_token is not _VALIDATION_TOKEN
    ):
        raise _error("projection must originate from the canonical binding owner")
    return {
        "status": "reviewed_external_caller_evidence_linked",
        "binding_key": "ordered_evidence_id_and_source_sha256",
        "aggregate_audit_status": binding.aggregate_audit_status,
        "evidence_count": len(binding.evidence_documents),
        "external_caller_count": binding.external_caller_count,
        "migrated_external_caller_count": (
            binding.migrated_external_caller_count
        ),
        "unresolved_external_caller_count": (
            binding.unresolved_external_caller_count
        ),
        "evidence_documents": [
            {
                "evidence_id": evidence.evidence_id,
                "entry_id": evidence.entry_id,
                "target_release": evidence.target_release,
                "audit_scope_id": evidence.audit_scope_id,
                "audit_method": evidence.audit_method,
                "inventory_snapshot_sha256": evidence.inventory_snapshot_sha256,
                "external_caller_audit_status": (
                    evidence.external_caller_audit_status
                ),
                "external_caller_count": evidence.external_caller_count,
                "migrated_external_caller_count": (
                    evidence.migrated_external_caller_count
                ),
                "unresolved_external_caller_count": (
                    evidence.unresolved_external_caller_count
                ),
                "reviewed_at_utc": evidence.reviewed_at_utc,
                "source_size_bytes": evidence.source_size_bytes,
                "source_sha256": evidence.source_sha256,
            }
            for evidence in binding.evidence_documents
        ],
        "source_paths_disclosed": False,
        "raw_caller_identifiers_disclosed": False,
        "claim_boundary": (
            "strict reviewed external-caller inventory document linkage only; "
            "does not authenticate the reviewer or inventory source, discover "
            "omitted callers, execute parity, or authorize removal"
        ),
    }
