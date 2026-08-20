#!/usr/bin/env python3
"""Strict target-release authorization document for compatibility removal."""

from __future__ import annotations

from dataclasses import dataclass, field, replace
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Dict, Tuple

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        compatibility_evidence_utc_second_datetime,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_sha256_list,
        require_compatibility_evidence_token,
        require_compatibility_evidence_token_list,
        validate_compatibility_evidence_disjoint_id_groups,
        validate_compatibility_evidence_reference_tuples,
    )
    from .strict_json_document import (
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
    from .compatibility_removal_authorization_auth import (
        COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ID,
        COMPATIBILITY_REMOVAL_AUTHORIZATION_SIGNATURE_ALGORITHM,
        COMPATIBILITY_REMOVAL_AUTHORIZATION_TRUSTED_KEY_BYTES,
        compatibility_removal_authorization_signature as _authorization_signature,
        verify_registered_compatibility_removal_authorization_signature,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        compatibility_evidence_utc_second_datetime,
        require_compatibility_evidence_sha256,
        require_compatibility_evidence_sha256_list,
        require_compatibility_evidence_token,
        require_compatibility_evidence_token_list,
        validate_compatibility_evidence_disjoint_id_groups,
        validate_compatibility_evidence_reference_tuples,
    )
    from strict_json_document import (  # type: ignore
        StrictJsonDocumentError,
        load_strict_json_object_document,
    )
    from compatibility_removal_authorization_auth import (  # type: ignore
        COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ID,
        COMPATIBILITY_REMOVAL_AUTHORIZATION_SIGNATURE_ALGORITHM,
        COMPATIBILITY_REMOVAL_AUTHORIZATION_TRUSTED_KEY_BYTES,
        compatibility_removal_authorization_signature as _authorization_signature,
        verify_registered_compatibility_removal_authorization_signature,
    )


@dataclass(frozen=True)
class CompatibilityRemovalAuthorization:
    schema_version: int
    artifact_type: str
    authorization_id: str
    entry_id: str
    retirement_evidence_id: str
    retirement_evidence_sha256: str
    external_caller_evidence_ids: Tuple[str, ...]
    external_caller_evidence_sha256: Tuple[str, ...]
    parity_evidence_ids: Tuple[str, ...]
    parity_evidence_sha256: Tuple[str, ...]
    authorization_authority_role: str
    authorization_key_id: str
    authorization_signature_algorithm: str
    authorization_signature: str
    authorization_decision: str
    authorized_at_utc: str
    expires_at_utc: str
    target_release: str
    source_size_bytes: int
    source_sha256: str
    _validation_token: object = field(repr=False, compare=False)


COMPATIBILITY_REMOVAL_AUTHORIZATION_SCHEMA_VERSION = 3
COMPATIBILITY_REMOVAL_AUTHORIZATION_ARTIFACT_TYPE = (
    "inferlab_compatibility_removal_authorization"
)
COMPATIBILITY_REMOVAL_AUTHORIZATION_DECISION = (
    "authorized_for_target_release_removal"
)
COMPATIBILITY_REMOVAL_AUTHORIZATION_AUTHORITY_ROLE = "release_owner"
COMPATIBILITY_REMOVAL_AUTHORIZATION_MAX_VALIDITY_SECONDS = 7 * 24 * 60 * 60
COMPATIBILITY_REMOVAL_AUTHORIZATION_CLOCK_SKEW_SECONDS = 5 * 60
_MAX_INPUT_BYTES = 64 * 1024
_FIELDS = frozenset(
    {
        "schema_version",
        "artifact_type",
        "authorization_id",
        "entry_id",
        "retirement_evidence_id",
        "retirement_evidence_sha256",
        "external_caller_evidence_ids",
        "external_caller_evidence_sha256",
        "parity_evidence_ids",
        "parity_evidence_sha256",
        "authorization_authority_role",
        "authorization_key_id",
        "authorization_signature_algorithm",
        "authorization_signature",
        "authorization_decision",
        "authorized_at_utc",
        "expires_at_utc",
        "target_release",
    }
)
_SIGNED_FIELDS = _FIELDS - {"authorization_signature"}
_VALIDATION_TOKEN = object()
_UNVERIFIED_TOKEN = object()


def compatibility_removal_authorization_signature(
    unsigned_payload: Dict[str, Any],
) -> str:
    if not isinstance(unsigned_payload, dict) or (
        frozenset(unsigned_payload) != _SIGNED_FIELDS
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization unsigned fields do not match the contract"
        )
    return _authorization_signature(unsigned_payload)


def _unsigned_payload_from_authorization(
    authorization: CompatibilityRemovalAuthorization,
) -> Dict[str, Any]:
    return {
        "schema_version": authorization.schema_version,
        "artifact_type": authorization.artifact_type,
        "authorization_id": authorization.authorization_id,
        "entry_id": authorization.entry_id,
        "retirement_evidence_id": authorization.retirement_evidence_id,
        "retirement_evidence_sha256": authorization.retirement_evidence_sha256,
        "external_caller_evidence_ids": list(
            authorization.external_caller_evidence_ids
        ),
        "external_caller_evidence_sha256": list(
            authorization.external_caller_evidence_sha256
        ),
        "parity_evidence_ids": list(authorization.parity_evidence_ids),
        "parity_evidence_sha256": list(authorization.parity_evidence_sha256),
        "authorization_authority_role": (
            authorization.authorization_authority_role
        ),
        "authorization_key_id": authorization.authorization_key_id,
        "authorization_signature_algorithm": (
            authorization.authorization_signature_algorithm
        ),
        "authorization_decision": authorization.authorization_decision,
        "authorized_at_utc": authorization.authorized_at_utc,
        "expires_at_utc": authorization.expires_at_utc,
        "target_release": authorization.target_release,
    }


def _verify_registered_authorization_signature(
    authorization: CompatibilityRemovalAuthorization,
) -> None:
    verify_registered_compatibility_removal_authorization_signature(
        _unsigned_payload_from_authorization(authorization),
        authorization.authorization_signature,
    )


def _require_loader_origin(
    authorization: CompatibilityRemovalAuthorization,
) -> None:
    if (
        not isinstance(authorization, CompatibilityRemovalAuthorization)
        or authorization._validation_token is not _VALIDATION_TOKEN
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization must originate from the strict loader"
        )


def validate_loaded_compatibility_removal_authorization(
    authorization: CompatibilityRemovalAuthorization,
    *,
    expected_entry_id: str,
    expected_target_release: str,
) -> None:
    _require_loader_origin(authorization)
    _validate_authorization_fields(
        authorization,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
    )
    _verify_registered_authorization_signature(authorization)


def _validate_authorization_fields(
    authorization: CompatibilityRemovalAuthorization,
    *,
    expected_entry_id: str,
    expected_target_release: str,
) -> None:
    if not isinstance(authorization, CompatibilityRemovalAuthorization):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization object is not canonical"
        )
    if (
        type(authorization.schema_version) is not int
        or authorization.schema_version
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_SCHEMA_VERSION
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization schema_version is not canonical"
        )
    if (
        authorization.artifact_type
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_ARTIFACT_TYPE
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization artifact_type is not canonical"
        )
    require_compatibility_evidence_token(
        "authorization_id", authorization.authorization_id
    )
    require_compatibility_evidence_token("entry_id", authorization.entry_id)
    if authorization.entry_id != expected_entry_id:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization entry_id does not match"
        )
    require_compatibility_evidence_token(
        "target_release", authorization.target_release
    )
    if authorization.target_release != expected_target_release:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization target_release does not match"
        )
    require_compatibility_evidence_token(
        "retirement_evidence_id", authorization.retirement_evidence_id
    )
    require_compatibility_evidence_sha256(
        "retirement_evidence_sha256",
        authorization.retirement_evidence_sha256,
    )
    validate_compatibility_evidence_reference_tuples(
        "external_caller_evidence",
        authorization.external_caller_evidence_ids,
        authorization.external_caller_evidence_sha256,
    )
    validate_compatibility_evidence_reference_tuples(
        "parity_evidence",
        authorization.parity_evidence_ids,
        authorization.parity_evidence_sha256,
    )
    validate_compatibility_evidence_disjoint_id_groups(
        (
            ("authorization", (authorization.authorization_id,)),
            ("retirement", (authorization.retirement_evidence_id,)),
            ("external_caller", authorization.external_caller_evidence_ids),
            ("parity", authorization.parity_evidence_ids),
        )
    )
    require_compatibility_evidence_token(
        "authorization_authority_role",
        authorization.authorization_authority_role,
    )
    if (
        authorization.authorization_authority_role
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_AUTHORITY_ROLE
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization authority role is not canonical"
        )
    require_compatibility_evidence_token(
        "authorization_key_id", authorization.authorization_key_id
    )
    if (
        authorization.authorization_key_id
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ID
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization key id does not match"
        )
    if (
        authorization.authorization_signature_algorithm
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_SIGNATURE_ALGORITHM
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization signature algorithm is not canonical"
        )
    require_compatibility_evidence_sha256(
        "authorization_signature", authorization.authorization_signature
    )
    if (
        authorization.authorization_decision
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_DECISION
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization decision is not canonical"
        )
    authorized_at = compatibility_evidence_utc_second_datetime(
        "authorized_at_utc", authorization.authorized_at_utc
    )
    expires_at = compatibility_evidence_utc_second_datetime(
        "expires_at_utc", authorization.expires_at_utc
    )
    if expires_at <= authorized_at:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization expiry must follow authorization time"
        )
    if (
        expires_at - authorized_at
        > timedelta(
            seconds=(
                COMPATIBILITY_REMOVAL_AUTHORIZATION_MAX_VALIDITY_SECONDS
            )
        )
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization validity window exceeds policy"
        )
    observed_at = datetime.now(timezone.utc).replace(microsecond=0)
    clock_skew = timedelta(
        seconds=COMPATIBILITY_REMOVAL_AUTHORIZATION_CLOCK_SKEW_SECONDS
    )
    if authorized_at > observed_at + clock_skew:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization time is too far in the future"
        )
    if expires_at < observed_at - clock_skew:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization has expired"
        )
    if type(authorization.source_size_bytes) is not int or not (
        0 < authorization.source_size_bytes <= _MAX_INPUT_BYTES
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization source size is invalid"
        )
    require_compatibility_evidence_sha256(
        "compatibility removal authorization source fingerprint",
        authorization.source_sha256,
    )


