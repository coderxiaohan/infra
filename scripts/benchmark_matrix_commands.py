#!/usr/bin/env python3
"""Shared benchmark-matrix command parsing helpers.

The benchmark matrix is the command catalog for InferLab refresh artifacts.
These helpers intentionally support the simple YAML shape used by
configs/benchmark_matrix.yaml without adding a PyYAML dependency.
"""

from __future__ import annotations

import shlex
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Sequence

from command_redaction import artifact_path_fields, redact_path_error, redacted_path_label as path_label


def path_status_payload(path: Path, status: str, entry_count: int = 0, error: Any = "") -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "status": status,
        **artifact_path_fields(path.as_posix()),
        "entry_count": entry_count,
    }
    if error:
        payload["error"] = redact_path_error(error, path)
    return payload


@dataclass(frozen=True)
class MatrixCommand:
    parts: List[str]
    command: str
    artifact: str
    source: str


REPO_ROOT_AWARE_SCRIPTS = {
    "scripts/collect_system_info.py",
    "scripts/compare_backends.py",
    "scripts/compare_gguf_fixture.py",
    "scripts/evaluate_backend_quality.py",
    "scripts/export_mini_transformer_tables.py",
    "scripts/generate_gguf_fixture.py",
    "scripts/parse_profiling_artifacts.py",
    "scripts/plan_android_deploy.py",
    "scripts/plan_profiling_runs.py",
    "scripts/plan_runtime_deploy.py",
    "scripts/plot_results.py",
    "scripts/render_evidence_checklist.py",
    "scripts/render_final_report.py",
    "scripts/render_mini_transformer_report.py",
    "scripts/run_android_edge_benchmark.py",
    "scripts/run_cuda_profile_matrix.py",
    "scripts/run_llama_cpp_benchmark.py",
    "scripts/run_llama_cpp_matrix.py",
    "scripts/run_native_probes.py",
    "scripts/run_trtllm_benchmark.py",
    "scripts/run_vllm_benchmark.py",
    "scripts/validate_outputs.py",
    "scripts/verify_source_traces.py",
}

SCRIPT_OUTPUT_FLAGS = {
    "scripts/export_mini_transformer_tables.py": "--output-dir",
    "scripts/validate_outputs.py": "",
}


def script_supports_repo_root(script: str) -> bool:
    return script in REPO_ROOT_AWARE_SCRIPTS or script.startswith("advanced_serving/")


def script_invocation(script: str) -> List[Any]:
    if script.startswith("advanced_serving/") and script.endswith(".py"):
        module = script[:-3].replace("/", ".")
        return ["python3", "-m", module]
    if script.startswith("scripts/product_profiles/profile_") and script.endswith(".py"):
        module = script[:-3].replace("/", ".")
        return ["python3", "-m", module]
    return ["python3", script]


