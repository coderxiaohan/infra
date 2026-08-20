#!/usr/bin/env python3
"""Shared lexical and bounded-reference contracts for compatibility evidence."""

from __future__ import annotations

import re
from datetime import datetime, timezone
from typing import Any, Tuple


class CompatibilityRetirementEvidenceError(ValueError):
    """Raised when compatibility governance evidence violates its contract."""


def compatibility_parity_evidence_error(
    message: str,
) -> CompatibilityRetirementEvidenceError:
    return CompatibilityRetirementEvidenceError(
        f"compatibility parity evidence {message}"
    )


MAX_COMPATIBILITY_EVIDENCE_IDS = 64
_TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
_UTC_SECOND_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def require_compatibility_evidence_token(field_name: str, value: Any) -> str:
    if not isinstance(value, str) or not _TOKEN_RE.fullmatch(value):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} must be a stable token"
        )
    return value


def compatibility_evidence_utc_second_datetime(
    field_name: str,
    value: Any,
) -> datetime:
    if not isinstance(value, str) or not _UTC_SECOND_RE.fullmatch(value):
        raise CompatibilityRetirementEvidenceError(
            f"{field_name} must use YYYY-MM-DDTHH:MM:SSZ"
        )
    try:
        parsed = datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise CompatibilityRetirementEvidenceError(
            f"{field_name} is not a valid UTC timestamp"
        ) from exc
    return parsed.replace(tzinfo=timezone.utc)


def require_compatibility_evidence_utc_second(
    field_name: str,
    value: Any,
) -> str:
    compatibility_evidence_utc_second_datetime(field_name, value)
    return value


def require_compatibility_evidence_sha256(
    field_name: str,
    value: Any,
) -> str:
    if not isinstance(value, str) or not _SHA256_RE.fullmatch(value):
        raise CompatibilityRetirementEvidenceError(
            f"{field_name} must be a lowercase SHA-256 digest"
        )
    return value


def require_compatibility_evidence_token_list(
    field_name: str,
    value: Any,
    *,
    allow_empty: bool,
) -> Tuple[str, ...]:
    if (
        not isinstance(value, list)
        or len(value) > MAX_COMPATIBILITY_EVIDENCE_IDS
        or (not allow_empty and not value)
    ):
        qualifier = "a token list" if allow_empty else "a non-empty token list"
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} must be {qualifier} "
            f"with at most {MAX_COMPATIBILITY_EVIDENCE_IDS} entries"
        )
    tokens = tuple(
        require_compatibility_evidence_token(f"{field_name}[{index}]", item)
        for index, item in enumerate(value)
    )
    if len(set(tokens)) != len(tokens):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} contains duplicates"
        )
    return tokens


def require_compatibility_evidence_sha256_list(
    field_name: str,
    value: Any,
) -> Tuple[str, ...]:
    if (
        not isinstance(value, list)
        or not value
        or len(value) > MAX_COMPATIBILITY_EVIDENCE_IDS
    ):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} must be a non-empty "
            f"digest list with at most {MAX_COMPATIBILITY_EVIDENCE_IDS} entries"
        )
    digests = tuple(
        require_compatibility_evidence_sha256(f"{field_name}[{index}]", item)
        for index, item in enumerate(value)
    )
    if len(set(digests)) != len(digests):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} contains duplicates"
        )
    return digests


def validate_compatibility_evidence_token_tuple(
    field_name: str,
    values: Any,
    *,
    allow_empty: bool,
) -> Tuple[str, ...]:
    if (
        not isinstance(values, tuple)
        or len(values) > MAX_COMPATIBILITY_EVIDENCE_IDS
        or (not allow_empty and not values)
    ):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} is not a canonical "
            "bounded token tuple"
        )
    for index, token in enumerate(values):
        require_compatibility_evidence_token(f"{field_name}[{index}]", token)
    if len(set(values)) != len(values):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} contains duplicates"
        )
    return values


def validate_compatibility_evidence_sha256_tuple(
    field_name: str,
    values: Any,
) -> Tuple[str, ...]:
    if (
        not isinstance(values, tuple)
        or not values
        or len(values) > MAX_COMPATIBILITY_EVIDENCE_IDS
    ):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} is not a canonical "
            "bounded digest tuple"
        )
    for index, digest in enumerate(values):
        require_compatibility_evidence_sha256(f"{field_name}[{index}]", digest)
    if len(set(values)) != len(values):
        raise CompatibilityRetirementEvidenceError(
            f"compatibility evidence field {field_name} contains duplicates"
        )
    return values


def validate_compatibility_evidence_reference_tuples(
    reference_name: str,
    evidence_ids: Any,
    evidence_sha256: Any,
) -> None:
    ids = validate_compatibility_evidence_token_tuple(
        f"{reference_name}_ids", evidence_ids, allow_empty=False
    )
    digests = validate_compatibility_evidence_sha256_tuple(
        f"{reference_name}_sha256", evidence_sha256
    )
    if len(ids) != len(digests):
        raise CompatibilityRetirementEvidenceError(
            f"{reference_name} id and source fingerprint counts do not reconcile"
        )


def validate_compatibility_evidence_disjoint_id_groups(
    groups: Any,
) -> None:
    if not isinstance(groups, tuple) or not groups:
        raise CompatibilityRetirementEvidenceError(
            "compatibility evidence id namespace requires canonical groups"
        )
    seen_group_names = set()
    seen_ids = {}
    for group in groups:
        if (
            not isinstance(group, tuple)
            or len(group) != 2
            or not isinstance(group[0], str)
        ):
            raise CompatibilityRetirementEvidenceError(
                "compatibility evidence id namespace group is not canonical"
            )
        group_name, evidence_ids = group
        require_compatibility_evidence_token("evidence id group", group_name)
        if group_name in seen_group_names:
            raise CompatibilityRetirementEvidenceError(
                "compatibility evidence id namespace contains duplicate groups"
            )
        seen_group_names.add(group_name)
        canonical_ids = validate_compatibility_evidence_token_tuple(
            group_name, evidence_ids, allow_empty=False
        )
        for evidence_id in canonical_ids:
            previous_group = seen_ids.get(evidence_id)
            if previous_group is not None:
                raise CompatibilityRetirementEvidenceError(
                    "compatibility evidence id is reused across "
                    f"{previous_group} and {group_name}"
                )
            seen_ids[evidence_id] = group_name