def load_compatibility_removal_authorization_document(
    authorization_path: str | Path,
    *,
    expected_entry_id: str,
    expected_target_release: str,
) -> CompatibilityRemovalAuthorization:
    try:
        document = load_strict_json_object_document(
            authorization_path,
            document_label="compatibility removal authorization",
            maximum_bytes=_MAX_INPUT_BYTES,
        )
    except StrictJsonDocumentError as exc:
        raise CompatibilityRetirementEvidenceError(str(exc)) from exc
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
            "compatibility removal authorization fields do not match the contract: "
            + "; ".join(details)
        )
    string_fields = (
        "artifact_type",
        "entry_id",
        "authorization_signature_algorithm",
        "authorization_decision",
        "authorized_at_utc",
        "expires_at_utc",
        "target_release",
    )
    if any(
        not isinstance(payload[field_name], str)
        for field_name in string_fields
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization scalar fields must be strings"
        )
    authorization = CompatibilityRemovalAuthorization(
        schema_version=payload["schema_version"],
        artifact_type=payload["artifact_type"],
        authorization_id=require_compatibility_evidence_token(
            "authorization_id", payload["authorization_id"]
        ),
        entry_id=payload["entry_id"],
        retirement_evidence_id=require_compatibility_evidence_token(
            "retirement_evidence_id", payload["retirement_evidence_id"]
        ),
        retirement_evidence_sha256=require_compatibility_evidence_sha256(
            "retirement_evidence_sha256",
            payload["retirement_evidence_sha256"],
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
        parity_evidence_ids=require_compatibility_evidence_token_list(
            "parity_evidence_ids",
            payload["parity_evidence_ids"],
            allow_empty=False,
        ),
        parity_evidence_sha256=require_compatibility_evidence_sha256_list(
            "parity_evidence_sha256", payload["parity_evidence_sha256"]
        ),
        authorization_authority_role=require_compatibility_evidence_token(
            "authorization_authority_role",
            payload["authorization_authority_role"],
        ),
        authorization_key_id=require_compatibility_evidence_token(
            "authorization_key_id", payload["authorization_key_id"]
        ),
        authorization_signature_algorithm=payload[
            "authorization_signature_algorithm"
        ],
        authorization_signature=require_compatibility_evidence_sha256(
            "authorization_signature", payload["authorization_signature"]
        ),
        authorization_decision=payload["authorization_decision"],
        authorized_at_utc=payload["authorized_at_utc"],
        expires_at_utc=payload["expires_at_utc"],
        target_release=payload["target_release"],
        source_size_bytes=document.source_size_bytes,
        source_sha256=document.source_sha256,
        _validation_token=_UNVERIFIED_TOKEN,
    )
    _validate_authorization_fields(
        authorization,
        expected_entry_id=expected_entry_id,
        expected_target_release=expected_target_release,
    )
    _verify_registered_authorization_signature(authorization)
    return replace(authorization, _validation_token=_VALIDATION_TOKEN)


def compatibility_removal_authorization_payload(
    authorization: CompatibilityRemovalAuthorization,
) -> Dict[str, object]:
    _require_loader_origin(authorization)
    _validate_authorization_fields(
        authorization,
        expected_entry_id=authorization.entry_id,
        expected_target_release=authorization.target_release,
    )
    _verify_registered_authorization_signature(authorization)
    return {
        "status": "validated_target_release_authorization",
        "schema_version": authorization.schema_version,
        "artifact_type": authorization.artifact_type,
        "authorization_id": authorization.authorization_id,
        "entry_id": authorization.entry_id,
        "retirement_evidence_id": authorization.retirement_evidence_id,
        "retirement_evidence_sha256": authorization.retirement_evidence_sha256,
        "external_caller_evidence_ids": list(
            authorization.external_caller_evidence_ids
        ),
        "external_caller_evidence_sha256": list(
            authorization.external_caller_evidence_sha256
        ),
        "parity_evidence_ids": list(authorization.parity_evidence_ids),
        "parity_evidence_sha256": list(authorization.parity_evidence_sha256),
        "authorization_authority_role": (
            authorization.authorization_authority_role
        ),
        "authorization_key_id": authorization.authorization_key_id,
        "authorization_signature_algorithm": (
            authorization.authorization_signature_algorithm
        ),
        "authorization_signature_verified": True,
        "authorization_signature_disclosed": False,
        "trusted_key_source": "fixed_environment_registry",
        "trust_root_cli_override_allowed": False,
        "authorization_decision": authorization.authorization_decision,
        "authorized_at_utc": authorization.authorized_at_utc,
        "expires_at_utc": authorization.expires_at_utc,
        "temporal_validity_verified": True,
        "maximum_validity_seconds": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_MAX_VALIDITY_SECONDS
        ),
        "allowed_clock_skew_seconds": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_CLOCK_SKEW_SECONDS
        ),
        "target_release": authorization.target_release,
        "source_size_bytes": authorization.source_size_bytes,
        "source_sha256": authorization.source_sha256,
        "source_path_disclosed": False,
        "claim_boundary": (
            "strict content, configured-key, canonical-role, and current-time-"
            "window authenticated target-release authorization only; does not "
            "identify a human signer, delete source, execute commands, or prove "
            "parity"
        ),
    }


def missing_compatibility_removal_authorization_payload(
    *, required_for_removal_permission: bool
) -> Dict[str, object]:
    return {
        "status": "not_provided",
        "required_schema_version": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_SCHEMA_VERSION
        ),
        "maximum_input_bytes": _MAX_INPUT_BYTES,
        "required_signature_algorithm": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_SIGNATURE_ALGORITHM
        ),
        "required_authorization_key_id": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ID
        ),
        "required_authorization_authority_role": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_AUTHORITY_ROLE
        ),
        "maximum_validity_seconds": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_MAX_VALIDITY_SECONDS
        ),
        "allowed_clock_skew_seconds": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_CLOCK_SKEW_SECONDS
        ),
        "required_temporal_fields": [
            "authorized_at_utc",
            "expires_at_utc",
        ],
        "trust_root_cli_override_allowed": False,
        "required_trusted_key_bytes": (
            COMPATIBILITY_REMOVAL_AUTHORIZATION_TRUSTED_KEY_BYTES
        ),
        "required_for_removal_permission": required_for_removal_permission,
        "source_path_disclosed": False,
    }
