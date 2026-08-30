#!/usr/bin/env python3
"""Build a cloud/edge backend comparison report from InferLab artifacts.

This script is intentionally evidence-driven and tolerant of missing runtime
data. It does not run benchmarks. It reads existing JSON artifacts and turns
them into a comparison table for vLLM, TensorRT-LLM, llama.cpp, and native CPU
reference/probe evidence.
"""

from __future__ import annotations

import argparse
import json
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from artifact_json import (
    load_json_dict_or_empty as load_json,
    load_json_object_list,
)
from command_redaction import artifact_path_payload as path_payload, redacted_path_label as path_label
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


def get(data: Dict[str, Any], path: Iterable[str], default: Any = None) -> Any:
    current: Any = data
    for item in path:
        if not isinstance(current, dict) or item not in current:
            return default
        current = current[item]
    return current


def number(value: Any) -> Optional[float]:
    if isinstance(value, bool):
        return 1.0 if value else 0.0
    if isinstance(value, (int, float)):
        return float(value)
    return None


def cost_for(records: List[Dict[str, Any]], backend: str) -> Dict[str, Any]:
    candidates = []
    for item in records:
        tags = " ".join(str(item.get(key, "")) for key in ("backend", "purpose", "notes", "instance", "gpu")).lower()
        if backend.replace(".", "") in tags.replace(".", ""):
            candidates.append(item)
    total = 0.0
    known = False
    for item in candidates:
        value = number(item.get("total_cost"))
        if value is None:
            hours = number(item.get("hours"))
            unit = number(item.get("unit_cost"))
            value = hours * unit if hours is not None and unit is not None else None
        if value is not None:
            total += value
            known = True
    return {"records": len(candidates), "total_cost": round(total, 6) if known else None}


def quality_for(quality: Dict[str, Any], backend: str) -> Dict[str, Any]:
    summary = get(quality, ("summary", "backend_summary"), {})
    if not isinstance(summary, dict):
        return {}
    aliases = {
        "llama.cpp": ("llama.cpp", "llama_cpp"),
        "trtllm": ("trtllm", "tensorrt_llm"),
        "vllm": ("vllm",),
        "native": ("native",),
    }
    for alias in aliases.get(backend, (backend,)):
        item = summary.get(alias)
        if isinstance(item, dict):
            return item
    return {}


def profiling_for(profiling: Dict[str, Any], backend: str) -> Dict[str, Any]:
    categories = get(profiling, ("summary", "categories"), {})
    if not isinstance(categories, dict):
        return {}
    aliases = {
        "vllm": ("vllm",),
        "trtllm": ("trtllm",),
        "llama.cpp": ("llama_cpp", "perf"),
        "native": ("nsight_compute", "generic"),
    }
    files = bytes_ = metrics = 0
    for alias in aliases.get(backend, (backend,)):
        item = categories.get(alias)
        if isinstance(item, dict):
            files += int(item.get("files") or 0)
            bytes_ += int(item.get("bytes") or 0)
            metrics += int(item.get("metrics") or 0)
    return {"files": files, "bytes": bytes_, "metrics": metrics}


def quality_full_text_count(quality: Dict[str, Any]) -> Any:
    return quality.get("with_full_text", quality.get("with_text", 0))


def quality_redacted_preview_count(quality: Dict[str, Any]) -> Any:
    return quality.get("with_redacted_preview", 0)


def build_vllm(results: Path, quality: Dict[str, Any], profiling: Dict[str, Any], costs: List[Dict[str, Any]]) -> Dict[str, Any]:
    data = load_json(results / "vllm_serving_benchmark.json")
    summary = data.get("summary", {}) if isinstance(data.get("summary"), dict) else {}
    return {
        "backend": "vllm",
        "runtime_role": "cloud_openai_compatible_serving",
        "status": data.get("status", "missing") if data else "missing",
        "model": data.get("model"),
        "readiness": {
            "gpu_available": get(data, ("environment", "gpu_available")),
            "runtime_available": get(data, ("environment", "vllm", "available")),
            "can_benchmark_external_endpoint": get(data, ("preflight", "can_benchmark_external_endpoint")),
        },
        "latency": {
            "avg_latency_ms": summary.get("avg_latency_ms"),
            "median_latency_ms": summary.get("median_latency_ms"),
            "p90_latency_ms": summary.get("p90_latency_ms"),
            "min_latency_ms": summary.get("min_latency_ms"),
            "max_latency_ms": summary.get("max_latency_ms"),
        },
        "throughput": {
            "request_throughput_per_s": summary.get("request_throughput_per_s"),
            "completion_token_throughput_per_s": summary.get("completion_token_throughput_per_s"),
            "completed_requests": summary.get("completed_requests"),
            "failed_requests": summary.get("failed_requests"),
        },
        "quality": quality_for(quality, "vllm"),
        "profiling": profiling_for(profiling, "vllm"),
        "cost": cost_for(costs, "vllm"),
        "evidence": ["vllm_serving_benchmark.json"],
    }