def strip_yaml_scalar(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
        return value[1:-1]
    return value


def parse_yaml_scalar(value: str) -> Any:
    stripped = strip_yaml_scalar(value)
    if stripped in {"true", "false"}:
        return stripped == "true"
    try:
        return int(stripped)
    except ValueError:
        pass
    try:
        return float(stripped)
    except ValueError:
        pass
    return stripped


def strip_yaml_comment(line: str) -> str:
    in_single = False
    in_double = False
    escaped = False
    for index, char in enumerate(line):
        if escaped:
            escaped = False
            continue
        if in_double and char == "\\":
            escaped = True
            continue
        if char == "'" and not in_double:
            in_single = not in_single
            continue
        if char == '"' and not in_single:
            in_double = not in_double
            continue
        if char == "#" and not in_single and not in_double and (index == 0 or line[index - 1].isspace()):
            return line[:index].rstrip()
    return line.rstrip()


def load_matrix_entries_with_status(path: Path) -> tuple[List[Dict[str, Any]], Dict[str, Any]]:
    try:
        exists = path.exists()
    except OSError as exc:
        return [], path_status_payload(path, "matrix_probe_failed", error=exc)
    if not exists:
        return [], path_status_payload(path, "missing_matrix")
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        return [], path_status_payload(path, "unreadable_matrix", error=exc)
    except UnicodeDecodeError as exc:
        return [], path_status_payload(path, "invalid_matrix_encoding", error=exc)

    entries: List[Dict[str, Any]] = []
    current: Dict[str, Any] | None = None
    in_runs = False
    for raw_line in lines:
        line = strip_yaml_comment(raw_line)
        if not line.strip():
            continue
        if line.strip() == "runs:":
            in_runs = True
            continue
        if not in_runs:
            continue
        stripped = line.strip()
        if stripped.startswith("- "):
            if current:
                entries.append(current)
            current = {}
            item = stripped[2:]
            if ":" in item:
                key, value = item.split(":", 1)
                current[key.strip()] = parse_yaml_scalar(value)
            continue
        if current is not None and ":" in stripped:
            key, value = stripped.split(":", 1)
            current[key.strip()] = parse_yaml_scalar(value)
    if current:
        entries.append(current)
    return entries, path_status_payload(path, "ok" if entries else "empty_matrix", entry_count=len(entries))


def shell_join(parts: Iterable[Any]) -> str:
    return " ".join(shlex.quote(str(part)) for part in parts)


def matrix_output_path(entry: Dict[str, Any]) -> str:
    name = str(entry.get("name", "unnamed"))
    return str(entry.get("output") or f"results/raw/{name}.json")


def _script_command(entry: Dict[str, Any], output: str) -> List[Any]:
    script = str(entry["script"])
    entry_module = str(entry.get("entry_module", ""))
    output_flag = SCRIPT_OUTPUT_FLAGS.get(script, "--output")
    command: List[Any] = ["python3", "-m", entry_module] if entry_module else script_invocation(script)
    if output_flag:
        command.extend([output_flag, output])
    if script_supports_repo_root(script):
        command.extend(["--repo-root", "."])
    for key in (
        "model",
        "base_url",
        "engine_dir",
        "fixture",
        "expected_json",
        "probe_exe",
        "probe_output",
        "binary",
        "threads",
        "report",
        "config",
        "workload",
        "prompt_file",
        "checks",
        "cost_records",
        "artifact_root",
        "profiling_root",
        "matrix",
        "run_dir",
        "json_output",
        "env_output",
    ):
        if key in entry:
            command.extend(["--" + key.replace("_", "-"), entry[key]])
    return command


def derived_command_parts(entry: Dict[str, Any]) -> List[Any]:
    module = str(entry.get("module", ""))
    script = str(entry.get("script", ""))
    backend = str(entry.get("backend", ""))
    binary = str(entry.get("binary", ""))
    output = matrix_output_path(entry)

    if module == "mini_llm.shape_probe":
        return [
            "python3",
            "-m",
            "mini_llm.shape_probe",
            "--batch-size",
            entry.get("batch_size", 1),
            "--seq-len",
            entry.get("seq_len", 128),
            "--decode-steps",
            entry.get("decode_steps", 16),
            "--num-layers",
            entry.get("num_layers", 12),
            "--num-heads",
            entry.get("num_heads", 12),
            "--head-dim",
            entry.get("head_dim", 64),
            "--output",
            output,
        ]

    if module == "mini_llm.workload_profile":
        return [
            "python3",
            "-m",
            "mini_llm.workload_profile",
            "--prompt-tokens",
            entry.get("prompt_tokens", entry.get("seq_len", 128)),
            "--max-new-tokens",
            entry.get("output_tokens", entry.get("decode_steps", 16)),
            "--output",
            output,
        ]

    if script:
        return _script_command(entry, output)

    if binary:
        binary_command = binary if binary.startswith("./") or binary.startswith("/") else f"./{binary}"
        command: List[Any] = [binary_command]
        if backend.startswith("native"):
            command.extend(["--repo-root", "."])
        command.extend(["--output", output])
        return command

    if backend == "native_probes":
        return [
            "python3",
            "scripts/run_native_probes.py",
            "--repo-root",
            ".",
            "--build-dir",
            entry.get("build_dir", "build"),
            "--results",
            entry.get("results", "results/raw"),
            "--output",
            output,
        ]

    if backend == "final_report":
        return [
            "python3",
            "scripts/render_final_report.py",
            "--repo-root",
            ".",
            "--output",
            entry.get("report_output", "reports/final_report.md"),
            "--json-output",
            output,
        ]

    return []


def command_for_entry(entry: Dict[str, Any], use_replay_command: bool = False) -> MatrixCommand:
    artifact = matrix_output_path(entry)
    if use_replay_command and entry.get("replay_command"):
        command = str(entry["replay_command"])
        try:
            return MatrixCommand(shlex.split(command), command, artifact, "replay_command")
        except ValueError as exc:
            return MatrixCommand(
                [],
                f"# Invalid replay_command for matrix entry {entry.get('name', 'unnamed')}: {exc}",
                artifact,
                "invalid_replay_command",
            )

    if entry.get("command"):
        command = str(entry["command"])
        try:
            return MatrixCommand(shlex.split(command), command, artifact, "command")
        except ValueError as exc:
            return MatrixCommand(
                [],
                f"# Invalid command for matrix entry {entry.get('name', 'unnamed')}: {exc}",
                artifact,
                "invalid_command",
            )

    parts = [str(part) for part in derived_command_parts(entry)]
    if parts:
        return MatrixCommand(parts, shell_join(parts), artifact, "derived")

    name = str(entry.get("name", "unnamed"))
    comment = f"# No reproducible command could be derived for matrix entry: {name}"
    return MatrixCommand([], comment, artifact, "missing")


def reproducible_commands_for_entries(entries: Sequence[Dict[str, Any]]) -> List[str]:
    commands: List[str] = []
    seen = set()
    for entry in entries:
        entry_commands: List[str] = []
        for key in ("command", "replay_command", "optional_snapshot_baseline_command"):
            if entry.get(key):
                entry_commands.append(str(entry[key]))
        if not entry_commands:
            entry_commands.append(command_for_entry(entry).command)
        for command in entry_commands:
            if not command or command in seen:
                continue
            commands.append(command)
            seen.add(command)
    return commands


def reproducible_commands_from_matrix_with_status(path: Path) -> tuple[List[str], Dict[str, Any]]:
    entries, status = load_matrix_entries_with_status(path)
    return reproducible_commands_for_entries(entries), status
