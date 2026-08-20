#!/usr/bin/env python3
"""Bind target-release authorization to a validated retirement candidate."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
    )
    from .compatibility_removal_authorization import (
        CompatibilityRemovalAuthorization,
        compatibility_removal_authorization_payload,
        missing_compatibility_removal_authorization_payload,
        validate_loaded_compatibility_removal_authorization,
    )
    from .compatibility_retirement_candidate_evidence import (
        CompatibilityRetirementCandidateEvidence,
        validate_compatibility_retirement_candidate_evidence,
    )
    from .compatibility_retirement_evidence import (
        CompatibilityRetirementEvidence,
        validate_compatibility_retirement_evidence,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
    )
    from compatibility_removal_authorization import (  # type: ignore
        CompatibilityRemovalAuthorization,
        compatibility_removal_authorization_payload,
        missing_compatibility_removal_authorization_payload,
        validate_loaded_compatibility_removal_authorization,
    )
    from compatibility_retirement_candidate_evidence import (  # type: ignore
        CompatibilityRetirementCandidateEvidence,
        validate_compatibility_retirement_candidate_evidence,
    )
    from compatibility_retirement_evidence import (  # type: ignore
        CompatibilityRetirementEvidence,
        validate_compatibility_retirement_evidence,
    )


_VALIDATION_TOKEN = object()


@dataclass(frozen=True)
class CompatibilityRemovalAuthorizationBinding:
    authorization: CompatibilityRemovalAuthorization
    _validation_token: object = field(repr=False, compare=False)


def _error(message: str) -> CompatibilityRetirementEvidenceError:
    return CompatibilityRetirementEvidenceError(
        f"compatibility removal authorization binding {message}"
    )


def bind_compatibility_removal_authorization(
    *,
    expected_entry_id: str,
    expected_target_release: str,
    repository_runtime_callers: Tuple[str, ...],
    retirement_evidence: CompatibilityRetirementEvidence,
    candidate_evidence: CompatibilityRetirementCandidateEvidence,
    authorization: CompatibilityRemovalAuthorization,
) -> CompatibilityRemovalAuthorizationBinding:
    validate_compatibility_retirement_evidence(
        retirement_evidence,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
        repository_runtime_callers=repository_runtime_callers,
    )
    validate_compatibility_retirement_candidate_evidence(candidate_evidence)
    validate_loaded_compatibility_removal_authorization(
        authorization,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
    )
    external_documents = (
        candidate_evidence.external_caller_binding.evidence_documents
    )
    parity_documents = candidate_evidence.parity_binding.evidence_documents
    expected_external_ids = tuple(
        document.evidence_id for document in external_documents
    )
    expected_external_sha256 = tuple(
        document.source_sha256 for document in external_documents
    )
    expected_parity_ids = tuple(
        document.evidence_id for document in parity_documents
    )
    expected_parity_sha256 = tuple(
        document.source_sha256 for document in parity_documents
    )
    if (
        authorization.retirement_evidence_id != retirement_evidence.evidence_id
        or authorization.retirement_evidence_sha256
        != retirement_evidence.source_sha256
    ):
        raise _error(
            "retirement attestation identity or fingerprint does not match"
        )
    if (
        authorization.external_caller_evidence_ids != expected_external_ids
        or authorization.external_caller_evidence_sha256
        != expected_external_sha256
    ):
        raise _error(
            "external caller evidence references do not match the candidate"
        )
    if (
        authorization.parity_evidence_ids != expected_parity_ids
        or authorization.parity_evidence_sha256 != expected_parity_sha256
    ):
        raise _error("parity evidence references do not match the candidate")
    if authorization.authorized_at_utc < retirement_evidence.reviewed_at_utc:
        raise _error("authorization cannot predate the retirement attestation")
    return CompatibilityRemovalAuthorizationBinding(
        authorization=authorization,
        _validation_token=_VALIDATION_TOKEN,
    )


def compatibility_removal_authorization_binding_payload(
    binding: Optional[CompatibilityRemovalAuthorizationBinding],
    *,
    required_for_removal_permission: bool,
) -> Dict[str, object]:
    if binding is None:
        return {
            "status": "not_provided",
            "removal_allowed": False,
            "automatic_removal": False,
            "authorization": missing_compatibility_removal_authorization_payload(
                required_for_removal_permission=required_for_removal_permission
            ),
        }
    if (
        not isinstance(binding, CompatibilityRemovalAuthorizationBinding)
        or binding._validation_token is not _VALIDATION_TOKEN
    ):
        raise _error("projection must originate from the canonical binding owner")
    return {
        "status": "target_release_removal_authorized",
        "removal_allowed": True,
        "automatic_removal": False,
        "authorization": compatibility_removal_authorization_payload(
            binding.authorization
        ),
        "claim_boundary": (
            "content, configured-key, canonical-role, and current-time-window "
            "authenticated target-release permission only; human identity is not "
            "established, and source removal remains an explicit operator action "
            "not performed by this projection"
        ),
    }
