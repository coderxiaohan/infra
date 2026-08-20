#!/usr/bin/env python3
"""Inspect one compatibility lifecycle without signing or deleting source."""

from __future__ import annotations

import argparse
import json

if __package__:
    from .cmake_compatibility_lifecycle import (
        load_cmake_compatibility_lifecycle,
    )
    from .compatibility_repository_caller_audit import (
        CompatibilityRepositoryCallerAuditError,
    )
    from .cmake_compatibility_manifest import CMakeCompatibilityManifestError
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
    )
    from .compatibility_governance_cli import (
        CompatibilityGovernanceCliError,
        add_compatibility_governance_arguments,
        compatibility_governance_location_values,
        load_compatibility_governance_projection,
        resolve_distinct_compatibility_locations,
        validate_compatibility_governance_arguments,
    )
    from .implementation_lifecycle import governed_lifecycle_for
    from .repo_paths import default_repo_root, resolved_repo_root
else:
    from cmake_compatibility_lifecycle import (  # type: ignore
        load_cmake_compatibility_lifecycle,
    )
    from compatibility_repository_caller_audit import (  # type: ignore
        CompatibilityRepositoryCallerAuditError,
    )
    from cmake_compatibility_manifest import (  # type: ignore
        CMakeCompatibilityManifestError,
    )
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
    )
    from compatibility_governance_cli import (  # type: ignore
        CompatibilityGovernanceCliError,
        add_compatibility_governance_arguments,
        compatibility_governance_location_values,
        load_compatibility_governance_projection,
        resolve_distinct_compatibility_locations,
        validate_compatibility_governance_arguments,
    )
    from implementation_lifecycle import governed_lifecycle_for  # type: ignore
    from repo_paths import default_repo_root, resolved_repo_root  # type: ignore


def parse_args() -> tuple[argparse.ArgumentParser, argparse.Namespace]:
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    entry_group = parser.add_mutually_exclusive_group(required=True)
    entry_group.add_argument("--entry-id")
    entry_group.add_argument("--cmake-entry-id")
    parser.add_argument("--cmake-compatibility-manifest")
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--pretty", action="store_true")
    add_compatibility_governance_arguments(parser)
    return parser, parser.parse_args()


def main() -> int:
    parser, args = parse_args()
    try:
        validate_compatibility_governance_arguments(args)
        if bool(args.cmake_entry_id) != bool(
            args.cmake_compatibility_manifest
        ):
            raise CompatibilityGovernanceCliError(
                "CMake entry id and compatibility manifest must be supplied together"
            )
        project_root = resolved_repo_root(args.repo_root)
        location_values = compatibility_governance_location_values(args)
        if args.cmake_compatibility_manifest:
            location_values["cmake_manifest"] = (
                args.cmake_compatibility_manifest
            )
        locations = resolve_distinct_compatibility_locations(
            location_values,
            repo_root=project_root,
        )
        lifecycle = (
            load_cmake_compatibility_lifecycle(
                locations["cmake_manifest"], args.cmake_entry_id, project_root
            )
            if args.cmake_entry_id
            else governed_lifecycle_for(args.entry_id, project_root)
        )
        projection = load_compatibility_governance_projection(
            lifecycle=lifecycle,
            args=args,
            resolved_locations=locations,
        )
    except (
        CompatibilityGovernanceCliError,
        CompatibilityRetirementEvidenceError,
        CMakeCompatibilityManifestError,
        CompatibilityRepositoryCallerAuditError,
        KeyError,
        OSError,
        RuntimeError,
        ValueError,
    ) as exc:
        parser.error(str(exc))
    print(
        json.dumps(
            projection,
            ensure_ascii=True,
            indent=2 if args.pretty else None,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
