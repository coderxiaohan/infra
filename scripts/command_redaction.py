#!/usr/bin/env python3
"""Shared command/output redaction helpers for artifact-producing runners."""

from __future__ import annotations

import re
import shlex
import hashlib
from pathlib import Path
from typing import Any, Dict, List, Sequence


REDACTED_VALUE = "<redacted>"
LOCAL_PATH_PREFIXES = {
    ".",
    "..",
    "artifact",
    "artifacts",
    "benchmark",
    "benchmarks",
    "build",
    "cache",
    "checkpoint",
    "checkpoints",
    "config",
    "configs",
    "data",
    "dataset",
    "datasets",
    "deploy",
    "dist",
    "engine",
    "engines",
    "fixture",
    "fixtures",
    "log",
    "logs",
    "model",
    "models",
    "output",
    "outputs",
    "profile",
    "profiles",
    "profiling",
    "report",
    "reports",
    "result",
    "results",
    "run",
    "runs",
    "tmp",
    "tokenizer",
    "tokenizers",
    "weight",
    "weights",
}
LOCAL_PATH_SUFFIXES = {
    ".bin",
    ".ckpt",
    ".csv",
    ".engine",
    ".gguf",
    ".json",
    ".jsonl",
    ".log",
    ".ncu-rep",
    ".nsys-rep",
    ".onnx",
    ".out",
    ".plan",
    ".prom",
    ".pt",
    ".pth",
    ".safetensors",
    ".sqlite",
    ".stderr",
    ".stdout",
    ".txt",
    ".yaml",
    ".yml",
}
PATH_COMMAND_OPTION_NAMES = {
    "artifact",
    "artifacts",
    "baseline",
    "binary",
    "build",
    "build-dir",
    "cache",
    "check",
    "checks",
    "clang",
    "cmake",
    "config",
    "cuda-home",
    "device-model",
    "engine-dir",
    "engine_dir",
    "evidence-dir",
    "input",
    "json-output",
    "llama-cpp-dir",
    "model-path",
    "model_path",
    "ndk",
    "output",
    "profile",
    "profiling-root",
    "prompt-file",
    "report",
    "repo-root",
    "results",
    "root",
    "summary",
    "sysroot",
    "template",
    "textfile-dir",
    "toolchain",
}
CONTENT_COMMAND_OPTION_NAMES = {
    "body",
    "content",
    "d",
    "data",
    "data-ascii",
    "data-binary",
    "data-raw",
    "data-urlencode",
    "json",
    "message",
    "messages",
    "p",
    "payload",
    "prompt",
    "request-body",
}

KEY_VALUE_RE = re.compile(
    r"(?i)\b(?P<key>[a-z][a-z0-9_.-]{0,63})(?P<pre>\s*)"
    r"(?P<sep>[:=])(?P<post>\s*)"
    r"(?P<quote>['\"]?)(?P<value>[^'\"\s,;]+)(?P=quote)"
)
QUOTED_KEY_VALUE_RE = re.compile(
    r"(?is)\b(?P<key>[a-z][a-z0-9_.-]{0,63})(?P<pre>\s*)"
    r"(?P<sep>[:=])(?P<post>\s*)"
    r"(?P<quote>['\"])(?P<value>.*?)(?P=quote)"
)
PEM_ASSIGNMENT_START_RE = re.compile(
    r"(?i)\b(?P<key>[a-z][a-z0-9_.-]{0,63})(?P<pre>\s*)"
    r"(?P<sep>[:=])(?P<post>[ \t]*)(?P<quote>['\"]?)"
    r"(?P<header>-----BEGIN[ \t]+"
    r"(?P<label>[a-z0-9][a-z0-9 _./-]{0,80})-----)"
)
PEM_RECORD_BOUNDARY_RE = re.compile(
    r"\r?\n(?=(?:[ \t]*\r?(?:\n|\Z)|"
    r"[ \t]*[a-z][a-z0-9_.-]{0,63}[ \t]*[:=]))",
    re.IGNORECASE | re.MULTILINE,
)
INLINE_FIELD_BOUNDARY_RE = re.compile(
    r"[ \t]+(?=[a-z][a-z0-9_.-]{0,63}[ \t]*[:=])",
    re.IGNORECASE,
)
AUTHORIZATION_HEADER_RE = re.compile(
    r"(?i)\b((?:proxy[-_])?authorization\s*[:=]\s*)(['\"]?)([^'\"\r\n,;]+)(\2)"
)
BEARER_VALUE_RE = re.compile(r"(?i)\b(bearer\s+)([A-Za-z0-9._~+/-]{8,})")
OPENAI_KEY_RE = re.compile(r"\bsk-[A-Za-z0-9_-]{12,}\b")
URL_CREDENTIAL_RE = re.compile(r"(?i)\b([a-z][a-z0-9+.-]*://)([^/\s:@]+):([^/\s@]+)@")
USERPASS_RE = re.compile(r"^[^:\s/@]+:[^:\s/@]+$")
BARE_USERPASS_RE = re.compile(
    r"(?i)(?<![a-z0-9._%+-])(?P<username>[a-z][a-z0-9._-]{0,63}):"
    r"(?P<password>[^\s/@:,;]{4,})(?![a-z0-9._%+-])"
)
COMMON_CREDENTIAL_USERNAMES = {
    "admin",
    "client",
    "proxy",
    "root",
    "service",
    "user",
    "username",
}

