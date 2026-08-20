#!/usr/bin/env python3
"""Trust root, HMAC, and child-process secret boundary for removal authorization."""

from __future__ import annotations

import hashlib
import hmac
import json
import os
import re
from typing import Any, Dict

if __package__:
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
    )
else:
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_sha256,
    )


COMPATIBILITY_REMOVAL_AUTHORIZATION_SIGNATURE_ALGORITHM = "hmac-sha256"
COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ID = "inferlab_release_owner_v1"
COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ENV = (
    "INFERLAB_COMPATIBILITY_REMOVAL_AUTHORIZATION_HMAC_KEY_HEX"
)
COMPATIBILITY_REMOVAL_AUTHORIZATION_TRUSTED_KEY_BYTES = 32

_TRUSTED_KEY_HEX_RE = re.compile(r"^[0-9A-Fa-f]{64}$")


def _canonical_authorization_message(
    unsigned_payload: Dict[str, Any],
) -> bytes:
    if not isinstance(unsigned_payload, dict) or not unsigned_payload:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization unsigned payload is not canonical"
        )
    try:
        return json.dumps(
            unsigned_payload,
            ensure_ascii=True,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        ).encode("utf-8")
    except (TypeError, ValueError, RecursionError) as exc:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization signing payload is not canonical"
        ) from exc


def _require_trusted_key(trusted_key: bytes) -> None:
    if (
        not isinstance(trusted_key, bytes)
        or len(trusted_key)
        != COMPATIBILITY_REMOVAL_AUTHORIZATION_TRUSTED_KEY_BYTES
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization trusted key must contain exactly 32 bytes"
        )


def _load_registered_trusted_key() -> bytes:
    encoded_key = os.environ.get(COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ENV)
    if encoded_key is None:
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization trusted key is not configured"
        )
    if not _TRUSTED_KEY_HEX_RE.fullmatch(encoded_key):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization trusted key must contain "
            "exactly 32 bytes encoded as hexadecimal"
        )
    trusted_key = bytes.fromhex(encoded_key)
    _require_trusted_key(trusted_key)
    return trusted_key


def compatibility_removal_authorization_child_environment() -> Dict[str, str]:
    child_environment = dict(os.environ)
    child_environment.pop(COMPATIBILITY_REMOVAL_AUTHORIZATION_KEY_ENV, None)
    return child_environment


def _authorization_signature_with_trusted_key(
    unsigned_payload: Dict[str, Any],
    *,
    trusted_key: bytes,
) -> str:
    _require_trusted_key(trusted_key)
    return hmac.new(
        trusted_key,
        _canonical_authorization_message(unsigned_payload),
        hashlib.sha256,
    ).hexdigest()


def compatibility_removal_authorization_signature(
    unsigned_payload: Dict[str, Any],
) -> str:
    return _authorization_signature_with_trusted_key(
        unsigned_payload,
        trusted_key=_load_registered_trusted_key(),
    )


def verify_registered_compatibility_removal_authorization_signature(
    unsigned_payload: Dict[str, Any],
    signature: str,
) -> None:
    require_compatibility_evidence_sha256(
        "authorization_signature", signature
    )
    expected_signature = _authorization_signature_with_trusted_key(
        unsigned_payload,
        trusted_key=_load_registered_trusted_key(),
    )
    if not hmac.compare_digest(signature, expected_signature):
        raise CompatibilityRetirementEvidenceError(
            "compatibility removal authorization signature verification failed"
        )
