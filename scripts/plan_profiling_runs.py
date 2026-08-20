#!/usr/bin/env python3
"""Generate a hardware-backed profiling run plan for InferLab.

The planner does not execute Nsight, vLLM, TensorRT-LLM, llama.cpp, perf,
simpleperf, Docker, or Android commands. It records local tool availability and
writes a JSON plus Markdown command plan that can be moved to a capable machine.
"""

from __future__ import annotations

import argparse
import json
import platform
import shutil
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, Iterable, List

from command_redaction import (
    artifact_command_parts as shared_artifact_command_parts,
    artifact_command_template_parts as shared_artifact_command_template_parts,
    artifact_path_or_value,
    artifact_path_payload as path_payload,
    looks_like_local_path,
    redact_path_error,
    redacted_path_label as path_label,
)
from cli_validation import require_int_range
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


def shell_join(parts: Iterable[str]) -> str:
    quoted: List[str] = []
    for part in parts:
        value = str(part)
        if value == "":
            quoted.append('""')
        elif all(ch.isalnum() or ch in "._+-=/:,@%" for ch in value):
            quoted.append(value)
        else:
            quoted.append("'" + value.replace("'", "'\\''") + "'")
    return " ".join(quoted)


PATH_COMMAND_OPTIONS = {
    "-B",
    "-o",
    "--artifact-root",
    "--build-dir",
    "--cuda-build-dir",
    "--engine-dir",
    "--export",
    "--json-output",
    "--llama-binary",
    "--llama-model",
    "--output",
    "--profiling-root",
    "--report",
    "--results",
}

CONTENT_COMMAND_OPTIONS = {
    "-p",
    "--prompt",
}


def artifact_path_value(value: Any) -> str:
    return artifact_path_or_value(value)


def artifact_command_parts(parts: Iterable[Any]) -> List[str]:
    return shared_artifact_command_parts(
        [str(part) for part in parts],
        path_options=tuple(PATH_COMMAND_OPTIONS),
        content_options=tuple(CONTENT_COMMAND_OPTIONS),
    )


def command_template_parts(parts: Iterable[Any]) -> List[str]:
    return shared_artifact_command_template_parts(
        [str(part) for part in parts],
        path_options=tuple(PATH_COMMAND_OPTIONS),
        content_options=tuple(CONTENT_COMMAND_OPTIONS),
        content_placeholder="<prompt>",
    )


def tool_probe(name: str) -> Dict[str, Any]:
    path = shutil.which(name)
    return {
        "name": name,
        **(path_payload(path) if path else {"path": None, "path_redacted": False}),
        "available": bool(path),
    }


def safe_path_probe(path: Path) -> Dict[str, Any]:
    try:
        exists = path.exists()
        is_dir = path.is_dir() if exists else False
        is_file = path.is_file() if exists else False
    except OSError as exc:
        return {
            "exists": False,
            "is_dir": False,
            "is_file": False,
            "probe_status": "path_probe_failed",
            "probe_error": redact_path_error(str(exc), path),
        }
    return {
        "exists": exists,
        "is_dir": is_dir,
        "is_file": is_file,
        "probe_status": "ok" if exists else "missing",
        "probe_error": "",
    }


def path_probe(repo_root: Path, path: str) -> Dict[str, Any]:
    value = resolve_repo_path(repo_root, path)
    probe = safe_path_probe(value)
    return {
        "path": artifact_path_value(path),
        "path_redacted": looks_like_local_path(path),
        "probe_kind": "filesystem_path",
        **path_payload(value, "resolved_path"),
        **probe,
    }


def executable_probe(repo_root: Path, value: str) -> Dict[str, Any]:
    has_path_separator = "/" in value or "\\" in value
    if has_path_separator or Path(value).is_absolute():
        resolved = resolve_repo_path(repo_root, value)
        probe = safe_path_probe(resolved)
        return {
            "path": artifact_path_value(value),
            "path_redacted": looks_like_local_path(value),
            "probe_kind": "filesystem_executable",
            **path_payload(resolved, "resolved_path"),
            "which_path": None,
            **probe,
            "available": bool(probe["is_file"]),
        }
    which_path = shutil.which(value)
    which_path_fields = (
        path_payload(which_path, "which_path", "which_path_redacted")
        if which_path
        else {"which_path": None, "which_path_redacted": False}
    )
    return {
        "path": value,
        "path_redacted": False,
        "probe_kind": "path_executable",
        "resolved_path": None,
        **which_path_fields,
        "exists": bool(which_path),
        "is_file": bool(which_path),
        "available": bool(which_path),
    }


