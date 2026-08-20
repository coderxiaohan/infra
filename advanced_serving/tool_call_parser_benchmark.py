#!/usr/bin/env python3
"""Benchmark tool-call and reasoning parser robustness from offline samples."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


import json
import math
import re
from typing import Any, Dict, Iterable, List, Tuple

from advanced_serving.common import (
    artifact_safe_value,
    base_parser,
    command_record,
    config_object_list,
    config_string_value,
    path_label,
    read_json,
    safe_div,
    status_from_execution,
    utc_now,
    write_json,
    write_report,
)


DEFAULT_CONFIG = {
    "backends": {
        "vllm": {"base_url": "http://127.0.0.1:8000/v1", "model": "model", "tool_call_parser": "hermes"},
        "sglang": {"base_url": "http://127.0.0.1:30000/v1", "model": "model", "tool_call_parser": "qwen"},
    },
    "tools": [
        {
            "name": "search_docs",
            "required": ["query", "top_k"],
            "properties": {"query": "string", "top_k": "integer"},
        },
        {
            "name": "create_ticket",
            "required": ["title", "priority"],
            "properties": {"title": "string", "priority": "string", "owner": "string"},
        },
    ],
    "cases": [
        {
            "id": "hermes_valid",
            "parser": "hermes",
            "expected_tool": "search_docs",
            "expect_reasoning": False,
            "raw_output": "<tool_call>{\"name\":\"search_docs\",\"arguments\":{\"query\":\"paged kv cache\",\"top_k\":3}}</tool_call>",
        },
        {
            "id": "qwen_reasoning_valid",
            "parser": "qwen",
            "expected_tool": "create_ticket",
            "expect_reasoning": True,
            "raw_output": "<think>Need to file an infra ticket.</think>\n<tool_call>{\"name\":\"create_ticket\",\"arguments\":{\"title\":\"KV cache offload regression\",\"priority\":\"high\",\"owner\":\"serving\"}}</tool_call>",
        },
        {
            "id": "openai_style_valid",
            "parser": "openai",
            "expected_tool": "search_docs",
            "expect_reasoning": False,
            "raw_output": "{\"recipient_name\":\"functions.search_docs\",\"parameters\":{\"query\":\"structured outputs\",\"top_k\":5}}",
        },
        {
            "id": "malformed_arguments",
            "parser": "hermes",
            "expected_tool": "search_docs",
            "expect_reasoning": False,
            "raw_output": "<tool_call>{\"name\":\"search_docs\",\"arguments\":{\"query\":\"missing top k\"}}</tool_call>",
        },
        {
            "id": "broken_json",
            "parser": "raw_auto",
            "expected_tool": "create_ticket",
            "expect_reasoning": False,
            "raw_output": "{\"name\":\"create_ticket\",\"arguments\":{\"title\":\"bad json\", \"priority\":\"low\"",
        },
    ],
}

SUPPORTED_PROPERTY_TYPES = {"array", "boolean", "integer", "number", "object", "string"}


def _balanced_json_fragments(text: str) -> Iterable[str]:
    start = -1
    depth = 0
    in_string = False
    escaped = False
    for index, char in enumerate(text):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            continue
        if char == "{":
            if depth == 0:
                start = index
            depth += 1
        elif char == "}" and depth:
            depth -= 1
            if depth == 0 and start >= 0:
                yield text[start : index + 1]
                start = -1


def _extract_reasoning(text: str) -> Tuple[bool, int]:
    think_spans = re.findall(r"<think>(.*?)</think>", text, flags=re.DOTALL | re.IGNORECASE)
    reasoning_spans = re.findall(r"<reasoning>(.*?)</reasoning>", text, flags=re.DOTALL | re.IGNORECASE)
    tokens = sum(len(span.split()) for span in think_spans + reasoning_spans)
    return bool(think_spans or reasoning_spans), tokens


def _normalize_call(obj: Dict[str, Any]) -> Dict[str, Any]:
    if "recipient_name" in obj and "parameters" in obj:
        name = str(obj.get("recipient_name", "")).split(".")[-1]
        return {"name": name, "arguments": obj.get("parameters", {})}
    if "tool" in obj and "arguments" in obj:
        return {"name": obj.get("tool"), "arguments": obj.get("arguments", {})}
    return {"name": obj.get("name"), "arguments": obj.get("arguments", {})}


def parse_tool_call(text: str) -> Tuple[Dict[str, Any] | None, str]:
    for fragment in _balanced_json_fragments(text):
        try:
            obj = json.loads(fragment)
        except json.JSONDecodeError:
            continue
        if isinstance(obj, dict):
            call = _normalize_call(obj)
            if call.get("name"):
                return call, "json_object"
    return None, "no_parseable_json"


def _bool_value(name: str, value: Any) -> bool:
    if not isinstance(value, bool):
        raise SystemExit(f"{name} must be a boolean, got {value!r}")
    return value


def _text_value(name: str, value: Any, allow_empty: bool = False) -> str:
    if isinstance(value, bool) or not isinstance(value, str):
        raise SystemExit(f"{name} must be a string, got {value!r}")
    if not allow_empty and not value.strip():
        raise SystemExit(f"{name} must be a non-empty string")
    return value


def _string_list(name: str, raw: Any) -> List[str]:
    if not isinstance(raw, list):
        raise SystemExit(f"{name} must be a string list")
    return [config_string_value(f"{name}[{index}]", item) for index, item in enumerate(raw, start=1)]


def _normalize_backends(config: Dict[str, Any]) -> Dict[str, Dict[str, str]]:
    raw = config.get("backends", DEFAULT_CONFIG["backends"])
    if not isinstance(raw, dict) or not raw:
        raise SystemExit("config.backends must be a non-empty object")
    backends: Dict[str, Dict[str, str]] = {}
    for name, backend in raw.items():
        backend_name = config_string_value("config.backends.<name>", name)
        if not isinstance(backend, dict):
            raise SystemExit(f"config.backends.{backend_name} must be an object")
        backends[backend_name] = {
            "base_url": config_string_value(f"config.backends.{backend_name}.base_url", backend.get("base_url", "")),
            "model": config_string_value(f"config.backends.{backend_name}.model", backend.get("model", "model")),
            "tool_call_parser": config_string_value(
                f"config.backends.{backend_name}.tool_call_parser",
                backend.get("tool_call_parser", "auto"),
            ),
        }
    return backends


def _normalize_tools(config: Dict[str, Any]) -> List[Dict[str, Any]]:
    tools = config_object_list(config, "tools")
    seen: set[str] = set()
    normalized: List[Dict[str, Any]] = []
    for index, tool in enumerate(tools, start=1):
        name = config_string_value(f"config.tools[{index}].name", tool.get("name", ""))
        if name in seen:
            raise SystemExit(f"config.tools[{index}].name duplicates tool {name!r}")
        seen.add(name)
        required = _string_list(f"config.tools[{index}].required", tool.get("required", []))
        raw_properties = tool.get("properties", {})
        if not isinstance(raw_properties, dict):
            raise SystemExit(f"config.tools[{index}].properties must be an object")
        properties: Dict[str, str] = {}
        for field, kind in raw_properties.items():
            field_name = config_string_value(f"config.tools[{index}].properties.<name>", field)
            expected = config_string_value(f"config.tools[{index}].properties.{field_name}", kind)
            if expected not in SUPPORTED_PROPERTY_TYPES:
                raise SystemExit(
                    f"config.tools[{index}].properties.{field_name} must be one of "
                    f"{sorted(SUPPORTED_PROPERTY_TYPES)}, got {expected!r}"
                )
            properties[field_name] = expected
        normalized.append({"name": name, "required": required, "properties": properties})
    return normalized


def _normalize_cases(config: Dict[str, Any]) -> List[Dict[str, Any]]:
    cases = config_object_list(config, "cases")
    normalized: List[Dict[str, Any]] = []
    for index, case in enumerate(cases, start=1):
        normalized.append(
            {
                "id": config_string_value(f"config.cases[{index}].id", case.get("id", f"case_{index}")),
                "parser": config_string_value(f"config.cases[{index}].parser", case.get("parser", "raw_auto")),
                "expected_tool": config_string_value(
                    f"config.cases[{index}].expected_tool",
                    case.get("expected_tool", ""),
                ),
                "expect_reasoning": _bool_value(
                    f"config.cases[{index}].expect_reasoning",
                    case.get("expect_reasoning", False),
                ),
                "raw_output": _text_value(
                    f"config.cases[{index}].raw_output",
                    case.get("raw_output", ""),
                    allow_empty=True,
                ),
            }
        )
    return normalized


def validate_config(config: Dict[str, Any]) -> Dict[str, Any]:
    config["backends"] = _normalize_backends(config)
    config["tools"] = _normalize_tools(config)
    config["cases"] = _normalize_cases(config)
    return config


def _argument_matches_type(value: Any, expected: str) -> bool:
    if expected == "array":
        return isinstance(value, list)
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            return False
        try:
            numeric = float(value)
        except (OverflowError, TypeError, ValueError):
            return False
        return math.isfinite(numeric)
    if expected == "object":
        return isinstance(value, dict)
    if expected == "string":
        return isinstance(value, str)
    return False


def validate_arguments(call: Dict[str, Any] | None, tool_specs: Dict[str, Dict[str, Any]]) -> Tuple[bool, List[str]]:
    if not call:
        return False, ["no_tool_call"]
    name = str(call.get("name", ""))
    spec = tool_specs.get(name)
    if not spec:
        return False, ["unknown_tool"]
    args = call.get("arguments", {})
    if not isinstance(args, dict):
        return False, ["arguments_not_object"]
    failure_reasons = [f"missing:{field}" for field in spec.get("required", []) if field not in args]
    for field, expected in spec.get("properties", {}).items():
        if field in args and not _argument_matches_type(args[field], str(expected)):
            failure_reasons.append(f"type:{field}:expected_{expected}")
    return not failure_reasons, failure_reasons


def retry_tokens(failure_reasons: List[str], parsed: bool) -> int:
    if parsed and not failure_reasons:
        return 0
    if not parsed:
        return 64
    return 16 + 8 * len(failure_reasons)


def build_payload(args: Any) -> Dict[str, Any]:
    config = validate_config(read_json(args.config, DEFAULT_CONFIG))
    tool_specs = {tool["name"]: tool for tool in config["tools"]}
    records: List[Dict[str, Any]] = []
    for case in config["cases"]:
        raw_output = case["raw_output"]
        call, parse_mode = parse_tool_call(raw_output)
        has_reasoning, reasoning_tokens = _extract_reasoning(raw_output)
        expected_tool = str(case.get("expected_tool", ""))
        valid_json = call is not None
        tool_name_match = bool(call and call.get("name") == expected_tool)
        schema_valid, failure_reasons = validate_arguments(call, tool_specs)
        reasoning_ok = has_reasoning if case.get("expect_reasoning") else True
        record = {
            "id": case.get("id", "case"),
            "parser": case.get("parser", "raw_auto"),
            "expected_tool": expected_tool,
            "parsed_tool": call.get("name") if call else "",
            "parse_mode": parse_mode,
            "valid_json": valid_json,
            "tool_name_match": tool_name_match,
            "argument_schema_valid": schema_valid,
            "reasoning_wrapper_ok": reasoning_ok,
            "reasoning_tokens": reasoning_tokens,
            "failure_reasons": failure_reasons,
            "retry_tokens": retry_tokens(failure_reasons, valid_json),
            "status": "ok" if valid_json and tool_name_match and schema_valid and reasoning_ok else "needs_repair",
        }
        records.append(record)

    commands = []
    for backend_name, backend in config["backends"].items():
        url = str(backend.get("base_url", "")).rstrip("/") + "/chat/completions"
        commands.append(
            command_record(
                f"{backend_name}_tool_call_probe",
                [
                    "POST",
                    url,
                    "--tool-call-parser",
                    backend.get("tool_call_parser", "auto"),
                    "--model",
                    backend.get("model", "model"),
                    "--tools",
                    "<tools-json>",
                ],
                stage="planned",
            )
        )

    count = len(records)
    failures = sum(1 for record in records if record["status"] != "ok")
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "status": status_from_execution(False, failures, count),
        "mode": "offline_parse",
        "executed": False,
        "requested_execute": bool(args.execute),
        "execute_supported": False,
        "claim_boundary": "This artifact parses offline samples only; --execute does not contact product backends.",
        "config": artifact_safe_value(config),
        "commands": commands,
        "records": records,
        "summary": {
            "case_count": count,
            "tool_count": len(tool_specs),
            "valid_json_rate": round(safe_div(sum(1 for r in records if r["valid_json"]), count), 6),
            "tool_name_match_rate": round(safe_div(sum(1 for r in records if r["tool_name_match"]), count), 6),
            "argument_schema_rate": round(safe_div(sum(1 for r in records if r["argument_schema_valid"]), count), 6),
            "reasoning_wrapper_rate": round(safe_div(sum(1 for r in records if r["reasoning_wrapper_ok"]), count), 6),
            "repair_required_rate": round(safe_div(failures, count), 6),
            "retry_tokens": sum(int(r["retry_tokens"]) for r in records),
        },
        "next_actions": [
            "Add live --execute adapters once a vLLM, SGLang, or TGI endpoint is available.",
            "Feed malformed samples into structured output retry policy and compare repair token cost.",
            "Track streaming partial-tool-call fragments in the SSE benchmark.",
        ],
    }


def main() -> int:
    parser = base_parser(__doc__ or "")
    parser.add_argument("--config", default="configs/tool_call_cases.example.json")
    parser.add_argument("--output", default="results/raw/tool_call_parser_benchmark.json")
    parser.add_argument("--report", default="reports/17_tool_call_reasoning_parser.md")
    args = parser.parse_args()
    payload = build_payload(args)
    write_json(args.output, payload)
    write_report(
        args.report,
        "Tool Calling And Reasoning Parser",
        payload,
        (
            (
                r["id"],
                r["parser"],
                r["expected_tool"],
                r["parsed_tool"],
                r["argument_schema_valid"],
                r["reasoning_wrapper_ok"],
                r["status"],
            )
            for r in payload["records"]
        ),
        ("Case", "Parser", "Expected", "Parsed", "Schema", "Reasoning", "Status"),
        payload["next_actions"],
    )
    print(f"wrote {path_label(args.output)}")
    print(f"wrote {path_label(args.report)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
