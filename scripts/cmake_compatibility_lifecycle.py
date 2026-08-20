#!/usr/bin/env python3
"""Adapt one CMake-derived compatibility entry to the canonical lifecycle."""

from __future__ import annotations

from pathlib import Path
from typing import Optional

if __package__:
    from .cmake_compatibility_manifest import (
        CMakeCompatibilityManifestError,
        load_cmake_compatibility_entrypoint,
    )
    from .compatibility_repository_caller_audit import (
        CompatibilityRepositorySourceSnapshot,
        audit_cmake_compatibility_callers,
    )
    from .implementation_lifecycle import (
        ImplementationLifecycle,
        _compatibility_lifecycle_descriptor,
    )
else:
    from cmake_compatibility_manifest import (  # type: ignore
        CMakeCompatibilityManifestError,
        load_cmake_compatibility_entrypoint,
    )
    from compatibility_repository_caller_audit import (  # type: ignore
        CompatibilityRepositorySourceSnapshot,
        audit_cmake_compatibility_callers,
    )
    from implementation_lifecycle import (  # type: ignore
        ImplementationLifecycle,
        _compatibility_lifecycle_descriptor,
    )


def load_cmake_compatibility_lifecycle(
    manifest_path: str | Path,
    entry_id: str,
    repo_root: str | Path,
    *,
    source_snapshot: Optional[CompatibilityRepositorySourceSnapshot] = None,
) -> ImplementationLifecycle:
    try:
        entrypoint = load_cmake_compatibility_entrypoint(
            Path(manifest_path), entry_id
        )
    except KeyError as exc:
        raise CMakeCompatibilityManifestError(
            "unknown CMake compatibility entry"
        ) from exc
    audit = audit_cmake_compatibility_callers(
        Path(repo_root),
        entrypoint.compatibility_target,
        source_snapshot=source_snapshot,
    )
    scanned_callers = tuple(
        reference.location for reference in audit.runtime_callers
    )
    return _compatibility_lifecycle_descriptor(
        entry_id=entrypoint.entry_id,
        path=entrypoint.path,
        canonical_owner=entrypoint.canonical_target,
        replacement=entrypoint.replacement,
        target_release=entrypoint.target_release,
        repository_runtime_callers=scanned_callers,
        display_path=f"cmake_target:{entrypoint.compatibility_target}",
        display_replacement=f"cmake_target:{entrypoint.canonical_target}",
        repository_caller_audit_method=audit.method,
        repository_caller_audit_status=audit.status,
        repository_caller_audit_scope=(
            "repository_source_files_excluding_generated_virtualenv_results_"
            "reports_and_markdown"
        ),
        repository_caller_audit_file_count=audit.scanned_file_count,
        repository_caller_audit_byte_count=audit.scanned_byte_count,
        repository_non_runtime_references=tuple(
            f"{reference.location} [{reference.classification}]"
            for reference in audit.non_runtime_references
        ),
    )