def build_trtllm(results: Path, quality: Dict[str, Any], profiling: Dict[str, Any], costs: List[Dict[str, Any]]) -> Dict[str, Any]:
    data = load_json(results / "trtllm_benchmark.json")
    summary = data.get("summary", {}) if isinstance(data.get("summary"), dict) else {}
    metrics = data.get("metrics", {}) if isinstance(data.get("metrics"), dict) else {}
    status = data.get("status", "missing") if data else "missing"
    if status == "ok" and data.get("mode") == "preflight":
        status = "preflight_only"
    return {
        "backend": "trtllm",
        "runtime_role": "compiled_gpu_engine",
        "status": status,
        "model": data.get("model"),
        "readiness": {
            "gpu_available": get(data, ("environment", "torch", "cuda_available")),
            "tensorrt_llm_available": get(data, ("environment", "modules", "tensorrt_llm", "available")),
            "tensorrt_available": get(data, ("environment", "modules", "tensorrt", "available")),
        },
        "latency": {
            "latency_ms": summary.get("latency_ms") or metrics.get("latency_ms"),
            "avg_latency_ms": summary.get("avg_latency_ms") or metrics.get("avg_latency_ms"),
            "p50_latency_ms": summary.get("p50_latency_ms") or metrics.get("p50_latency_ms"),
            "p90_latency_ms": summary.get("p90_latency_ms") or metrics.get("p90_latency_ms"),
            "p99_latency_ms": summary.get("p99_latency_ms") or metrics.get("p99_latency_ms"),
        },
        "throughput": {
            "token_throughput_per_s": summary.get("token_throughput_per_s") or metrics.get("token_throughput_per_s"),
            "request_throughput_per_s": summary.get("request_throughput_per_s") or metrics.get("request_throughput_per_s"),
            "throughput_per_s": summary.get("throughput_per_s") or metrics.get("throughput_per_s"),
        },
        "quality": quality_for(quality, "trtllm"),
        "profiling": profiling_for(profiling, "trtllm"),
        "cost": cost_for(costs, "trtllm"),
        "evidence": ["trtllm_benchmark.json"],
    }


def build_llama(results: Path, quality: Dict[str, Any], profiling: Dict[str, Any], costs: List[Dict[str, Any]]) -> Dict[str, Any]:
    data = load_json(results / "llama_cpp_benchmark.json")
    summary = data.get("summary", {}) if isinstance(data.get("summary"), dict) else {}
    config = data.get("config", {}) if isinstance(data.get("config"), dict) else {}
    return {
        "backend": "llama.cpp",
        "runtime_role": "edge_gguf_runtime",
        "status": data.get("status", "missing") if data else "missing",
        "model": data.get("model"),
        "readiness": {
            "can_run_cli": get(data, ("preflight", "can_run_cli")),
            "can_run_bench": get(data, ("preflight", "can_run_bench")),
            "threads": config.get("threads"),
            "ngl": config.get("ngl"),
        },
        "latency": {
            "load_ms": summary.get("load_ms"),
            "prompt_eval_ms": summary.get("prompt_eval_ms"),
            "eval_ms": summary.get("eval_ms"),
            "total_ms": summary.get("total_ms"),
        },
        "throughput": {
            "prompt_eval_tokens_per_s": summary.get("prompt_eval_tokens_per_s"),
            "eval_tokens_per_s": summary.get("eval_tokens_per_s"),
            "prompt_eval_tokens": summary.get("prompt_eval_tokens"),
            "eval_tokens": summary.get("eval_tokens"),
        },
        "quality": quality_for(quality, "llama.cpp"),
        "profiling": profiling_for(profiling, "llama.cpp"),
        "cost": cost_for(costs, "llama"),
        "evidence": ["llama_cpp_benchmark.json"],
    }


