#!/usr/bin/env python3
"""Shared product-profile runner for mature inference products.

The product profile layer is intentionally glue code: it records environment
preflight, version/config snapshots, dry-run/execute/replay commands, endpoint
health, offline evidence, and raw metrics contracts. It does not implement
vLLM, SGLang, TensorRT-LLM, FlashInfer, LMCache, Mooncake, LMDeploy, Dynamo,
KServe, or any other product internals.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.metadata
import importlib.util
import json
import os
import platform
import re
import shlex
import shutil
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

try:
    from scripts.cli_validation import require_float_range, require_int_range
    from scripts.text_output import try_write_text_file as write_text_file
    from scripts.time_utils import utc_now
except ModuleNotFoundError:
    from cli_validation import require_float_range, require_int_range
    from text_output import try_write_text_file as write_text_file
    from time_utils import utc_now

try:
    from scripts.bounded_http import BoundedHttpBody, read_bounded_http_body
    from scripts.bounded_process import run_bounded_process
    from scripts.command_redaction import (
        REDACTED_VALUE,
        artifact_command_parts as shared_artifact_command_parts,
        artifact_command_template_parts,
        artifact_path_or_value,
        artifact_preview_fields,
        command_line,
        is_sensitive_field_name,
        looks_like_local_path,
        normalized_command_option,
        redact_artifact_output,
        redact_path_error,
        redact_sensitive_text,
        redacted_path_label as metric_path_label,
    )
    from scripts.repo_paths import resolve_repo_path
except ModuleNotFoundError:
    scripts_dir = Path(__file__).resolve().parents[1]
    if str(scripts_dir) not in sys.path:
        sys.path.insert(0, str(scripts_dir))
    from bounded_http import BoundedHttpBody, read_bounded_http_body  # type: ignore
    from bounded_process import run_bounded_process  # type: ignore
    from command_redaction import (  # type: ignore
        REDACTED_VALUE,
        artifact_command_parts as shared_artifact_command_parts,
        artifact_command_template_parts,
        artifact_path_or_value,
        artifact_preview_fields,
        command_line,
        is_sensitive_field_name,
        looks_like_local_path,
        normalized_command_option,
        redact_artifact_output,
        redact_path_error,
        redact_sensitive_text,
        redacted_path_label as metric_path_label,
    )
    from repo_paths import resolve_repo_path  # type: ignore

try:
    from scripts.product_profiles import product_profile_command_prefix, product_profile_module
except ModuleNotFoundError:
    from product_profiles import product_profile_command_prefix, product_profile_module  # type: ignore


REPLAY_EXTENSIONS = {
    ".json",
    ".jsonl",
    ".txt",
    ".log",
    ".prom",
    ".csv",
    ".out",
    ".stderr",
    ".stdout",
    ".nsys-rep",
    ".ncu-rep",
    ".sqlite",
}

ENV_VAR_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
PROMETHEUS_RE = re.compile(
    r"^(?P<name>[a-zA-Z_:][a-zA-Z0-9_:]*)(?:\{(?P<labels>[^}]*)\})?\s+"
    r"(?P<value>[-+0-9.eE]+)"
)
TEXT_METRIC_RE = re.compile(
    r"(?P<name>[A-Za-z][A-Za-z0-9_.:/ -]{1,80}?)\s*(?:=|:)\s*"
    r"(?P<value>[-+]?\d+(?:\.\d+)?)\s*"
    r"(?P<unit>ms|msec|s|sec|seconds|tokens/s|tok/s|req/s|requests/s|bytes|gb|gib|mb|mib|%)?",
    re.IGNORECASE,
)
PRODUCT_COMMAND_STREAM_LIMIT_BYTES = 16 * 1024 * 1024
PRODUCT_HTTP_BODY_LIMIT_BYTES = 64 * 1024


PRODUCTS: Dict[str, Dict[str, Any]] = {
    "vllm": {
        "name": "vLLM",
        "category": "serving_engine",
        "scope": "default_required",
        "artifact": "results/raw/product_vllm_capability_profile.json",
        "report": "reports/49_product_vllm_profile.md",
        "config": "configs/product_profiles/vllm.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://docs.vllm.ai/en/latest/"],
        "key_features": ["DBO", "data parallel", "expert parallel", "KV connectors", "LoRA", "structured outputs", "production stack"],
        "required_modules": ["vllm"],
        "optional_modules": ["torch"],
        "required_tools": ["python3"],
        "optional_tools": ["nvidia-smi"],
        "expected_metrics": ["ttft_ms", "tpot_ms", "throughput_tokens_per_s", "p99_latency_ms", "kv_prefix_hit_rate", "error_rate"],
        "not_implemented": ["vLLM scheduler", "PagedAttention backend", "DBO implementation", "KV connector implementation", "expert parallel runtime"],
        "local_optimization_scope": ["profile normalization", "capacity/SLO explanation", "artifact contract validation", "source-level bottleneck notes"],
    },
    "sglang": {
        "name": "SGLang",
        "category": "serving_engine",
        "scope": "default_required",
        "artifact": "results/raw/product_sglang_capability_profile.json",
        "report": "reports/50_product_sglang_profile.md",
        "config": "configs/product_profiles/sglang.example.yaml",
        "default_endpoint": "http://127.0.0.1:30000",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://docs.sglang.io/docs/advanced_features/server_arguments"],
        "key_features": ["TBO", "HiCache", "PD/EPD disaggregation", "expert parallelism", "structured outputs", "tool/reasoning parser", "model gateway"],
        "required_modules": ["sglang"],
        "optional_modules": ["torch"],
        "required_tools": ["python3"],
        "optional_tools": ["nvidia-smi"],
        "expected_metrics": ["ttft_ms", "tpot_ms", "throughput_tokens_per_s", "hicache_hit_rate", "parser_success_rate", "p99_latency_ms"],
        "not_implemented": ["SGLang runtime", "TBO scheduler", "RadixAttention", "HiCache storage", "model gateway"],
        "local_optimization_scope": ["server-args snapshot", "cache/gateway metric normalization", "source-level bottleneck notes"],
    },
    "kv_offload": {
        "name": "LMCache / Ray KV Offload / Mooncake",
        "category": "kv_cache_system",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_lmcache_kv_offload_profile.json",
        "report": "reports/51_product_kv_offload_profile.md",
        "config": "configs/product_profiles/kv_offload.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://docs.lmcache.ai/", "https://docs.ray.io/en/latest/serve/llm/user-guides/kv-cache-offloading.html", "https://github.com/kvcache-ai/Mooncake"],
        "key_features": ["KV cache offloading", "CPU/disk KV reuse", "connector integration", "distributed KV transfer", "tier hit/miss metrics"],
        "required_modules": [],
        "optional_modules": ["lmcache", "ray"],
        "required_tools": ["python3"],
        "optional_tools": ["nvidia-smi"],
        "expected_metrics": ["cache_hit_rate", "prefix_reuse_rate", "offload_bytes", "restore_latency_ms", "hbm_saved_gb", "ttft_delta_ms"],
        "not_implemented": ["LMCache connector", "Ray Serve KV offload engine", "Mooncake storage engine", "KV correctness policy"],
        "local_optimization_scope": ["cache artifact schema", "tier/cost explanation", "correctness-risk labeling"],
    },
    "mooncake": {
        "name": "Mooncake",
        "category": "kv_cache_system",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_mooncake_kv_profile.json",
        "report": "reports/51_product_kv_offload_profile.md",
        "config": "configs/product_profiles/mooncake.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/health", "/metrics"],
        "official_sources": ["https://github.com/kvcache-ai/Mooncake"],
        "key_features": ["distributed KV cache", "remote KV reuse", "KV transfer path", "cache locality", "multi-tier cache integration"],
        "required_modules": [],
        "optional_modules": [],
        "required_tools": ["python3"],
        "optional_tools": ["kubectl", "nvidia-smi"],
        "expected_metrics": ["remote_cache_hit_rate", "kv_transfer_bytes", "transfer_latency_ms", "restore_latency_ms", "hbm_saved_gb", "ttft_delta_ms"],
        "not_implemented": ["Mooncake storage engine", "Mooncake KV transfer runtime", "distributed cache consistency protocol", "KV correctness policy"],
        "local_optimization_scope": ["Mooncake config snapshot", "KV transfer metric normalization", "cache locality and correctness-risk labeling"],
    },
    "tensorrtllm": {
        "name": "TensorRT-LLM / Triton",
        "category": "runtime_backend",
        "scope": "default_required",
        "artifact": "results/raw/product_tensorrtllm_runtime_profile.json",
        "report": "reports/52_product_runtime_backend_profile.md",
        "config": "configs/product_profiles/tensorrtllm.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/v2/health/ready", "/v2/models", "/metrics"],
        "official_sources": ["https://nvidia.github.io/TensorRT-LLM/"],
        "key_features": ["compiled engine", "FP8/FP4/NVFP4", "disaggregated serving", "Triton backend", "in-flight batching"],
        "required_modules": [],
        "optional_modules": ["tensorrt_llm", "tensorrt"],
        "required_tools": ["python3"],
        "optional_tools": ["trtllm-bench", "trtllm-build", "trtllm-serve", "tritonserver", "nvidia-smi"],
        "expected_metrics": ["build_time_s", "engine_size_bytes", "ttft_ms", "tpot_ms", "tokens_per_s", "gpu_memory_bytes", "triton_queue_ms"],
        "not_implemented": ["TensorRT-LLM builder", "runtime plugins", "Triton backend", "low precision kernels"],
        "local_optimization_scope": ["engine artifact parsing", "runtime config normalization", "capacity/SLO explanation"],
    },
    "lmdeploy": {
        "name": "LMDeploy",
        "category": "runtime_backend",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_lmdeploy_runtime_profile.json",
        "report": "reports/52_product_runtime_backend_profile.md",
        "config": "configs/product_profiles/lmdeploy.example.yaml",
        "default_endpoint": "http://127.0.0.1:23333",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://lmdeploy.readthedocs.io/"],
        "key_features": ["TurboMind runtime", "persistent batching", "blocked KV cache", "KV cache quantization", "AWQ", "multi-model serving"],
        "required_modules": [],
        "optional_modules": ["lmdeploy"],
        "required_tools": ["python3"],
        "optional_tools": ["lmdeploy", "nvidia-smi"],
        "expected_metrics": ["ttft_ms", "tpot_ms", "tokens_per_s", "p99_latency_ms", "gpu_memory_bytes", "kv_cache_quant_enabled", "persistent_batching_enabled"],
        "not_implemented": ["LMDeploy TurboMind runtime", "persistent batching scheduler", "blocked KV cache runtime", "KV cache quantization kernels", "multi-model serving runtime"],
        "local_optimization_scope": ["runtime config snapshot", "blocked-KV metric normalization", "capacity/SLO explanation", "multi-model profile artifact schema"],
    },
    "flashinfer": {
        "name": "FlashInfer",
        "category": "kernel_library",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_flashinfer_kernel_profile.json",
        "report": "reports/53_product_kernel_library_profile.md",
        "config": "configs/product_profiles/flashinfer.example.yaml",
        "default_endpoint": "",
        "health_paths": [],
        "official_sources": ["https://docs.flashinfer.ai/api/cascade.html"],
        "key_features": ["attention kernels", "GEMM kernels", "MoE kernels", "cascade inference", "paged KV APIs"],
        "required_modules": [],
        "optional_modules": ["flashinfer", "torch"],
        "required_tools": ["python3"],
        "optional_tools": ["nvidia-smi"],
        "expected_metrics": ["kernel_available", "supported_shape_ratio", "dtype_coverage", "fallback_count", "latency_ms"],
        "not_implemented": ["FlashInfer attention kernels", "FlashInfer GEMM kernels", "FlashInfer MoE kernels"],
        "local_optimization_scope": ["capability matrix", "shape coverage report", "fallback explanation"],
    },
    "ktransformers": {
        "name": "KTransformers",
        "category": "hybrid_runtime",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_ktransformers_hybrid_profile.json",
        "report": "reports/53_product_kernel_library_profile.md",
        "config": "configs/product_profiles/ktransformers.example.yaml",
        "default_endpoint": "http://127.0.0.1:10002",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://github.com/kvcache-ai/ktransformers"],
        "key_features": ["CPU/GPU heterogeneous inference", "MoE expert placement", "hybrid memory", "large model low-resource deployment"],
        "required_modules": [],
        "optional_modules": ["ktransformers"],
        "required_tools": ["python3"],
        "optional_tools": ["nvidia-smi"],
        "expected_metrics": ["load_time_ms", "cpu_memory_bytes", "gpu_memory_bytes", "tokens_per_s", "expert_placement_ratio", "fallback_count"],
        "not_implemented": ["KTransformers runtime", "heterogeneous MoE placement runtime", "custom kernels"],
        "local_optimization_scope": ["placement artifact schema", "memory/cost explanation", "fallback report"],
    },
    "dynamo": {
        "name": "NVIDIA Dynamo",
        "category": "cloud_control_plane",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_dynamo_serving_profile.json",
        "report": "reports/54_product_cloud_control_plane_profile.md",
        "config": "configs/product_profiles/dynamo.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://docs.dynamo.nvidia.com/dynamo/getting-started/introduction"],
        "key_features": ["disaggregated inference runtime", "router", "worker pool", "KV transfer", "control plane"],
        "required_modules": [],
        "optional_modules": [],
        "required_tools": ["python3"],
        "optional_tools": ["kubectl", "nvidia-smi"],
        "expected_metrics": ["router_latency_ms", "prefill_queue_depth", "decode_queue_depth", "kv_transfer_bytes", "transfer_latency_ms", "slo_violation_rate"],
        "not_implemented": ["Dynamo router", "Dynamo control plane", "worker runtime", "KV transfer library"],
        "local_optimization_scope": ["topology/config snapshot", "router metric normalization", "KV transfer artifact schema"],
    },
    "kserve_llmd": {
        "name": "KServe / llm-d",
        "category": "cloud_control_plane",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_kserve_llmd_profile.json",
        "report": "reports/54_product_cloud_control_plane_profile.md",
        "config": "configs/product_profiles/kserve_llmd.example.yaml",
        "default_endpoint": "",
        "health_paths": [],
        "official_sources": ["https://kserve.github.io/website/docs/model-serving/generative-inference/llmisvc/llmisvc-overview"],
        "key_features": ["LLMInferenceService", "KV-cache-aware scheduling", "prefill-decode separation", "distributed inference", "gateway routing"],
        "required_modules": [],
        "optional_modules": [],
        "required_tools": ["python3"],
        "optional_tools": ["kubectl"],
        "expected_metrics": ["manifest_valid", "ready_replicas", "gateway_latency_ms", "kv_locality_policy", "autoscaling_status", "slo_status"],
        "not_implemented": ["Kubernetes controller", "llm-d scheduler", "gateway", "CRD reconciliation"],
        "local_optimization_scope": ["manifest rendering", "dry-run field validation", "deployment status artifact schema"],
    },
    "ray_serve": {
        "name": "Ray Serve LLM",
        "category": "cloud_control_plane",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_ray_serve_profile.json",
        "report": "reports/54_product_cloud_control_plane_profile.md",
        "config": "configs/product_profiles/ray_serve.example.yaml",
        "default_endpoint": "http://127.0.0.1:8000",
        "health_paths": ["/health", "/v1/models", "/metrics"],
        "official_sources": ["https://docs.ray.io/en/latest/serve/llm/index.html", "https://docs.ray.io/en/latest/serve/llm/user-guides/kv-cache-offloading.html"],
        "key_features": ["Serve DAG", "autoscaling", "OpenAI ingress", "KV offload", "deployment graph"],
        "required_modules": [],
        "optional_modules": ["ray"],
        "required_tools": ["python3"],
        "optional_tools": ["ray"],
        "expected_metrics": ["deployment_ready", "replica_count", "autoscaling_status", "qps", "p99_latency_ms", "kv_offload_status"],
        "not_implemented": ["Ray Serve controller", "Serve DAG runtime", "autoscaler", "KV offload engine"],
        "local_optimization_scope": ["deployment graph snapshot", "autoscaling policy report", "profile normalization"],
    },
    "tgi": {
        "name": "Hugging Face TGI",
        "category": "serving_engine",
        "scope": "optional_on_demand",
        "artifact": "results/raw/product_tgi_profile.json",
        "report": "reports/52_product_runtime_backend_profile.md",
        "config": "configs/product_profiles/tgi.example.yaml",
        "default_endpoint": "http://127.0.0.1:8081",
        "health_paths": ["/health", "/info", "/metrics"],
        "official_sources": ["https://huggingface.co/docs/text-generation-inference/en/index"],
        "key_features": ["production server", "SSE streaming", "Prometheus/OpenTelemetry", "continuous batching", "quantization"],
        "required_modules": [],
        "optional_modules": [],
        "required_tools": ["python3"],
        "optional_tools": ["text-generation-launcher", "nvidia-smi"],
        "expected_metrics": ["first_chunk_ms", "stream_gap_ms", "request_throughput_per_s", "tokens_per_s", "prometheus_metric_count", "error_rate"],
        "not_implemented": ["TGI server", "continuous batching runtime", "attention backend", "quantization runtime"],
        "local_optimization_scope": ["HTTP/SSE artifact schema", "Prometheus metric normalization", "streaming latency report"],
    },
}


def product_profile_keys_for_scope(scope: str) -> Tuple[str, ...]:
    if scope not in {"default_required", "optional_on_demand"}:
        raise ValueError(f"unsupported product profile scope: {scope}")
    keys = tuple(
        key for key, definition in PRODUCTS.items()
        if definition.get("scope") == scope
    )
    if not keys:
        raise RuntimeError(f"product profile scope is empty: {scope}")
    return keys


def default_product_profile_keys() -> Tuple[str, ...]:
    return product_profile_keys_for_scope("default_required")


def default_repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def prepare_path_args(args: argparse.Namespace) -> None:
    repo_root = Path(args.repo_root).expanduser()
    if not repo_root.is_absolute():
        repo_root = (Path.cwd() / repo_root).resolve()
    else:
        repo_root = repo_root.resolve()

    args.repo_root_path = repo_root
    args.config_path = resolve_repo_path(repo_root, args.config)
    args.output_path = resolve_repo_path(repo_root, args.output)
    args.report_path = resolve_repo_path(repo_root, args.report)
    args.evidence_dir_paths = [resolve_repo_path(repo_root, item) for item in args.evidence_dir or []]
    args.evidence_file_paths = [resolve_repo_path(repo_root, item) for item in args.evidence_file or []]
    args.metrics_file_paths = [resolve_repo_path(repo_root, item) for item in args.metrics_file or []]
    args.log_dir_paths = [resolve_repo_path(repo_root, item) for item in args.log_dir or []]


def deep_merge(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    result = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value
    return result


STRING_LIST_CONFIG_FIELDS = (
    "health_paths",
    "official_sources",
    "key_features",
    "required_modules",
    "optional_modules",
    "required_tools",
    "optional_tools",
    "expected_metrics",
    "not_implemented",
    "local_optimization_scope",
    "warnings",
    "report_sections",
    "risks",
)

REPLAY_INPUT_LIST_FIELDS = ("evidence_dirs", "evidence_files", "metrics_files", "log_dirs")


def config_schema_error(field: str, message: str, value: Any) -> Dict[str, Any]:
    return {
        "status": "invalid_config_field",
        "field": field,
        "error": message,
        "value_type": type(value).__name__,
    }


def normalize_config_string_list(
    config: Dict[str, Any],
    field: str,
    errors: List[Dict[str, Any]],
    display_field: Optional[str] = None,
) -> List[str]:
    label = display_field or field
    raw = config.get(field, [])
    if raw is None:
        config[field] = []
        return []
    if not isinstance(raw, list):
        errors.append(config_schema_error(label, f"{label} must be a string list", raw))
        config[field] = []
        return []
    values: List[str] = []
    for index, item in enumerate(raw, start=1):
        if isinstance(item, bool) or not isinstance(item, str):
            errors.append(config_schema_error(f"{label}[{index}]", f"{label}[{index}] must be a string", item))
            continue
        value = item.strip()
        if not value:
            errors.append(config_schema_error(f"{label}[{index}]", f"{label}[{index}] must be a non-empty string", item))
            continue
        values.append(value)
    config[field] = values
    return values


def normalize_replay_inputs(config: Dict[str, Any], errors: List[Dict[str, Any]]) -> Dict[str, Any]:
    raw = config.get("replay_inputs", {})
    if raw in (None, ""):
        config["replay_inputs"] = {}
        return {}
    if not isinstance(raw, dict):
        errors.append(config_schema_error("replay_inputs", "replay_inputs must be an object", raw))
        config["replay_inputs"] = {}
        return {}
    normalized = dict(raw)
    for field in REPLAY_INPUT_LIST_FIELDS:
        values = normalize_config_string_list(normalized, field, errors, f"replay_inputs.{field}")
        normalized[field] = values
    config["replay_inputs"] = normalized
    return normalized


def normalize_tool_version_args(config: Dict[str, Any], errors: List[Dict[str, Any]]) -> Dict[str, List[str]]:
    raw = config.get("tool_version_args", {})
    if raw in (None, ""):
        config["tool_version_args"] = {}
        return {}
    if not isinstance(raw, dict):
        errors.append(config_schema_error("tool_version_args", "tool_version_args must be an object", raw))
        config["tool_version_args"] = {}
        return {}
    normalized: Dict[str, List[str]] = {}
    for tool, args in raw.items():
        if isinstance(tool, bool) or not isinstance(tool, str) or not tool.strip():
            errors.append(config_schema_error("tool_version_args.<name>", "tool_version_args keys must be non-empty strings", tool))
            continue
        if not isinstance(args, list):
            errors.append(config_schema_error(f"tool_version_args.{tool}", f"tool_version_args.{tool} must be a string list", args))
            continue
        values: List[str] = []
        for index, item in enumerate(args, start=1):
            if isinstance(item, bool) or not isinstance(item, str) or not item.strip():
                errors.append(config_schema_error(f"tool_version_args.{tool}[{index}]", f"tool_version_args.{tool}[{index}] must be a non-empty string", item))
                continue
            values.append(item.strip())
        normalized[tool.strip()] = values
    config["tool_version_args"] = normalized
    return normalized


def normalize_product_config(config: Dict[str, Any]) -> List[Dict[str, Any]]:
    errors: List[Dict[str, Any]] = []
    for field in STRING_LIST_CONFIG_FIELDS:
        normalize_config_string_list(config, field, errors)
    normalize_replay_inputs(config, errors)
    normalize_tool_version_args(config, errors)
    return errors


def normalized_config_list(config: Dict[str, Any], field: str) -> List[str]:
    value = config.get(field, [])
    return list(value) if isinstance(value, list) else []


def read_config(path: Path) -> Tuple[Dict[str, Any], Dict[str, Any]]:
    safe_path = metric_path_label(path.as_posix())
    path_redacted = bool(safe_path != path.as_posix())
    try:
        exists = path.exists()
    except OSError as exc:
        return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "config_probe_failed", "parser": None, "error": redact_path_error(exc, path)}
    if not exists:
        return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "missing", "parser": None}
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "unreadable", "parser": None, "error": redact_path_error(exc, path)}
    except UnicodeDecodeError as exc:
        return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "invalid_encoding", "parser": None, "error": redact_path_error(exc, path)}
    try:
        data = json.loads(text)
        if not isinstance(data, dict):
            return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "invalid_schema", "parser": "json", "error": f"expected object, got {type(data).__name__}"}
        return data, {"path": safe_path, "path_redacted": path_redacted, "status": "ok", "parser": "json"}
    except json.JSONDecodeError as json_error:
        try:
            import yaml  # type: ignore

            data = yaml.safe_load(text)
            if not isinstance(data, dict):
                return {}, {"path": safe_path, "path_redacted": path_redacted, "status": "invalid_schema", "parser": "yaml", "error": f"expected object, got {type(data).__name__}"}
            return data, {"path": safe_path, "path_redacted": path_redacted, "status": "ok", "parser": "yaml"}
        except Exception as yaml_error:  # pragma: no cover - optional dependency fallback
            return {}, {
                "path": safe_path,
                "path_redacted": path_redacted,
                "status": "parse_error",
                "parser": "json/yaml",
                "json_error": str(json_error),
                "yaml_error": redact_path_error(yaml_error, path),
            }


def numeric(value: Any) -> Optional[float]:
    if isinstance(value, bool):
        return 1.0 if value else 0.0
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        cleaned = value.strip().replace(",", "").replace("_", "")
        if cleaned.endswith("%"):
            cleaned = cleaned[:-1]
            try:
                return float(cleaned) / 100.0
            except ValueError:
                return None
        try:
            return float(cleaned)
        except ValueError:
            return None
    return None


def stable_metric_name(name: str) -> str:
    lowered = re.sub(r"[^a-zA-Z0-9]+", "_", name.strip().lower()).strip("_")
    aliases = {
        "time_to_first_token": "ttft_ms",
        "time_to_first_token_ms": "ttft_ms",
        "first_token_latency": "ttft_ms",
        "first_token_latency_ms": "ttft_ms",
        "ttft": "ttft_ms",
        "ttft_ms": "ttft_ms",
        "time_per_output_token": "tpot_ms",
        "time_per_output_token_ms": "tpot_ms",
        "inter_token_latency": "tpot_ms",
        "inter_token_latency_ms": "tpot_ms",
        "tpot": "tpot_ms",
        "tpot_ms": "tpot_ms",
        "p95": "p95_latency_ms",
        "p95_latency": "p95_latency_ms",
        "p95_latency_ms": "p95_latency_ms",
        "p99": "p99_latency_ms",
        "p99_latency": "p99_latency_ms",
        "p99_latency_ms": "p99_latency_ms",
        "latency_p99_ms": "p99_latency_ms",
        "throughput": "throughput_tokens_per_s",
        "token_throughput": "throughput_tokens_per_s",
        "tokens_per_second": "throughput_tokens_per_s",
        "tokens_per_s": "throughput_tokens_per_s",
        "tok_s": "throughput_tokens_per_s",
        "output_tokens_per_s": "throughput_tokens_per_s",
        "request_throughput": "request_throughput_per_s",
        "requests_per_second": "request_throughput_per_s",
        "requests_per_s": "request_throughput_per_s",
        "qps": "request_throughput_per_s",
        "error_rate": "error_rate",
        "errors_per_request": "error_rate",
        "kv_hit_rate": "kv_prefix_hit_rate",
        "kv_prefix_hit_rate": "kv_prefix_hit_rate",
        "prefix_cache_hit_rate": "kv_prefix_hit_rate",
        "cache_hit_rate": "cache_hit_rate",
        "remote_cache_hit_rate": "remote_cache_hit_rate",
        "restore_latency": "restore_latency_ms",
        "restore_latency_ms": "restore_latency_ms",
        "transfer_latency": "transfer_latency_ms",
        "transfer_latency_ms": "transfer_latency_ms",
        "kv_transfer_bytes": "kv_transfer_bytes",
        "offload_bytes": "offload_bytes",
        "gpu_memory": "gpu_memory_bytes",
        "gpu_memory_bytes": "gpu_memory_bytes",
        "gpu_memory_used_bytes": "gpu_memory_bytes",
        "engine_size": "engine_size_bytes",
        "engine_size_bytes": "engine_size_bytes",
        "build_time": "build_time_s",
        "build_time_s": "build_time_s",
        "queue_latency": "queue_latency_ms",
        "queue_latency_ms": "queue_latency_ms",
        "router_latency": "router_latency_ms",
        "router_latency_ms": "router_latency_ms",
        "ready_replicas": "ready_replicas",
        "replica_count": "replica_count",
        "fallback_count": "fallback_count",
        "kernel_available": "kernel_available",
        "supported_shape_ratio": "supported_shape_ratio",
        "shape_coverage": "supported_shape_ratio",
        "dtype_coverage": "dtype_coverage",
        "first_chunk_ms": "first_chunk_ms",
        "stream_gap_ms": "stream_gap_ms",
    }
    return aliases.get(lowered, lowered)


def metric_unit(name: str, raw_unit: str = "") -> str:
    unit = raw_unit.lower()
    if unit in {"ms", "msec"} or name.endswith("_ms"):
        return "milliseconds"
    if unit in {"tokens/s", "tok/s"} or name.endswith("_per_s"):
        return "per_second"
    if unit in {"req/s", "requests/s"}:
        return "requests_per_second"
    if unit in {"s", "sec", "seconds"} or name.endswith("_s"):
        return "seconds"
    if unit in {"bytes"} or name.endswith("_bytes"):
        return "bytes"
    if unit in {"gb", "gib", "mb", "mib"}:
        return unit
    if unit == "%" or name.endswith("_rate") or name.endswith("_ratio"):
        return "ratio"
    return "count_or_scalar"


def replay_category(path: Path) -> str:
    lowered = path.as_posix().lower()
    if path.suffix in {".nsys-rep", ".ncu-rep", ".sqlite"}:
        return "binary_profiler_artifact"
    if "prom" in lowered or path.suffix == ".prom":
        return "prometheus_text"
    if "vllm" in lowered:
        return "vllm"
    if "sglang" in lowered:
        return "sglang"
    if "triton" in lowered or "tensorrt" in lowered or "trtllm" in lowered:
        return "tensorrtllm_triton"
    if "llama" in lowered:
        return "llama_cpp"
    if "kserve" in lowered or "llmd" in lowered:
        return "kserve_llmd"
    if "ray" in lowered:
        return "ray_serve"
    if "moon" in lowered:
        return "mooncake"
    if "lmcache" in lowered or "kv" in lowered:
        return "kv_cache"
    return "generic"


def sha256_prefix(path: Path, limit_bytes: int = 1024 * 1024) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        remaining = limit_bytes
        while remaining > 0:
            chunk = file.read(min(65536, remaining))
            if not chunk:
                break
            digest.update(chunk)
            remaining -= len(chunk)
    return digest.hexdigest()


def metric_path_payload(value: Any, field_name: str = "path", redacted_field_name: str | None = None) -> Dict[str, Any]:
    raw = value.as_posix() if isinstance(value, Path) else ("" if value is None else str(value))
    safe_path = metric_path_label(raw)
    return {
        field_name: safe_path,
        redacted_field_name or f"{field_name}_redacted": bool(raw and safe_path != raw),
    }


def artifact_command_line(parts: Sequence[Any]) -> str:
    return command_line(shared_artifact_command_parts(parts))


def artifact_command_template_line(parts: Sequence[Any]) -> str:
    return command_line(artifact_command_template_parts(parts, content_options=("--prompt", "-p"), content_placeholder="<prompt>"))


def artifact_command_line_from_string(value: str) -> str:
    try:
        parts = shlex.split(value)
    except ValueError:
        return redact_sensitive_text(value)
    return artifact_command_line(parts) if parts else ""


def artifact_command_template_line_from_string(value: str) -> str:
    try:
        parts = shlex.split(value)
    except ValueError:
        return redact_sensitive_text(value)
    return artifact_command_template_line(parts) if parts else ""


PATH_CONFIG_KEY_PARTS = {
    "artifact",
    "artifacts",
    "baseline",
    "binary",
    "build",
    "cache",
    "check",
    "checks",
    "config",
    "dir",
    "directory",
    "engine",
    "evidence",
    "file",
    "fixture",
    "log",
    "manifest",
    "model",
    "output",
    "path",
    "profile",
    "profiling",
    "report",
    "repo",
    "result",
    "results",
    "root",
    "summary",
    "template",
    "tokenizer",
}

NON_PATH_CONFIG_KEYS = {
    "default-endpoint",
    "endpoint",
    "health-paths",
    "metrics-url",
    "namespace",
    "official-sources",
    "product",
    "product-name",
    "url",
}


def is_path_config_key(key: str) -> bool:
    normalized = normalized_command_option(key)
    if normalized in NON_PATH_CONFIG_KEYS:
        return False
    return any(part in normalized.split("-") or normalized.endswith(f"-{part}") for part in PATH_CONFIG_KEY_PARTS)


def redact_configured_commands(value: Any) -> Any:
    if isinstance(value, dict):
        return {item_key: redact_configured_commands(item_value) for item_key, item_value in value.items()}
    if isinstance(value, list):
        return [redact_configured_commands(item) for item in value]
    if isinstance(value, str):
        return artifact_command_line_from_string(value)
    return value


def templated_configured_commands(value: Any) -> Any:
    if isinstance(value, dict):
        return {item_key: templated_configured_commands(item_value) for item_key, item_value in value.items()}
    if isinstance(value, list):
        return [templated_configured_commands(item) for item in value]
    if isinstance(value, str):
        return artifact_command_template_line_from_string(value)
    return value


def path_issue(path: Path, status: str, exc: Optional[BaseException] = None, expected: str = "") -> Dict[str, Any]:
    raw = path.as_posix()
    safe_path = metric_path_label(raw)
    issue: Dict[str, Any] = {
        "path": safe_path,
        "path_redacted": bool(raw and safe_path != raw),
        "status": status,
    }
    if expected:
        issue["expected"] = expected
    if exc is not None:
        issue["error"] = redact_path_error(exc, path)
    return issue


def path_label_list(values: Iterable[Any]) -> List[str]:
    return [metric_path_label(str(value)) for value in values if str(value)]


def append_metric(
    metrics: List[Dict[str, Any]],
    name: str,
    value: Any,
    source: str,
    path: str,
    raw_unit: str = "",
    labels: str = "",
) -> None:
    parsed = numeric(value)
    if parsed is None:
        return
    safe_raw_name = redact_sensitive_text(str(name))
    safe_path = metric_path_label(path)
    stable = stable_metric_name(safe_raw_name)
    metrics.append(
        {
            "name": stable,
            "raw_name": safe_raw_name,
            "value": parsed,
            "unit": metric_unit(stable, raw_unit),
            "source": source,
            "path": safe_path,
            "path_redacted": bool(path and safe_path != path),
            "labels": redact_sensitive_text(labels),
        }
    )


def walk_json_metrics(value: Any, metrics: List[Dict[str, Any]], source: str, path: str, prefix: str = "") -> None:
    if isinstance(value, dict):
        for key, item in value.items():
            next_prefix = f"{prefix}.{key}" if prefix else str(key)
            walk_json_metrics(item, metrics, source, path, next_prefix)
    elif isinstance(value, list):
        for index, item in enumerate(value[:200]):
            walk_json_metrics(item, metrics, source, path, f"{prefix}[{index}]")
    else:
        leaf = prefix.split(".")[-1].split("[")[0]
        append_metric(metrics, leaf, value, source, path)


def parse_json_replay_with_errors(path: Path, rel: str, metric_limit: int) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    metrics: List[Dict[str, Any]] = []
    errors: List[Dict[str, Any]] = []
    try:
        if path.suffix == ".jsonl":
            for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
                if line_number > metric_limit and len(metrics) >= metric_limit:
                    break
                if not line.strip():
                    continue
                try:
                    row = json.loads(line)
                except json.JSONDecodeError as exc:
                    errors.append(
                        {
                            "line": line_number,
                            "error": redact_sensitive_text(str(exc)),
                            **artifact_preview_fields(line.strip(), max_chars=160),
                        }
                    )
                    continue
                walk_json_metrics(row, metrics, "jsonl", rel)
                if len(metrics) >= metric_limit:
                    break
        else:
            try:
                payload = json.loads(path.read_text(encoding="utf-8", errors="replace"))
            except json.JSONDecodeError as exc:
                errors.append({"line": None, "error": redact_path_error(exc, path), "raw_preview": ""})
                return [], errors
            walk_json_metrics(payload, metrics, "json", rel)
    except OSError as exc:
        return [], [{"line": None, "error": redact_path_error(exc, path), "raw_preview": ""}]
    return metrics[:metric_limit], errors[:20]


def parse_prometheus_replay(text: str, rel: str, metric_limit: int) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        match = PROMETHEUS_RE.match(stripped)
        if not match:
            continue
        labels = redact_sensitive_text(match.group("labels") or "")
        append_metric(metrics, match.group("name"), match.group("value"), "prometheus_text", rel, labels=labels)
        if len(metrics) >= metric_limit:
            break
    return metrics


def parse_text_replay(text: str, rel: str, metric_limit: int) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    for match in TEXT_METRIC_RE.finditer(text):
        append_metric(metrics, match.group("name"), match.group("value"), "text_log", rel, match.group("unit") or "")
        if len(metrics) >= metric_limit:
            break
    return metrics


def parse_csv_replay(path: Path, rel: str, metric_limit: int) -> List[Dict[str, Any]]:
    metrics: List[Dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8", errors="replace", newline="") as file:
            reader = csv.DictReader(file)
            for row_index, row in enumerate(reader):
                if row_index >= metric_limit:
                    break
                explicit_name = row.get("metric") or row.get("Metric") or row.get("name") or row.get("Name") or row.get("Metric Name")
                explicit_value = row.get("value") or row.get("Value") or row.get("metric_value") or row.get("Metric Value")
                if explicit_name and explicit_value:
                    append_metric(metrics, explicit_name, explicit_value, "csv", rel)
                    continue
                for key, value in row.items():
                    if key:
                        append_metric(metrics, key, value, "csv", rel)
                if len(metrics) >= metric_limit:
                    break
    except OSError:
        return []
    return metrics[:metric_limit]


def collect_replay_paths(args: argparse.Namespace, config: Dict[str, Any]) -> Tuple[List[Path], List[Dict[str, Any]]]:
    configured = config.get("replay_inputs", {})
    repo_root = getattr(args, "repo_root_path", default_repo_root())
    dirs: List[Path] = []
    files: List[Path] = []
    discovery_errors: List[Dict[str, Any]] = []
    if isinstance(configured, dict):
        for field, target in (
            ("evidence_dirs", dirs),
            ("log_dirs", dirs),
            ("evidence_files", files),
            ("metrics_files", files),
        ):
            raw_items = configured.get(field, [])
            if not isinstance(raw_items, list):
                discovery_errors.append(config_schema_error(f"replay_inputs.{field}", f"replay_inputs.{field} must be a string list", raw_items))
                continue
            for index, item in enumerate(raw_items, start=1):
                if isinstance(item, bool) or not isinstance(item, str) or not item.strip():
                    discovery_errors.append(config_schema_error(f"replay_inputs.{field}[{index}]", f"replay_inputs.{field}[{index}] must be a non-empty string", item))
                    continue
                target.append(resolve_repo_path(repo_root, item.strip()))
    elif configured not in ({}, None, ""):
        discovery_errors.append(config_schema_error("replay_inputs", "replay_inputs must be an object", configured))
    dirs.extend(getattr(args, "evidence_dir_paths", []))
    dirs.extend(getattr(args, "log_dir_paths", []))
    files.extend(getattr(args, "evidence_file_paths", []))
    files.extend(getattr(args, "metrics_file_paths", []))

    paths: List[Path] = []
    for item in files:
        try:
            is_file = item.is_file()
        except OSError as exc:
            discovery_errors.append(path_issue(item, "unreadable_replay_file_probe", exc))
            continue
        if is_file:
            paths.append(item)
            continue
        try:
            exists = item.exists()
        except OSError as exc:
            discovery_errors.append(path_issue(item, "unreadable_replay_file_probe", exc))
            continue
        discovery_errors.append(path_issue(item, "not_replay_file" if exists else "missing_replay_file", expected="file"))
    for item in dirs:
        try:
            exists = item.exists()
        except OSError as exc:
            discovery_errors.append(path_issue(item, "unreadable_replay_dir_probe", exc))
            continue
        if not exists:
            discovery_errors.append(path_issue(item, "missing_replay_dir", expected="directory"))
            continue
        try:
            is_dir = item.is_dir()
        except OSError as exc:
            discovery_errors.append(path_issue(item, "unreadable_replay_dir_probe", exc))
            continue
        if not is_dir:
            discovery_errors.append(path_issue(item, "not_replay_dir", expected="directory"))
            continue
        try:
            for path in item.rglob("*"):
                try:
                    if path.is_file() and path.suffix.lower() in REPLAY_EXTENSIONS:
                        paths.append(path)
                except OSError as exc:
                    discovery_errors.append(path_issue(path, "unreadable_replay_file_probe", exc))
        except OSError as exc:
            discovery_errors.append(path_issue(item, "unreadable_replay_dir_scan", exc))

    deduped: List[Path] = []
    seen = set()
    for path in sorted(paths):
        try:
            resolved = str(path.resolve())
        except OSError as exc:
            discovery_errors.append(path_issue(path, "unreadable_replay_path_resolve", exc))
            resolved = path.as_posix()
        if resolved in seen:
            continue
        seen.add(resolved)
        deduped.append(path)
    return deduped, discovery_errors


def parse_replay_file(path: Path, root: Optional[Path], text_limit: int, metric_limit: int) -> Dict[str, Any]:
    rel = path.as_posix()
    if root is not None:
        try:
            rel = path.relative_to(root).as_posix()
        except ValueError:
            rel = path.as_posix()
    safe_rel = metric_path_label(rel)
    try:
        stat = path.stat()
    except OSError as exc:
        return {
            "path": safe_rel,
            "path_redacted": bool(rel and safe_rel != rel),
            "category": replay_category(path),
            "suffix": path.suffix,
            "size_bytes": 0,
            "sha256_prefix": "",
            "metrics": [],
            "text_preview": "",
            "status": "unreadable_replay_file",
            "read_error": redact_path_error(exc, path),
        }
    try:
        digest = sha256_prefix(path)
    except OSError as exc:
        digest = ""
        digest_error = redact_path_error(exc, path)
    else:
        digest_error = ""
    record: Dict[str, Any] = {
        "path": safe_rel,
        "path_redacted": bool(rel and safe_rel != rel),
        "category": replay_category(path),
        "suffix": path.suffix,
        "size_bytes": stat.st_size,
        "sha256_prefix": digest,
        "metrics": [],
        "text_preview": "",
    }
    if digest_error:
        record["status"] = "unreadable_replay_file"
        record["read_error"] = digest_error
    if path.suffix in {".ncu-rep", ".nsys-rep", ".sqlite"}:
        record["binary_artifact"] = True
        return record
    if path.suffix in {".json", ".jsonl"}:
        metrics, parse_errors = parse_json_replay_with_errors(path, rel, metric_limit)
        record["metrics"] = metrics
        if parse_errors:
            record["parse_errors"] = parse_errors
            record["parse_error_count"] = len(parse_errors)
        return record
    try:
        text = path.read_text(encoding="utf-8", errors="replace")[:text_limit]
    except OSError as exc:
        record["status"] = "unreadable_replay_file"
        record["read_error"] = redact_path_error(exc, path)
        return record
    record.update(
        artifact_preview_fields(
            "\n".join(text.splitlines()[:20]),
            field_name="text_preview",
            max_chars=4000,
        )
    )
    if path.suffix == ".csv":
        record["metrics"] = parse_csv_replay(path, rel, metric_limit)
    elif path.suffix == ".prom":
        record["metrics"] = parse_prometheus_replay(text, rel, metric_limit)
    else:
        metrics = parse_prometheus_replay(text, rel, metric_limit)
        metrics.extend(parse_text_replay(text, rel, metric_limit))
        record["metrics"] = metrics[:metric_limit]
    return record


def summarize_replay_metrics(files: List[Dict[str, Any]], metric_limit: int) -> Dict[str, Any]:
    metric_rows: List[Dict[str, Any]] = []
    for item in files:
        metric_rows.extend(item.get("metrics", []))
    metric_rows = metric_rows[:metric_limit]

    by_name: Dict[str, Dict[str, Any]] = {}
    for metric in metric_rows:
        name = str(metric.get("name", ""))
        if not name:
            continue
        entry = by_name.setdefault(
            name,
            {
                "name": name,
                "unit": metric.get("unit"),
                "latest_value": metric.get("value"),
                "source_count": 0,
                "paths": [],
            },
        )
        entry["latest_value"] = metric.get("value")
        entry["source_count"] += 1
        path = metric.get("path")
        if path and path not in entry["paths"]:
            entry["paths"].append(path)
    categories: Dict[str, Dict[str, Any]] = {}
    for item in files:
        category = item.get("category", "generic")
        bucket = categories.setdefault(category, {"files": 0, "bytes": 0, "metrics": 0})
        bucket["files"] += 1
        bucket["bytes"] += item.get("size_bytes", 0)
        bucket["metrics"] += len(item.get("metrics", []))
    return {
        "evidence_file_count": len(files),
        "parsed_metric_count": len(metric_rows),
        "canonical_metric_count": len(by_name),
        "categories": categories,
        "metrics_by_name": by_name,
        "metric_rows": metric_rows,
    }


def build_replay_evidence(args: argparse.Namespace, config: Dict[str, Any]) -> Dict[str, Any]:
    paths, discovery_errors = collect_replay_paths(args, config)
    root: Optional[Path] = None
    evidence_dir_paths = getattr(args, "evidence_dir_paths", [])
    if evidence_dir_paths:
        first_dir = evidence_dir_paths[0]
        try:
            first_dir_exists = first_dir.exists()
        except OSError as exc:
            discovery_errors.append(path_issue(first_dir, "unreadable_replay_root_probe", exc))
            first_dir_exists = False
        if first_dir_exists:
            root = first_dir
    files = [parse_replay_file(path, root, args.text_limit, args.metric_limit) for path in paths]
    summary = summarize_replay_metrics(files, args.metric_limit)
    summary["discovery_error_count"] = len(discovery_errors)
    return {
        "schema_version": "product_replay_evidence.v1",
        "generated_at": utc_now(),
        "inputs": {
            "evidence_dirs": path_label_list(args.evidence_dir or []),
            "evidence_files": path_label_list(args.evidence_file or []),
            "metrics_files": path_label_list(args.metrics_file or []),
            "log_dirs": path_label_list(args.log_dir or []),
            "resolved_evidence_dirs": [metric_path_label(path.as_posix()) for path in evidence_dir_paths],
            "resolved_evidence_files": [metric_path_label(path.as_posix()) for path in getattr(args, "evidence_file_paths", [])],
            "resolved_metrics_files": [metric_path_label(path.as_posix()) for path in getattr(args, "metrics_file_paths", [])],
            "resolved_log_dirs": [metric_path_label(path.as_posix()) for path in getattr(args, "log_dir_paths", [])],
            "paths_redacted": True,
        },
        "summary": summary,
        "discovery_errors": discovery_errors,
        "files": files,
        "notes": [
            "offline replay reads already-collected logs, metrics, JSON, CSV, and profiler artifacts",
            "binary Nsight artifacts are recorded by metadata; export text or CSV next to them for metric extraction",
            "replay mode does not launch products, install dependencies, download models, or probe endpoints",
        ],
    }


def module_probe(module_name: str) -> Dict[str, Any]:
    result: Dict[str, Any] = {"name": module_name, "available": False, "version": None}
    if importlib.util.find_spec(module_name) is None:
        return result
    result["available"] = True
    package_name = module_name.replace("_", "-") if module_name == "tensorrt_llm" else module_name
    try:
        result["version"] = importlib.metadata.version(package_name)
    except Exception:
        result["version"] = None
    return result


def tool_probe(tool_name: str) -> Dict[str, Any]:
    path = shutil.which(tool_name)
    safe_path = metric_path_label(path or "missing")
    return {
        "name": tool_name,
        "path": safe_path or "missing",
        "path_redacted": bool(path and safe_path != path),
        "available": bool(path),
    }


def run_command(command: Sequence[str], timeout_seconds: float) -> Dict[str, Any]:
    command_parts = [str(part) for part in command]
    redacted_command = shared_artifact_command_parts(command_parts)
    capture = run_bounded_process(
        command_parts,
        cwd=None,
        timeout_seconds=timeout_seconds,
        stdout_limit_bytes=PRODUCT_COMMAND_STREAM_LIMIT_BYTES,
        stderr_limit_bytes=PRODUCT_COMMAND_STREAM_LIMIT_BYTES,
        launch_error_formatter=lambda exc: (
            "missing_executable"
            if isinstance(exc, FileNotFoundError)
            else redact_path_error(exc, *command_parts)
        ),
    )
    stdout = capture.stdout.decode("utf-8", errors="replace")
    stderr = capture.stderr.decode("utf-8", errors="replace")
    result = {
        "command": redacted_command,
        "returncode": capture.returncode,
        "elapsed_ms": capture.elapsed_ms,
        "stdout_tail": redact_artifact_output(
            "\n".join(stdout.splitlines()[-40:]), paths=command_parts
        ),
        "stderr_tail": redact_artifact_output(
            "\n".join(stderr.splitlines()[-40:]), paths=command_parts
        ),
        "timed_out": capture.timed_out,
    }
    if capture.status in {"ok", "nonzero_exit"}:
        return result

    result["returncode"] = None
    if capture.status == "launch_failed" and capture.error == "missing_executable":
        result["elapsed_ms"] = 0
        result["stderr_tail"] = "missing executable"
        result["error"] = "missing_executable"
    elif capture.status == "launch_failed":
        result["stderr_tail"] = capture.error
        result["error"] = capture.error
    elif capture.status == "timed_out":
        result["error"] = redact_sensitive_text(
            f"timed out after {timeout_seconds}s"
        )
    else:
        result["error"] = redact_sensitive_text(capture.error or capture.status)
    return result


def product_http_body_fields(body: BoundedHttpBody) -> Dict[str, Any]:
    return {
        "body_status": body.status,
        "body_bytes": body.observed_bytes,
        "body_limit_bytes": PRODUCT_HTTP_BODY_LIMIT_BYTES,
        "body_complete": body.status == "ok",
        "body_output_limit_exceeded": body.output_limit_exceeded,
    }


def http_get(url: str, timeout_seconds: float, api_key: str = "") -> Dict[str, Any]:
    request = urllib.request.Request(url, method="GET")
    if api_key:
        request.add_header("Authorization", f"Bearer {api_key}")
    started = time.perf_counter()
    try:
        with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
            body = read_bounded_http_body(response, PRODUCT_HTTP_BODY_LIMIT_BYTES)
            text = body.data.decode("utf-8", errors="replace")
            result = {
                "url": redact_sensitive_text(url),
                "status_code": response.status,
                "elapsed_ms": round((time.perf_counter() - started) * 1000.0, 3),
                "ok": 200 <= response.status < 300 and body.status == "ok",
                **product_http_body_fields(body),
                **artifact_preview_fields(text, field_name="body_prefix", max_chars=1000),
            }
            if body.status != "ok":
                result["error"] = redact_sensitive_text(body.error or body.status)
            return result
    except urllib.error.HTTPError as exc:
        body = read_bounded_http_body(exc, PRODUCT_HTTP_BODY_LIMIT_BYTES)
        text = body.data.decode("utf-8", errors="replace")
        return {
            "url": redact_sensitive_text(url),
            "status_code": exc.code,
            "elapsed_ms": round((time.perf_counter() - started) * 1000.0, 3),
            "ok": False,
            **product_http_body_fields(body),
            **artifact_preview_fields(text, field_name="body_prefix", max_chars=1000),
            "error": redact_sensitive_text(body.error or str(exc)),
        }
    except Exception as exc:
        return {
            "url": redact_sensitive_text(url),
            "status_code": 0,
            "elapsed_ms": round((time.perf_counter() - started) * 1000.0, 3),
            "ok": False,
            "body_status": "request_failed",
            "body_bytes": 0,
            "body_limit_bytes": PRODUCT_HTTP_BODY_LIMIT_BYTES,
            "body_complete": False,
            "body_output_limit_exceeded": False,
            "error": redact_sensitive_text(str(exc)),
        }


def endpoint_checks(endpoint: str, paths: Iterable[str], timeout_seconds: float, api_key: str) -> List[Dict[str, Any]]:
    if not endpoint:
        return []
    base = endpoint.rstrip("/")
    checks = []
    for path in paths:
        if not path:
            continue
        suffix = path if path.startswith("/") else f"/{path}"
        checks.append(http_get(f"{base}{suffix}", timeout_seconds, api_key))
    return checks


def nvidia_smi_snapshot(execute: bool, timeout_seconds: float) -> Dict[str, Any]:
    path = shutil.which("nvidia-smi")
    safe_path = metric_path_label(path or "missing")
    result: Dict[str, Any] = {
        "path": safe_path or "missing",
        "path_redacted": bool(path and safe_path != path),
        "available": bool(path),
        "gpus": [],
    }
    if not execute or not path:
        return result
    completed = run_command(
        [
            "nvidia-smi",
            "--query-gpu=name,memory.total,driver_version",
            "--format=csv,noheader",
        ],
        timeout_seconds,
    )
    result["query"] = completed
    if completed.get("returncode") == 0:
        gpus: List[Dict[str, str]] = []
        for line in str(completed.get("stdout_tail", "")).splitlines():
            parts = [item.strip() for item in line.split(",")]
            if len(parts) >= 3:
                gpus.append({"name": parts[0], "memory_total": parts[1], "driver_version": parts[2]})
        result["gpus"] = gpus
    return result


def normalize_env_var_name(label: str, value: Any) -> str:
    if value is None:
        return ""
    name = str(value).strip()
    if not name:
        return ""
    if not ENV_VAR_NAME_RE.fullmatch(name):
        raise SystemExit(f"{label} must be an environment variable name, not a secret value")
    return name


def is_sensitive_config_key(key: str) -> bool:
    return is_sensitive_field_name(key, redact_env_names=True)


def redact_config_value(value: Any, key: str = "") -> Any:
    if is_sensitive_config_key(key):
        return REDACTED_VALUE
    if isinstance(value, dict):
        return {item_key: redact_config_value(item_value, str(item_key)) for item_key, item_value in value.items()}
    if isinstance(value, list):
        return [redact_config_value(item, key) for item in value]
    if isinstance(value, str):
        if is_path_config_key(key) or (key and looks_like_local_path(value)):
            return artifact_path_or_value(value)
        return redact_sensitive_text(value)
    return value


def path_preflight_record(repo_root: Path, value: str) -> Dict[str, Any]:
    resolved = resolve_repo_path(repo_root, value)
    path_fields = metric_path_payload(value)
    resolved_fields = metric_path_payload(resolved, "resolved_path", "resolved_path_redacted")
    try:
        exists = resolved.exists()
        is_dir = resolved.is_dir()
    except OSError as exc:
        return {
            **path_fields,
            **resolved_fields,
            "exists": False,
            "is_dir": False,
            "status": "unreadable_path_probe",
            "error": redact_path_error(exc, resolved),
        }
    return {
        **path_fields,
        **resolved_fields,
        "exists": exists,
        "is_dir": is_dir,
        "status": "ok",
    }


def build_preflight(config: Dict[str, Any], execute: bool, timeout_seconds: float, repo_root: Path) -> Dict[str, Any]:
    required_modules = normalized_config_list(config, "required_modules")
    optional_modules = normalized_config_list(config, "optional_modules")
    required_tools = normalized_config_list(config, "required_tools")
    optional_tools = normalized_config_list(config, "optional_tools")
    paths = config.get("paths", {})
    module_results = {name: module_probe(name) for name in sorted(set(required_modules + optional_modules))}
    tool_results = {name: tool_probe(name) for name in sorted(set(required_tools + optional_tools))}
    path_results = {
        name: path_preflight_record(repo_root, str(path))
        for name, path in sorted(paths.items())
        if isinstance(path, str) and path
    } if isinstance(paths, dict) else {}
    missing = []
    for name in required_modules:
        if not module_results.get(name, {}).get("available"):
            missing.append(f"module:{name}")
    for name in required_tools:
        if not tool_results.get(name, {}).get("available"):
            missing.append(f"tool:{name}")
    for name, item in path_results.items():
        if name.startswith("required_") and not item.get("exists"):
            missing.append(f"path:{name}")
    return {
        "python": sys.version.split()[0],
        "platform": platform.platform(),
        "machine": platform.machine(),
        "mode": "execute" if execute else "dry_run",
        "modules": module_results,
        "tools": tool_results,
        "paths": path_results,
        "gpu": nvidia_smi_snapshot(execute, timeout_seconds),
        "missing": missing,
        "warnings": normalized_config_list(config, "warnings"),
    }


def version_capture(config: Dict[str, Any], execute: bool, timeout_seconds: float) -> Dict[str, Any]:
    required_modules = normalized_config_list(config, "required_modules")
    optional_modules = normalized_config_list(config, "optional_modules")
    required_tools = normalized_config_list(config, "required_tools")
    optional_tools = normalized_config_list(config, "optional_tools")
    modules = {
        name: module_probe(name)
        for name in sorted(set(required_modules + optional_modules))
    }
    tools = {}
    for name in sorted(set(required_tools + optional_tools)):
        probe = tool_probe(name)
        if execute and probe.get("available"):
            version_args = config.get("tool_version_args", {})
            args = version_args.get(name, ["--version"]) if isinstance(version_args, dict) else ["--version"]
            tools[name] = {**probe, "version_command": run_command([name, *[str(item) for item in args]], timeout_seconds)}
        else:
            tools[name] = probe
    return {"modules": modules, "tools": tools, "status": "captured" if execute else "planned"}


def planned_commands(product_key: str, args: argparse.Namespace, config: Dict[str, Any]) -> Dict[str, Any]:
    repo_root = args.repo_root
    prefix = product_profile_command_prefix(product_key)
    dry = [*prefix, "--repo-root", repo_root, "--config", args.config, "--output", args.output, "--report", args.report, "--dry-run"]
    execute = [*prefix, "--repo-root", repo_root, "--config", args.config, "--output", args.output, "--report", args.report, "--execute"]
    replay = [*prefix, "--repo-root", repo_root, "--config", args.config, "--output", args.output, "--report", args.report, "--replay"]
    endpoint = args.endpoint or config.get("endpoint") or config.get("default_endpoint")
    if endpoint:
        execute.extend(["--endpoint", str(endpoint)])
    if args.namespace:
        execute.extend(["--namespace", args.namespace])
    if args.engine_dir:
        execute.extend(["--engine-dir", args.engine_dir])
    if args.model_path:
        execute.extend(["--model-path", args.model_path])
    replay_inputs = config.get("replay_inputs", {})
    replay_dir = ""
    if args.evidence_dir:
        replay_dir = args.evidence_dir[0]
    elif isinstance(replay_inputs, dict) and replay_inputs.get("evidence_dirs"):
        replay_dir = str(replay_inputs["evidence_dirs"][0])
    if replay_dir:
        replay.extend(["--evidence-dir", replay_dir])
    configured = redact_config_value(config.get("planned_commands", {}))
    return {
        "dry_run": artifact_command_line(dry),
        "dry_run_template": artifact_command_template_line(dry),
        "execute": artifact_command_line(execute),
        "execute_template": artifact_command_template_line(execute),
        "replay": artifact_command_line(replay),
        "replay_template": artifact_command_template_line(replay),
        "configured": redact_configured_commands(configured) if isinstance(configured, dict) else {},
        "configured_template": templated_configured_commands(configured) if isinstance(configured, dict) else {},
        "template_policy": "template fields replace local paths and prompt-like content with target-machine placeholders",
    }


def config_snapshot(config: Dict[str, Any]) -> Dict[str, Any]:
    return redact_config_value(json.loads(json.dumps(config)))


def render_yaml_template(config: Dict[str, Any], repo_root: Path) -> Dict[str, Any]:
    template_path = config.get("manifest_template")
    if not isinstance(template_path, str) or not template_path:
        return {"status": "not_configured"}
    path = resolve_repo_path(repo_root, template_path)
    resolved_raw = path.as_posix()
    resolved_safe = metric_path_label(resolved_raw)
    resolved_path_redacted = bool(resolved_safe != resolved_raw)
    template_safe = metric_path_label(template_path)
    template_path_redacted = bool(template_path and template_safe != template_path)
    try:
        exists = path.exists()
    except OSError as exc:
        return {"status": "template_probe_failed", "path": template_safe, "path_redacted": template_path_redacted, "resolved_path": resolved_safe, "resolved_path_redacted": resolved_path_redacted, "error": redact_path_error(exc, path)}
    if not exists:
        return {"status": "missing", "path": template_safe, "path_redacted": template_path_redacted, "resolved_path": resolved_safe, "resolved_path_redacted": resolved_path_redacted}
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return {"status": "unreadable", "path": template_safe, "path_redacted": template_path_redacted, "resolved_path": resolved_safe, "resolved_path_redacted": resolved_path_redacted, "error": redact_path_error(exc, path)}
    except UnicodeDecodeError as exc:
        return {"status": "invalid_encoding", "path": template_safe, "path_redacted": template_path_redacted, "resolved_path": resolved_safe, "resolved_path_redacted": resolved_path_redacted, "error": redact_path_error(exc, path)}
    replacements = config.get("manifest_replacements", {})
    if isinstance(replacements, dict):
        for key, value in replacements.items():
            text = text.replace("{{" + str(key) + "}}", str(value))
    return {
        "status": "rendered",
        "path": template_safe,
        "path_redacted": template_path_redacted,
        "resolved_path": resolved_safe,
        "resolved_path_redacted": resolved_path_redacted,
        "rendered_bytes": len(text.encode("utf-8")),
        "contains_llm_inference_service": "LLMInferenceService" in text,
        "contains_gateway": "gateway" in text.lower(),
        "contains_prefill": "prefill" in text.lower(),
        **artifact_preview_fields(text, field_name="preview", max_chars=4000),
    }


def execute_extra_commands(commands: Any, timeout_seconds: float) -> List[Dict[str, Any]]:
    if not isinstance(commands, list):
        return []
    results = []
    for item in commands:
        if isinstance(item, str):
            try:
                parts = shlex.split(item)
            except ValueError as exc:
                results.append({"command": redact_sensitive_text(item), "returncode": None, "error": redact_sensitive_text(str(exc))})
                continue
            if parts:
                results.append(run_command(parts, timeout_seconds))
        elif isinstance(item, list):
            results.append(run_command([str(part) for part in item], timeout_seconds))
    return results


def extra_command_failed(item: Dict[str, Any]) -> bool:
    if item.get("error") or item.get("timed_out"):
        return True
    return item.get("returncode") != 0


def build_payload(product_key: str, args: argparse.Namespace) -> Dict[str, Any]:
    definition = PRODUCTS[product_key]
    repo_root = getattr(args, "repo_root_path", default_repo_root())
    default_config = {
        "product": product_key,
        "product_name": definition["name"],
        "category": definition["category"],
        "endpoint": definition.get("default_endpoint", ""),
        "health_paths": definition.get("health_paths", []),
        "official_sources": definition.get("official_sources", []),
        "key_features": definition.get("key_features", []),
        "required_modules": definition.get("required_modules", []),
        "optional_modules": definition.get("optional_modules", []),
        "required_tools": definition.get("required_tools", []),
        "optional_tools": definition.get("optional_tools", []),
        "expected_metrics": definition.get("expected_metrics", []),
        "not_implemented": definition.get("not_implemented", []),
        "local_optimization_scope": definition.get("local_optimization_scope", []),
        "risks": [],
    }
    loaded_config, config_status = read_config(getattr(args, "config_path", resolve_repo_path(repo_root, args.config)))
    config = deep_merge(default_config, loaded_config)
    config_schema_errors = normalize_product_config(config)
    if config_schema_errors:
        config_status = {
            **config_status,
            "schema_error_count": len(config_schema_errors),
            "schema_errors": config_schema_errors[:50],
        }
    execute = bool(args.execute)
    replay = bool(args.replay)
    mode = "execute" if execute else "replay" if replay else "dry_run"
    endpoint = args.endpoint or str(config.get("endpoint", ""))
    metrics_url = args.metrics_url or str(config.get("metrics_url", ""))
    api_key_env = normalize_env_var_name("api_key_env", args.api_key_env or config.get("api_key_env", ""))
    api_key = os.environ.get(api_key_env, "") if api_key_env else ""
    namespace = args.namespace or str(config.get("namespace", ""))
    if args.engine_dir:
        config.setdefault("paths", {})["engine_dir"] = args.engine_dir
    if args.model_path:
        config.setdefault("paths", {})["model_path"] = args.model_path
    if namespace:
        config["namespace"] = namespace

    preflight = build_preflight(config, execute, args.timeout_seconds, repo_root)
    preflight["mode"] = mode
    versions = version_capture(config, execute, args.timeout_seconds)
    if replay:
        versions["status"] = "replayed_from_offline_evidence"
    health_checks = endpoint_checks(endpoint, config.get("health_paths", []), args.timeout_seconds, api_key) if execute else []
    metrics_check = http_get(metrics_url, args.timeout_seconds, api_key) if execute and metrics_url else None
    manifest = render_yaml_template(config, repo_root)
    extra_command_results = execute_extra_commands(config.get("execute_commands"), args.timeout_seconds) if execute else []
    replay_evidence = build_replay_evidence(args, config) if replay else None
    failures = len(preflight.get("missing", []))
    failures += sum(1 for item in health_checks if not item.get("ok"))
    failures += sum(1 for item in extra_command_results if extra_command_failed(item))
    if execute and metrics_check is not None and not metrics_check.get("ok"):
        failures += 1

    status = "planned"
    if execute:
        status = "ok" if failures == 0 else "partial" if health_checks or extra_command_results or metrics_check else "failed"
    elif replay:
        replay_summary = replay_evidence.get("summary", {}) if isinstance(replay_evidence, dict) else {}
        evidence_count = int(replay_summary.get("evidence_file_count", 0))
        metric_count = int(replay_summary.get("parsed_metric_count", 0))
        status = "ok" if evidence_count and metric_count else "partial" if evidence_count else "failed"
    if config_schema_errors:
        status = "invalid_config"

    raw_metrics: Dict[str, Any] = {
        "expected_metrics": normalized_config_list(config, "expected_metrics"),
        "collected": {},
        "unavailable_reason": "dry-run only; move to a real product environment and pass --execute, or pass --replay with already-collected evidence files",
    }
    if execute:
        raw_metrics = {
            "expected_metrics": normalized_config_list(config, "expected_metrics"),
            "collected": {
                "endpoint_check_count": len(health_checks),
                "endpoint_success_count": sum(1 for item in health_checks if item.get("ok")),
                "metrics_endpoint_ok": bool(metrics_check and metrics_check.get("ok")) if metrics_check is not None else None,
                "extra_command_count": len(extra_command_results),
                "extra_command_success_count": sum(1 for item in extra_command_results if item.get("returncode") == 0),
            },
            "unavailable_reason": None,
        }
    elif replay:
        replay_summary = replay_evidence.get("summary", {}) if isinstance(replay_evidence, dict) else {}
        raw_metrics = {
            "expected_metrics": normalized_config_list(config, "expected_metrics"),
            "collected": {
                "evidence_file_count": replay_summary.get("evidence_file_count", 0),
                "parsed_metric_count": replay_summary.get("parsed_metric_count", 0),
                "canonical_metric_count": replay_summary.get("canonical_metric_count", 0),
                "metrics_by_name": replay_summary.get("metrics_by_name", {}),
            },
            "normalized_metric_rows": replay_summary.get("metric_rows", []),
            "evidence_mode": "offline_replay",
            "unavailable_reason": None if replay_summary.get("parsed_metric_count", 0) else "no metrics parsed from offline evidence",
        }

    claim_scope = {
        "mode": mode,
        "real_performance_claim": (
            "not_supported_from_invalid_config" if config_schema_errors else
            "allowed_from_execute_artifact" if execute else
            "allowed_from_offline_replay_evidence" if replay and status == "ok" else
            "not_supported_from_replay_without_metrics" if replay else
            "not_supported_from_dry_run"
        ),
        "requires_target_environment": execute,
        "requires_external_product_core": True,
        "inferlab_role": "adapter/profile/config/artifact/report/contract; product core remains external",
        "notes": [
            "dry-run artifacts are plans only",
            "execute artifacts are collected by probing a target environment",
            "replay artifacts are normalized from already-collected product logs, metrics, profiler exports, or JSON evidence",
        ],
    }

    payload = {
        "schema": {"name": "product_profile", "version": "v1"},
        "schema_version": "product_profile.v1",
        "product": product_key,
        "product_name": definition["name"],
        "category": definition["category"],
        "profile_scope": definition["scope"],
        "mode": mode,
        "status": status,
        "executed": execute,
        "replayed": replay,
        "generated_at": utc_now(),
        "official_sources": normalized_config_list(config, "official_sources"),
        "inferlab_adapter": {
            "script": product_profile_module(product_key),
            "script_entry_kind": "python_module",
            "script_module": product_profile_module(product_key),
            "script_command_prefix": command_line(product_profile_command_prefix(product_key)),
            "repo_root": metric_path_label(repo_root.as_posix()),
            **metric_path_payload(args.config, "config", "config_path_redacted"),
            **metric_path_payload(args.output, "artifact", "artifact_path_redacted"),
            **metric_path_payload(args.report, "report", "report_path_redacted"),
            "resolved_config": metric_path_label(getattr(args, "config_path", resolve_repo_path(repo_root, args.config)).as_posix()),
            "resolved_artifact": metric_path_label(getattr(args, "output_path", resolve_repo_path(repo_root, args.output)).as_posix()),
            "resolved_report": metric_path_label(getattr(args, "report_path", resolve_repo_path(repo_root, args.report)).as_posix()),
            "paths_redacted": True,
            "adapter_type": "external_product_profile",
            "boundary": "adapter/profile/config/artifact/report only; product core remains external",
        },
        "config_load": config_status,
        "environment_preflight": preflight,
        "version_capture": versions,
        "config_snapshot": config_snapshot(config),
        "planned_commands": planned_commands(product_key, args, config),
        "redaction_policy": {
            "status": "enabled",
            "fields": [
                "config_snapshot",
                "planned_commands.configured",
                "command_results.command",
                "command_results.stdout_tail",
                "command_results.stderr_tail",
                "endpoint_checks.url",
                "endpoint_checks.body_prefix",
                "metrics_endpoint.url",
                "metrics_endpoint.body_prefix",
            ],
            "placeholder": REDACTED_VALUE,
        },
        "endpoint_checks": health_checks,
        "metrics_endpoint": metrics_check,
        "manifest": manifest,
        "command_results": extra_command_results,
        "raw_metrics": raw_metrics,
        "replay_evidence": replay_evidence,
        "config_schema_errors": config_schema_errors,
        "report_sections": normalized_config_list(config, "report_sections"),
        "key_features": normalized_config_list(config, "key_features"),
        "risks": normalized_config_list(config, "risks"),
        "not_implemented_by_inferlab": normalized_config_list(config, "not_implemented"),
        "local_optimization_scope": normalized_config_list(config, "local_optimization_scope"),
        "claim_scope": claim_scope,
    }
    if replay_evidence is None:
        payload.pop("replay_evidence")
    return payload


def write_json(path: str | Path, payload: Dict[str, Any]) -> Dict[str, Any]:
    output = Path(path)
    return write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")


def md_cell(value: Any) -> str:
    if isinstance(value, (list, dict)):
        text = json.dumps(value, ensure_ascii=False, sort_keys=True)
    else:
        text = str(value)
    return text.replace("|", "\\|").replace("\n", "<br>")


def write_report(path: str | Path, payload: Dict[str, Any]) -> Dict[str, Any]:
    preflight = payload.get("environment_preflight", {})
    raw_metrics = payload.get("raw_metrics", {})
    replay = payload.get("replay_evidence", {})
    replay_summary = replay.get("summary", {}) if isinstance(replay, dict) else {}
    rows = [
        ("Product", payload.get("product_name")),
        ("Mode", payload.get("mode")),
        ("Status", payload.get("status")),
        ("Artifact", payload.get("inferlab_adapter", {}).get("artifact")),
        ("Config", payload.get("inferlab_adapter", {}).get("config")),
        ("Missing preflight", preflight.get("missing")),
        ("Expected metrics", raw_metrics.get("expected_metrics")),
        ("Collected metrics", raw_metrics.get("collected")),
        ("Replay evidence files", replay_summary.get("evidence_file_count")),
        ("Replay parsed metrics", replay_summary.get("parsed_metric_count")),
    ]
    lines = [
        f"# {payload.get('product_name')} Product Profile",
        "",
        f"- Generated: `{payload.get('generated_at')}`",
        f"- Mode: `{payload.get('mode')}`",
        f"- Status: `{payload.get('status')}`",
        "",
        "## Boundary",
        "",
        "InferLab uses this product through adapters, configuration, artifact contracts, and reports. It does not reimplement the product core.",
        "",
        "## Summary",
        "",
        "| Field | Value |",
        "|---|---|",
    ]
    lines.extend(f"| {md_cell(key)} | {md_cell(value)} |" for key, value in rows)
    lines.extend(["", "## Product Capabilities", ""])
    lines.extend(f"- {item}" for item in payload.get("key_features", []))
    lines.extend(["", "## InferLab Does Not Implement", ""])
    lines.extend(f"- {item}" for item in payload.get("not_implemented_by_inferlab", []))
    lines.extend(["", "## Local Optimization / Integration Scope", ""])
    lines.extend(f"- {item}" for item in payload.get("local_optimization_scope", []))
    lines.extend(["", "## Official Sources", ""])
    lines.extend(f"- {item}" for item in payload.get("official_sources", []))
    lines.extend(["", "## Commands", "", "```bash"])
    commands = payload.get("planned_commands", {})
    for key in ("dry_run", "execute"):
        if commands.get(key):
            lines.append(str(commands[key]))
    if commands.get("replay"):
        lines.append(str(commands["replay"]))
    lines.append("```")
    if replay_summary:
        lines.extend([
            "",
            "## Offline Replay Evidence",
            "",
            "| Field | Value |",
            "|---|---|",
            f"| Evidence files | {md_cell(replay_summary.get('evidence_file_count'))} |",
            f"| Parsed metrics | {md_cell(replay_summary.get('parsed_metric_count'))} |",
            f"| Canonical metrics | {md_cell(replay_summary.get('canonical_metric_count'))} |",
        ])
    output = Path(path)
    return write_text_file(output, "\n".join(lines) + "\n")


def parse_args(product_key: str) -> argparse.Namespace:
    definition = PRODUCTS[product_key]
    parser = argparse.ArgumentParser(description=f"Profile {definition['name']} as an external InferLab product backend.")
    parser.add_argument("--repo-root", default=str(default_repo_root()), help="InferLab project root used to resolve relative config/output/report/evidence paths.")
    parser.add_argument("--config", default=definition["config"])
    parser.add_argument("--output", default=definition["artifact"])
    parser.add_argument("--report", default=definition["report"])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="Force plan-only mode. This is the default unless --execute or --replay is passed.")
    mode.add_argument("--execute", action="store_true", help="Probe real endpoint/tools. Does not install products or download models.")
    mode.add_argument("--replay", action="store_true", help="Normalize already-collected logs, metrics, profiler exports, or JSON evidence into a product profile artifact.")
    parser.add_argument("--endpoint", default="", help="Product endpoint root, for example http://127.0.0.1:8000.")
    parser.add_argument("--metrics-url", default="", help="Optional explicit metrics URL.")
    parser.add_argument("--namespace", default="", help="Kubernetes namespace for KServe/Dynamo/Ray style deployments.")
    parser.add_argument("--engine-dir", default="", help="TensorRT-LLM engine directory to snapshot.")
    parser.add_argument("--model-path", default="", help="Model path to snapshot for runtimes that require it.")
    parser.add_argument("--api-key-env", default="", help="Environment variable containing an API key for endpoint probes.")
    parser.add_argument("--timeout-seconds", type=float, default=10.0)
    parser.add_argument("--evidence-dir", action="append", default=[], help="Directory containing already-collected product evidence for --replay. Can be repeated.")
    parser.add_argument("--evidence-file", action="append", default=[], help="Single JSON/log/CSV/prometheus/profiler evidence file for --replay. Can be repeated.")
    parser.add_argument("--metrics-file", action="append", default=[], help="Metrics-focused JSON/CSV/prometheus file for --replay. Can be repeated.")
    parser.add_argument("--log-dir", action="append", default=[], help="Directory containing product logs for --replay. Can be repeated.")
    parser.add_argument("--text-limit", type=int, default=200000)
    parser.add_argument("--metric-limit", type=int, default=2000)
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    require_float_range("--timeout-seconds", args.timeout_seconds, 0.001, 86400.0)
    require_int_range("--text-limit", args.text_limit, 1, 10000000)
    require_int_range("--metric-limit", args.metric_limit, 1, 1000000)
    args.api_key_env = normalize_env_var_name("--api-key-env", args.api_key_env)


def main_for_product(product_key: str) -> int:
    args = parse_args(product_key)
    validate_args(args)
    prepare_path_args(args)
    payload = build_payload(product_key, args)
    output_status = write_json(args.output_path, payload)
    report_status = write_report(args.report_path, payload)
    if output_status["status"] == "ok":
        print(f"wrote {metric_path_label(args.output_path.as_posix())}")
    else:
        print(f"failed to write {metric_path_label(args.output_path.as_posix())}: {output_status.get('error')}")
    if report_status["status"] == "ok":
        print(f"wrote {metric_path_label(args.report_path.as_posix())}")
    else:
        print(f"failed to write {metric_path_label(args.report_path.as_posix())}: {report_status.get('error')}")
    return 0 if output_status["status"] == "ok" and report_status["status"] == "ok" else 1