SAFE_ENV_NAME_OPTIONS = {"api_key_env"}
SENSITIVE_COMMAND_OPTION_NAMES = {
    "api_key",
    "apikey",
    "access_token",
    "auth_token",
    "bearer_token",
    "password",
    "passwd",
    "secret",
    "authorization",
    "credential",
    "credentials",
    "hf_token",
    "token",
}
SENSITIVE_VALUE_OPTIONS = {
    "u",
    "user",
    "proxy_user",
    "proxy_user_pass",
    "proxy_user_password",
}


def command_line(parts: Sequence[Any]) -> str:
    return " ".join(shlex.quote(str(part)) for part in parts)


def normalized_secret_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def is_sensitive_field_name(name: str, *, redact_env_names: bool = False) -> bool:
    normalized = normalized_secret_name(name)
    if not normalized:
        return False
    if not redact_env_names and (normalized in SAFE_ENV_NAME_OPTIONS or normalized.endswith("_env")):
        return False
    return (
        normalized in SENSITIVE_COMMAND_OPTION_NAMES
        or normalized in SENSITIVE_VALUE_OPTIONS
        or normalized.endswith("_password")
        or normalized.endswith("_passwd")
        or normalized.endswith("_secret")
        or normalized.endswith("_token")
        or normalized == "private_key"
        or normalized.endswith("_private_key")
        or "secret" in normalized.split("_")
        or (redact_env_names and normalized.endswith("_env"))
        or "api_key" in normalized
    )


def is_sensitive_command_option(option: str) -> bool:
    raw = option.strip()
    if "=" in raw:
        raw = raw.split("=", 1)[0]
    return is_sensitive_field_name(raw.lstrip("-"))


def redact_pem_assignments(text: str) -> str:
    chunks = []
    cursor = 0
    search_from = 0
    while True:
        match = PEM_ASSIGNMENT_START_RE.search(text, search_from)
        if match is None:
            break
        if not is_sensitive_field_name(match.group("key")):
            search_from = match.end("header")
            continue

        value_start = match.start("header")
        end_marker = re.compile(
            rf"-----END[ \t]+{re.escape(match.group('label'))}-----",
            re.IGNORECASE,
        ).search(text, match.end("header"))
        if end_marker is not None:
            value_end = end_marker.end()
        else:
            boundary_matches = [
                candidate
                for candidate in (
                    PEM_RECORD_BOUNDARY_RE.search(text, match.end("header")),
                    INLINE_FIELD_BOUNDARY_RE.search(text, match.end("header")),
                )
                if candidate is not None
            ]
            value_end = min(
                (candidate.start() for candidate in boundary_matches),
                default=len(text),
            )

        chunks.append(text[cursor:value_start])
        chunks.append(REDACTED_VALUE)
        cursor = value_end
        search_from = value_end

    if not chunks:
        return text
    chunks.append(text[cursor:])
    return "".join(chunks)


