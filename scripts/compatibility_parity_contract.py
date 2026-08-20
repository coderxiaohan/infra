#!/usr/bin/env python3
"""Input and normalized JSON comparison contract for compatibility parity."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import re
import stat
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Mapping, Sequence, Tuple

if __package__:
    from .command_redaction import (
        is_sensitive_command_option,
        redact_sensitive_text,
    )
    from .strict_json_document import StrictJsonObjectDocument
else:
    from command_redaction import (  # type: ignore
        is_sensitive_command_option,
        redact_sensitive_text,
    )
    from strict_json_document import StrictJsonObjectDocument  # type: ignore


DEFAULT_OUTPUT_LIMIT_BYTES = 1024 * 1024
DEFAULT_ARTIFACT_LIMIT_BYTES = 16 * 1024 * 1024
HARD_OUTPUT_LIMIT_BYTES = 16 * 1024 * 1024
HARD_ARTIFACT_LIMIT_BYTES = 64 * 1024 * 1024

_MAX_COMMAND_JSON_CHARS = 32 * 1024
_MAX_COMMAND_ARGUMENTS = 256
_MAX_COMMAND_ARGUMENT_CHARS = 4096
_MAX_IGNORE_POINTERS = 64
_MAX_POINTER_CHARS = 512
_INLINE_CONTENT_OPTIONS = frozenset({"--prompt", "--query", "--text"})
_GOVERNANCE_IMPORT_OPTIONS = frozenset(
    {
        "--compatibility-retirement-evidence",
        "--compatibility-external-caller-evidence",
        "--compatibility-parity-evidence",
        "--compatibility-removal-authorization",
        "--compatibility-removal-authorization-key-id",
        "--compatibility-removal-authorization-key-env",
    }
)
_FORBIDDEN_IGNORED_ROOT_FIELDS = frozenset(
    {
        "artifact_type",
        "automatic_removal",
        "claim_boundary",
        "claim_scope",
        "executed",
        "mode",
        "removal_allowed",
        "replayed",
        "release_decision",
        "schema_version",
        "status",
    }
)
_PYTHON_LAUNCHER_RE = re.compile(r"^python(?:3(?:\.[0-9]+)*)?$")
_ENTRYPOINT_IDENTITY_DOMAIN = b"inferlab-compatibility-parity-entrypoint-v1"
MAX_COMPATIBILITY_ENTRYPOINT_BYTES = 512 * 1024 * 1024
_ENTRYPOINT_READ_CHUNK_BYTES = 1024 * 1024


class CompatibilityParityContractError(ValueError):
    """Raised when parity command or normalization inputs are not canonical."""


@dataclass(frozen=True)
class LifecycleEntrypointSnapshot:
    size_bytes: int
    sha256: str


def parse_command_json(
    parser: argparse.ArgumentParser,
    label: str,
    raw: str,
) -> Tuple[str, ...]:
    if not isinstance(raw, str) or not raw or len(raw) > _MAX_COMMAND_JSON_CHARS:
        parser.error(f"{label} command JSON is empty or exceeds its input limit")
    try:
        value = json.loads(raw)
    except (json.JSONDecodeError, RecursionError, ValueError):
        parser.error(f"{label} command must be a valid JSON string array")
    if not isinstance(value, list) or not (1 <= len(value) <= _MAX_COMMAND_ARGUMENTS):
        parser.error(
            f"{label} command must contain 1..{_MAX_COMMAND_ARGUMENTS} arguments"
        )
    command = []
    for argument in value:
        if (
            not isinstance(argument, str)
            or not argument
            or len(argument) > _MAX_COMMAND_ARGUMENT_CHARS
            or "\x00" in argument
        ):
            parser.error(f"{label} command contains an invalid argument")
        if any(
            argument == option or argument.startswith(option + "=")
            for option in _GOVERNANCE_IMPORT_OPTIONS
        ):
            parser.error(
                f"{label} command must not import governance evidence during parity capture"
            )
        option_name = argument.split("=", 1)[0]
        if is_sensitive_command_option(option_name) or (
            redact_sensitive_text(argument) != argument
        ):
            parser.error(
                f"{label} command must use environment-backed credentials"
            )
        if option_name in _INLINE_CONTENT_OPTIONS:
            parser.error(
                f"{label} command must use a controlled fixture file instead of inline content"
            )
        command.append(argument)
    return tuple(command)


def resolve_repo_path(
    parser: argparse.ArgumentParser,
    repo_root: Path,
    label: str,
    value: str,
) -> Path:
    if not isinstance(value, str) or not value.strip():
        parser.error(f"{label} must be a non-empty path")
    try:
        candidate = Path(value).expanduser()
        return (
            candidate.resolve(strict=False)
            if candidate.is_absolute()
            else (repo_root / candidate).resolve(strict=False)
        )
    except (OSError, RuntimeError, TypeError, ValueError):
        parser.error(f"{label} path could not be resolved")


def require_distinct_paths(
    parser: argparse.ArgumentParser,
    paths: Sequence[Tuple[str, Path]],
) -> None:
    owners: Dict[Path, str] = {}
    for label, path in paths:
        prior = owners.get(path)
        if prior is not None:
            parser.error(f"{label} must differ from {prior}")
        owners[path] = label


def lifecycle_entrypoint_identity(role: str, entrypoint: str) -> str:
    if role not in {"canonical", "compatibility"}:
        raise CompatibilityParityContractError(
            "lifecycle entrypoint role is not canonical"
        )
    if (
        not isinstance(entrypoint, str)
        or not entrypoint.strip()
        or "\x00" in entrypoint
        or len(entrypoint) > 4096
    ):
        raise CompatibilityParityContractError(
            f"{role} lifecycle entrypoint is invalid"
        )
    normalized = Path(entrypoint).as_posix()
    message = b"\x00".join(
        (
            _ENTRYPOINT_IDENTITY_DOMAIN,
            role.encode("ascii"),
            normalized.encode("utf-8"),
        )
    )
    return hashlib.sha256(message).hexdigest()


def _resolved_lifecycle_entrypoint(repo_root: Path, entrypoint: str) -> Path:
    if (
        not isinstance(repo_root, Path)
        or not isinstance(entrypoint, str)
        or not entrypoint.strip()
        or "\x00" in entrypoint
    ):
        raise CompatibilityParityContractError(
            "lifecycle entrypoint snapshot request is invalid"
        )
    try:
        candidate = Path(entrypoint).expanduser()
        return (
            candidate.resolve(strict=False)
            if candidate.is_absolute()
            else (repo_root / candidate).resolve(strict=False)
        )
    except (OSError, RuntimeError, TypeError, ValueError) as exc:
        raise CompatibilityParityContractError(
            "lifecycle entrypoint could not be resolved"
        ) from exc


def _snapshot_regular_entrypoint(path: Path) -> LifecycleEntrypointSnapshot:
    descriptor: int | None = None
    try:
        open_flags = os.O_RDONLY
        open_flags |= getattr(os, "O_CLOEXEC", 0)
        open_flags |= getattr(os, "O_NONBLOCK", 0)
        descriptor = os.open(path, open_flags)
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise CompatibilityParityContractError(
                "lifecycle entrypoint must be a regular file"
            )
        if not (0 < before.st_size <= MAX_COMPATIBILITY_ENTRYPOINT_BYTES):
            raise CompatibilityParityContractError(
                "lifecycle entrypoint size is outside the supported range"
            )
        digest = hashlib.sha256()
        observed_size = 0
        while True:
            chunk = os.read(descriptor, _ENTRYPOINT_READ_CHUNK_BYTES)
            if not chunk:
                break
            observed_size += len(chunk)
            if observed_size > MAX_COMPATIBILITY_ENTRYPOINT_BYTES:
                raise CompatibilityParityContractError(
                    "lifecycle entrypoint exceeds the bounded read limit"
                )
            digest.update(chunk)
        after = os.fstat(descriptor)
    except CompatibilityParityContractError:
        raise
    except (OSError, ValueError) as exc:
        raise CompatibilityParityContractError(
            "lifecycle entrypoint could not be fingerprinted"
        ) from exc
    finally:
        if descriptor is not None:
            try:
                os.close(descriptor)
            except OSError:
                pass
    before_identity = (
        before.st_dev,
        before.st_ino,
        before.st_size,
        before.st_mtime_ns,
        before.st_ctime_ns,
    )
    after_identity = (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
        after.st_ctime_ns,
    )
    if before_identity != after_identity or observed_size != before.st_size:
        raise CompatibilityParityContractError(
            "lifecycle entrypoint changed while it was fingerprinted"
        )
    return LifecycleEntrypointSnapshot(
        size_bytes=observed_size,
        sha256=digest.hexdigest(),
    )


def lifecycle_entrypoint_content_snapshot(
    repo_root: Path,
    entrypoint: str,
) -> LifecycleEntrypointSnapshot:
    return _snapshot_regular_entrypoint(
        _resolved_lifecycle_entrypoint(repo_root, entrypoint)
    )


def _resolved_command_entrypoint(
    parser: argparse.ArgumentParser,
    repo_root: Path,
    label: str,
    command: Tuple[str, ...],
) -> Path:
    argument_index = _command_entrypoint_index(parser, label, command)
    raw_entrypoint = command[argument_index]
    if raw_entrypoint.startswith("-"):
        parser.error(f"{label} command entrypoint must be an explicit path")
    try:
        candidate = Path(raw_entrypoint).expanduser()
        return (
            candidate.resolve(strict=False)
            if candidate.is_absolute()
            else (repo_root / candidate).resolve(strict=False)
        )
    except (OSError, RuntimeError, TypeError, ValueError):
        parser.error(f"{label} command entrypoint could not be resolved")


def _command_entrypoint_index(
    parser: argparse.ArgumentParser,
    label: str,
    command: Tuple[str, ...],
) -> int:
    if _PYTHON_LAUNCHER_RE.fullmatch(Path(command[0]).name):
        if len(command) < 2:
            parser.error(f"{label} Python command is missing its script entrypoint")
        return 1
    return 0


def _bind_compatibility_delegate(
    parser: argparse.ArgumentParser,
    *,
    repo_root: Path,
    compatibility_command: Tuple[str, ...],
    expected_canonical_entrypoint: str,
    delegate_override_option: str | None,
    canonical_entrypoint_identity_sha256: str,
) -> Dict[str, object]:
    if delegate_override_option is None:
        return {
            "compatibility_delegate_binding_status": "fixed_lifecycle_replacement",
            "compatibility_delegate_override_option": "",
            "compatibility_delegate_override_present": False,
            "compatibility_delegate_identity_sha256": (
                canonical_entrypoint_identity_sha256
            ),
            "compatibility_delegate_path_disclosed": False,
        }
    if (
        not isinstance(delegate_override_option, str)
        or not delegate_override_option.startswith("--")
        or "=" in delegate_override_option
        or len(delegate_override_option) > 64
    ):
        parser.error("lifecycle delegate override option is invalid")

    entrypoint_index = _command_entrypoint_index(
        parser, "compatibility", compatibility_command
    )
    arguments = compatibility_command[entrypoint_index + 1 :]
    override_values = []
    argument_index = 0
    while argument_index < len(arguments):
        argument = arguments[argument_index]
        if argument == "--":
            break
        if argument == delegate_override_option:
            if argument_index + 1 >= len(arguments):
                parser.error("compatibility delegate override is missing its value")
            override_values.append(arguments[argument_index + 1])
            argument_index += 2
            continue
        if argument.startswith(delegate_override_option + "="):
            override_values.append(argument[len(delegate_override_option) + 1 :])
            argument_index += 1
            continue
        if (
            argument.startswith("--")
            and len(argument) < len(delegate_override_option)
            and delegate_override_option.startswith(argument)
        ):
            parser.error("compatibility delegate override abbreviations are forbidden")
        argument_index += 1
    if len(override_values) > 1:
        parser.error("compatibility delegate override must not be repeated")
    override_present = bool(override_values)
    if override_present:
        override_value = override_values[0]
        if not override_value or "\x00" in override_value:
            parser.error("compatibility delegate override value is invalid")
        expected_path = _resolved_lifecycle_entrypoint(
            repo_root, expected_canonical_entrypoint
        )
        actual_path = _resolved_lifecycle_entrypoint(repo_root, override_value)
        if actual_path != expected_path:
            parser.error(
                "compatibility delegate override does not match the lifecycle replacement"
            )
    return {
        "compatibility_delegate_binding_status": (
            "explicit_lifecycle_replacement"
            if override_present
            else "default_lifecycle_replacement"
        ),
        "compatibility_delegate_override_option": delegate_override_option,
        "compatibility_delegate_override_present": override_present,
        "compatibility_delegate_identity_sha256": (
            canonical_entrypoint_identity_sha256
        ),
        "compatibility_delegate_path_disclosed": False,
    }


def bind_lifecycle_command_entrypoints(
    parser: argparse.ArgumentParser,
    *,
    repo_root: Path,
    canonical_command: Tuple[str, ...],
    compatibility_command: Tuple[str, ...],
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    compatibility_delegate_override_option: str | None,
    capture_content: bool,
) -> Dict[str, object]:
    expected_paths = {
        "canonical": resolve_repo_path(
            parser,
            repo_root,
            "canonical_lifecycle_entrypoint",
            expected_canonical_entrypoint,
        ),
        "compatibility": resolve_repo_path(
            parser,
            repo_root,
            "compatibility_lifecycle_entrypoint",
            expected_compatibility_entrypoint,
        ),
    }
    commands = {
        "canonical": canonical_command,
        "compatibility": compatibility_command,
    }
    for label, command in commands.items():
        actual_path = _resolved_command_entrypoint(
            parser, repo_root, label, command
        )
        if actual_path != expected_paths[label]:
            parser.error(
                f"{label} command entrypoint does not match the lifecycle registry"
            )
    try:
        canonical_identity = lifecycle_entrypoint_identity(
            "canonical", expected_canonical_entrypoint
        )
        compatibility_identity = lifecycle_entrypoint_identity(
            "compatibility", expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        parser.error(str(exc))
    delegate_binding = _bind_compatibility_delegate(
        parser,
        repo_root=repo_root,
        compatibility_command=compatibility_command,
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        delegate_override_option=compatibility_delegate_override_option,
        canonical_entrypoint_identity_sha256=canonical_identity,
    )
    if capture_content:
        try:
            canonical_snapshot = _snapshot_regular_entrypoint(
                expected_paths["canonical"]
            )
            compatibility_snapshot = _snapshot_regular_entrypoint(
                expected_paths["compatibility"]
            )
        except CompatibilityParityContractError as exc:
            parser.error(str(exc))
        content_snapshot_status = "captured_before_canonical_execution"
        content_snapshot_checkpoint_count = 1
    else:
        canonical_snapshot = LifecycleEntrypointSnapshot(0, "")
        compatibility_snapshot = LifecycleEntrypointSnapshot(0, "")
        content_snapshot_status = "not_run_dry_run"
        content_snapshot_checkpoint_count = 0
    return {
        "status": "lifecycle_entrypoints_exact",
        "source": "implementation_lifecycle",
        "canonical_entrypoint_identity_sha256": canonical_identity,
        "compatibility_entrypoint_identity_sha256": compatibility_identity,
        "content_snapshot_status": content_snapshot_status,
        "content_snapshot_checkpoint_count": content_snapshot_checkpoint_count,
        "canonical_entrypoint_size_bytes": canonical_snapshot.size_bytes,
        "canonical_entrypoint_sha256": canonical_snapshot.sha256,
        "compatibility_entrypoint_size_bytes": compatibility_snapshot.size_bytes,
        "compatibility_entrypoint_sha256": compatibility_snapshot.sha256,
        **delegate_binding,
        "raw_entrypoint_paths_disclosed": False,
        "entrypoint_content_disclosed": False,
    }


def _require_lifecycle_entrypoint_snapshot_unchanged(
    parser: argparse.ArgumentParser,
    *,
    binding: Mapping[str, object],
    repo_root: Path,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
) -> None:
    try:
        canonical_current = lifecycle_entrypoint_content_snapshot(
            repo_root, expected_canonical_entrypoint
        )
        compatibility_current = lifecycle_entrypoint_content_snapshot(
            repo_root, expected_compatibility_entrypoint
        )
    except CompatibilityParityContractError as exc:
        parser.error(str(exc))
    expected_current = {
        "canonical_entrypoint_size_bytes": canonical_current.size_bytes,
        "canonical_entrypoint_sha256": canonical_current.sha256,
        "compatibility_entrypoint_size_bytes": compatibility_current.size_bytes,
        "compatibility_entrypoint_sha256": compatibility_current.sha256,
    }
    if any(binding.get(key) != value for key, value in expected_current.items()):
        parser.error("lifecycle entrypoint content changed between parity phases")


def checkpoint_lifecycle_command_entrypoints_after_canonical(
    parser: argparse.ArgumentParser,
    *,
    binding: Mapping[str, object],
    repo_root: Path,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
) -> Dict[str, object]:
    checkpointed = dict(binding)
    if (
        checkpointed.get("content_snapshot_status")
        != "captured_before_canonical_execution"
        or checkpointed.get("content_snapshot_checkpoint_count") != 1
    ):
        parser.error("canonical entrypoint pre-snapshot state is not canonical")
    _require_lifecycle_entrypoint_snapshot_unchanged(
        parser,
        binding=checkpointed,
        repo_root=repo_root,
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        expected_compatibility_entrypoint=expected_compatibility_entrypoint,
    )
    checkpointed["content_snapshot_status"] = (
        "captured_before_and_after_canonical_execution_stable"
    )
    checkpointed["content_snapshot_checkpoint_count"] = 2
    return checkpointed


def finalize_lifecycle_command_entrypoints(
    parser: argparse.ArgumentParser,
    *,
    binding: Mapping[str, object],
    repo_root: Path,
    expected_canonical_entrypoint: str,
    expected_compatibility_entrypoint: str,
    executed: bool,
    compatibility_executed: bool,
) -> Dict[str, object]:
    finalized = dict(binding)
    if not executed:
        if (
            finalized.get("content_snapshot_status") != "not_run_dry_run"
            or finalized.get("content_snapshot_checkpoint_count") != 0
            or compatibility_executed
        ):
            parser.error("dry-run entrypoint snapshot state is not canonical")
        return finalized
    if (
        finalized.get("content_snapshot_status")
        != "captured_before_and_after_canonical_execution_stable"
        or finalized.get("content_snapshot_checkpoint_count") != 2
    ):
        parser.error("canonical entrypoint checkpoint state is not complete")
    _require_lifecycle_entrypoint_snapshot_unchanged(
        parser,
        binding=finalized,
        repo_root=repo_root,
        expected_canonical_entrypoint=expected_canonical_entrypoint,
        expected_compatibility_entrypoint=expected_compatibility_entrypoint,
    )
    finalized["content_snapshot_status"] = (
        "captured_before_after_canonical_and_after_compatibility_execution_stable"
        if compatibility_executed
        else "captured_before_after_canonical_and_after_compatibility_skip_stable"
    )
    finalized["content_snapshot_checkpoint_count"] = 3
    return finalized


def _decode_pointer(pointer: str) -> Tuple[str, ...]:
    if (
        not isinstance(pointer, str)
        or not pointer.startswith("/")
        or len(pointer) > _MAX_POINTER_CHARS
    ):
        raise CompatibilityParityContractError(
            "ignored JSON pointers must be non-root RFC 6901 pointers"
        )
    decoded = []
    for raw_segment in pointer[1:].split("/"):
        segment = []
        index = 0
        while index < len(raw_segment):
            if raw_segment[index] != "~":
                segment.append(raw_segment[index])
                index += 1
                continue
            if index + 1 >= len(raw_segment) or raw_segment[index + 1] not in "01":
                raise CompatibilityParityContractError(
                    "ignored JSON pointer contains an invalid escape"
                )
            segment.append("~" if raw_segment[index + 1] == "0" else "/")
            index += 2
        decoded.append("".join(segment))
    return tuple(decoded)


def validate_ignore_pointers(
    values: Sequence[str],
) -> Tuple[Tuple[str, ...], ...]:
    if len(values) > _MAX_IGNORE_POINTERS:
        raise CompatibilityParityContractError(
            f"at most {_MAX_IGNORE_POINTERS} ignored JSON pointers are allowed"
        )
    pointers = tuple(_decode_pointer(value) for value in values)
    if len(set(pointers)) != len(pointers):
        raise CompatibilityParityContractError(
            "ignored JSON pointers must be unique"
        )
    if any(pointer[0] in _FORBIDDEN_IGNORED_ROOT_FIELDS for pointer in pointers):
        raise CompatibilityParityContractError(
            "governance-critical root fields cannot be ignored"
        )
    for index, pointer in enumerate(pointers):
        for other in pointers[index + 1 :]:
            shorter, longer = (
                (pointer, other)
                if len(pointer) <= len(other)
                else (other, pointer)
            )
            if longer[: len(shorter)] == shorter:
                raise CompatibilityParityContractError(
                    "ignored JSON pointers must not overlap"
                )
    return pointers


def parse_ignore_pointers(
    parser: argparse.ArgumentParser,
    values: Sequence[str],
) -> Tuple[Tuple[str, ...], ...]:
    try:
        return validate_ignore_pointers(values)
    except CompatibilityParityContractError as exc:
        parser.error(str(exc))


def _remove_object_pointer(payload: Dict[str, Any], pointer: Tuple[str, ...]) -> None:
    current: Any = payload
    for segment in pointer[:-1]:
        if not isinstance(current, dict) or segment not in current:
            raise ValueError("ignored JSON pointer is missing from an artifact")
        current = current[segment]
    if not isinstance(current, dict):
        raise ValueError("ignored JSON pointers may only remove object members")
    if pointer[-1] not in current:
        raise ValueError("ignored JSON pointer is missing from an artifact")
    if isinstance(current[pointer[-1]], (dict, list)):
        raise ValueError("ignored JSON pointers may only remove scalar values")
    del current[pointer[-1]]


def normalized_document(
    document: StrictJsonObjectDocument,
    pointers: Tuple[Tuple[str, ...], ...],
) -> bytes:
    payload = copy.deepcopy(document.payload)
    for pointer in pointers:
        _remove_object_pointer(payload, pointer)
    return json.dumps(
        payload,
        ensure_ascii=True,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
