#!/usr/bin/env python3
"""Render an index for InferLab source-trace notes.

The index is useful before external framework repositories are available. It
tracks which notes exist, which upstream paths or symbols should be verified,
and which runtime evidence still needs to be attached after migration.
"""

from __future__ import annotations

import argparse
import json
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Tuple

from command_redaction import artifact_path_payload as path_payload, redact_path_error, redacted_path_label as path_label
from path_status import safe_exists as safe_path_probe
from repo_paths import default_repo_root
from text_output import write_text_file


TRACE_TARGETS = (
    {
        "name": "Transformers forward",
        "note": "source_notes/transformers_forward_trace.md",
        "framework": "transformers",
        "expected_symbols": [
            "AutoModelForCausalLM.from_pretrained",
            "GenerationMixin.generate",
            "forward",
            "past_key_values",
        ],
        "evidence": [
            "model config and dtype",
            "prefill/decode tensor shapes",
            "KV cache length and memory",
        ],
    },
    {
        "name": "vLLM request lifecycle",
        "note": "source_notes/vllm_request_lifecycle.md",
        "framework": "vllm",
        "expected_symbols": [
            "OpenAI-compatible API server",
            "AsyncLLMEngine",
            "Scheduler",
            "worker execute_model",
        ],
        "evidence": [
            "request arrival time",
            "queue wait",
            "TTFT",
            "TPOT",
            "stream chunk timeline",
        ],
    },
    {
        "name": "vLLM PagedAttention",
        "note": "source_notes/vllm_paged_attention_trace.md",
        "framework": "vllm",
        "expected_symbols": [
            "block manager",
            "KV cache blocks",
            "paged attention kernel/backend",
            "prefix cache",
        ],
        "evidence": [
            "block allocation snapshots",
            "cache utilization",
            "fragmentation or eviction events",
        ],
    },
    {
        "name": "TensorRT-LLM engine runtime",
        "note": "source_notes/trtllm_engine_runtime_trace.md",
        "framework": "TensorRT-LLM",
        "expected_symbols": [
            "engine build",
            "runtime session",
            "in-flight batching",
            "executor",
        ],
        "evidence": [
            "engine build log",
            "engine artifact size",
            "runtime latency and throughput",
            "CUDA Graph or plugin settings",
        ],
    },
    {
        "name": "llama.cpp and ggml",
        "note": "source_notes/llama_cpp_ggml_trace.md",
        "framework": "llama.cpp",
        "expected_symbols": [
            "llama_model_load_from_file",
            "llama_decode",
            "ggml graph build",
            "backend dispatch",
            "sampler",
        ],
        "evidence": [
            "prompt eval time",
            "decode eval time",
            "thread count",
            "mmap and backend selection",
            "perf counters",
        ],
    },
)


def resolve_repo_path(root: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else root / path


def path_probe_status(exists: bool, error: str) -> str:
    if error:
        return "path_probe_failed"
    return "ok" if exists else "missing"


def read_heading(path: Path, exists: bool, probe_error: str) -> Tuple[str, str, str]:
    if probe_error:
        return "", "path_probe_failed", probe_error
    if not exists:
        return "", "missing", ""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return "", "read_failed", redact_path_error(str(exc), path)
    for line in text.splitlines():
        if line.startswith("# "):
            return line[2:].strip(), "ok", ""
    return "", "heading_missing", ""


def build_payload(root: Path) -> Dict[str, Any]:
    targets: List[Dict[str, Any]] = []
    for target in TRACE_TARGETS:
        note_path = root / target["note"]
        exists, probe_error = safe_path_probe(note_path)
        heading, heading_status, heading_error = read_heading(note_path, exists, probe_error)
        targets.append(
            {
                **target,
                "exists": exists,
                "path_probe_status": path_probe_status(exists, probe_error),
                "path_probe_error": probe_error,
                "heading": heading,
                "heading_status": heading_status,
                "heading_error": heading_error,
            }
        )
    return {
        "schema_version": 1,
        "generated_at": utc_now(),
        **path_payload(root, "root", "root_path_redacted"),
        "targets": targets,
    }


def render_markdown(payload: Dict[str, Any]) -> str:
    lines = [
        "# InferLab Source Trace Index",
        "",
        f"- Generated: `{payload['generated_at']}`",
        "",
        "## Coverage",
        "",
        "| Trace | Framework | Note | Exists | Path status | Heading status | Evidence to attach |",
        "|---|---|---|---:|---|---|---|",
    ]
    for target in payload["targets"]:
        evidence = ", ".join(target["evidence"])
        lines.append(
            f"| {target['name']} | {target['framework']} | `{target['note']}` | {target['exists']} | "
            f"{target.get('path_probe_status', 'unknown')} | {target.get('heading_status', 'unknown')} | {evidence} |"
        )
    lines.extend(["", "## Verification Commands", "", "```bash"])
    lines.extend(
        [
            "python3 scripts/render_source_trace_index.py --output reports/source_trace_index.md --json-output results/raw/source_trace_index.json",
            "grep -RIn \"AsyncLLMEngine\\|Scheduler\\|block manager\" /path/to/vllm",
            "grep -RIn \"runtime session\\|in-flight\\|executor\" /path/to/TensorRT-LLM",
            "grep -RIn \"llama_decode\\|llama_model_load_from_file\\|ggml_graph\" /path/to/llama.cpp",
        ]
    )
    lines.append("```")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", "--repo-root", dest="root", default=str(default_repo_root()))
    parser.add_argument("--output", default="reports/source_trace_index.md")
    parser.add_argument("--json-output", default="results/raw/source_trace_index.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    payload = build_payload(root)
    output = resolve_repo_path(root, args.output)
    json_output = resolve_repo_path(root, args.json_output)
    write_text_file(output, render_markdown(payload))
    write_text_file(json_output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")
    print(f"wrote {path_label(json_output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