def redact_sensitive_text(value: Any) -> str:
    text = "" if value is None else str(value)
    if not text:
        return text
    text = URL_CREDENTIAL_RE.sub(lambda match: f"{match.group(1)}{REDACTED_VALUE}@", text)
    text = AUTHORIZATION_HEADER_RE.sub(
        lambda match: f"{match.group(1)}{match.group(2)}{REDACTED_VALUE}{match.group(2)}",
        text,
    )
    text = BEARER_VALUE_RE.sub(lambda match: f"{match.group(1)}{REDACTED_VALUE}", text)
    text = OPENAI_KEY_RE.sub(REDACTED_VALUE, text)

    def replace_quoted_secret(match: re.Match[str]) -> str:
        if not is_sensitive_field_name(match.group("key")):
            return match.group(0)
        return (
            f"{match.group('key')}{match.group('pre')}{match.group('sep')}"
            f"{match.group('post')}{match.group('quote')}{REDACTED_VALUE}"
            f"{match.group('quote')}"
        )

    text = QUOTED_KEY_VALUE_RE.sub(replace_quoted_secret, text)
    text = redact_pem_assignments(text)

    def replace_secret(match: re.Match[str]) -> str:
        if not is_sensitive_field_name(match.group("key")):
            return match.group(0)
        return (
            f"{match.group('key')}{match.group('pre')}{match.group('sep')}"
            f"{match.group('post')}{match.group('quote')}{REDACTED_VALUE}"
            f"{match.group('quote')}"
        )

    text = KEY_VALUE_RE.sub(replace_secret, text)

    def replace_userpass(match: re.Match[str]) -> str:
        username = match.group("username").lower()
        password = match.group("password")
        has_mixed_secret_shape = (
            len(password) >= 8
            or (any(char.isalpha() for char in password) and any(char.isdigit() for char in password))
        )
        return REDACTED_VALUE if username in COMMON_CREDENTIAL_USERNAMES or has_mixed_secret_shape else match.group(0)

    return BARE_USERPASS_RE.sub(replace_userpass, text)


def redacted_path_label(value: Any) -> str:
    """Return a stable artifact-safe label for a local path-like value."""
    text = "" if value is None else redact_sensitive_text(str(value))
    if not text or text == "missing":
        return text
    digest = hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]
    suffix = Path(text).suffix.lower()
    if suffix and len(suffix) <= 16:
        return f"path_{digest}{suffix}"
    return f"path_{digest}"


def normalized_command_option(option: str) -> str:
    raw = str(option).strip()
    if "=" in raw:
        raw = raw.split("=", 1)[0]
    return raw.lstrip("-").replace("_", "-").lower()


def is_path_command_option(option: str, extra_options: Sequence[str] = ()) -> bool:
    normalized = normalized_command_option(option)
    extra = {normalized_command_option(item) for item in extra_options}
    return normalized in PATH_COMMAND_OPTION_NAMES or normalized in extra


def is_content_command_option(option: str, extra_options: Sequence[str] = ()) -> bool:
    normalized = normalized_command_option(option)
    extra = {normalized_command_option(item) for item in extra_options}
    return normalized in CONTENT_COMMAND_OPTION_NAMES or normalized in extra


def _short_option_names(extra_options: Sequence[str] = ()) -> set[str]:
    names = set(CONTENT_COMMAND_OPTION_NAMES)
    names.update(normalized_command_option(item) for item in extra_options)
    return {name for name in names if len(name) == 1}


def split_combined_content_option(text: str, extra_options: Sequence[str] = ()) -> tuple[str, str] | None:
    """Return (option, value) for curl/CLI forms such as -d'{}' or -pPrompt."""
    if not text.startswith("-") or text.startswith("--") or len(text) <= 2:
        return None
    option = text[:2]
    if normalized_command_option(option) not in _short_option_names(extra_options):
        return None
    return option, text[2:]


