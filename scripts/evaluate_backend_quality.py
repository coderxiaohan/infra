#!/usr/bin/env python3
"""Evaluate fixed-prompt backend outputs and cross-backend text differences.

This is a lightweight quality-evaluation/reporting tool, not a test suite. It
works with partial artifacts: if a backend did not run or did not include text,
the report records that explicitly instead of failing.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from collections import defaultdict
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from artifact_json import load_json_dict_or_empty as load_json
from command_redaction import (
    artifact_path_payload as path_payload,
    artifact_preview_fields,
    redact_path_error,
    redact_sensitive_text,
    redact_value,
    redacted_path_label as path_label,
)
from path_status import safe_exists as safe_path_exists
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


def load_prompt_specs(path: Path) -> Tuple[Dict[str, Dict[str, Any]], List[Dict[str, Any]]]:
    specs: Dict[str, Dict[str, Any]] = {}
    errors: List[Dict[str, Any]] = []
    exists, exists_error = safe_path_exists(path)
    if exists_error:
        return specs, [
            {
                "status": "prompt_file_probe_failed",
                "error": exists_error,
                **path_payload(path),
            }
        ]
    if not exists:
        return specs, [
            {
                "status": "missing_prompt_file",
                "error": f"prompt file does not exist: {path_label(path)}",
                **path_payload(path),
            }
        ]
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        return specs, [{"status": "unreadable_prompt_file", "error": redact_path_error(exc, path), **path_payload(path)}]
    except UnicodeDecodeError as exc:
        return specs, [{"status": "invalid_prompt_file_encoding", "error": redact_path_error(exc, path), **path_payload(path)}]
    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            row = json.loads(stripped)
        except json.JSONDecodeError as exc:
            errors.append(
                {
                    "status": "invalid_prompt_jsonl",
                    "line": line_number,
                    "error": redact_sensitive_text(str(exc)),
                    **artifact_preview_fields(stripped, max_chars=160),
                }
            )
            continue
        if not isinstance(row, dict):
            errors.append(
                {
                    "status": "invalid_prompt_jsonl_row",
                    "line": line_number,
                    "error": "prompt JSONL row must be an object",
                    "value_type": type(row).__name__,
                }
            )
            continue
        prompt_id = str(row.get("id") or f"prompt_{line_number}")
        specs[prompt_id] = {
            "id": prompt_id,
            "prompt": str(row.get("prompt", "")),
            "category": str(row.get("category", "uncategorized")),
            "expected_contains": row.get("expected_contains", []),
            "expected_regex": row.get("expected_regex", []),
            "reference_answer": row.get("reference_answer", ""),
            "numeric_answer": row.get("numeric_answer"),
            "numeric_tolerance": row.get("numeric_tolerance", 0),
        }
    return specs, errors


BAD_CHECK_FILE_STATUSES = {
    "invalid_artifact",
    "invalid_encoding",
    "invalid_json",
    "path_probe_failed",
    "unreadable_json",
}


def load_checks(path: Path) -> Tuple[Dict[str, Dict[str, Any]], List[Dict[str, Any]]]:
    exists, exists_error = safe_path_exists(path)
    if exists_error:
        return {}, [
            {
                "status": "check_file_probe_failed",
                "error": exists_error,
                **path_payload(path),
            }
        ]
    if not exists:
        return {}, [
            {
                "status": "missing_check_file",
                "error": f"check file does not exist: {path_label(path)}",
                **path_payload(path),
            }
        ]
    payload = load_json(path)
    status = payload.get("status")
    if isinstance(status, str) and status in BAD_CHECK_FILE_STATUSES:
        return {}, [
            {
                "status": status,
                "error": payload.get("error"),
                **path_payload(path),
            }
        ]
    checks = payload.get("prompts", {})
    if isinstance(checks, dict):
        return checks, []
    return {}, [
        {
            "status": "invalid_check_file_schema",
            "error": "`prompts` must be an object",
            **path_payload(path),
        }
    ]


def normalize(text: str) -> str:
    lowered = text.lower()
    lowered = re.sub(r"\s+", " ", lowered)
    return lowered.strip()


def tokens(text: str) -> List[str]:
    return re.findall(r"[a-zA-Z0-9_]+", normalize(text))


def jaccard(a: str, b: str) -> Optional[float]:
    left, right = set(tokens(a)), set(tokens(b))
    if not left and not right:
        return None
    return len(left & right) / max(1, len(left | right))


def first_number(text: str) -> Optional[float]:
    match = re.search(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)", text)
    if not match:
        return None
    try:
        return float(match.group(0))
    except ValueError:
        return None


def parse_finite_float(value: Any) -> Tuple[Optional[float], Optional[str]]:
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        return None, redact_sensitive_text(str(exc))
    if not math.isfinite(parsed):
        return None, "value must be finite"
    return parsed, None


def as_list(value: Any) -> List[str]:
    if value is None:
        return []
    if isinstance(value, list):
        return [str(item) for item in value]
    return [str(value)]


def safe_artifact_text(value: Any) -> str:
    return redact_sensitive_text(value) if isinstance(value, str) else ""


def content_sha256(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest() if value else ""


def text_fields_from_artifact(value: Any, source: str, existing_hash: Any = None) -> Dict[str, Any]:
    text = safe_artifact_text(value)
    if not text:
        return {"text": "", "text_source": "missing", "text_redacted": False, "text_sha256": existing_hash or None}
    return {
        "text": text,
        "text_source": source,
        "text_redacted": True,
        "text_sha256": existing_hash or content_sha256(text),
    }


def vllm_text_fields(item: Dict[str, Any]) -> Dict[str, Any]:
    completion_text = safe_artifact_text(item.get("completion_text"))
    completion_preview = safe_artifact_text(item.get("completion_preview"))
    completion_preview_policy = str(item.get("completion_preview_artifact_policy", ""))
    completion_hash = item.get("completion_sha256")
    if completion_text:
        return text_fields_from_artifact(completion_text, "completion_text", completion_hash)
    if completion_preview and completion_preview_policy == "content_hash_only":
        return {
            "text": "",
            "text_source": "completion_preview_hash_only",
            "text_redacted": True,
            "text_sha256": completion_hash,
        }
    if completion_preview:
        return text_fields_from_artifact(completion_preview, "completion_preview", completion_hash)
    if completion_hash:
        return {
            "text": "",
            "text_source": "completion_sha256",
            "text_redacted": True,
            "text_sha256": completion_hash,
        }
    return {"text": "", "text_source": "missing", "text_redacted": False, "text_sha256": None}


def vllm_request_status(item: Dict[str, Any]) -> str:
    if item.get("ok") is True:
        return "ok"
    status_code = item.get("status_code")
    http_ok = item.get("http_ok")
    if http_ok is False or (
        isinstance(status_code, int) and not (200 <= status_code < 300)
    ):
        return "http_failed"
    error_text = str(item.get("error", "")).lower()
    if "decode" in error_text:
        return "decode_error"
    if item.get("response_valid") is False:
        return "invalid_response"
    return "failed"


def extract_vllm(path: Path, payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    for item in payload.get("requests", []) if isinstance(payload.get("requests"), list) else []:
        if not isinstance(item, dict):
            continue
        text_fields = vllm_text_fields(item)
        records.append(
            {
                "backend": "vllm",
                **path_payload(path, "source", "source_path_redacted"),
                "prompt_id": str(item.get("prompt_id", f"request_{item.get('index', len(records))}")),
                "category": str(item.get("category", "unknown")),
                "ok": bool(item.get("ok")),
                "status": vllm_request_status(item),
                **text_fields,
                "status_code": item.get("status_code"),
                "http_ok": item.get("http_ok"),
                "response_valid": item.get("response_valid"),
                "latency_ms": item.get("elapsed_ms"),
                "prompt_tokens": item.get("prompt_tokens"),
                "output_tokens": item.get("completion_tokens"),
                "error": redact_value(item.get("error")),
            }
        )
    if not records:
        records.append(
            {
                "backend": "vllm",
                **path_payload(path, "source", "source_path_redacted"),
                "prompt_id": "preflight",
                "category": "preflight",
                "ok": payload.get("status") == "ok",
                "status": str(payload.get("status", "missing_text")),
                "text": "",
                "text_source": "missing",
                "text_redacted": False,
                "error": redact_value(payload.get("preflight", {}).get("reasons")) if isinstance(payload.get("preflight"), dict) else None,
            }
        )
    return records


def extract_llama(path: Path, payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    prompt = payload.get("prompt", {}) if isinstance(payload.get("prompt"), dict) else {}
    run = payload.get("run", {}) if isinstance(payload.get("run"), dict) else {}
    raw_output_tail = safe_artifact_text(run.get("output_tail"))
    generated_text = safe_artifact_text(payload.get("generated_text"))
    return [
        {
            "backend": "llama.cpp",
            **path_payload(path, "source", "source_path_redacted"),
            "prompt_id": str(prompt.get("id", "prompt_0")),
            "category": str(prompt.get("category", "edge")),
            "ok": payload.get("status") == "ok",
            "status": str(payload.get("status", "missing")),
            "text": generated_text,
            "text_source": "generated_text" if generated_text else "missing",
            "text_redacted": bool(generated_text),
            "text_sha256": content_sha256(generated_text) if generated_text else None,
            "raw_output_tail": raw_output_tail,
            "raw_output_tail_sha256": content_sha256(raw_output_tail) if raw_output_tail else None,
            "latency_ms": payload.get("summary", {}).get("total_ms") if isinstance(payload.get("summary"), dict) else None,
            "prompt_tokens": payload.get("summary", {}).get("prompt_eval_tokens") if isinstance(payload.get("summary"), dict) else None,
            "output_tokens": payload.get("summary", {}).get("eval_tokens") if isinstance(payload.get("summary"), dict) else None,
            "error": redact_value(payload.get("preflight", {}).get("reasons")) if isinstance(payload.get("preflight"), dict) else None,
        }
    ]


def extract_trtllm(path: Path, payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    run = payload.get("run", {}) if isinstance(payload.get("run"), dict) else {}
    raw_output_tail = ""
    for key in ("stdout_tail", "stderr_tail", "output_tail"):
        if isinstance(run.get(key), str) and run[key]:
            raw_output_tail += ("\n" if raw_output_tail else "") + safe_artifact_text(run[key])
    return [
        {
            "backend": "trtllm",
            **path_payload(path, "source", "source_path_redacted"),
            "prompt_id": "prompt_0",
            "category": "trtllm",
            "ok": payload.get("status") == "ok",
            "status": str(payload.get("status", "missing")),
            "text": "",
            "text_source": "missing_generation_text",
            "raw_output_tail": raw_output_tail,
            "raw_output_tail_sha256": content_sha256(raw_output_tail) if raw_output_tail else None,
            "latency_ms": payload.get("summary", {}).get("latency_ms") if isinstance(payload.get("summary"), dict) else None,
            "prompt_tokens": payload.get("config", {}).get("input_tokens") if isinstance(payload.get("config"), dict) else None,
            "output_tokens": payload.get("config", {}).get("output_tokens") if isinstance(payload.get("config"), dict) else None,
            "error": redact_value(payload.get("preflight", {}).get("reasons")) if isinstance(payload.get("preflight"), dict) else None,
        }
    ]


def extract_records(results_root: Path, paths: List[str]) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    for rel in paths:
        path = results_root / rel
        payload = load_json(path)
        if not payload:
            records.append(
                {
                    "backend": rel.replace(".json", ""),
                    **path_payload(path, "source", "source_path_redacted"),
                    "prompt_id": "missing",
                    "category": "missing",
                    "ok": False,
                    "status": "missing",
                    "text": "",
                }
            )
            continue
        if rel == "vllm_serving_benchmark.json":
            records.extend(extract_vllm(path, payload))
        elif rel == "llama_cpp_benchmark.json":
            records.extend(extract_llama(path, payload))
        elif rel == "trtllm_benchmark.json":
            records.extend(extract_trtllm(path, payload))
        else:
            records.append(
                {
                    "backend": rel.replace(".json", ""),
                    **path_payload(path, "source", "source_path_redacted"),
                    "prompt_id": "unknown",
                    "category": "unknown",
                    "ok": payload.get("status") == "ok",
                    "status": str(payload.get("status", "unknown")),
                    "text": "",
                    "text_source": "unsupported_artifact",
                    "raw_artifact_summary": {
                        "top_level_keys": sorted(str(key) for key in payload.keys())[:32],
                    },
                }
            )
    return records


def merge_spec(prompt_id: str, prompt_specs: Dict[str, Dict[str, Any]], checks: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
    merged = dict(prompt_specs.get(prompt_id, {"id": prompt_id}))
    merged.update(checks.get(prompt_id, {}))
    return merged


def evaluate_record(record: Dict[str, Any], spec: Dict[str, Any]) -> Dict[str, Any]:
    text = str(record.get("text", ""))
    text_redacted = bool(record.get("text_redacted"))
    normalized = normalize(text)
    expected_contains = as_list(spec.get("expected_contains"))
    expected_regex = as_list(spec.get("expected_regex"))
    contains_results = []
    for needle in expected_contains:
        needle_text = str(needle)
        contains_results.append(
            {
                "matched": normalize(needle_text) in normalized,
                "needle_chars": len(needle_text),
                "needle_sha256": content_sha256(redact_sensitive_text(needle_text)) or None,
                "needle_artifact_policy": "content_hash_only",
            }
        )
    regex_results = []
    for pattern in expected_regex:
        pattern_text = str(pattern)
        safe_pattern = {
            "pattern_chars": len(pattern_text),
            "pattern_sha256": content_sha256(redact_sensitive_text(pattern_text)) or None,
            "pattern_artifact_policy": "content_hash_only",
        }
        try:
            matched = re.search(pattern_text, text, flags=re.IGNORECASE | re.MULTILINE) is not None
            regex_results.append({**safe_pattern, "matched": matched})
        except re.error as exc:
            regex_results.append({**safe_pattern, "matched": False, "error": redact_sensitive_text(str(exc))})

    numeric_answer = spec.get("numeric_answer")
    numeric_result: Dict[str, Any] = {"expected": numeric_answer, "observed": None, "matched": None}
    if numeric_answer is not None:
        observed = first_number(text)
        expected, expected_error = parse_finite_float(numeric_answer)
        tolerance_raw = spec.get("numeric_tolerance", 0) or 0
        tolerance, tolerance_error = parse_finite_float(tolerance_raw)
        if tolerance is not None and tolerance < 0:
            tolerance = None
            tolerance_error = "numeric_tolerance must be non-negative"
        if expected_error or tolerance_error:
            numeric_result = {
                "expected": numeric_answer,
                "observed": observed,
                "tolerance": tolerance_raw,
                "matched": False,
                "status": "invalid_numeric_check",
                "error": expected_error or tolerance_error,
            }
        else:
            numeric_result = {
                "expected": numeric_answer,
                "observed": observed,
                "tolerance": tolerance,
                "matched": observed is not None and math.isclose(expected, float(observed), abs_tol=tolerance),
            }

    reference_answer = str(spec.get("reference_answer", "") or "")
    similarity = jaccard(text, reference_answer) if reference_answer else None
    checks = [item["matched"] for item in contains_results] + [item["matched"] for item in regex_results]
    if numeric_result["matched"] is not None:
        checks.append(bool(numeric_result["matched"]))
    quality_status = "pass" if checks and all(checks) else "unchecked" if not checks else "fail"
    if text_redacted:
        quality_status = "redacted"
    return {
        **record,
        "text_chars": len(text),
        "text_words": len(tokens(text)),
        "text_sha256": record.get("text_sha256") or content_sha256(text) or None,
        "contains": contains_results,
        "regex": regex_results,
        "numeric": numeric_result,
        "reference_jaccard": similarity,
        "quality_status": quality_status,
    }


def artifact_safe_record(record: Dict[str, Any]) -> Dict[str, Any]:
    safe = dict(record)
    text = str(safe.pop("text", "") or "")
    raw_output_tail = str(safe.pop("raw_output_tail", "") or "")
    safe["text_present"] = bool(text)
    safe["text_artifact_policy"] = "content_hash_only" if text else "no_text"
    safe["text_sha256"] = safe.get("text_sha256") or content_sha256(text) or None
    safe["raw_output_tail_present"] = bool(raw_output_tail)
    safe["raw_output_tail_artifact_policy"] = "content_hash_only" if raw_output_tail else "no_output_tail"
    safe["raw_output_tail_chars"] = len(raw_output_tail)
    safe["raw_output_tail_sha256"] = safe.get("raw_output_tail_sha256") or content_sha256(raw_output_tail) or None
    return safe


def pairwise(records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    by_prompt: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    for record in records:
        if record.get("text") and not record.get("text_redacted"):
            by_prompt[str(record.get("prompt_id"))].append(record)
    pairs: List[Dict[str, Any]] = []
    for prompt_id, items in by_prompt.items():
        for i, left in enumerate(items):
            for right in items[i + 1 :]:
                left_text, right_text = str(left.get("text", "")), str(right.get("text", ""))
                pairs.append(
                    {
                        "prompt_id": prompt_id,
                        "left_backend": left.get("backend"),
                        "right_backend": right.get("backend"),
                        "left_chars": len(left_text),
                        "right_chars": len(right_text),
                        "length_delta_chars": len(left_text) - len(right_text),
                        "exact_match": normalize(left_text) == normalize(right_text),
                        "token_jaccard": jaccard(left_text, right_text),
                    }
                )
    return pairs


def summarize(records: List[Dict[str, Any]], pairs: List[Dict[str, Any]]) -> Dict[str, Any]:
    by_backend: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    for record in records:
        by_backend[str(record.get("backend"))].append(record)
    backend_summary = {}
    for backend, items in by_backend.items():
        backend_summary[backend] = {
            "records": len(items),
            "ok_records": sum(1 for item in items if item.get("ok")),
            "with_text": sum(1 for item in items if item.get("text")),
            "with_full_text": sum(1 for item in items if item.get("text") and not item.get("text_redacted")),
            "with_redacted_preview": sum(1 for item in items if item.get("text") and item.get("text_redacted")),
            "pass": sum(1 for item in items if item.get("quality_status") == "pass"),
            "fail": sum(1 for item in items if item.get("quality_status") == "fail"),
            "unchecked": sum(1 for item in items if item.get("quality_status") == "unchecked"),
            "redacted": sum(1 for item in items if item.get("text_redacted") or item.get("quality_status") == "redacted"),
        }
    return {
        "record_count": len(records),
        "pairwise_comparisons": len(pairs),
        "backend_summary": backend_summary,
    }




def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--prompt-file", default="configs/eval_prompts.jsonl")
    parser.add_argument("--checks", default="configs/quality_checks.example.json")
    parser.add_argument("--sources", nargs="*", default=["vllm_serving_benchmark.json", "llama_cpp_benchmark.json", "trtllm_benchmark.json"])
    parser.add_argument("--output", default="results/raw/backend_quality_eval.json")
    args = parser.parse_args()

    repo_root = resolved_repo_root(args.repo_root)
    results_root = resolve_repo_path(repo_root, args.results)
    prompt_file = resolve_repo_path(repo_root, args.prompt_file)
    checks_file = resolve_repo_path(repo_root, args.checks)
    prompt_specs, prompt_file_errors = load_prompt_specs(prompt_file)
    checks, check_file_errors = load_checks(checks_file)
    raw_records = extract_records(results_root, args.sources)
    records = [
        evaluate_record(record, merge_spec(str(record.get("prompt_id")), prompt_specs, checks))
        for record in raw_records
    ]
    pairs = pairwise(records)
    summary = summarize(records, pairs)
    artifact_records = [artifact_safe_record(record) for record in records]
    summary["prompt_file_error_count"] = len(prompt_file_errors)
    summary["check_file_error_count"] = len(check_file_errors)
    payload = {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        **path_payload(args.results, "results_root", "results_root_path_redacted"),
        **path_payload(results_root, "resolved_results_root"),
        **path_payload(args.prompt_file, "prompt_file", "prompt_file_path_redacted"),
        **path_payload(prompt_file, "resolved_prompt_file"),
        **path_payload(args.checks, "check_file", "check_file_path_redacted"),
        **path_payload(checks_file, "resolved_check_file"),
        "sources": args.sources,
        "records_artifact_policy": "generated text and output tails are used only for local checks; emitted records keep hash/length/policy metadata",
        "summary": summary,
        "prompt_file_errors": prompt_file_errors,
        "check_file_errors": check_file_errors,
        "records": artifact_records,
        "pairwise": pairs,
    }

    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")


if __name__ == "__main__":
    main()
