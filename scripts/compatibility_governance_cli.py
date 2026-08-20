#!/usr/bin/env python3
"""Shared CLI plumbing for compatibility governance evidence inspection."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Mapping

if __package__:
    from .compatibility_evidence_primitives import MAX_COMPATIBILITY_EVIDENCE_IDS
    from .compatibility_external_caller_evidence import (
        load_compatibility_external_caller_evidence,
    )
    from .implementation_lifecycle import (
        ImplementationLifecycle,
        lifecycle_payload_for_entry,
        load_compatibility_parity_evidence_for_lifecycle,
        load_compatibility_removal_authorization_for_lifecycle,
        load_compatibility_retirement_evidence_for_lifecycle,
    )
    from .repo_paths import resolve_repo_path
else:
    from compatibility_evidence_primitives import (  # type: ignore
        MAX_COMPATIBILITY_EVIDENCE_IDS,
    )
    from compatibility_external_caller_evidence import (  # type: ignore
        load_compatibility_external_caller_evidence,
    )
    from implementation_lifecycle import (  # type: ignore
        ImplementationLifecycle,
        lifecycle_payload_for_entry,
        load_compatibility_parity_evidence_for_lifecycle,
        load_compatibility_removal_authorization_for_lifecycle,
        load_compatibility_retirement_evidence_for_lifecycle,
    )
    from repo_paths import resolve_repo_path  # type: ignore


class CompatibilityGovernanceCliError(ValueError):
    """Bounded user-input error for compatibility governance CLI adapters."""


def add_compatibility_governance_arguments(parser: Any) -> None:
    parser.add_argument(
        "--compatibility-retirement-evidence",
        default=None,
        help=(
            "Import a strict caller/parity attestation for removal-candidate "
            "review; this never deletes the compatibility entry"
        ),
    )
    parser.add_argument(
        "--compatibility-external-caller-evidence",
        action="append",
        default=[],
        help=(
            "Import one strict reviewed external-caller inventory artifact "
            "referenced by the retirement attestation; repeat in attested "
            "id/fingerprint order"
        ),
    )
    parser.add_argument(
        "--compatibility-parity-evidence",
        action="append",
        default=[],
        help=(
            "Import one strict execute/verified parity artifact referenced by "
            "the retirement attestation; repeat in attested id/fingerprint order"
        ),
    )
    parser.add_argument(
        "--compatibility-removal-authorization",
        default=None,
        help=(
            "Import one strict target-release authorization bound to the "
            "retirement, external-caller, and parity evidence; this never "
            "deletes the compatibility entry"
        ),
    )


def validate_compatibility_governance_arguments(args: Any) -> None:
    parity_paths = args.compatibility_parity_evidence
    external_paths = args.compatibility_external_caller_evidence
    if len(parity_paths) > MAX_COMPATIBILITY_EVIDENCE_IDS:
        raise CompatibilityGovernanceCliError(
            "too many compatibility parity evidence documents were supplied"
        )
    if len(external_paths) > MAX_COMPATIBILITY_EVIDENCE_IDS:
        raise CompatibilityGovernanceCliError(
            "too many compatibility external caller evidence documents were supplied"
        )
    if not (
        bool(args.compatibility_retirement_evidence)
        == bool(external_paths)
        == bool(parity_paths)
    ):
        raise CompatibilityGovernanceCliError(
            "retirement attestation, external caller evidence, and parity "
            "evidence must be supplied together"
        )
    if (
        args.compatibility_removal_authorization
        and not args.compatibility_retirement_evidence
    ):
        raise CompatibilityGovernanceCliError(
            "target-release authorization requires retirement candidate evidence"
        )


def compatibility_governance_location_values(args: Any) -> Dict[str, str]:
    values: Dict[str, str] = {}
    if args.compatibility_retirement_evidence:
        values["retirement_evidence"] = args.compatibility_retirement_evidence
    if args.compatibility_removal_authorization:
        values["removal_authorization"] = (
            args.compatibility_removal_authorization
        )
    for index, path in enumerate(args.compatibility_external_caller_evidence):
        values[f"external_caller_evidence_{index}"] = path
    for index, path in enumerate(args.compatibility_parity_evidence):
        values[f"parity_evidence_{index}"] = path
    return values


def resolve_distinct_compatibility_locations(
    values: Mapping[str, str | Path],
    *,
    repo_root: Path,
) -> Dict[str, Path]:
    resolved: Dict[str, Path] = {}
    owners: Dict[Path, str] = {}
    for field_name, value in values.items():
        if not isinstance(value, (str, Path)) or not str(value).strip():
            raise CompatibilityGovernanceCliError(
                f"{field_name} must be a non-empty path"
            )
        try:
            location = resolve_repo_path(repo_root, str(value)).resolve(
                strict=False
            )
        except (OSError, RuntimeError, ValueError) as exc:
            raise CompatibilityGovernanceCliError(
                f"{field_name} path could not be resolved: {type(exc).__name__}"
            ) from exc
        prior_owner = owners.get(location)
        if prior_owner is not None:
            raise CompatibilityGovernanceCliError(
                f"{field_name} must differ from {prior_owner}"
            )
        owners[location] = field_name
        resolved[field_name] = location
    return resolved


def load_compatibility_governance_projection(
    *,
    lifecycle: ImplementationLifecycle,
    args: Any,
    resolved_locations: Mapping[str, Path],
) -> Dict[str, object]:
    if lifecycle.role != "compatibility_wrapper":
        raise CompatibilityGovernanceCliError(
            "governance evidence can only target a compatibility wrapper"
        )

    retirement_evidence = None
    external_caller_evidence = ()
    parity_evidence = ()
    removal_authorization = None
    if args.compatibility_retirement_evidence:
        retirement_evidence = load_compatibility_retirement_evidence_for_lifecycle(
            lifecycle,
            resolved_locations["retirement_evidence"],
        )
        external_caller_evidence = tuple(
            load_compatibility_external_caller_evidence(
                resolved_locations[f"external_caller_evidence_{index}"],
                expected_entry_id=lifecycle.entry_id,
                expected_target_release=lifecycle.target_release,
            )
            for index in range(
                len(args.compatibility_external_caller_evidence)
            )
        )
        parity_evidence = tuple(
            load_compatibility_parity_evidence_for_lifecycle(
                lifecycle,
                resolved_locations[f"parity_evidence_{index}"],
            )
            for index in range(len(args.compatibility_parity_evidence))
        )
        if args.compatibility_removal_authorization:
            removal_authorization = (
                load_compatibility_removal_authorization_for_lifecycle(
                    lifecycle,
                    resolved_locations["removal_authorization"],
                )
            )

    return lifecycle_payload_for_entry(
        lifecycle,
        retirement_evidence=retirement_evidence,
        parity_evidence=parity_evidence,
        external_caller_evidence=external_caller_evidence,
        removal_authorization=removal_authorization,
    )
