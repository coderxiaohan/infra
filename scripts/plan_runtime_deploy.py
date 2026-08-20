#!/usr/bin/env python3
"""Generate Docker/runtime deployment plans for InferLab.

This planner is deliberately non-invasive: it does not build images, start
containers, pull models, or contact registries. It records local tool
availability and writes an env file plus a Markdown runbook for a later machine
that has Docker, NVIDIA Container Toolkit, model access, and monitoring.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List

from command_redaction import artifact_path_payload as path_payload, redact_path_error, redacted_path_label
from cli_validation import require_int_range
from path_status import tool_availability as tool_probe
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


SENSITIVE_ENV_KEY_FRAGMENTS = ("PASSWORD", "TOKEN", "SECRET", "API_KEY", "PIP_INDEX_URL")
REDACTED_VALUE = "<redacted>"
SENSITIVE_ENV_PLACEHOLDER = "<set-on-target-machine>"


def path_inside(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
    except ValueError:
        return False
    except OSError:
        return True
    return True


def path_probe(repo_root: Path, path: str) -> Dict[str, Any]:
    value = resolve_repo_path(repo_root, path)
    path_fields = path_payload(path)
    resolved_fields = path_payload(value, "resolved_path", "resolved_path_redacted")
    try:
        exists = value.exists()
        is_dir = value.is_dir()
    except OSError as exc:
        return {
            **path_fields,
            **resolved_fields,
            "exists": False,
            "is_dir": False,
            "status": "unreadable_path_probe",
            "error": redact_path_error(exc, value),
        }
    return {
        **path_fields,
        **resolved_fields,
        "exists": exists,
        "is_dir": is_dir,
        "status": "ok",
    }


def shell_join(parts: List[str]) -> str:
    quoted: List[str] = []
    for part in parts:
        if all(ch.isalnum() or ch in "._+-=/:,@" for ch in part):
            quoted.append(part)
        else:
            quoted.append("'" + part.replace("'", "'\\''") + "'")
    return " ".join(quoted)


def build_env(args: argparse.Namespace) -> Dict[str, str]:
    return {
        "INFERLAB_DEV_IMAGE": args.dev_image,
        "INFERLAB_VLLM_IMAGE": args.vllm_image,
        "CUDA_BASE_IMAGE": args.cuda_base_image,
        "VLLM_PACKAGE": args.vllm_package,
        "EXTRA_PIP_INDEX_URL": args.extra_pip_index_url,
        "INFERLAB_MODEL_ROOT": args.model_root,
        "INFERLAB_MODEL": args.model,
        "INFERLAB_PORT": str(args.port),
        "INFERLAB_DTYPE": args.dtype,
        "INFERLAB_MAX_MODEL_LEN": str(args.max_model_len),
        "INFERLAB_GPU_COUNT": args.gpu_count,
        "INFERLAB_TEXTFILE_DIR": args.textfile_dir,
        "PROMETHEUS_PORT": str(args.prometheus_port),
        "GRAFANA_PORT": str(args.grafana_port),
        "NODE_EXPORTER_PORT": str(args.node_exporter_port),
        "GRAFANA_ADMIN_USER": args.grafana_admin_user,
        "GRAFANA_ADMIN_PASSWORD": args.grafana_admin_password,
    }


def is_sensitive_env_key(key: str) -> bool:
    upper = key.upper()
    return any(fragment in upper for fragment in SENSITIVE_ENV_KEY_FRAGMENTS)


def redact_env(env: Dict[str, str]) -> Dict[str, str]:
    return {
        key: REDACTED_VALUE if is_sensitive_env_key(key) and value else value
        for key, value in env.items()
    }


def build_commands(args: argparse.Namespace) -> Dict[str, List[str]]:
    env_file = args.env_output
    return {
        "build": [
            shell_join(["docker", "build", "-f", "docker/Dockerfile.dev", "-t", args.dev_image, "."]),
            shell_join(["docker", "build", "-f", "docker/Dockerfile.vllm", "-t", args.vllm_image, "."]),
        ],
        "dev": [
            shell_join(
                [
                    "docker",
                    "run",
                    "--rm",
                    "-it",
                    "-v",
                    "$PWD:/workspace/InferLab",
                    args.dev_image,
                    "bash",
                ]
            )
        ],
        "vllm": [
            shell_join(
                [
                    "docker",
                    "compose",
                    "--env-file",
                    env_file,
                    "-f",
                    "deploy/docker-compose.inferlab.yml",
                    "--profile",
                    "gpu",
                    "up",
                    "vllm-server",
                ]
            )
        ],
        "observability": [
            shell_join(
                [
                    "python3",
                    "scripts/export_prometheus_metrics.py",
                    "--repo-root",
                    ".",
                    "--results",
                    "results/raw",
                    "--output",
                    "results/tables/inferlab_metrics.prom",
                ]
            ),
            shell_join(
                [
                    "docker",
                    "compose",
                    "--env-file",
                    env_file,
                    "-f",
                    "deploy/docker-compose.observability.yml",
                    "up",
                    "-d",
                ]
            ),
        ],
        "benchmark": [
            shell_join(
                [
                    "python3",
                    "scripts/run_vllm_benchmark.py",
                    "--repo-root",
                    ".",
                    "--base-url",
                    f"http://127.0.0.1:{args.port}/v1",
                    "--model",
                    args.model_name,
                    "--output",
                    "results/raw/vllm_serving_benchmark.json",
                ]
            ),
            shell_join(
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
                ]
            ),
        ],
    }


def build_plan(args: argparse.Namespace, repo_root: Path, runtime_env: Dict[str, str]) -> Dict[str, Any]:
    redacted_env = redact_env(runtime_env)
    redacted_keys = sorted(key for key, value in runtime_env.items() if redacted_env.get(key) != value)
    sensitive_keys = sorted(key for key in runtime_env if is_sensitive_env_key(key))
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "profile": args.profile,
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
        },
        "preflight": {
            "docker": tool_probe("docker"),
            "nvidia_smi": tool_probe("nvidia-smi"),
            "model_root": path_probe(repo_root, args.model_root),
            "textfile_dir": path_probe(repo_root, args.textfile_dir),
            "compose_files": {
                "inferlab": path_probe(repo_root, "deploy/docker-compose.inferlab.yml"),
                "observability": path_probe(repo_root, "deploy/docker-compose.observability.yml"),
            },
        },
        "env": redacted_env,
        "env_redaction": {
            "status": "redacted" if redacted_keys else "no_sensitive_values",
            "redacted_keys": redacted_keys,
            "sensitive_keys": sensitive_keys,
            **path_payload(args.env_output, "runtime_env_file", "runtime_env_file_path_redacted"),
            "env_file_sensitive_values": "included" if args.write_sensitive_env else "omitted_as_comments",
            "claim_boundary": "JSON and Markdown plans are review artifacts and must not expose credentials; generated env files omit sensitive values unless --write-sensitive-env is explicitly used on a private target machine.",
        },
        "commands": build_commands(args),
        "notes": [
            "This planner writes commands only; it does not build or run containers.",
            "Use the dev image on CPU-only machines for editing and native builds.",
            "Use the vLLM service profile only on a GPU machine with NVIDIA Container Toolkit.",
            "Generate inferlab_metrics.prom before starting the observability stack if no benchmark pipeline has run yet.",
            "Sensitive environment values are redacted from JSON/report artifacts and omitted from generated env files by default.",
        ],
    }


def env_text(env: Dict[str, str], *, include_sensitive: bool = False) -> str:
    lines = ["# Generated by scripts/plan_runtime_deploy.py"]
    for key in sorted(env):
        if is_sensitive_env_key(key) and not include_sensitive:
            lines.append(f"# {key}={SENSITIVE_ENV_PLACEHOLDER}")
            continue
        value = env[key].replace("\\", "\\\\").replace("\n", "")
        lines.append(f"{key}={value}")
    return "\n".join(lines) + "\n"


def markdown_plan(plan: Dict[str, Any]) -> str:
    lines = [
        "# InferLab Runtime Deployment Plan",
        "",
        f"- Generated: `{plan['timestamp_utc']}`",
        f"- Profile: `{plan['profile']}`",
        f"- Host: `{plan['host']['platform']}` / `{plan['host']['machine']}`",
        "",
        "## Preflight",
        "",
        "| Item | Value | Ready |",
        "|---|---|---:|",
    ]
    preflight = plan["preflight"]
    rows = [
        ("docker", preflight["docker"].get("path"), preflight["docker"].get("available")),
        ("nvidia-smi", preflight["nvidia_smi"].get("path"), preflight["nvidia_smi"].get("available")),
        ("model root", preflight["model_root"].get("path"), preflight["model_root"].get("exists")),
        ("textfile dir", preflight["textfile_dir"].get("path"), preflight["textfile_dir"].get("exists")),
        (
            "inferlab compose",
            preflight["compose_files"]["inferlab"].get("path"),
            preflight["compose_files"]["inferlab"].get("exists"),
        ),
        (
            "observability compose",
            preflight["compose_files"]["observability"].get("path"),
            preflight["compose_files"]["observability"].get("exists"),
        ),
    ]
    for item, value, ready in rows:
        lines.append(f"| {item} | `{value or ''}` | `{bool(ready)}` |")

    for section, commands in plan["commands"].items():
        lines.extend(["", f"## {section.title()} Commands", "", "```bash"])
        lines.extend(commands)
        lines.append("```")

    lines.extend(["", "## Notes", ""])
    for note in plan["notes"]:
        lines.append(f"- {note}")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--profile", choices=("dev", "gpu", "observability", "full"), default="full")
    parser.add_argument("--dev-image", default="inferlab-dev:local")
    parser.add_argument("--vllm-image", default="inferlab-vllm:local")
    parser.add_argument("--cuda-base-image", default="nvidia/cuda:12.4.1-devel-ubuntu22.04")
    parser.add_argument("--vllm-package", default="vllm")
    parser.add_argument("--extra-pip-index-url", default="")
    parser.add_argument("--model-root", default="./models")
    parser.add_argument("--model", default="/models/model")
    parser.add_argument("--model-name", default="model")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--dtype", default="auto")
    parser.add_argument("--max-model-len", type=int, default=4096)
    parser.add_argument("--gpu-count", default="all")
    parser.add_argument("--textfile-dir", default="../results/tables")
    parser.add_argument("--prometheus-port", type=int, default=9090)
    parser.add_argument("--grafana-port", type=int, default=3000)
    parser.add_argument("--node-exporter-port", type=int, default=9100)
    parser.add_argument("--grafana-admin-user", default="admin")
    parser.add_argument("--grafana-admin-password", default=os.environ.get("GRAFANA_ADMIN_PASSWORD", ""))
    parser.add_argument("--write-sensitive-env", action="store_true", help="Write sensitive values to --env-output. Use only on a private target machine.")
    parser.add_argument("--output", default="reports/runtime_deploy_plan.md")
    parser.add_argument("--json-output", default="results/raw/runtime_deploy_plan.json")
    parser.add_argument("--env-output", default="deploy/inferlab.runtime.env")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_int_range("--port", args.port, 1, 65_535)
    require_int_range("--prometheus-port", args.prometheus_port, 1, 65_535)
    require_int_range("--grafana-port", args.grafana_port, 1, 65_535)
    require_int_range("--node-exporter-port", args.node_exporter_port, 1, 65_535)
    require_int_range("--max-model-len", args.max_model_len, 1, 10_000_000)
    if args.write_sensitive_env and not args.grafana_admin_password:
        raise SystemExit("--write-sensitive-env requires --grafana-admin-password or GRAFANA_ADMIN_PASSWORD")


def main() -> int:
    args = parse_args()
    validate_args(args)
    repo_root = resolved_repo_root(args.repo_root)
    env_output = resolve_repo_path(repo_root, args.env_output)
    if args.write_sensitive_env and path_inside(env_output, repo_root):
        raise SystemExit("--write-sensitive-env requires --env-output outside the repository root")
    runtime_env = build_env(args)
    plan = build_plan(args, repo_root, runtime_env)
    output = resolve_repo_path(repo_root, args.output)
    json_output = resolve_repo_path(repo_root, args.json_output)
    write_text_file(output, markdown_plan(plan))
    write_text_file(json_output, json.dumps(plan, indent=2, sort_keys=True) + "\n")
    write_text_file(env_output, env_text(runtime_env, include_sensitive=args.write_sensitive_env))
    print(f"wrote {redacted_path_label(output.as_posix())}")
    print(f"wrote {redacted_path_label(json_output.as_posix())}")
    print(f"wrote {redacted_path_label(env_output.as_posix())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
