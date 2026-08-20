#!/usr/bin/env python3
"""Bounded repository caller audit for compatibility entrypoints."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import re
from typing import Callable, Iterable, Optional, Tuple


_MAX_SCANNED_FILES = 4096
_MAX_FILE_BYTES = 2 * 1024 * 1024
_MAX_TOTAL_BYTES = 64 * 1024 * 1024
_MAX_REFERENCES = 256
_EXCLUDED_DIRECTORIES = frozenset(
    {
        ".git",
        ".venv",
        "__pycache__",
        "Testing",
        "build",
        "build-cuda-check",
        "build-cuda-preflight",
        "reports",
        "results",
    }
)
_SCANNABLE_SUFFIXES = frozenset(
    {
        ".bash",
        ".cc",
        ".cfg",
        ".cmake",
        ".cpp",
        ".cu",
        ".cuh",
        ".cxx",
        ".h",
        ".hpp",
        ".ini",
        ".json",
        ".py",
        ".sh",
        ".toml",
        ".yaml",
        ".yml",
    }
)
_SCANNABLE_FILENAMES = frozenset({"CMakeLists.txt", "Dockerfile", "Makefile"})
_SOURCE_SNAPSHOT_VALIDATION_TOKEN = object()


class CompatibilityRepositoryCallerAuditError(ValueError):
    """Raised when repository caller inventory cannot be established safely."""


@dataclass(frozen=True)
class CompatibilityRepositoryCallerReference:
    location: str
    classification: str


@dataclass(frozen=True)
class CompatibilityRepositoryCallerAudit:
    subject: str
    status: str
    scanned_file_count: int
    scanned_byte_count: int
    runtime_callers: Tuple[CompatibilityRepositoryCallerReference, ...]
    non_runtime_references: Tuple[CompatibilityRepositoryCallerReference, ...]
    method: str = "bounded_exact_token_repository_source_scan_v2"


@dataclass(frozen=True)
class _CompatibilityRepositorySource:
    relative_path: str
    text: str = field(repr=False)
    size_bytes: int


@dataclass(frozen=True)
class CompatibilityRepositorySourceSnapshot:
    """One bounded repository read shared by a caller-audit request."""

    repo_root: Path = field(repr=False)
    sources: Tuple[_CompatibilityRepositorySource, ...] = field(repr=False)
    scanned_file_count: int
    scanned_byte_count: int
    _validation_token: object = field(default=None, repr=False, compare=False)


def _is_scannable(path: Path) -> bool:
    return path.name in _SCANNABLE_FILENAMES or path.suffix in _SCANNABLE_SUFFIXES


def _source_files(repo_root: Path) -> Iterable[Path]:
    pending = [repo_root]
    while pending:
        directory = pending.pop()
        try:
            children = sorted(directory.iterdir(), key=lambda path: path.name)
        except OSError as exc:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit could not enumerate repository source"
            ) from exc
        for path in children:
            if path.name in _EXCLUDED_DIRECTORIES:
                continue
            try:
                if path.is_symlink():
                    if _is_scannable(path):
                        raise CompatibilityRepositoryCallerAuditError(
                            "compatibility caller audit encountered a source symlink"
                        )
                    continue
                if path.is_dir():
                    pending.append(path)
                elif path.is_file() and _is_scannable(path):
                    yield path
            except OSError as exc:
                raise CompatibilityRepositoryCallerAuditError(
                    "compatibility caller audit could not classify repository source"
                ) from exc


def _resolved_repository_root(repo_root: Path) -> Path:
    try:
        resolved = repo_root.resolve(strict=True)
        if not resolved.is_dir():
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit repository root is not a directory"
            )
    except CompatibilityRepositoryCallerAuditError:
        raise
    except OSError as exc:
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit repository root is unavailable"
        ) from exc
    return resolved


def capture_compatibility_repository_source_snapshot(
    repo_root: str | Path,
) -> CompatibilityRepositorySourceSnapshot:
    """Read the bounded source corpus once for one governance request."""

    root = _resolved_repository_root(Path(repo_root))
    sources: list[_CompatibilityRepositorySource] = []
    scanned_byte_count = 0
    for path in _source_files(root):
        if len(sources) >= _MAX_SCANNED_FILES:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit file count exceeds its bound"
            )
        try:
            size = path.stat().st_size
        except OSError as exc:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit could not stat repository source"
            ) from exc
        if not (0 <= size <= _MAX_FILE_BYTES):
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit source file exceeds its size bound"
            )
        scanned_byte_count += size
        if scanned_byte_count > _MAX_TOTAL_BYTES:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit total input exceeds its bound"
            )
        try:
            content = path.read_bytes()
            if len(content) != size:
                raise CompatibilityRepositoryCallerAuditError(
                    "compatibility caller audit source changed while reading"
                )
            text = content.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit source is not UTF-8"
            ) from exc
        except OSError as exc:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit could not read repository source"
            ) from exc
        sources.append(
            _CompatibilityRepositorySource(
                relative_path=path.relative_to(root).as_posix(),
                text=text,
                size_bytes=size,
            )
        )
    return CompatibilityRepositorySourceSnapshot(
        repo_root=root,
        sources=tuple(sources),
        scanned_file_count=len(sources),
        scanned_byte_count=scanned_byte_count,
        _validation_token=_SOURCE_SNAPSHOT_VALIDATION_TOKEN,
    )


def _require_repository_source_snapshot(
    repo_root: Path,
    source_snapshot: CompatibilityRepositorySourceSnapshot,
) -> CompatibilityRepositorySourceSnapshot:
    root = _resolved_repository_root(repo_root)
    if (
        type(source_snapshot) is not CompatibilityRepositorySourceSnapshot
        or source_snapshot._validation_token is not _SOURCE_SNAPSHOT_VALIDATION_TOKEN
    ):
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit source snapshot is not canonical"
        )
    if source_snapshot.repo_root != root:
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit source snapshot repository root mismatch"
        )
    if (
        type(source_snapshot.sources) is not tuple
        or type(source_snapshot.scanned_file_count) is not int
        or source_snapshot.scanned_file_count != len(source_snapshot.sources)
        or not (0 <= source_snapshot.scanned_file_count <= _MAX_SCANNED_FILES)
    ):
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit source snapshot file count is invalid"
        )
    checked_byte_count = 0
    seen_paths: set[str] = set()
    for source in source_snapshot.sources:
        if type(source) is not _CompatibilityRepositorySource:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit source snapshot record is invalid"
            )
        relative_path = Path(source.relative_path)
        if (
            not source.relative_path
            or relative_path.is_absolute()
            or ".." in relative_path.parts
            or source.relative_path in seen_paths
            or type(source.text) is not str
            or type(source.size_bytes) is not int
            or not (0 <= source.size_bytes <= _MAX_FILE_BYTES)
        ):
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit source snapshot record is invalid"
            )
        seen_paths.add(source.relative_path)
        checked_byte_count += source.size_bytes
        if checked_byte_count > _MAX_TOTAL_BYTES:
            raise CompatibilityRepositoryCallerAuditError(
                "compatibility caller audit source snapshot byte count exceeds its bound"
            )
    if (
        type(source_snapshot.scanned_byte_count) is not int
        or source_snapshot.scanned_byte_count != checked_byte_count
    ):
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit source snapshot byte count is invalid"
        )
    return source_snapshot


def _bounded_token_pattern(value: str) -> re.Pattern[str]:
    if not value or len(value) > 512 or "\x00" in value:
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit token is invalid"
        )
    return re.compile(
        rf"(?<![A-Za-z0-9_]){re.escape(value)}(?![A-Za-z0-9_])"
    )


def _audit_repository_tokens(
    repo_root: Path,
    *,
    subject: str,
    tokens: Tuple[str, ...],
    classify: Callable[[str, Tuple[str, ...], int, Tuple[str, ...]], str],
    source_snapshot: Optional[CompatibilityRepositorySourceSnapshot] = None,
) -> CompatibilityRepositoryCallerAudit:
    if not tokens or len(set(tokens)) != len(tokens):
        raise CompatibilityRepositoryCallerAuditError(
            "compatibility caller audit tokens are empty or duplicated"
        )
    patterns = tuple(_bounded_token_pattern(token) for token in tokens)
    snapshot = (
        capture_compatibility_repository_source_snapshot(repo_root)
        if source_snapshot is None
        else _require_repository_source_snapshot(repo_root, source_snapshot)
    )
    runtime_callers: list[CompatibilityRepositoryCallerReference] = []
    non_runtime_references: list[CompatibilityRepositoryCallerReference] = []

    for source in snapshot.sources:
        if not any(token in source.text for token in tokens):
            continue
        lines = tuple(source.text.splitlines())
        for line_index, line in enumerate(lines):
            matched_tokens = tuple(
                token
                for token, pattern in zip(tokens, patterns)
                if pattern.search(line) is not None
            )
            if not matched_tokens:
                continue
            classification = classify(
                source.relative_path, lines, line_index, matched_tokens
            )
            reference = CompatibilityRepositoryCallerReference(
                location=f"{source.relative_path}:{line_index + 1}",
                classification=classification,
            )
            destination = (
                runtime_callers
                if classification == "repository_runtime_caller_candidate"
                else non_runtime_references
            )
            destination.append(reference)
            if len(runtime_callers) + len(non_runtime_references) > _MAX_REFERENCES:
                raise CompatibilityRepositoryCallerAuditError(
                    "compatibility caller audit reference count exceeds its bound"
                )

    return CompatibilityRepositoryCallerAudit(
        subject=subject,
        status=(
            "runtime_callers_detected" if runtime_callers else "scanned_clear"
        ),
        scanned_file_count=snapshot.scanned_file_count,
        scanned_byte_count=snapshot.scanned_byte_count,
        runtime_callers=tuple(runtime_callers),
        non_runtime_references=tuple(non_runtime_references),
    )


def _cmake_dispatch_declaration_lines(lines: Tuple[str, ...]) -> frozenset[int]:
    declarations: set[int] = set()
    in_dispatch = False
    in_compatibility_targets = False
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("inferlab_add_shared_dispatch_group("):
            in_dispatch = True
            in_compatibility_targets = False
        if in_dispatch and "COMPATIBILITY_TARGETS" in stripped:
            in_compatibility_targets = True
        if in_dispatch and in_compatibility_targets:
            declarations.add(index)
        if in_dispatch and stripped.endswith(")"):
            in_dispatch = False
            in_compatibility_targets = False
    return frozenset(declarations)


def audit_cmake_compatibility_callers(
    repo_root: Path,
    compatibility_target: str,
    *,
    source_snapshot: Optional[CompatibilityRepositorySourceSnapshot] = None,
) -> CompatibilityRepositoryCallerAudit:
    if re.fullmatch(r"[A-Za-z0-9_.-]{1,256}", compatibility_target) is None:
        raise CompatibilityRepositoryCallerAuditError(
            "CMake compatibility caller audit target is not a bounded token"
        )

    def classify(
        relative_path: str,
        lines: Tuple[str, ...],
        line_index: int,
        matched_tokens: Tuple[str, ...],
    ) -> str:
        del matched_tokens
        line = lines[line_index]
        if relative_path == "CMakeLists.txt" and line_index in (
            _cmake_dispatch_declaration_lines(lines)
        ):
            return "cmake_dispatch_declaration"
        if re.search(r"[\"']compat_target[\"']\s*:", line):
            return "compatibility_metadata"
        nearby = " ".join(lines[max(0, line_index - 2) : line_index + 1])
        if re.search(r"CompatibilityTarget\w*\s*(?:\[\])?\s*=", nearby):
            return "compatibility_invocation_discriminator"
        return "repository_runtime_caller_candidate"

    audit = _audit_repository_tokens(
        repo_root,
        subject=f"cmake_target:{compatibility_target}",
        tokens=(compatibility_target,),
        classify=classify,
        source_snapshot=source_snapshot,
    )
    dispatch_declaration_count = sum(
        reference.classification == "cmake_dispatch_declaration"
        for reference in audit.non_runtime_references
    )
    if dispatch_declaration_count != 1:
        raise CompatibilityRepositoryCallerAuditError(
            "CMake compatibility caller audit requires one dispatch declaration"
        )
    return audit


def audit_python_compatibility_callers(
    repo_root: Path,
    *,
    entry_id: str,
    entrypoint_path: str,
    source_snapshot: Optional[CompatibilityRepositorySourceSnapshot] = None,
) -> CompatibilityRepositoryCallerAudit:
    path = Path(entrypoint_path)
    if (
        path.is_absolute()
        or path.suffix != ".py"
        or not path.parts
        or ".." in path.parts
    ):
        raise CompatibilityRepositoryCallerAuditError(
            "Python compatibility entrypoint path is invalid"
        )
    if re.fullmatch(r"[A-Za-z0-9_.-]{1,256}", entry_id) is None:
        raise CompatibilityRepositoryCallerAuditError(
            "Python compatibility entry id is not a bounded token"
        )
    module_name = entrypoint_path[:-3].replace("/", ".")
    filename = path.name
    stem = path.stem
    strong_token_values = (entrypoint_path, entry_id, module_name, filename)
    strong_tokens = frozenset(strong_token_values)
    tokens = tuple(dict.fromkeys((*strong_token_values, stem)))

    def classify(
        relative_path: str,
        lines: Tuple[str, ...],
        line_index: int,
        matched_tokens: Tuple[str, ...],
    ) -> str:
        if relative_path == "scripts/implementation_lifecycle.py":
            return "lifecycle_registry_declaration"
        if relative_path == entrypoint_path:
            return "compatibility_entrypoint_self_reference"
        if not (strong_tokens & frozenset(matched_tokens)):
            line = lines[line_index]
            python_import = re.search(
                rf"^\s*(?:import\s+{re.escape(stem)}\b|"
                rf"from\s+[A-Za-z0-9_.]+\s+import\s+[^#]*\b{re.escape(stem)}\b)",
                line,
            )
            return (
                "repository_runtime_caller_candidate"
                if python_import is not None
                else "unrelated_symbol_name_collision"
            )
        return "repository_runtime_caller_candidate"

    audit = _audit_repository_tokens(
        repo_root,
        subject=f"python_entry:{entry_id}",
        tokens=tokens,
        classify=classify,
        source_snapshot=source_snapshot,
    )
    classifications = tuple(
        reference.classification for reference in audit.non_runtime_references
    )
    if "lifecycle_registry_declaration" not in classifications:
        raise CompatibilityRepositoryCallerAuditError(
            "Python compatibility caller audit requires a lifecycle registry declaration"
        )
    if "compatibility_entrypoint_self_reference" not in classifications:
        raise CompatibilityRepositoryCallerAuditError(
            "Python compatibility caller audit requires an entrypoint self reference"
        )
    return audit
