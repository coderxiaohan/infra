#!/usr/bin/env python3
"""Compose strict evidence bindings before lifecycle candidate projection."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
    )
    from .compatibility_external_caller_evidence import (
        CompatibilityExternalCallerEvidence,
    )
    from .compatibility_external_caller_evidence_binding import (
        CompatibilityExternalCallerEvidenceBinding,
        bind_compatibility_external_caller_evidence,
        compatibility_external_caller_evidence_binding_payload,
    )
    from .compatibility_parity_evidence import CompatibilityParityEvidence
    from .compatibility_parity_evidence_binding import (
        CompatibilityParityEvidenceBinding,
        compatibility_parity_evidence_binding_payload,
        validate_compatibility_parity_evidence_binding,
    )
    from .compatibility_retirement_evidence import (
        CompatibilityRetirementEvidence,
        validate_compatibility_retirement_evidence,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
    )
    from compatibility_external_caller_evidence import (  # type: ignore
        CompatibilityExternalCallerEvidence,
    )
    from compatibility_external_caller_evidence_binding import (  # type: ignore
        CompatibilityExternalCallerEvidenceBinding,
        bind_compatibility_external_caller_evidence,
        compatibility_external_caller_evidence_binding_payload,
    )
    from compatibility_parity_evidence import (  # type: ignore
        CompatibilityParityEvidence,
    )
    from compatibility_parity_evidence_binding import (  # type: ignore
        CompatibilityParityEvidenceBinding,
        compatibility_parity_evidence_binding_payload,
        validate_compatibility_parity_evidence_binding,
    )
    from compatibility_retirement_evidence import (  # type: ignore
        CompatibilityRetirementEvidence,
        validate_compatibility_retirement_evidence,
    )


@dataclass(frozen=True)
class CompatibilityRetirementCandidateEvidence:
    external_caller_binding: CompatibilityExternalCallerEvidenceBinding
    parity_binding: CompatibilityParityEvidenceBinding
    _validation_token: object = field(repr=False, compare=False)


_VALIDATION_TOKEN = object()


def validate_compatibility_retirement_candidate_evidence(
    candidate_evidence: CompatibilityRetirementCandidateEvidence,
) -> None:
    if (
        not isinstance(
            candidate_evidence, CompatibilityRetirementCandidateEvidence
        )
        or candidate_evidence._validation_token is not _VALIDATION_TOKEN
    ):
        raise CompatibilityRetirementEvidenceError(
            "retirement candidate evidence must originate from the canonical "
            "composition owner"
        )
    compatibility_external_caller_evidence_binding_payload(
        candidate_evidence.external_caller_binding,
        required_for_candidate_promotion=True,
    )
    compatibility_parity_evidence_binding_payload(
        candidate_evidence.parity_binding,
        required_for_candidate_promotion=True,
    )


def bind_compatibility_retirement_candidate_evidence(
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
    repository_runtime_callers: Tuple[str, ...],
    retirement_evidence: CompatibilityRetirementEvidence,
    external_caller_evidence: Tuple[CompatibilityExternalCallerEvidence, ...],
    parity_evidence: Tuple[CompatibilityParityEvidence, ...],
) -> CompatibilityRetirementCandidateEvidence:
    validate_compatibility_retirement_evidence(
        retirement_evidence,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        repository_runtime_callers=repository_runtime_callers,
    )
    external_binding = bind_compatibility_external_caller_evidence(
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        expected_audit_status=(
            retirement_evidence.external_caller_audit_status
        ),
        expected_evidence_ids=(
            retirement_evidence.external_caller_evidence_ids
        ),
        expected_evidence_sha256=(
            retirement_evidence.external_caller_evidence_sha256
        ),
        evidence_documents=external_caller_evidence,
    )
    parity_binding = validate_compatibility_parity_evidence_binding(
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        expected_compatibility_entrypoint=expected_compatibility_entrypoint,
        expected_compatibility_delegate_override_option=(
            expected_compatibility_delegate_override_option
        ),
        expected_evidence_ids=retirement_evidence.parity_evidence_ids,
        expected_evidence_sha256=retirement_evidence.parity_evidence_sha256,
        evidence_documents=parity_evidence,
    )
    if any(
        evidence.reviewed_at_utc > retirement_evidence.reviewed_at_utc
        for evidence in external_caller_evidence
    ):
        raise CompatibilityRetirementEvidenceError(
            "external caller evidence cannot be reviewed after its retirement attestation"
        )
    if any(
        evidence.timestamp_utc > retirement_evidence.reviewed_at_utc
        for evidence in parity_evidence
    ):
        raise CompatibilityRetirementEvidenceError(
            "parity evidence cannot be generated after its retirement attestation"
        )
    return CompatibilityRetirementCandidateEvidence(
        external_caller_binding=external_binding,
        parity_binding=parity_binding,
        _validation_token=_VALIDATION_TOKEN,
    )


def compatibility_retirement_candidate_evidence_payloads(
    candidate_evidence: Optional[CompatibilityRetirementCandidateEvidence],
    *,
    required_for_candidate_promotion: bool,
) -> Dict[str, object]:
    if candidate_evidence is not None:
        validate_compatibility_retirement_candidate_evidence(candidate_evidence)
    return {
        "external_caller_evidence_binding": (
            compatibility_external_caller_evidence_binding_payload(
                candidate_evidence.external_caller_binding
                if candidate_evidence
                else None,
                required_for_candidate_promotion=(
                    required_for_candidate_promotion
                ),
            )
        ),
        "parity_evidence_binding": compatibility_parity_evidence_binding_payload(
            candidate_evidence.parity_binding if candidate_evidence else None,
            required_for_candidate_promotion=required_for_candidate_promotion,
        ),
    }
