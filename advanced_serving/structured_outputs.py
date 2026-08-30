#!/usr/bin/env python3
"""Plan or execute structured output / guided decoding checks."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


import json
from typing import Any, Dict, List

from advanced_serving.common import (
    DEFAULT_BACKENDS,
    api_key_metadata,
    api_key_from_env,
    base_parser,
    command_record,
    config_backend_map,
    config_object_list,
    config_raw_string_value,
    config_string_value,
    post_json,
    post_json_ok,
    path_label,
    read_json,
    read_json_with_status,
    artifact_safe_value,
    require_int_range,
    status_from_execution,
    utc_now,
    write_json,
)


DEFAULT_CONFIG = {
    "backends": DEFAULT_BACKENDS,
    "prompt_file": "configs/eval_prompts.jsonl",
    "schemas": [
        {"name": "order_json", "kind": "json_schema", "path": "configs/schemas/order.json"},
        {"name": "tool_call_regex", "kind": "regex", "pattern": "\\{\"tool\":\"[a-z_]+\",\"arguments\":.*\\}"},
    ],
    "prompts": [{"id": "order", "prompt": "Return a JSON order with id, item, quantity, and status."}],
}

SUPPORTED_SCHEMA_KINDS = {"json_schema", "regex"}


def _normalize_schemas(config: Dict[str, Any]) -> List[Dict[str, Any]]:
    schemas = config_object_list(config, "schemas")
    normalized: List[Dict[str, Any]] = []
    seen: set[str] = set()
    for index, schema in enumerate(schemas, start=1):
        name = config_string_value(f"config.schemas[{index}].name", schema.get("name", ""))
        if name in seen:
            raise SystemExit(f"config.schemas[{index}].name duplicates schema {name!r}")
        seen.add(name)
        kind = config_string_value(f"config.schemas[{index}].kind", schema.get("kind", ""))
        if kind not in SUPPORTED_SCHEMA_KINDS:
            raise SystemExit(f"config.schemas[{index}].kind must be one of {sorted(SUPPORTED_SCHEMA_KINDS)}, got {kind!r}")
        normalized_schema: Dict[str, Any] = {"name": name, "kind": kind}
        if kind == "json_schema":
            normalized_schema["path"] = config_string_value(
                f"config.schemas[{index}].path",
                schema.get("path", ""),
            )
        if kind == "regex":
            normalized_schema["pattern"] = config_string_value(
                f"config.schemas[{index}].pattern",
                schema.get("pattern", ""),
            )
        normalized.append(normalized_schema)
    return normalized


def _normalize_prompts(config: Dict[str, Any]) -> List[Dict[str, str]]:
    prompts = config_object_list(config, "prompts")
    normalized: List[Dict[str, str]] = []
    seen: set[str] = set()
    for index, prompt in enumerate(prompts, start=1):
        prompt_id = config_string_value(f"config.prompts[{index}].id", prompt.get("id", f"prompt_{index}"))
        if prompt_id in seen:
            raise SystemExit(f"config.prompts[{index}].id duplicates prompt {prompt_id!r}")
        seen.add(prompt_id)
        normalized.append(
            {
                "id": prompt_id,
                "prompt": config_raw_string_value(
                    f"config.prompts[{index}].prompt", prompt.get("prompt", "")
                ),
            }
        )
    return normalized


def validate_config(config: Dict[str, Any]) -> Dict[str, Any]:
    config["backends"] = config_backend_map(config, DEFAULT_BACKENDS)
    config["schemas"] = _normalize_schemas(config)
    config["prompts"] = _normalize_prompts(config)
    return config


def schema_payload(schema: Dict[str, Any]) -> tuple[Dict[str, Any], Dict[str, Any]]:
    name = str(schema.get("name", "unknown"))
    if schema.get("kind") == "json_schema" and schema.get("path"):
        schema_doc, read_status = read_json_with_status(schema["path"], {})
        if read_status.get("status") != "ok":
            schema_doc = {}
        return {"type": "json_schema", "json_schema": {"name": name, "schema": schema_doc}}, read_status
    if schema.get("kind") == "regex":
        return {"type": "regex", "regex": schema.get("pattern", "")}, {"status": "ok", "kind": "regex", "fallback_used": False}
    return {"type": schema.get("kind", "unknown"), "name": name}, {"status": "unsupported_schema_kind", "kind": str(schema.get("kind", "unknown")), "fallback_used": True}


def build_payload(args) -> Dict[str, Any]:
    config = validate_config(read_json(args.config, DEFAULT_CONFIG))
    api_key = api_key_from_env(args.api_key_env)
    records: List[Dict[str, Any]] = []
    commands: List[Dict[str, Any]] = []
    failures = 0
    for backend_name, backend in config["backends"].items():
        base_url = str(backend.get("base_url", "")).rstrip("/")
        model = str(backend.get("model", "model"))
        for schema in config["schemas"]:
            for prompt in config["prompts"]:
                name = f"{backend_name}:{schema.get('name')}:{prompt.get('id', 'prompt')}"
                url = f"{base_url}/chat/completions"
                response_format, schema_read_status = schema_payload(schema)
                body = {
                    "model": model,
                    "messages": [{"role": "user", "content": prompt.get("prompt", "")}],
                    "temperature": 0,
                    "max_tokens": args.max_tokens,
                    "response_format": response_format,
                }
                commands.append(command_record(name, ["POST", url, "<payload>"]))
                record: Dict[str, Any] = {"name": name, "backend": backend_name, "schema": schema.get("name"), "prompt_id": prompt.get("id"), "status": "planned", "schema_read_status": schema_read_status, "request": artifact_safe_value(body)}
                if args.execute:
                    result = post_json(url, body, args.timeout_seconds, api_key)
                    record["result"] = result
                    record["status"] = "ok" if post_json_ok(result) else "failed"
                    failures += 0 if record["status"] == "ok" else 1
                records.append(record)
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "status": status_from_execution(args.execute, failures, len(records)),
        "executed": bool(args.execute),
        "config": artifact_safe_value(config),
        "api_key": api_key_metadata(args.api_key_env, api_key),
        "commands": commands,
        "records": records,
        "summary": {"backend_count": len(config["backends"]), "schema_count": len(config["schemas"]), "record_count": len(records), "failed_records": failures},
        "next_actions": ["Run with --execute against OpenAI-compatible backends.", "Compare constrained latency against unconstrained generation artifacts."],
    }


def main() -> int:
    parser = base_parser(__doc__ or "")
    parser.add_argument("--config", default="configs/structured_outputs.example.json")
    parser.add_argument("--api-key-env", default="", help="Environment variable containing an API key for endpoint probes.")
    parser.add_argument("--max-tokens", type=int, default=128)
    parser.add_argument("--output", default="results/raw/structured_outputs.json")
    args = parser.parse_args()
    require_int_range("--max-tokens", args.max_tokens, 1, 1_000_000)
    payload = build_payload(args)
    write_json(args.output, payload)
    print(f"wrote {path_label(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