def split_combined_header_option(text: str) -> tuple[str, str] | None:
    """Return (option, value) for curl -HHeader:Value forms."""
    if len(text) <= 2 or text[:2] not in {"-H"}:
        return None
    return text[:2], text[2:]


def looks_like_local_path(value: Any) -> bool:
    """Heuristic for private local paths while preserving public model ids."""
    text = "" if value is None else str(value).strip()
    if not text or text == "missing" or "://" in text:
        return False
    text = text.strip("'\"")
    if not text:
        return False
    if text.startswith(("/", "./", "../", "~")) or "\\" in text:
        return True
    normalized = text.replace("\\", "/")
    parts = [part for part in normalized.split("/") if part]
    if len(parts) < 2:
        return False
    first = parts[0].lower()
    suffix = Path(normalized).suffix.lower()
    return first in LOCAL_PATH_PREFIXES or suffix in LOCAL_PATH_SUFFIXES


def artifact_path_or_value(value: Any) -> str:
    text = "" if value is None else str(value)
    return redacted_path_label(text) if looks_like_local_path(text) else redact_sensitive_text(text)


def path_redaction_candidates(paths: Sequence[Any]) -> List[Any]:
    candidates: List[Any] = []
    for path in paths:
        if path in ("", None):
            continue
        raw = str(path)
        if raw in {".", ".."} or raw.startswith("-"):
            continue
        if isinstance(path, Path) or looks_like_local_path(raw):
            candidates.append(path)
    return candidates


def path_was_redacted(value: Any, label: str) -> bool:
    text = "" if value is None else str(value)
    return bool(text and text != label)


def artifact_path_fields(value: Any, field_name: str = "path") -> Dict[str, Any]:
    raw = "" if value is None else str(value)
    label = redacted_path_label(raw)
    return {
        field_name: label,
        f"{field_name}_redacted": path_was_redacted(raw, label),
    }


def artifact_path_payload(
    value: Any,
    field_name: str = "path",
    redacted_field_name: str | None = None,
) -> Dict[str, Any]:
    """Return path fields while supporting legacy redaction flag names."""
    raw = value.as_posix() if isinstance(value, Path) else value
    fields = artifact_path_fields(raw, field_name)
    default_redacted_field = f"{field_name}_redacted"
    if redacted_field_name and redacted_field_name != default_redacted_field:
        fields[redacted_field_name] = fields.pop(default_redacted_field)
    return fields


def redact_path_error(value: Any, *paths: Any) -> str:
    """Redact secrets and replace known raw path strings inside error text."""
    text = redact_sensitive_text(value)
    for path in path_redaction_candidates(paths):
        raw = "" if path is None else str(path)
        if not raw:
            continue
        label = redacted_path_label(raw)
        if not label or label == raw:
            continue
        for needle in {raw, redact_sensitive_text(raw)}:
            if needle:
                text = text.replace(needle, label)
    return text


def redact_known_content(value: Any, *contents: Any) -> str:
    text = redact_sensitive_text(value)
    for content in contents:
        raw = "" if content is None else str(content)
        if raw:
            text = text.replace(raw, REDACTED_VALUE)
    return text


def redact_artifact_output(value: Any, *, paths: Sequence[Any] = (), contents: Sequence[Any] = ()) -> str:
    text = redact_known_content(value, *contents)
    return redact_path_error(text, *path_redaction_candidates(paths))


def artifact_output_tail(
    text: Any,
    command: Sequence[Any],
    cwd: Path | None,
    max_lines: int,
) -> str:
    return redact_artifact_output(
        "\n".join(str(text).splitlines()[-max_lines:]),
        paths=[cwd, *command],
    )