def command_record(name: str, category: str, command: List[str], purpose: str, output: str = "") -> Dict[str, Any]:
    safe_command = artifact_command_parts(command)
    template_command = command_template_parts(command)
    return {
        "name": name,
        "category": category,
        "purpose": purpose,
        "command": shell_join(safe_command),
        "command_template": shell_join(template_command),
        "command_redacted": safe_command != [str(part) for part in command],
        "template_policy": "local paths and prompt content are represented by target-machine placeholders",
        "path_redaction_policy": "local path-like command arguments are replaced with stable path labels",
        "output": output,
    }


def cuda_commands(args: argparse.Namespace) -> List[Dict[str, Any]]:
    build_dir = args.cuda_build_dir
    commands = [
        command_record(
            "configure_cuda_build",
            "cuda",
            ["cmake", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release", "-DINFERLAB_ENABLE_CUDA=ON"],
            "Configure optional CUDA targets on a CUDA toolkit machine.",
            build_dir,
        ),
        command_record(
            "build_cuda_targets",
            "cuda",
            ["cmake", "--build", build_dir, "-j", str(args.jobs)],
            "Build CUDA runtime wrappers and correctness executables.",
            build_dir,
        ),
    ]
    cuda_targets = [
        "vector_add_cuda_test",
        "matrix_ops_cuda_test",
        "normalization_cuda_test",
        "rope_cuda_test",
        "attention_cuda_test",
        "quantization_cuda_test",
        "paged_kv_cuda_test",
    ]
    for target in cuda_targets:
        exe = f"{build_dir}/{target}"
        commands.append(
            command_record(
                f"run_{target}",
                "cuda",
                [exe],
                f"Run CUDA correctness/timing target `{target}` and capture stdout.",
                f"profiling/nsight_compute/{target}.log",
            )
        )
        commands.append(
            command_record(
                f"ncu_{target}",
                "cuda",
                [
                    "ncu",
                    "--set",
                    args.ncu_set,
                    "--target-processes",
                    "all",
                    "--export",
                    f"profiling/nsight_compute/{target}",
                    exe,
                ],
                f"Collect Nsight Compute report for `{target}`.",
                f"profiling/nsight_compute/{target}.ncu-rep",
            )
        )
    return commands


def serving_commands(args: argparse.Namespace) -> List[Dict[str, Any]]:
    return [
        command_record(
            "vllm_serve",
            "vllm",
            [
                "vllm",
                "serve",
                args.model,
                "--host",
                args.host,
                "--port",
                str(args.port),
                "--dtype",
                args.dtype,
                "--max-model-len",
                str(args.max_model_len),
            ],
            "Start an OpenAI-compatible vLLM server in a separate terminal.",
        ),
        command_record(
            "vllm_endpoint_benchmark",
            "vllm",
            [
                "python3",
                "scripts/run_vllm_benchmark.py",
                "--repo-root",
                ".",
                "--base-url",
                args.base_url,
                "--model",
                args.model_name,
                "--request-kind",
                args.request_kind,
                "--max-prompts",
                str(args.max_prompts),
                "--output",
                "results/raw/vllm_serving_benchmark.json",
            ],
            "Measure endpoint latency and token usage against a live vLLM service.",
            "results/raw/vllm_serving_benchmark.json",
        ),
        command_record(
            "vllm_nsys_profile",
            "vllm",
            [
                "nsys",
                "profile",
                "-o",
                "profiling/vllm/vllm_endpoint_benchmark",
                "python3",
                "scripts/run_vllm_benchmark.py",
                "--repo-root",
                ".",
                "--base-url",
                args.base_url,
                "--model",
                args.model_name,
                "--output",
                "results/raw/vllm_serving_benchmark.json",
            ],
            "Collect Nsight Systems timeline around the endpoint benchmark client.",
            "profiling/vllm/vllm_endpoint_benchmark.nsys-rep",
        ),
        command_record(
            "export_vllm_prometheus_snapshot",
            "vllm",
            ["curl", "-sS", args.metrics_url, "-o", "profiling/vllm/prometheus_metrics.txt"],
            "Capture vLLM Prometheus metrics endpoint for later report annotation.",
            "profiling/vllm/prometheus_metrics.txt",
        ),
    ]


def trtllm_commands(args: argparse.Namespace) -> List[Dict[str, Any]]:
    return [
        command_record(
            "trtllm_preflight_or_bench",
            "trtllm",
            [
                "python3",
                "scripts/run_trtllm_benchmark.py",
                "--repo-root",
                ".",
                "--mode",
                "bench",
                "--engine-dir",
                args.engine_dir,
                "--input-tokens",
                str(args.input_tokens),
                "--output-tokens",
                str(args.output_tokens),
                "--output",
                "results/raw/trtllm_benchmark.json",
            ],
            "Run TensorRT-LLM benchmark harness against a built engine, or emit preflight status.",
            "results/raw/trtllm_benchmark.json",
        ),
        command_record(
            "trtllm_nsys_profile",
            "trtllm",
            [
                "nsys",
                "profile",
                "-o",
                "profiling/trtllm/trtllm_benchmark",
                "python3",
                "scripts/run_trtllm_benchmark.py",
                "--repo-root",
                ".",
                "--mode",
                "bench",
                "--engine-dir",
                args.engine_dir,
                "--output",
                "results/raw/trtllm_benchmark.json",
            ],
            "Collect Nsight Systems timeline for TensorRT-LLM benchmark execution.",
            "profiling/trtllm/trtllm_benchmark.nsys-rep",
        ),
    ]


def edge_commands(args: argparse.Namespace) -> List[Dict[str, Any]]:
    return [
        command_record(
            "llama_cpp_benchmark",
            "llama_cpp",
            [
                "python3",
                "scripts/run_llama_cpp_benchmark.py",
                "--repo-root",
                ".",
                "--llama-cli",
                args.llama_binary,
                "--model",
                args.llama_model,
                "--threads",
                str(args.threads),
                "--output",
                "results/raw/llama_cpp_benchmark.json",
            ],
            "Run or preflight llama.cpp GGUF benchmark and parse prompt/eval timing logs.",
            "results/raw/llama_cpp_benchmark.json",
        ),
        command_record(
            "llama_cpp_perf_stat",
            "llama_cpp",
            [
                "perf",
                "stat",
                "-d",
                args.llama_binary,
                "-m",
                args.llama_model,
                "-p",
                args.prompt,
                "-n",
                str(args.output_tokens),
                "-t",
                str(args.threads),
            ],
            "Collect CPU cache, branch, context-switch, and cycle counters for llama.cpp.",
            "profiling/llama_cpp/perf_stat.txt",
        ),
        command_record(
            "android_deployment_plan",
            "android",
            [
                "python3",
                "scripts/plan_android_deploy.py",
                "--repo-root",
                ".",
                "--model",
                args.android_model,
                "--output",
                "reports/android_deploy_plan.md",
                "--json-output",
                "results/raw/android_deploy_plan.json",
            ],
            "Regenerate Android/ARM command plan on the host that has NDK and adb.",
            "reports/android_deploy_plan.md",
        ),
    ]


def refresh_commands(args: argparse.Namespace) -> List[Dict[str, Any]]:
    return [
        command_record(
            "summarize_benchmarks",
            "reporting",
            [
                "python3",
                "scripts/summarize_benchmarks.py",
                "--repo-root",
                ".",
                "--results",
                "results/raw",
                "--output",
                "results/tables/benchmark_summary.md",
                "--json-output",
                "results/raw/benchmark_summary.json",
            ],
            "Refresh human and machine-readable benchmark summary after runtime runs.",
            "results/tables/benchmark_summary.md",
        ),
        command_record(
            "export_prometheus_metrics",
            "reporting",
            [
                "python3",
                "scripts/export_prometheus_metrics.py",
                "--repo-root",
                ".",
                "--results",
                "results/raw",
                "--output",
                "results/tables/inferlab_metrics.prom",
            ],
            "Refresh node_exporter textfile metrics from raw JSON artifacts.",
            "results/tables/inferlab_metrics.prom",
        ),
        command_record(
            "render_final_report",
            "reporting",
            [
                "python3",
                "scripts/render_final_report.py",
                "--repo-root",
                ".",
                "--output",
                "reports/final_report.md",
                "--json-output",
                "results/raw/final_report_index.json",
            ],
            "Refresh final report after benchmark and profiling evidence is collected.",
            "reports/final_report.md",
        ),
    ]


def build_plan(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    commands = cuda_commands(args) + serving_commands(args) + trtllm_commands(args) + edge_commands(args) + refresh_commands(args)
    tools = [
        "cmake",
        "nvcc",
        "nvidia-smi",
        "ncu",
        "nsys",
        "vllm",
        "trtllm-bench",
        "llama-cli",
        "perf",
        "adb",
        "simpleperf",
        "curl",
        "docker",
    ]
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root", "repo_root_path_redacted"),
        "host": {"platform": platform.platform(), "machine": platform.machine()},
        "preflight": {
            "tools": {name: tool_probe(name) for name in tools},
            "paths": {
                "cuda_build_dir": path_probe(repo_root, args.cuda_build_dir),
                "engine_dir": path_probe(repo_root, args.engine_dir),
                "llama_binary": executable_probe(repo_root, args.llama_binary),
                "llama_model": path_probe(repo_root, args.llama_model),
                "android_model": path_probe(repo_root, args.android_model),
            },
        },
        "commands": commands,
        "notes": [
            "This plan is command generation only; it does not execute runtime benchmarks or profilers.",
            "Run CUDA and Nsight commands only on a machine with CUDA toolkit, NVIDIA driver, and a compatible GPU.",
            "Run vLLM commands with a legal model path and model name accepted by the server.",
            "Run TensorRT-LLM commands only after building an engine for the target GPU and sequence bounds.",
            "Run llama.cpp and Android commands only with legal GGUF model files and the required device/toolchain.",
            "After collecting runtime JSON/logs, run the reporting commands at the end of the plan.",
        ],
    }


def write_json(path: Path, payload: Dict[str, Any]) -> None:
    write_text_file(path, json.dumps(payload, indent=2, sort_keys=True) + "\n")


def markdown(plan: Dict[str, Any]) -> str:
    lines = [
        "# InferLab Runtime Profiling Plan",
        "",
        f"- Generated: `{plan['timestamp_utc']}`",
        f"- Host: `{plan['host']['platform']}` / `{plan['host']['machine']}`",
        "",
        "## Tool Preflight",
        "",
        "| Tool | Path | Available |",
        "|---|---|---:|",
    ]
    for name, probe in sorted(plan["preflight"]["tools"].items()):
        lines.append(f"| `{name}` | `{probe.get('path') or ''}` | `{bool(probe.get('available'))}` |")
    lines.extend(["", "## Path Preflight", "", "| Path | Exists | Kind |", "|---|---:|---|"])
    for name, probe in sorted(plan["preflight"]["paths"].items()):
        kind = "dir" if probe.get("is_dir") else "file" if probe.get("is_file") else ""
        lines.append(f"| `{name}`: `{probe.get('path')}` | `{bool(probe.get('exists'))}` | {kind} |")

    categories = []
    for command in plan["commands"]:
        category = command["category"]
        if category not in categories:
            categories.append(category)

    for category in categories:
        lines.extend(["", f"## {category.replace('_', ' ').title()} Commands", ""])
        for command in [item for item in plan["commands"] if item["category"] == category]:
            lines.extend(
                [
                    f"### {command['name']}",
                    "",
                    command["purpose"],
                    "",
                    "```bash",
                    command["command"],
                    "```",
                ]
            )
            if command.get("output"):
                lines.extend(["", f"Expected artifact: `{command['output']}`"])
            lines.append("")

    lines.extend(["## Notes", ""])
    for note in plan["notes"]:
        lines.append(f"- {note}")
    return "\n".join(lines).rstrip() + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--output", default="reports/profiling_plan.md")
    parser.add_argument("--json-output", default="results/raw/profiling_plan.json")
    parser.add_argument("--cuda-build-dir", default="build-cuda")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--ncu-set", default="full")
    parser.add_argument("--model", default="/models/model")
    parser.add_argument("--model-name", default="model")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--base-url", default="http://127.0.0.1:8000/v1")
    parser.add_argument("--metrics-url", default="http://127.0.0.1:8000/metrics")
    parser.add_argument("--dtype", default="auto")
    parser.add_argument("--max-model-len", type=int, default=4096)
    parser.add_argument("--request-kind", choices=("chat", "completion"), default="chat")
    parser.add_argument("--max-prompts", type=int, default=3)
    parser.add_argument("--engine-dir", default="engines/model")
    parser.add_argument("--input-tokens", type=int, default=128)
    parser.add_argument("--output-tokens", type=int, default=32)
    parser.add_argument("--llama-binary", default="llama-cli")
    parser.add_argument("--llama-model", default="/models/model.gguf")
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--prompt", default="Explain KV cache in one sentence.")
    parser.add_argument("--android-model", default="/models/model.gguf")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--jobs", args.jobs, 1, 1024)
    require_int_range("--port", args.port, 1, 65_535)
    require_int_range("--max-model-len", args.max_model_len, 1, 10_000_000)
    require_int_range("--max-prompts", args.max_prompts, 0, 100_000)
    require_int_range("--input-tokens", args.input_tokens, 1, 10_000_000)
    require_int_range("--output-tokens", args.output_tokens, 1, 10_000_000)
    require_int_range("--threads", args.threads, 1, 4096)


def main() -> None:
    args = parse_args()
    validate_args(args)
    repo_root = resolved_repo_root(args.repo_root)
    plan = build_plan(args, repo_root)
    json_output = resolve_repo_path(repo_root, args.json_output)
    output = resolve_repo_path(repo_root, args.output)
    write_json(json_output, plan)
    write_text_file(output, markdown(plan))
    print(f"wrote {path_label(output)}")
    print(f"wrote {path_label(json_output)}")


if __name__ == "__main__":
    main()
