#!/usr/bin/env python3
"""Bind reviewed parity evidence ids to strict loaded execute documents."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

if __package__:
    from .compatibility_parity_evidence import (
        CompatibilityParityEvidence,
        validate_loaded_compatibility_parity_evidence,
    )
    from .compatibility_evidence_primitives import (
        compatibility_parity_evidence_error as _error,
        validate_compatibility_evidence_reference_tuples,
    )
else:
    from compatibility_parity_evidence import (  # type: ignore
        CompatibilityParityEvidence,
        validate_loaded_compatibility_parity_evidence,
    )
    from compatibility_evidence_primitives import (  # type: ignore
        compatibility_parity_evidence_error as _error,
        validate_compatibility_evidence_reference_tuples,
    )


_VALIDATION_TOKEN = object()


@dataclass(frozen=True)
class CompatibilityParityEvidenceBinding:
    evidence_documents: Tuple[CompatibilityParityEvidence, ...]
    _validation_token: object = field(repr=False, compare=False)


def validate_compatibility_parity_evidence_binding(
    *,
    expected_entry_id: str,
    expected_target_release: str,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    expected_compatibility_delegate_override_option: str | None,
    expected_evidence_ids: Tuple[str, ...],
    expected_evidence_sha256: Tuple[str, ...],
    evidence_documents: Tuple[CompatibilityParityEvidence, ...],
) -> CompatibilityParityEvidenceBinding:
    validate_compatibility_evidence_reference_tuples(
        "parity_evidence",
        expected_evidence_ids,
        expected_evidence_sha256,
    )
    if not isinstance(evidence_documents, tuple) or not evidence_documents:
        raise _error("documents must be supplied for every attested evidence id")
    for evidence in evidence_documents:
        validate_loaded_compatibility_parity_evidence(
            evidence,
            expected_entry_id=expected_entry_id,
            expected_target_release=expected_target_release,
            expected_canonical_entrypoint=expected_canonical_entrypoint,
            expected_compatibility_entrypoint=expected_compatibility_entrypoint,
            expected_compatibility_delegate_override_option=(
                expected_compatibility_delegate_override_option
            ),
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
    return CompatibilityParityEvidenceBinding(
        evidence_documents=evidence_documents,
        _validation_token=_VALIDATION_TOKEN,
    )


def compatibility_parity_evidence_binding_payload(
    binding: Optional[CompatibilityParityEvidenceBinding],
    *,
    required_for_candidate_promotion: bool,
) -> Dict[str, object]:
    if binding is None:
        return {
            "status": "not_provided",
            "binding_key": (
                "lifecycle_entrypoint_phase_content_delegate_target_release_and_ordered_evidence_id_and_source_sha256"
            ),
            "evidence_count": 0,
            "source_paths_disclosed": False,
            "required_for_candidate_promotion": required_for_candidate_promotion,
        }
    if (
        not isinstance(binding, CompatibilityParityEvidenceBinding)
        or binding._validation_token is not _VALIDATION_TOKEN
    ):
        raise _error("projection must originate from the canonical binding owner")
    documents = binding.evidence_documents
    return {
        "status": "verified_execute_evidence_linked",
        "binding_key": (
            "lifecycle_entrypoint_phase_content_delegate_target_release_and_ordered_evidence_id_and_source_sha256"
        ),
        "evidence_count": len(documents),
        "evidence_documents": [
            {
                "evidence_id": evidence.evidence_id,
                "entry_id": evidence.entry_id,
                "target_release": evidence.target_release,
                "canonical_entrypoint_identity_sha256": (
                    evidence.canonical_entrypoint_identity_sha256
                ),
                "compatibility_entrypoint_identity_sha256": (
                    evidence.compatibility_entrypoint_identity_sha256
                ),
                "content_snapshot_checkpoint_count": (
                    evidence.content_snapshot_checkpoint_count
                ),
                "canonical_entrypoint_size_bytes": (
                    evidence.canonical_entrypoint_size_bytes
                ),
                "canonical_entrypoint_sha256": (
                    evidence.canonical_entrypoint_sha256
                ),
                "compatibility_entrypoint_size_bytes": (
                    evidence.compatibility_entrypoint_size_bytes
                ),
                "compatibility_entrypoint_sha256": (
                    evidence.compatibility_entrypoint_sha256
                ),
                "compatibility_delegate_binding_status": (
                    evidence.compatibility_delegate_binding_status
                ),
                "compatibility_delegate_override_option": (
                    evidence.compatibility_delegate_override_option
                ),
                "compatibility_delegate_override_present": (
                    evidence.compatibility_delegate_override_present
                ),
                "compatibility_delegate_identity_sha256": (
                    evidence.compatibility_delegate_identity_sha256
                ),
                "timestamp_utc": evidence.timestamp_utc,
                "ignored_json_pointer_count": len(
                    evidence.ignored_json_pointers
                ),
                "source_size_bytes": evidence.source_size_bytes,
                "source_sha256": evidence.source_sha256,
            }
            for evidence in documents
        ],
        "source_paths_disclosed": False,
        "claim_boundary": (
            "strict current-entrypoint-phase-content-and-delegate-bound "
            "execute-evidence import only; "
            "does not authenticate the reviewer, inventory external callers, "
            "or authorize removal"
        ),
    }