def artifact_command_content_values(parts: Sequence[Any], *, content_options: Sequence[str] = ()) -> List[str]:
    values: List[str] = []
    capture_next = False
    for part in parts:
        text = str(part)
        if capture_next:
            values.append(text)
            capture_next = False
            continue
        if text.startswith("-") and "=" in text:
            option, _, value = text.partition("=")
            if is_content_command_option(option, content_options):
                values.append(value)
            continue
        combined = split_combined_content_option(text, content_options)
        if combined is not None:
            _, value = combined
            values.append(value)
            continue
        if text.startswith("-") and is_content_command_option(text, content_options):
            capture_next = True
    return values


def artifact_preview_fields(value: Any, *, field_name: str = "raw_preview", max_chars: int = 160) -> Dict[str, Any]:
    raw = "" if value is None else str(value)
    sampled = raw[:max(0, max_chars)]
    redacted_sample = redact_sensitive_text(sampled)
    if not raw:
        return {
            field_name: "",
            f"{field_name}_redacted": False,
            f"{field_name}_artifact_policy": "empty",
            f"{field_name}_chars": 0,
            f"{field_name}_sampled_chars": 0,
            f"{field_name}_sha256": "",
            f"{field_name}_hash_input": "redacted_sample",
        }
    return {
        field_name: REDACTED_VALUE,
        f"{field_name}_redacted": True,
        f"{field_name}_artifact_policy": "content_hash_only",
        f"{field_name}_chars": len(raw),
        f"{field_name}_sampled_chars": len(sampled),
        f"{field_name}_sha256": hashlib.sha256(redacted_sample.encode("utf-8")).hexdigest(),
        f"{field_name}_hash_input": "redacted_sample",
    }


def artifact_command_parts(
    parts: Sequence[Any],
    *,
    path_options: Sequence[str] = (),
    content_options: Sequence[str] = (),
) -> List[str]:
    redacted: List[str] = []
    redact_next_secret = False
    redact_next_path = False
    redact_next_content = False
    for part in parts:
        text = str(part)
        if redact_next_secret:
            redacted.append(REDACTED_VALUE)
            redact_next_secret = False
            continue
        if redact_next_path:
            redacted.append(redacted_path_label(text))
            redact_next_path = False
            continue
        if redact_next_content:
            redacted.append(REDACTED_VALUE)
            redact_next_content = False
            continue
        header = split_combined_header_option(text)
        if header is not None:
            option, value = header
            redacted.append(f"{option}{redact_sensitive_text(value)}")
            continue
        combined_content = split_combined_content_option(text, content_options)
        if combined_content is not None:
            option, _ = combined_content
            redacted.append(f"{option}{REDACTED_VALUE}")
            continue
        if len(text) > 2 and text[:2] in {"-u", "-U"} and USERPASS_RE.fullmatch(text[2:]):
            redacted.append(f"{text[:2]}{REDACTED_VALUE}")
            continue
        if text.startswith("-") and "=" in text:
            option, _, value = text.partition("=")
            if is_sensitive_command_option(option):
                redacted.append(f"{option}={REDACTED_VALUE}")
                continue
            if normalized_secret_name(option.lstrip("-")) in SENSITIVE_VALUE_OPTIONS and USERPASS_RE.fullmatch(value):
                redacted.append(f"{option}={REDACTED_VALUE}")
                continue
            if is_content_command_option(option, content_options):
                redacted.append(f"{option}={REDACTED_VALUE}")
                continue
            if is_path_command_option(option, path_options):
                redacted.append(f"{option}={redacted_path_label(value)}")
                continue
        if text.startswith("-") and is_sensitive_command_option(text):
            redacted.append(text)
            redact_next_secret = True
            continue
        if text.startswith("-") and is_content_command_option(text, content_options):
            redacted.append(text)
            redact_next_content = True
            continue
        if text.startswith("-") and is_path_command_option(text, path_options):
            redacted.append(text)
            redact_next_path = True
            continue
        redacted.append(artifact_path_or_value(text))
    return redacted