def build_native(results: Path, quality: Dict[str, Any], profiling: Dict[str, Any], costs: List[Dict[str, Any]]) -> Dict[str, Any]:
    probes = load_json(results / "native_probe_run.json")
    matrix = load_json(results / "matrix_ops_profile.json")
    attention = load_json(results / "attention_probe.json")
    return {
        "backend": "native",
        "runtime_role": "cpu_reference_and_cuda_kernel_oracles",
        "status": "ok" if probes else "missing",
        "model": "synthetic_native_probes",
        "readiness": {
            "passed_probes": get(probes, ("counts", "passed")),
            "failed_probes": get(probes, ("counts", "failed")),
            "skipped_probes": get(probes, ("counts", "skipped")),
        },
        "latency": {
            "total_probe_elapsed_ms": get(probes, ("timing", "total_elapsed_ms")),
            "slowest_probe_elapsed_ms": get(probes, ("timing", "slowest_probe_elapsed_ms")),
        },
        "throughput": {
            "matmul_tiled_global_bytes": get(matrix, ("matmul", "tiled_global_bytes")),
            "attention_score_intermediate_bytes": get(attention, ("profile", "score_intermediate_bytes")),
        },
        "quality": quality_for(quality, "native"),
        "profiling": profiling_for(profiling, "native"),
        "cost": cost_for(costs, "native"),
        "evidence": ["native_probe_run.json", "matrix_ops_profile.json", "attention_probe.json"],
    }


def readiness_score(record: Dict[str, Any]) -> float:
    status = str(record.get("status", ""))
    if status == "ok":
        score = 1.0
    elif status.startswith("partial"):
        return 0.25
    else:
        return 0.0
    readiness = record.get("readiness", {})
    if isinstance(readiness, dict) and readiness:
        bool_values = [value for value in readiness.values() if isinstance(value, bool)]
        if bool_values:
            score = (score + sum(1.0 for value in bool_values if value) / len(bool_values)) / 2.0
    return round(score, 4)


def recommendation(records: List[Dict[str, Any]]) -> List[str]:
    notes: List[str] = []
    by_backend = {item["backend"]: item for item in records}
    vllm = by_backend.get("vllm", {})
    trt = by_backend.get("trtllm", {})
    llama = by_backend.get("llama.cpp", {})
    native = by_backend.get("native", {})
    if readiness_score(vllm) > readiness_score(trt):
        notes.append("vLLM has stronger current serving readiness evidence than TensorRT-LLM.")
    elif readiness_score(trt) > readiness_score(vllm):
        notes.append("TensorRT-LLM has stronger current compiled-engine readiness evidence than vLLM.")
    else:
        notes.append("vLLM and TensorRT-LLM readiness are tied or both missing runtime evidence.")
    if llama.get("status") == "ok":
        notes.append("llama.cpp has edge runtime evidence and can anchor CPU/ARM comparison.")
    else:
        notes.append("llama.cpp still needs a legal GGUF model and binary run for edge throughput evidence.")
    if native.get("status") == "ok":
        notes.append("Native probes provide local correctness/profile baselines even without full model runtimes.")
    notes.append("Use quality and profiling imports to turn raw runtime logs into reportable backend trade-offs.")
    return notes




def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--cost-records", default="configs/cost_records.json")
    parser.add_argument("--output", default="results/raw/backend_comparison.json")
    args = parser.parse_args()

    repo_root = resolved_repo_root(args.repo_root)
    results = resolve_repo_path(repo_root, args.results)
    quality = load_json(results / "backend_quality_eval.json")
    profiling = load_json(results / "profiling_artifacts.json")
    cost_records_path = resolve_repo_path(repo_root, args.cost_records)
    costs = load_json_object_list(cost_records_path, "records")
    records = [
        build_vllm(results, quality, profiling, costs),
        build_trtllm(results, quality, profiling, costs),
        build_llama(results, quality, profiling, costs),
        build_native(results, quality, profiling, costs),
    ]
    for record in records:
        record["readiness_score"] = readiness_score(record)
    payload = {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        **path_payload(args.results, "results_root", "results_root_path_redacted"),
        **path_payload(results, "resolved_results_root"),
        **path_payload(args.cost_records, "cost_records", "cost_records_path_redacted"),
        **path_payload(cost_records_path, "resolved_cost_records"),
        "backends": records,
        "recommendations": recommendation(records),
    }
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")


if __name__ == "__main__":
    main()