def artifact_command_template_parts(
    parts: Sequence[Any],
    *,
    path_options: Sequence[str] = (),
    content_options: Sequence[str] = (),
    path_placeholder: str = "<target-path>",
    content_placeholder: str = "<content>",
) -> List[str]:
    templated: List[str] = []
    template_next_secret = False
    template_next_path = False
    template_next_content = False
    for part in parts:
        text = str(part)
        if template_next_secret:
            templated.append(REDACTED_VALUE)
            template_next_secret = False
            continue
        if template_next_path:
            templated.append(path_placeholder)
            template_next_path = False
            continue
        if template_next_content:
            templated.append(content_placeholder)
            template_next_content = False
            continue
        header = split_combined_header_option(text)
        if header is not None:
            option, value = header
            templated.append(f"{option}{redact_sensitive_text(value)}")
            continue
        combined_content = split_combined_content_option(text, content_options)
        if combined_content is not None:
            option, _ = combined_content
            templated.append(f"{option}{content_placeholder}")
            continue
        if len(text) > 2 and text[:2] in {"-u", "-U"} and USERPASS_RE.fullmatch(text[2:]):
            templated.append(f"{text[:2]}{REDACTED_VALUE}")
            continue
        if text.startswith("-") and "=" in text:
            option, _, value = text.partition("=")
            if is_sensitive_command_option(option):
                templated.append(f"{option}={REDACTED_VALUE}")
                continue
            if normalized_secret_name(option.lstrip("-")) in SENSITIVE_VALUE_OPTIONS and USERPASS_RE.fullmatch(value):
                templated.append(f"{option}={REDACTED_VALUE}")
                continue
            if is_content_command_option(option, content_options):
                templated.append(f"{option}={content_placeholder}")
                continue
            if is_path_command_option(option, path_options):
                templated.append(f"{option}={path_placeholder}")
                continue
        if text.startswith("-") and is_sensitive_command_option(text):
            templated.append(text)
            template_next_secret = True
            continue
        if text.startswith("-") and is_content_command_option(text, content_options):
            templated.append(text)
            template_next_content = True
            continue
        if text.startswith("-") and is_path_command_option(text, path_options):
            templated.append(text)
            template_next_path = True
            continue
        templated.append(path_placeholder if looks_like_local_path(text) else redact_sensitive_text(text))
    return templated


def redact_command_parts(parts: Sequence[Any]) -> List[str]:
    redacted: List[str] = []
    redact_next = False
    for part in parts:
        text = str(part)
        if redact_next:
            redacted.append(REDACTED_VALUE)
            redact_next = False
            continue
        if len(text) > 2 and text[:2] in {"-u", "-U"} and USERPASS_RE.fullmatch(text[2:]):
            redacted.append(f"{text[:2]}{REDACTED_VALUE}")
            continue
        header = split_combined_header_option(text)
        if header is not None:
            option, value = header
            redacted.append(f"{option}{redact_sensitive_text(value)}")
            continue
        if text.startswith("-") and "=" in text:
            option, _, value = text.partition("=")
            if is_sensitive_command_option(option):
                redacted.append(f"{option}={REDACTED_VALUE}")
                continue
            if normalized_secret_name(option.lstrip("-")) in SENSITIVE_VALUE_OPTIONS and USERPASS_RE.fullmatch(value):
                redacted.append(f"{option}={REDACTED_VALUE}")
                continue
        if text.startswith("-") and is_sensitive_command_option(text):
            redacted.append(text)
            redact_next = True
            continue
        redacted.append(redact_sensitive_text(text))
    return redacted


def redacted_command_line(parts: Sequence[Any]) -> str:
    return command_line(redact_command_parts(parts))


def redact_value(value: Any, key: str = "") -> Any:
    if key and is_sensitive_field_name(key):
        if value in ("", None):
            return value
        return REDACTED_VALUE
    if isinstance(value, dict):
        return {item_key: redact_value(item, str(item_key)) for item_key, item in value.items()}
    if isinstance(value, list):
        return [redact_value(item) for item in value]
    if isinstance(value, tuple):
        return [redact_value(item) for item in value]
    if isinstance(value, str):
        return redact_sensitive_text(value)
    return value
