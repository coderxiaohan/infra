#!/usr/bin/env python3
"""Summarize InferLab benchmark/probe JSON files into one JSON artifact."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from artifact_catalog_glue import (
    compose_catalog_extensions,
    resolve_catalog_sources,
    source_title_from_name,
)
from artifact_json import (
    artifact_status_exit_code,
    is_non_success_artifact_status,
    load_json_dict as load_json,
)
from command_redaction import artifact_path_payload as path_payload, redact_path_error
from evidence_projection import (
    CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS,
    DomainFieldSpec,
    NATIVE_CORE_AUDIT_CORE_FIELDS,
    artifact_evidence_projection,
    default_repo_root,
    domain_metric_pairs,
    mode_matches,
    path_label,
    resolve_repo_path,
    summary_domain_metric_pairs,
)
from json_projection import get_path
from text_output import write_text_file


Metric = Tuple[str, Any]


class SourcePayloads(dict):
    def __missing__(self, key: str) -> None:
        return None


def _summary_metric_items(
    data: Dict[str, Any], *field_names: str
) -> List[Metric]:
    return summary_domain_metric_pairs(
        data,
        tuple(DomainFieldSpec(field_name) for field_name in field_names),
    )


EXPECTED_SOURCES = compose_catalog_extensions()


def ratio_to_percent(value: Any) -> Optional[float]:
    if isinstance(value, (int, float)):
        return round(float(value) * 100.0, 4)
    return None


def short_text(value: Any, limit: int = 160) -> Any:
    if not isinstance(value, str):
        return value
    if len(value) <= limit:
        return value
    return value[: limit - 3] + "..."


def named_item(items: Any, name: str) -> Dict[str, Any]:
    if not isinstance(items, list):
        return {}
    for item in items:
        if isinstance(item, dict) and item.get("name") == name:
            return item
    return {}


def build_section(
    title: str,
    source: str,
    data: Optional[Dict[str, Any]],
    metrics: Iterable[Metric],
) -> Dict[str, Any]:
    evidence = artifact_evidence_projection(data, source)
    if data is None:
        return {
            "title": title,
            "source": source,
            "status": "missing",
            "evidence": evidence,
            "metrics": [
                {"name": "status", "value": "missing source file"},
                {
                    "name": "evidence_confidence",
                    "value": evidence["confidence"],
                },
            ],
        }
    if data.get("status") in {
        "invalid_artifact",
        "invalid_encoding",
        "invalid_json",
        "path_probe_failed",
        "unreadable_json",
    }:
        status = str(data.get("status"))
        return {
            "title": title,
            "source": source,
            "status": status,
            "evidence": evidence,
            "metrics": [
                {"name": "status", "value": status},
                {"name": "error", "value": data.get("error")},
                {"name": "path", "value": data.get("path")},
                {
                    "name": "evidence_confidence",
                    "value": evidence["confidence"],
                },
            ],
        }
    section_metrics = [{"name": name, "value": value} for name, value in metrics]
    if not any(
        metric.get("name") == "evidence_confidence"
        for metric in section_metrics
    ):
        section_metrics.append(
            {"name": "evidence_confidence", "value": evidence["confidence"]}
        )
    artifact_status = data.get("status")
    section_status = (
        str(artifact_status)
        if is_non_success_artifact_status(artifact_status)
        else "ok"
    )
    return {
        "title": title,
        "source": source,
        "status": section_status,
        "evidence": evidence,
        "metrics": section_metrics,
    }


def system_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        commit = get_path(data, ("git", "commit"))
        metrics = [
            ("git_branch", get_path(data, ("git", "branch"))),
            ("git_commit_short", commit[:12] if isinstance(commit, str) else commit),
            ("python", data.get("python")),
            ("cmake", get_path(data, ("tools", "cmake"))),
            ("nvidia_smi", get_path(data, ("tools", "nvidia_smi"))),
        ]
    return build_section("System", "system_info.json", data, metrics)


def cuda_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("cuda_available", data.get("cuda_available")),
            ("nvcc", get_path(data, ("tools", "nvcc"))),
            ("nvidia_smi", get_path(data, ("tools", "nvidia_smi"))),
            ("cmake_cuda_configure_attempted", get_path(data, ("cmake_cuda_configure", "attempted"))),
            ("cmake_cuda_configure_returncode", get_path(data, ("cmake_cuda_configure", "returncode"))),
            ("cmake_cuda_configure_summary", get_path(data, ("cmake_cuda_configure", "summary"))),
        ]
    return build_section("CUDA Preflight", "cuda_environment.json", data, metrics)


def vllm_serving_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("model", data.get("model")),
            ("base_url", data.get("base_url")),
            ("prompt_count", data.get("prompt_count")),
            ("vllm_available", get_path(data, ("environment", "vllm", "available"))),
            ("gpu_available", get_path(data, ("environment", "gpu_available"))),
            (
                "can_run_local_vllm",
                get_path(data, ("preflight", "can_run_local_vllm")),
            ),
            (
                "can_benchmark_external_endpoint",
                get_path(data, ("preflight", "can_benchmark_external_endpoint")),
            ),
            ("preflight_reasons", get_path(data, ("preflight", "reasons"))),
            ("request_count", get_path(data, ("summary", "request_count"))),
            ("completed_requests", get_path(data, ("summary", "completed_requests"))),
            ("failed_requests", get_path(data, ("summary", "failed_requests"))),
            ("avg_latency_ms", get_path(data, ("summary", "avg_latency_ms"))),
            ("median_latency_ms", get_path(data, ("summary", "median_latency_ms"))),
            ("p90_latency_ms", get_path(data, ("summary", "p90_latency_ms"))),
            (
                "request_throughput_per_s",
                get_path(data, ("summary", "request_throughput_per_s")),
            ),
            (
                "completion_token_throughput_per_s",
                get_path(data, ("summary", "completion_token_throughput_per_s")),
            ),
        ]
    return build_section(
        "vLLM Serving Benchmark",
        "vllm_serving_benchmark.json",
        data,
        metrics,
    )


def llama_cpp_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("backend_label", data.get("backend_label")),
            ("model", data.get("model")),
            ("llama_cli", get_path(data, ("environment", "tools", "llama_cli", "path"))),
            ("llama_bench", get_path(data, ("environment", "tools", "llama_bench", "path"))),
            ("can_run_cli", get_path(data, ("preflight", "can_run_cli"))),
            ("can_run_bench", get_path(data, ("preflight", "can_run_bench"))),
            ("preflight_reasons", get_path(data, ("preflight", "reasons"))),
            ("threads", get_path(data, ("config", "threads"))),
            ("ctx_size", get_path(data, ("config", "ctx_size"))),
            ("batch_size", get_path(data, ("config", "batch_size"))),
            ("n_predict", get_path(data, ("config", "n_predict"))),
            ("ngl", get_path(data, ("config", "ngl"))),
            ("returncode", get_path(data, ("run", "returncode"))),
            ("elapsed_ms", get_path(data, ("run", "elapsed_ms"))),
            ("load_ms", get_path(data, ("summary", "load_ms"))),
            ("prompt_eval_ms", get_path(data, ("summary", "prompt_eval_ms"))),
            ("prompt_eval_tokens", get_path(data, ("summary", "prompt_eval_tokens"))),
            (
                "prompt_eval_tokens_per_s",
                get_path(data, ("summary", "prompt_eval_tokens_per_s")),
            ),
            ("eval_ms", get_path(data, ("summary", "eval_ms"))),
            ("eval_tokens", get_path(data, ("summary", "eval_tokens"))),
            ("eval_tokens_per_s", get_path(data, ("summary", "eval_tokens_per_s"))),
            ("total_ms", get_path(data, ("summary", "total_ms"))),
        ]
    return build_section(
        "llama.cpp Edge Benchmark",
        "llama_cpp_benchmark.json",
        data,
        metrics,
    )


def trtllm_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("requested_mode", data.get("requested_mode")),
            ("model", data.get("model")),
            ("engine_dir", data.get("engine_dir")),
            ("tensorrt_llm_available", get_path(data, ("preflight", "tensorrt_llm_available"))),
            ("tensorrt_available", get_path(data, ("preflight", "tensorrt_available"))),
            ("gpu_available", get_path(data, ("preflight", "gpu_available"))),
            ("engine_dir_exists", get_path(data, ("preflight", "engine_dir_exists"))),
            ("can_run_command", get_path(data, ("preflight", "can_run_command"))),
            ("can_run_trtllm_bench", get_path(data, ("preflight", "can_run_trtllm_bench"))),
            ("preflight_reasons", get_path(data, ("preflight", "reasons"))),
            (
                "trtllm_bench",
                get_path(data, ("environment", "tools", "trtllm_bench", "path")),
            ),
            (
                "trtllm_build",
                get_path(data, ("environment", "tools", "trtllm_build", "path")),
            ),
            (
                "trtllm_serve",
                get_path(data, ("environment", "tools", "trtllm_serve", "path")),
            ),
            ("batch_size", get_path(data, ("config", "batch_size"))),
            ("input_tokens", get_path(data, ("config", "input_tokens"))),
            ("output_tokens", get_path(data, ("config", "output_tokens"))),
            ("returncode", get_path(data, ("summary", "returncode"))),
            ("elapsed_ms", get_path(data, ("summary", "elapsed_ms"))),
            ("latency_ms", get_path(data, ("summary", "latency_ms"))),
            ("avg_latency_ms", get_path(data, ("summary", "avg_latency_ms"))),
            ("p50_latency_ms", get_path(data, ("summary", "p50_latency_ms"))),
            ("p90_latency_ms", get_path(data, ("summary", "p90_latency_ms"))),
            ("p95_latency_ms", get_path(data, ("summary", "p95_latency_ms"))),
            ("p99_latency_ms", get_path(data, ("summary", "p99_latency_ms"))),
            (
                "token_throughput_per_s",
                get_path(data, ("summary", "token_throughput_per_s")),
            ),
            (
                "request_throughput_per_s",
                get_path(data, ("summary", "request_throughput_per_s")),
            ),
            ("throughput_per_s", get_path(data, ("summary", "throughput_per_s"))),
        ]
    return build_section(
        "TensorRT-LLM Benchmark",
        "trtllm_benchmark.json",
        data,
        metrics,
    )


def shape_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("batch_size", get_path(data, ("config", "batch_size"))),
            ("seq_len", get_path(data, ("config", "seq_len"))),
            ("decode_steps", get_path(data, ("config", "decode_steps"))),
            ("hidden_size", get_path(data, ("config", "hidden_size"))),
            ("num_layers", get_path(data, ("config", "num_layers"))),
            ("num_heads", get_path(data, ("config", "num_heads"))),
            ("prefill_kv_mib", get_path(data, ("prefill", "kv_cache", "total_mib"))),
            ("final_decode_kv_mib", get_path(data, ("decode", -1, "kv_total_mib"))),
        ]
    return build_section(title, source, data, metrics)


def mini_transformer_cache_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        steps = data.get("steps")
        last_step = steps[-1] if isinstance(steps, list) and steps else {}
        metrics = [
            ("generated_ids_match", data.get("generated_ids_match")),
            ("prompt_tokens", len(data.get("prompt_ids", []))),
            ("max_new_tokens", data.get("max_new_tokens")),
            ("hidden_size", get_path(data, ("config", "hidden_size"))),
            ("num_layers", get_path(data, ("config", "num_layers"))),
            ("num_heads", get_path(data, ("config", "num_heads"))),
            ("cached_input_tokens", get_path(data, ("totals", "cached_input_tokens"))),
            ("recompute_input_tokens", get_path(data, ("totals", "recompute_input_tokens"))),
            ("input_token_savings", get_path(data, ("totals", "input_token_savings"))),
            (
                "input_token_savings_percent",
                ratio_to_percent(get_path(data, ("totals", "input_token_savings_ratio"))),
            ),
            (
                "cached_attention_score_elements",
                get_path(data, ("totals", "cached_attention_score_elements")),
            ),
            (
                "recompute_attention_score_elements",
                get_path(data, ("totals", "recompute_attention_score_elements")),
            ),
            (
                "attention_score_element_savings",
                get_path(data, ("totals", "attention_score_element_savings")),
            ),
            (
                "attention_score_element_savings_percent",
                ratio_to_percent(get_path(data, ("totals", "attention_score_element_savings_ratio"))),
            ),
            (
                "cached_max_kv_cache_total_bytes",
                get_path(data, ("totals", "cached_max_kv_cache_total_bytes")),
            ),
            (
                "recompute_max_kv_cache_total_bytes",
                get_path(data, ("totals", "recompute_max_kv_cache_total_bytes")),
            ),
            (
                "last_cached_kv_cache_lengths",
                last_step.get("cached_kv_cache_lengths") if isinstance(last_step, dict) else None,
            ),
        ]
    return build_section(
        "Mini Transformer Cache Compare",
        "mini_transformer_cache_compare.json",
        data,
        metrics,
    )


def mini_transformer_workload_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        cached = data.get("cached_decode") if isinstance(data.get("cached_decode"), dict) else {}
        recompute = data.get("full_recompute") if isinstance(data.get("full_recompute"), dict) else {}
        metrics = [
            ("prompt_tokens", data.get("prompt_tokens")),
            ("max_new_tokens", data.get("max_new_tokens")),
            ("hidden_size", get_path(data, ("config", "hidden_size"))),
            ("num_layers", get_path(data, ("config", "num_layers"))),
            ("num_heads", get_path(data, ("config", "num_heads"))),
            ("dtype_bytes", data.get("dtype_bytes")),
            ("cached_total_flops", cached.get("total_flops")),
            ("recompute_total_flops", recompute.get("total_flops")),
            ("flops_saved", get_path(data, ("comparison", "flops_saved"))),
            (
                "flops_saved_percent",
                ratio_to_percent(get_path(data, ("comparison", "flops_saved_ratio"))),
            ),
            ("cached_total_bytes", cached.get("total_bytes")),
            ("recompute_total_bytes", recompute.get("total_bytes")),
            ("bytes_saved", get_path(data, ("comparison", "bytes_saved"))),
            (
                "bytes_saved_percent",
                ratio_to_percent(get_path(data, ("comparison", "bytes_saved_ratio"))),
            ),
            ("cached_attention_score_elements", cached.get("attention_score_elements")),
            ("recompute_attention_score_elements", recompute.get("attention_score_elements")),
            (
                "attention_score_elements_saved",
                get_path(data, ("comparison", "attention_score_elements_saved")),
            ),
            (
                "attention_score_elements_saved_percent",
                ratio_to_percent(get_path(data, ("comparison", "attention_score_elements_saved_ratio"))),
            ),
            (
                "cached_persistent_kv_cache_bytes",
                get_path(data, ("comparison", "cached_persistent_kv_cache_bytes")),
            ),
        ]
    return build_section(
        "Mini Transformer Workload Profile",
        "mini_transformer_workload_profile.json",
        data,
        metrics,
    )


def mini_transformer_bottleneck_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        cached_top = get_path(data, ("cached_decode", "group_rankings", 0), {})
        recompute_top = get_path(data, ("full_recompute", "group_rankings", 0), {})
        metrics = [
            ("prompt_tokens", data.get("prompt_tokens")),
            ("max_new_tokens", data.get("max_new_tokens")),
            ("hidden_size", get_path(data, ("config", "hidden_size"))),
            ("num_layers", get_path(data, ("config", "num_layers"))),
            ("num_heads", get_path(data, ("config", "num_heads"))),
            ("peak_tflops", get_path(data, ("hardware_model", "peak_tflops"))),
            ("bandwidth_gbps", get_path(data, ("hardware_model", "bandwidth_gbps"))),
            (
                "ridge_point_flops_per_byte",
                get_path(data, ("hardware_model", "ridge_point_flops_per_byte")),
            ),
            (
                "cached_top_bottleneck_group",
                get_path(data, ("cached_decode", "top_bottleneck_group")),
            ),
            (
                "cached_top_roofline_limiter",
                cached_top.get("roofline_limiter") if isinstance(cached_top, dict) else None,
            ),
            (
                "cached_top_arithmetic_intensity",
                cached_top.get("arithmetic_intensity_flops_per_byte")
                if isinstance(cached_top, dict)
                else None,
            ),
            (
                "cached_top_estimated_roofline_time_ms",
                cached_top.get("estimated_roofline_time_ms")
                if isinstance(cached_top, dict)
                else None,
            ),
            (
                "recompute_top_bottleneck_group",
                get_path(data, ("full_recompute", "top_bottleneck_group")),
            ),
            (
                "recompute_top_roofline_limiter",
                recompute_top.get("roofline_limiter") if isinstance(recompute_top, dict) else None,
            ),
            (
                "recompute_top_arithmetic_intensity",
                recompute_top.get("arithmetic_intensity_flops_per_byte")
                if isinstance(recompute_top, dict)
                else None,
            ),
            (
                "recompute_top_estimated_roofline_time_ms",
                recompute_top.get("estimated_roofline_time_ms")
                if isinstance(recompute_top, dict)
                else None,
            ),
            ("flops_saved", get_path(data, ("workload_comparison", "flops_saved"))),
            (
                "flops_saved_percent",
                ratio_to_percent(get_path(data, ("workload_comparison", "flops_saved_ratio"))),
            ),
        ]
    return build_section(
        "Mini Transformer Bottleneck Advisor",
        "mini_transformer_bottleneck_advisor.json",
        data,
        metrics,
    )


def mini_transformer_scenario_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        scenarios = data.get("scenarios")
        scenario_count = len(scenarios) if isinstance(scenarios, list) else data.get("scenario_count")
        flops_rank = get_path(data, ("rankings", "by_flops_saved_ratio", 0), {})
        bytes_rank = get_path(data, ("rankings", "by_bytes_saved_ratio", 0), {})
        kv_rank = get_path(data, ("rankings", "by_cached_persistent_kv_cache_bytes", 0), {})
        attention_rank = get_path(data, ("rankings", "by_attention_score_elements_saved_ratio", 0), {})
        metrics = [
            ("scenario_count", scenario_count),
            ("hidden_size", get_path(data, ("config", "hidden_size"))),
            ("num_layers", get_path(data, ("config", "num_layers"))),
            ("num_heads", get_path(data, ("config", "num_heads"))),
            ("peak_tflops", get_path(data, ("hardware_model", "peak_tflops"))),
            ("bandwidth_gbps", get_path(data, ("hardware_model", "bandwidth_gbps"))),
            (
                "top_flops_saved_scenario",
                flops_rank.get("name") if isinstance(flops_rank, dict) else None,
            ),
            (
                "top_flops_saved_percent",
                ratio_to_percent(flops_rank.get("flops_saved_ratio"))
                if isinstance(flops_rank, dict)
                else None,
            ),
            (
                "top_bytes_saved_scenario",
                bytes_rank.get("name") if isinstance(bytes_rank, dict) else None,
            ),
            (
                "top_bytes_saved_percent",
                ratio_to_percent(bytes_rank.get("bytes_saved_ratio"))
                if isinstance(bytes_rank, dict)
                else None,
            ),
            (
                "top_attention_saved_scenario",
                attention_rank.get("name") if isinstance(attention_rank, dict) else None,
            ),
            (
                "top_attention_saved_percent",
                ratio_to_percent(attention_rank.get("attention_score_elements_saved_ratio"))
                if isinstance(attention_rank, dict)
                else None,
            ),
            (
                "top_persistent_kv_scenario",
                kv_rank.get("name") if isinstance(kv_rank, dict) else None,
            ),
            (
                "top_persistent_kv_bytes",
                kv_rank.get("cached_persistent_kv_cache_bytes")
                if isinstance(kv_rank, dict)
                else None,
            ),
            (
                "top_persistent_kv_bottleneck_group",
                kv_rank.get("cached_top_bottleneck_group") if isinstance(kv_rank, dict) else None,
            ),
        ]
    return build_section(
        "Mini Transformer Scenario Sweep",
        "mini_transformer_scenario_sweep.json",
        data,
        metrics,
    )


def native_probe_run_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        covered_probes = data.get("covered_probes")
        covered_probe_artifact_count = (
            len(covered_probes) if isinstance(covered_probes, list) else None
        )
        metrics = [
            ("passed", get_path(data, ("counts", "passed"))),
            ("skipped", get_path(data, ("counts", "skipped"))),
            ("failed", get_path(data, ("counts", "failed"))),
            ("total_elapsed_ms", get_path(data, ("timing", "total_elapsed_ms"))),
            ("slowest_probe", get_path(data, ("timing", "slowest_probe"))),
            ("slowest_probe_elapsed_ms", get_path(data, ("timing", "slowest_probe_elapsed_ms"))),
            ("build_dir", data.get("build_dir")),
            ("coverage_source", data.get("coverage_source")),
            ("coverage_manifest", data.get("coverage_manifest")),
            ("covered_probe_count", data.get("covered_probe_count")),
            ("covered_probe_artifact_count", covered_probe_artifact_count),
        ]
    return build_section("Native Probe Runner", "native_probe_run.json", data, metrics)


def scheduler_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        steps = data.get("steps")
        request_metrics = data.get("requests")
        if isinstance(steps, list):
            total_admitted = sum(int(step.get("admitted", 0)) for step in steps if isinstance(step, dict))
            total_decoded = sum(int(step.get("decoded", 0)) for step in steps if isinstance(step, dict))
            total_finished = sum(int(step.get("finished", 0)) for step in steps if isinstance(step, dict))
            total_rejected = sum(int(step.get("rejected", 0)) for step in steps if isinstance(step, dict))
            total_backpressure_deferred = sum(
                int(step.get("backpressure_deferred", 0))
                for step in steps
                if isinstance(step, dict)
            )
            peak_used_blocks = max(
                (
                    int(get_path(step, ("cache", "used_blocks"), 0))
                    for step in steps
                    if isinstance(step, dict)
                ),
                default=0,
            )
            peak_reserved_bytes = max(
                (
                    int(get_path(step, ("cache", "reserved_bytes"), 0))
                    for step in steps
                    if isinstance(step, dict)
                ),
                default=0,
            )
            final_active = get_path(steps[-1], ("cache", "active_requests")) if steps else None
            metrics = [
                ("steps", len(steps)),
                ("total_admitted", total_admitted),
                ("total_decoded_tokens", total_decoded),
                ("total_finished", total_finished),
                ("total_rejected", total_rejected),
                ("total_backpressure_deferred", total_backpressure_deferred),
                ("peak_used_blocks", peak_used_blocks),
                ("peak_reserved_bytes", peak_reserved_bytes),
                ("final_active_requests", final_active),
            ]
        pressure_profile = data.get("pressure_profile")
        if isinstance(pressure_profile, dict):
            metrics.extend(
                [
                    ("pressure_steps", pressure_profile.get("steps")),
                    ("pressure_request_count", pressure_profile.get("request_count")),
                    ("pressure_finished_request_count", pressure_profile.get("finished_request_count")),
                    ("pressure_rejected_request_count", pressure_profile.get("rejected_request_count")),
                    (
                        "pressure_total_backpressure_deferred",
                        pressure_profile.get("total_backpressure_deferred"),
                    ),
                    ("pressure_peak_active_requests", pressure_profile.get("peak_active_requests")),
                    ("pressure_peak_used_blocks", pressure_profile.get("peak_used_blocks")),
                    ("pressure_peak_reserved_bytes", pressure_profile.get("peak_reserved_bytes")),
                    ("pressure_peak_total_tokens", pressure_profile.get("peak_total_tokens")),
                    ("pressure_peak_cache_utilization", pressure_profile.get("peak_cache_utilization")),
                    ("pressure_average_active_requests", pressure_profile.get("average_active_requests")),
                    ("pressure_average_used_blocks", pressure_profile.get("average_used_blocks")),
                    ("pressure_average_cache_utilization", pressure_profile.get("average_cache_utilization")),
                    ("pressure_average_queue_wait_steps", pressure_profile.get("average_queue_wait_steps")),
                    ("pressure_average_ttft_steps", pressure_profile.get("average_ttft_steps")),
                    (
                        "pressure_average_decode_steps_per_token",
                        pressure_profile.get("average_decode_steps_per_token"),
                    ),
                    ("pressure_tokens_per_step", pressure_profile.get("tokens_per_step")),
                ]
            )
        if isinstance(request_metrics, list):
            finished = [
                request
                for request in request_metrics
                if isinstance(request, dict) and request.get("state") == "finished"
            ]
            ttft_values = [
                float(request["time_to_first_token_steps"])
                for request in finished
                if isinstance(request.get("time_to_first_token_steps"), (int, float))
                and request["time_to_first_token_steps"] >= 0
            ]
            queue_wait_values = [
                float(request["queue_wait_steps"])
                for request in request_metrics
                if isinstance(request, dict)
                and isinstance(request.get("queue_wait_steps"), (int, float))
                and request["queue_wait_steps"] >= 0
            ]
            decode_step_values = [
                float(request["decode_steps_per_token"])
                for request in finished
                if isinstance(request.get("decode_steps_per_token"), (int, float))
                and request["decode_steps_per_token"] > 0
            ]
            metrics.extend(
                [
                    ("request_count", len(request_metrics)),
                    ("finished_request_count", len(finished)),
                    (
                        "avg_ttft_steps",
                        round(sum(ttft_values) / len(ttft_values), 6) if ttft_values else None,
                    ),
                    (
                        "max_queue_wait_steps",
                        max(queue_wait_values) if queue_wait_values else None,
                    ),
                    (
                        "avg_decode_steps_per_token",
                        round(sum(decode_step_values) / len(decode_step_values), 6)
                        if decode_step_values
                        else None,
                    ),
                ]
            )
    return build_section("Continuous Batching", "scheduler_trace.json", data, metrics)


def scheduler_pressure_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        tradeoff = data.get("tradeoff_summary")
        if isinstance(tradeoff, dict):
            metrics.extend(
                [
                    ("scenario_count", tradeoff.get("scenario_count")),
                    ("best_tokens_per_step_scenario", tradeoff.get("best_tokens_per_step_scenario")),
                    ("best_tokens_per_step", tradeoff.get("best_tokens_per_step")),
                    ("lowest_average_ttft_scenario", tradeoff.get("lowest_average_ttft_scenario")),
                    ("lowest_average_ttft_rejection_filter", tradeoff.get("lowest_average_ttft_rejection_filter")),
                    ("lowest_average_ttft_steps", tradeoff.get("lowest_average_ttft_steps")),
                    ("highest_peak_used_blocks_scenario", tradeoff.get("highest_peak_used_blocks_scenario")),
                    ("highest_peak_used_blocks", tradeoff.get("highest_peak_used_blocks")),
                    ("max_rejected_request_scenario", tradeoff.get("max_rejected_request_scenario")),
                    ("max_rejected_requests", tradeoff.get("max_rejected_requests")),
                    ("total_rejected_requests", tradeoff.get("total_rejected_requests")),
                ]
            )
        scenarios = data.get("scenarios")
        if isinstance(scenarios, list):
            for scenario in scenarios:
                if not isinstance(scenario, dict):
                    continue
                name = scenario.get("name")
                profile = scenario.get("pressure_profile")
                if not isinstance(name, str) or not isinstance(profile, dict):
                    continue
                metrics.extend(
                    [
                        (f"{name}_tokens_per_step", profile.get("tokens_per_step")),
                        (f"{name}_average_ttft_steps", profile.get("average_ttft_steps")),
                        (f"{name}_peak_used_blocks", profile.get("peak_used_blocks")),
                        (f"{name}_peak_cache_utilization", profile.get("peak_cache_utilization")),
                        (f"{name}_rejected_requests", profile.get("rejected_request_count")),
                    ]
                )
    return build_section("Scheduler Pressure Sweep", "scheduler_pressure_sweep.json", data, metrics)


def scheduler_workload_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        config = data.get("scheduler_config")
        if isinstance(config, dict):
            metrics.extend(
                [
                    ("config_max_active_requests", config.get("max_active_requests")),
                    ("config_num_blocks", config.get("num_blocks")),
                    ("config_block_size_tokens", config.get("block_size_tokens")),
                ]
            )
        tradeoff = data.get("tradeoff_summary")
        if isinstance(tradeoff, dict):
            metrics.extend(
                [
                    ("workload_count", tradeoff.get("workload_count")),
                    ("best_tokens_per_step_workload", tradeoff.get("best_tokens_per_step_workload")),
                    ("best_tokens_per_step", tradeoff.get("best_tokens_per_step")),
                    ("lowest_average_ttft_workload", tradeoff.get("lowest_average_ttft_workload")),
                    ("lowest_average_ttft_rejection_filter", tradeoff.get("lowest_average_ttft_rejection_filter")),
                    ("lowest_average_ttft_steps", tradeoff.get("lowest_average_ttft_steps")),
                    ("highest_average_queue_wait_workload", tradeoff.get("highest_average_queue_wait_workload")),
                    ("highest_average_queue_wait_steps", tradeoff.get("highest_average_queue_wait_steps")),
                    ("highest_peak_used_blocks_workload", tradeoff.get("highest_peak_used_blocks_workload")),
                    ("highest_peak_used_blocks", tradeoff.get("highest_peak_used_blocks")),
                    ("max_rejected_request_workload", tradeoff.get("max_rejected_request_workload")),
                    ("max_rejected_requests", tradeoff.get("max_rejected_requests")),
                    ("total_rejected_requests", tradeoff.get("total_rejected_requests")),
                ]
            )
        workloads = data.get("workloads")
        if isinstance(workloads, list):
            for workload in workloads:
                if not isinstance(workload, dict):
                    continue
                name = workload.get("name")
                profile = workload.get("pressure_profile")
                if not isinstance(name, str) or not isinstance(profile, dict):
                    continue
                metrics.extend(
                    [
                        (f"{name}_tokens_per_step", profile.get("tokens_per_step")),
                        (f"{name}_average_ttft_steps", profile.get("average_ttft_steps")),
                        (f"{name}_average_queue_wait_steps", profile.get("average_queue_wait_steps")),
                        (f"{name}_peak_used_blocks", profile.get("peak_used_blocks")),
                        (f"{name}_peak_cache_utilization", profile.get("peak_cache_utilization")),
                        (f"{name}_rejected_requests", profile.get("rejected_request_count")),
                    ]
                )
    return build_section("Scheduler Workload Sweep", "scheduler_workload_sweep.json", data, metrics)


def scheduler_capacity_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        config = data.get("sweep_config")
        if isinstance(config, dict):
            metrics.extend(
                [
                    ("config_max_active_requests", config.get("max_active_requests")),
                    ("config_block_size_tokens", config.get("block_size_tokens")),
                    ("config_capacity_points", config.get("capacity_points")),
                ]
            )
        threshold = data.get("threshold_summary")
        if isinstance(threshold, dict):
            metrics.extend(
                [
                    ("capacity_point_count", threshold.get("capacity_point_count")),
                    ("minimum_no_rejection_blocks", threshold.get("minimum_no_rejection_blocks")),
                    ("minimum_no_rejection_tokens_per_step", threshold.get("minimum_no_rejection_tokens_per_step")),
                    ("best_no_rejection_blocks", threshold.get("best_no_rejection_blocks")),
                    ("best_no_rejection_tokens_per_step", threshold.get("best_no_rejection_tokens_per_step")),
                    ("highest_tokens_per_step_blocks", threshold.get("highest_tokens_per_step_blocks")),
                    ("highest_tokens_per_step", threshold.get("highest_tokens_per_step")),
                    ("highest_peak_cache_utilization_blocks", threshold.get("highest_peak_cache_utilization_blocks")),
                    ("highest_peak_cache_utilization", threshold.get("highest_peak_cache_utilization")),
                    ("max_rejected_blocks", threshold.get("max_rejected_blocks")),
                    ("max_rejected_requests", threshold.get("max_rejected_requests")),
                    ("total_rejected_requests", threshold.get("total_rejected_requests")),
                ]
            )
        capacity_points = data.get("capacity_points")
        if isinstance(capacity_points, list):
            for point in capacity_points:
                if not isinstance(point, dict):
                    continue
                num_blocks = point.get("num_blocks")
                profile = point.get("pressure_profile")
                if not isinstance(num_blocks, int) or not isinstance(profile, dict):
                    continue
                prefix = f"blocks_{num_blocks}"
                metrics.extend(
                    [
                        (f"{prefix}_tokens_per_step", profile.get("tokens_per_step")),
                        (f"{prefix}_average_ttft_steps", profile.get("average_ttft_steps")),
                        (f"{prefix}_peak_used_blocks", profile.get("peak_used_blocks")),
                        (f"{prefix}_peak_cache_utilization", profile.get("peak_cache_utilization")),
                        (f"{prefix}_rejected_requests", profile.get("rejected_request_count")),
                    ]
                )
    return build_section("Scheduler Capacity Sweep", "scheduler_capacity_sweep.json", data, metrics)


def scheduler_admission_policy_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        config = data.get("scheduler_config")
        if isinstance(config, dict):
            metrics.extend(
                [
                    ("config_max_active_requests", config.get("max_active_requests")),
                    ("config_num_blocks", config.get("num_blocks")),
                    ("config_block_size_tokens", config.get("block_size_tokens")),
                ]
            )
        tradeoff = data.get("tradeoff_summary")
        if isinstance(tradeoff, dict):
            metrics.extend(
                [
                    ("policy_count", tradeoff.get("policy_count")),
                    ("best_finished_policy", tradeoff.get("best_finished_policy")),
                    ("best_finished_request_count", tradeoff.get("best_finished_request_count")),
                    ("best_tokens_per_step_policy", tradeoff.get("best_tokens_per_step_policy")),
                    ("best_tokens_per_step", tradeoff.get("best_tokens_per_step")),
                    ("lowest_average_ttft_policy", tradeoff.get("lowest_average_ttft_policy")),
                    ("lowest_average_ttft_steps", tradeoff.get("lowest_average_ttft_steps")),
                    ("lowest_average_queue_wait_policy", tradeoff.get("lowest_average_queue_wait_policy")),
                    ("lowest_average_queue_wait_steps", tradeoff.get("lowest_average_queue_wait_steps")),
                    ("max_rejected_policy", tradeoff.get("max_rejected_policy")),
                    ("max_rejected_requests", tradeoff.get("max_rejected_requests")),
                    ("total_rejected_requests", tradeoff.get("total_rejected_requests")),
                ]
            )
        policy_results = data.get("policy_results")
        if isinstance(policy_results, list):
            for result in policy_results:
                if not isinstance(result, dict):
                    continue
                policy = result.get("policy")
                profile = result.get("pressure_profile")
                if not isinstance(policy, str) or not isinstance(profile, dict):
                    continue
                metrics.extend(
                    [
                        (f"{policy}_tokens_per_step", profile.get("tokens_per_step")),
                        (f"{policy}_finished_requests", profile.get("finished_request_count")),
                        (f"{policy}_rejected_requests", profile.get("rejected_request_count")),
                        (f"{policy}_average_ttft_steps", profile.get("average_ttft_steps")),
                        (f"{policy}_average_queue_wait_steps", profile.get("average_queue_wait_steps")),
                        (f"{policy}_peak_used_blocks", profile.get("peak_used_blocks")),
                    ]
                )
    return build_section(
        "Scheduler Admission Policy Sweep",
        "scheduler_admission_policy_sweep.json",
        data,
        metrics,
    )


def scheduler_runtime_policy_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        config = data.get("scheduler_config")
        if isinstance(config, dict):
            metrics.extend(
                [
                    ("config_num_blocks", config.get("num_blocks")),
                    ("config_block_size_tokens", config.get("block_size_tokens")),
                ]
            )
        tradeoff = data.get("tradeoff_summary")
        if isinstance(tradeoff, dict):
            metrics.extend(
                [
                    ("policy_count", tradeoff.get("policy_count")),
                    ("best_finished_policy", tradeoff.get("best_finished_policy")),
                    ("best_finished_request_count", tradeoff.get("best_finished_request_count")),
                    ("best_tokens_per_step_policy", tradeoff.get("best_tokens_per_step_policy")),
                    ("best_tokens_per_step", tradeoff.get("best_tokens_per_step")),
                    ("lowest_average_ttft_policy", tradeoff.get("lowest_average_ttft_policy")),
                    ("lowest_average_ttft_steps", tradeoff.get("lowest_average_ttft_steps")),
                    ("highest_preemption_policy", tradeoff.get("highest_preemption_policy")),
                    ("highest_preemptions", tradeoff.get("highest_preemptions")),
                    (
                        "highest_backpressure_deferral_policy",
                        tradeoff.get("highest_backpressure_deferral_policy"),
                    ),
                    ("highest_backpressure_deferrals", tradeoff.get("highest_backpressure_deferrals")),
                    ("total_preemptions", tradeoff.get("total_preemptions")),
                    ("total_recompute_tokens", tradeoff.get("total_recompute_tokens")),
                    ("total_backpressure_deferrals", tradeoff.get("total_backpressure_deferrals")),
                ]
            )
        policy_results = data.get("policy_results")
        if isinstance(policy_results, list):
            for result in policy_results:
                if not isinstance(result, dict):
                    continue
                policy_config = result.get("policy_config")
                profile = result.get("pressure_profile")
                if not isinstance(policy_config, dict) or not isinstance(profile, dict):
                    continue
                policy = policy_config.get("name")
                if not isinstance(policy, str):
                    continue
                metrics.extend(
                    [
                        (f"{policy}_tokens_per_step", profile.get("tokens_per_step")),
                        (f"{policy}_finished_requests", profile.get("finished_request_count")),
                        (f"{policy}_rejected_requests", profile.get("rejected_request_count")),
                        (f"{policy}_average_ttft_steps", profile.get("average_ttft_steps")),
                        (f"{policy}_average_queue_wait_steps", profile.get("average_queue_wait_steps")),
                        (f"{policy}_peak_used_blocks", profile.get("peak_used_blocks")),
                        (f"{policy}_append_pressure_events", result.get("append_pressure_events")),
                        (f"{policy}_total_preemptions", result.get("total_preemptions")),
                        (
                            f"{policy}_total_backpressure_deferrals",
                            result.get("total_backpressure_deferrals"),
                        ),
                        (f"{policy}_total_recompute_tokens", result.get("total_recompute_tokens")),
                    ]
                )
    return build_section(
        "Scheduler Runtime Policy Sweep",
        "scheduler_runtime_policy_sweep.json",
        data,
        metrics,
    )


def scheduler_fairness_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        config = data.get("scheduler_config")
        if isinstance(config, dict):
            metrics.extend(
                [
                    ("config_max_active_requests", config.get("max_active_requests")),
                    ("config_num_blocks", config.get("num_blocks")),
                    ("config_block_size_tokens", config.get("block_size_tokens")),
                    (
                        "config_starvation_guard_threshold_steps",
                        config.get("starvation_guard_threshold_steps"),
                    ),
                    ("config_starvation_threshold_steps", config.get("starvation_threshold_steps")),
                ]
            )
        tradeoff = data.get("tradeoff_summary")
        if isinstance(tradeoff, dict):
            metrics.extend(
                [
                    ("policy_count", tradeoff.get("policy_count")),
                    ("balanced_jain_floor", tradeoff.get("balanced_jain_floor")),
                    ("balanced_policy", tradeoff.get("balanced_policy")),
                    (
                        "balanced_policy_average_queue_wait_steps",
                        tradeoff.get("balanced_policy_average_queue_wait_steps"),
                    ),
                    (
                        "balanced_policy_jain_queue_wait_fairness",
                        tradeoff.get("balanced_policy_jain_queue_wait_fairness"),
                    ),
                    ("best_tokens_per_step_policy", tradeoff.get("best_tokens_per_step_policy")),
                    ("best_tokens_per_step", tradeoff.get("best_tokens_per_step")),
                    ("highest_jain_fairness_policy", tradeoff.get("highest_jain_fairness_policy")),
                    ("highest_jain_queue_wait_fairness", tradeoff.get("highest_jain_queue_wait_fairness")),
                    ("lowest_average_ttft_policy", tradeoff.get("lowest_average_ttft_policy")),
                    ("lowest_average_ttft_steps", tradeoff.get("lowest_average_ttft_steps")),
                    ("lowest_starvation_policy", tradeoff.get("lowest_starvation_policy")),
                    ("lowest_starved_request_count", tradeoff.get("lowest_starved_request_count")),
                    (
                        "lowest_starvation_max_queue_wait_steps",
                        tradeoff.get("lowest_starvation_max_queue_wait_steps"),
                    ),
                    ("worst_starvation_policy", tradeoff.get("worst_starvation_policy")),
                    ("worst_starved_request_count", tradeoff.get("worst_starved_request_count")),
                    (
                        "worst_starvation_max_queue_wait_steps",
                        tradeoff.get("worst_starvation_max_queue_wait_steps"),
                    ),
                ]
            )
        policy_results = data.get("policy_results")
        if isinstance(policy_results, list):
            for result in policy_results:
                if not isinstance(result, dict):
                    continue
                policy = result.get("policy")
                profile = result.get("pressure_profile")
                fairness = result.get("fairness")
                if not isinstance(policy, str):
                    continue
                if isinstance(profile, dict):
                    metrics.extend(
                        [
                            (f"{policy}_tokens_per_step", profile.get("tokens_per_step")),
                            (f"{policy}_average_ttft_steps", profile.get("average_ttft_steps")),
                            (f"{policy}_finished_requests", profile.get("finished_request_count")),
                            (f"{policy}_rejected_requests", profile.get("rejected_request_count")),
                        ]
                    )
                if isinstance(fairness, dict):
                    metrics.extend(
                        [
                            (f"{policy}_starved_request_count", fairness.get("starved_request_count")),
                            (f"{policy}_max_queue_wait_steps", fairness.get("max_queue_wait_steps")),
                            (
                                f"{policy}_average_queue_wait_steps",
                                fairness.get("average_queue_wait_steps"),
                            ),
                            (
                                f"{policy}_queue_wait_stddev_steps",
                                fairness.get("queue_wait_stddev_steps"),
                            ),
                            (
                                f"{policy}_jain_queue_wait_fairness",
                                fairness.get("jain_queue_wait_fairness"),
                            ),
                            (
                                f"{policy}_worst_queue_wait_request_id",
                                fairness.get("worst_queue_wait_request_id"),
                            ),
                        ]
                    )
    return build_section(
        "Scheduler Fairness Sweep",
        "scheduler_fairness_sweep.json",
        data,
        metrics,
    )


def kv_allocation_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("static_reserved_bytes", get_path(data, ("static", "reserved_bytes"))),
            ("static_utilization_percent", ratio_to_percent(get_path(data, ("static", "utilization")))),
            ("paged_reserved_bytes", get_path(data, ("paged", "reserved_bytes"))),
            ("paged_utilization_percent", ratio_to_percent(get_path(data, ("paged", "utilization")))),
            ("reserved_bytes_saved", get_path(data, ("savings", "reserved_bytes_delta"))),
            ("paged_reserved_ratio_percent", ratio_to_percent(get_path(data, ("savings", "reserved_bytes_ratio")))),
        ]
    return build_section("Static vs Paged KV", "kv_allocation_compare.json", data, metrics)


def prefix_cache_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("no_prefix_reserved_bytes", get_path(data, ("no_prefix_cache", "reserved_bytes"))),
            ("shared_prefix_reserved_bytes", get_path(data, ("shared_prefix", "reserved_bytes"))),
            ("reserved_bytes_saved", get_path(data, ("savings", "reserved_bytes_delta"))),
            ("shared_prefix_reserved_ratio_percent", ratio_to_percent(get_path(data, ("savings", "reserved_bytes_ratio")))),
            ("shared_prefix_used_blocks", get_path(data, ("shared_prefix", "used_blocks"))),
            (
                "traffic_no_prefix_prefill_write_bytes",
                get_path(data, ("traffic_profile", "no_prefix_prefill_write_bytes")),
            ),
            (
                "traffic_shared_prefix_prefill_write_bytes",
                get_path(data, ("traffic_profile", "shared_prefix_prefill_write_bytes")),
            ),
            (
                "traffic_prefix_write_bytes_saved",
                get_path(data, ("traffic_profile", "prefix_write_bytes_saved")),
            ),
            (
                "traffic_physical_tokens_saved",
                get_path(data, ("traffic_profile", "physical_tokens_saved")),
            ),
            (
                "traffic_decode_attention_kv_read_bytes_per_step",
                get_path(data, ("traffic_profile", "decode_attention_kv_read_bytes_per_step")),
            ),
            (
                "traffic_decode_shared_prefix_read_bytes_per_step",
                get_path(data, ("traffic_profile", "decode_attention_shared_prefix_read_bytes_per_step")),
            ),
            (
                "traffic_shared_prefix_entry_block_table_entries",
                get_path(data, ("traffic_profile", "shared_prefix_entry_block_table_entries")),
            ),
        ]
    return build_section("Prefix Cache Sharing", "prefix_cache_compare.json", data, metrics)


def kv_fragmentation_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("num_blocks", get_path(data, ("config", "num_blocks"))),
            ("block_size_tokens", get_path(data, ("config", "block_size_tokens"))),
            ("after_holes_free_blocks", get_path(data, ("fragmentation", "after_holes_free_blocks"))),
            ("after_reuse_used_blocks", get_path(data, ("fragmentation", "after_reuse_used_blocks"))),
            ("reused_released_block_count", get_path(data, ("fragmentation", "reused_released_block_count"))),
            ("failed_allocations", get_path(data, ("fragmentation", "failed_allocations"))),
            ("final_free_blocks", get_path(data, ("fragmentation", "final_free_blocks"))),
            ("final_used_blocks", get_path(data, ("fragmentation", "final_used_blocks"))),
        ]
    return build_section("Paged KV Fragmentation", "kv_fragmentation_probe.json", data, metrics)


def paged_kv_locality_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("token_count", get_path(data, ("config", "token_count"))),
            ("logical_blocks", get_path(data, ("locality", "fragmented", "logical_blocks"))),
            ("fragmented_non_contiguous_transitions", get_path(data, ("locality", "fragmented", "non_contiguous_transitions"))),
            ("fragmented_max_physical_jump", get_path(data, ("locality", "fragmented", "max_physical_jump"))),
            ("contiguous_span_amplification", get_path(data, ("locality", "contiguous", "span_element_amplification"))),
            ("fragmented_span_amplification", get_path(data, ("locality", "fragmented", "span_element_amplification"))),
            ("span_amplification_delta", get_path(data, ("locality", "fragmented_vs_contiguous_span_amplification_delta"))),
            ("fragmented_block_amplification", get_path(data, ("locality", "fragmented", "block_element_amplification"))),
        ]
    return build_section("Paged KV Locality", "paged_kv_locality_probe.json", data, metrics)


def quantization_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("rows", get_path(data, ("config", "rows"))),
            ("cols", get_path(data, ("config", "cols"))),
            ("group_size", get_path(data, ("config", "group_size"))),
            ("int8_storage_ratio_percent", ratio_to_percent(get_path(data, ("quantization", "int8", "storage_ratio_vs_fp32")))),
            ("int8_mean_abs_error", get_path(data, ("quantization", "int8", "mean_abs_error"))),
            ("int4_storage_ratio_percent", ratio_to_percent(get_path(data, ("quantization", "int4", "storage_ratio_vs_fp32")))),
            ("int4_mean_abs_error", get_path(data, ("quantization", "int4", "mean_abs_error"))),
            ("int4_fused_vs_materialized_max_abs_error", get_path(data, ("weight_only_matvec", "int4_fused_vs_materialized_max_abs_error"))),
        ]
    return build_section("Weight-Only Quantization", "quantization_probe.json", data, metrics)


def quantization_sweep_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        rows = data.get("int4_group_sweep")
        if isinstance(rows, list) and rows:
            best_storage = min(rows, key=lambda row: row.get("storage_ratio_vs_fp32", float("inf")))
            best_error = min(rows, key=lambda row: row.get("mean_abs_error", float("inf")))
            metrics = [
                ("sweep_points", len(rows)),
                ("best_storage_group_size", best_storage.get("group_size")),
                ("best_storage_ratio_percent", ratio_to_percent(best_storage.get("storage_ratio_vs_fp32"))),
                ("best_error_group_size", best_error.get("group_size")),
                ("best_mean_abs_error", best_error.get("mean_abs_error")),
            ]
    return build_section("INT4 Group Sweep", "quantization_sweep.json", data, metrics)


def quantization_channel_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("per_channel_storage_ratio_percent", ratio_to_percent(get_path(data, ("comparison", "per_channel", "storage_ratio_vs_fp32")))),
            ("per_channel_mean_abs_error", get_path(data, ("comparison", "per_channel", "mean_abs_error"))),
            ("per_group_storage_ratio_percent", ratio_to_percent(get_path(data, ("comparison", "per_group", "storage_ratio_vs_fp32")))),
            ("per_group_mean_abs_error", get_path(data, ("comparison", "per_group", "mean_abs_error"))),
            ("per_group_size", get_path(data, ("config", "per_group_size"))),
        ]
    return build_section("Per-Channel vs Per-Group", "quantization_channel_compare.json", data, metrics)


def attention_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("query_length", get_path(data, ("attention", "query_length"))),
            ("key_length", get_path(data, ("attention", "key_length"))),
            ("num_heads", get_path(data, ("attention", "num_heads"))),
            ("head_dim", get_path(data, ("attention", "head_dim"))),
            ("score_elements", get_path(data, ("attention", "score_elements"))),
            (
                "non_causal_kv_element_reads_dense",
                get_path(data, ("attention", "profile", "non_causal", "read_profile", "kv_element_reads_dense")),
            ),
            (
                "non_causal_intermediate_bytes",
                get_path(data, ("attention", "profile", "non_causal", "read_profile", "total_intermediate_bytes")),
            ),
            (
                "causal_mask_sparsity",
                get_path(data, ("attention", "profile", "causal_square", "read_profile", "causal_mask_sparsity")),
            ),
            (
                "causal_value_reads_dense",
                get_path(data, ("attention", "profile", "causal_square", "read_profile", "value_element_reads_dense")),
            ),
            (
                "causal_value_reads_mask_aware",
                get_path(data, ("attention", "profile", "causal_square", "read_profile", "value_element_reads_mask_aware")),
            ),
            ("output_checksum", get_path(data, ("attention", "output_sum_checksum"))),
        ]
    return build_section("Attention Reference", "attention_probe.json", data, metrics)


def matrix_ops_profile_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        root = ("matrix_ops_profile",)
        metrics = [
            ("matmul_m", get_path(data, root + ("matmul", "m"))),
            ("matmul_k", get_path(data, root + ("matmul", "k"))),
            ("matmul_n", get_path(data, root + ("matmul", "n"))),
            ("matmul_flop_count", get_path(data, root + ("matmul", "flop_count"))),
            ("matmul_compulsory_bytes", get_path(data, root + ("matmul", "compulsory_bytes"))),
            ("matmul_naive_loop_bytes", get_path(data, root + ("matmul", "naive_loop_bytes"))),
            (
                "matmul_compulsory_arithmetic_intensity",
                get_path(data, root + ("matmul", "compulsory_arithmetic_intensity")),
            ),
            (
                "matmul_naive_loop_arithmetic_intensity",
                get_path(data, root + ("matmul", "naive_loop_arithmetic_intensity")),
            ),
            ("tiled_matmul_tile_m", get_path(data, root + ("tiled_matmul", "tile_m"))),
            ("tiled_matmul_tile_k", get_path(data, root + ("tiled_matmul", "tile_k"))),
            ("tiled_matmul_tile_n", get_path(data, root + ("tiled_matmul", "tile_n"))),
            (
                "tiled_matmul_global_bytes",
                get_path(data, root + ("tiled_matmul", "tiled_global_bytes")),
            ),
            (
                "tiled_matmul_arithmetic_intensity",
                get_path(data, root + ("tiled_matmul", "tiled_arithmetic_intensity")),
            ),
            (
                "naive_to_tiled_byte_reduction",
                get_path(data, root + ("tiled_matmul", "naive_to_tiled_byte_reduction")),
            ),
            ("matvec_flop_count", get_path(data, root + ("matvec", "flop_count"))),
            ("matvec_naive_loop_bytes", get_path(data, root + ("matvec", "naive_loop_bytes"))),
            (
                "softmax_total_memory_bytes",
                get_path(data, root + ("row_softmax", "total_memory_bytes")),
            ),
            ("softmax_exp_ops", get_path(data, root + ("row_softmax", "exp_ops"))),
            (
                "softmax_scalar_ops_per_byte",
                get_path(data, root + ("row_softmax", "scalar_ops_per_byte")),
            ),
        ]
    return build_section("Matrix/Softmax Profile", "matrix_ops_profile.json", data, metrics)


def decoder_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("seq_len", get_path(data, ("decoder_block", "seq_len"))),
            ("hidden_size", get_path(data, ("decoder_block", "hidden_size"))),
            ("num_heads", get_path(data, ("decoder_block", "num_heads"))),
            ("attention_score_elements", get_path(data, ("decoder_block", "attention_score_elements"))),
            ("ffn_elements", get_path(data, ("decoder_block", "ffn_elements"))),
            ("output_checksum", get_path(data, ("decoder_block", "output_checksum"))),
        ]
    return build_section("Decoder Block", "decoder_probe.json", data, metrics)


def decoder_kv_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("prompt_tokens", get_path(data, ("decoder_kv", "prompt_tokens"))),
            ("decode_steps", get_path(data, ("decoder_kv", "decode_steps"))),
            ("final_tokens", get_path(data, ("decoder_kv", "final_tokens"))),
            ("used_blocks", get_path(data, ("decoder_kv", "used_blocks"))),
            ("block_size_tokens", get_path(data, ("decoder_kv", "block_size_tokens"))),
            ("used_block_table_gather", get_path(data, ("decoder_kv", "used_block_table_gather"))),
            (
                "profile_final_logical_kv_bytes",
                get_path(data, ("decoder_kv", "cache_profile", "final_logical_kv_bytes")),
            ),
            (
                "profile_final_reserved_kv_bytes",
                get_path(data, ("decoder_kv", "cache_profile", "final_reserved_kv_bytes")),
            ),
            (
                "profile_final_kv_utilization",
                get_path(data, ("decoder_kv", "cache_profile", "final_kv_utilization")),
            ),
            (
                "profile_decode_attention_kv_read_elements",
                get_path(data, ("decoder_kv", "cache_profile", "decode_attention_kv_read_elements")),
            ),
            (
                "profile_paged_gather_read_elements",
                get_path(data, ("decoder_kv", "cache_profile", "paged_gather_read_elements")),
            ),
            (
                "profile_paged_gather_write_elements",
                get_path(data, ("decoder_kv", "cache_profile", "paged_gather_write_elements")),
            ),
            (
                "profile_block_table_lookup_count",
                get_path(data, ("decoder_kv", "cache_profile", "block_table_lookup_count")),
            ),
            (
                "profile_block_table_lookup_bytes",
                get_path(data, ("decoder_kv", "cache_profile", "block_table_lookup_bytes")),
            ),
            ("last_token_output_checksum", get_path(data, ("decoder_kv", "last_token_output_checksum"))),
        ]
    return build_section("Decoder KV Decode", "decoder_kv_probe.json", data, metrics)


def gguf_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        root = ("gguf",)
        metadata = get_path(data, root + ("metadata",), [])
        tensors = get_path(data, root + ("tensors",), [])
        layout_segments = get_path(data, root + ("layout_segments",), [])
        payload_previews = get_path(data, root + ("payload_previews",), [])
        header_segment = named_item(layout_segments, "header")
        metadata_segment = named_item(layout_segments, "metadata")
        tensor_info_segment = named_item(layout_segments, "tensor_infos")
        padding_segment = named_item(layout_segments, "tensor_data_padding")
        tensor_data_segment = named_item(layout_segments, "tensor_data")
        f32_preview = {}
        f64_preview = {}
        i64_preview = {}
        i32_preview = {}
        i16_preview = {}
        i8_preview = {}
        f16_preview = {}
        iq2_xxs_preview = {}
        q1_0_preview = {}
        q2_k_preview = {}
        q3_k_preview = {}
        q4_0_preview = {}
        q4_1_preview = {}
        q5_0_preview = {}
        q5_1_preview = {}
        iq4_nl_preview = {}
        mxfp4_preview = {}
        nvfp4_preview = {}
        bf16_preview = {}
        iq4_xs_preview = {}
        tq1_0_preview = {}
        tq2_0_preview = {}
        q4_k_preview = {}
        q5_k_preview = {}
        q6_k_preview = {}
        q8_0_preview = {}
        q8_1_preview = {}
        q8_k_preview = {}
        if isinstance(payload_previews, list):
            for item in payload_previews:
                if not isinstance(item, dict):
                    continue
                if item.get("type_name") == "f32":
                    f32_preview = item
                if item.get("type_name") == "f64":
                    f64_preview = item
                if item.get("type_name") == "i64":
                    i64_preview = item
                if item.get("type_name") == "i32":
                    i32_preview = item
                if item.get("type_name") == "i16":
                    i16_preview = item
                if item.get("type_name") == "i8":
                    i8_preview = item
                if item.get("type_name") == "f16":
                    f16_preview = item
                if item.get("type_name") == "iq2_xxs":
                    iq2_xxs_preview = item
                if item.get("type_name") == "q1_0":
                    q1_0_preview = item
                if item.get("type_name") == "q2_K":
                    q2_k_preview = item
                if item.get("type_name") == "q3_K":
                    q3_k_preview = item
                if item.get("type_name") == "q4_0":
                    q4_0_preview = item
                if item.get("type_name") == "q4_1":
                    q4_1_preview = item
                if item.get("type_name") == "q5_0":
                    q5_0_preview = item
                if item.get("type_name") == "q5_1":
                    q5_1_preview = item
                if item.get("type_name") == "iq4_nl":
                    iq4_nl_preview = item
                if item.get("type_name") == "mxfp4":
                    mxfp4_preview = item
                if item.get("type_name") == "nvfp4":
                    nvfp4_preview = item
                if item.get("type_name") == "bf16":
                    bf16_preview = item
                if item.get("type_name") == "iq4_xs":
                    iq4_xs_preview = item
                if item.get("type_name") == "tq1_0":
                    tq1_0_preview = item
                if item.get("type_name") == "tq2_0":
                    tq2_0_preview = item
                if item.get("type_name") == "q4_K":
                    q4_k_preview = item
                if item.get("type_name") == "q5_K":
                    q5_k_preview = item
                if item.get("type_name") == "q6_K":
                    q6_k_preview = item
                if item.get("type_name") == "q8_0":
                    q8_0_preview = item
                if item.get("type_name") == "q8_1":
                    q8_1_preview = item
                if item.get("type_name") == "q8_K":
                    q8_k_preview = item
        token_array_length = None
        if isinstance(metadata, list):
            for item in metadata:
                if isinstance(item, dict) and item.get("key") == "tokenizer.ggml.tokens":
                    token_array_length = item.get("array_length")
        first_tensor = tensors[0] if isinstance(tensors, list) and tensors else {}
        last_tensor = tensors[-1] if isinstance(tensors, list) and tensors else {}
        tensor_type_names = sorted(
            {
                tensor.get("type_name")
                for tensor in tensors
                if isinstance(tensor, dict) and isinstance(tensor.get("type_name"), str)
            }
        ) if isinstance(tensors, list) else []
        data_sizes = [
            tensor.get("data_size_bytes")
            for tensor in tensors
            if isinstance(tensor, dict) and isinstance(tensor.get("data_size_bytes"), int)
        ] if isinstance(tensors, list) else []
        metrics = [
            ("version", get_path(data, root + ("version",))),
            ("source", get_path(data, root + ("source",))),
            ("input_path", get_path(data, root + ("input_path",))),
            ("alignment", get_path(data, root + ("alignment",))),
            ("file_size_bytes", get_path(data, root + ("file_size_bytes",))),
            ("fixture_size_bytes", get_path(data, root + ("fixture_size_bytes",))),
            ("header_size_bytes", get_path(data, root + ("header_size_bytes",))),
            ("metadata_size_bytes", metadata_segment.get("size_bytes")),
            ("tensor_info_size_bytes", tensor_info_segment.get("size_bytes")),
            ("tensor_data_padding_bytes", get_path(data, root + ("tensor_data_padding_bytes",))),
            ("tensor_data_segment_bytes", tensor_data_segment.get("size_bytes")),
            ("header_hex_prefix", header_segment.get("hex_prefix")),
            ("metadata_ascii_prefix", metadata_segment.get("ascii_prefix")),
            (
                "payload_preview_types",
                [
                    item.get("type_name")
                    for item in payload_previews
                    if isinstance(item, dict) and isinstance(item.get("type_name"), str)
                ]
                if isinstance(payload_previews, list)
                else [],
            ),
            ("f32_raw_values_hex_prefix", f32_preview.get("raw_values_hex_prefix")),
            ("f32_raw_values_prefix", f32_preview.get("raw_values_prefix")),
            ("f32_decode_rule", f32_preview.get("decode_rule")),
            ("f32_values_prefix", f32_preview.get("values_prefix")),
            ("f32_stats", f32_preview.get("stats")),
            ("f64_raw_values_hex_prefix", f64_preview.get("raw_values_hex_prefix")),
            ("f64_raw_values_prefix", f64_preview.get("raw_values_prefix")),
            ("f64_decode_rule", f64_preview.get("decode_rule")),
            ("f64_values_prefix", f64_preview.get("values_prefix")),
            ("f64_stats", f64_preview.get("stats")),
            ("i64_raw_values_hex_prefix", i64_preview.get("raw_values_hex_prefix")),
            ("i64_raw_values_prefix", i64_preview.get("raw_values_prefix")),
            ("i64_decode_rule", i64_preview.get("decode_rule")),
            ("i64_values_prefix", i64_preview.get("values_prefix")),
            ("i64_stats", i64_preview.get("stats")),
            ("i32_raw_values_hex_prefix", i32_preview.get("raw_values_hex_prefix")),
            ("i32_raw_values_prefix", i32_preview.get("raw_values_prefix")),
            ("i32_decode_rule", i32_preview.get("decode_rule")),
            ("i32_values_prefix", i32_preview.get("values_prefix")),
            ("i32_stats", i32_preview.get("stats")),
            ("i16_raw_values_hex_prefix", i16_preview.get("raw_values_hex_prefix")),
            ("i16_raw_values_prefix", i16_preview.get("raw_values_prefix")),
            ("i16_decode_rule", i16_preview.get("decode_rule")),
            ("i16_values_prefix", i16_preview.get("values_prefix")),
            ("i16_stats", i16_preview.get("stats")),
            ("i8_raw_values_hex_prefix", i8_preview.get("raw_values_hex_prefix")),
            ("i8_raw_values_prefix", i8_preview.get("raw_values_prefix")),
            ("i8_decode_rule", i8_preview.get("decode_rule")),
            ("i8_values_prefix", i8_preview.get("values_prefix")),
            ("i8_stats", i8_preview.get("stats")),
            ("f16_raw_values_hex_prefix", f16_preview.get("raw_values_hex_prefix")),
            ("f16_raw_values_prefix", f16_preview.get("raw_values_prefix")),
            ("f16_decode_rule", f16_preview.get("decode_rule")),
            ("f16_values_prefix", f16_preview.get("values_prefix")),
            ("f16_stats", f16_preview.get("stats")),
            ("iq2_xxs_raw_bytes_prefix", iq2_xxs_preview.get("raw_bytes_prefix")),
            ("iq2_xxs_d_fp32", iq2_xxs_preview.get("d_fp32")),
            ("iq2_xxs_qs_words_prefix", iq2_xxs_preview.get("qs_words_prefix")),
            ("iq2_xxs_grid_indices_prefix", iq2_xxs_preview.get("grid_indices_prefix")),
            ("iq2_xxs_sign_codes_prefix", iq2_xxs_preview.get("sign_codes_prefix")),
            ("iq2_xxs_scale_nibbles", iq2_xxs_preview.get("scale_nibbles")),
            ("iq2_xxs_effective_scales", iq2_xxs_preview.get("effective_scales")),
            ("iq2_xxs_decode_rule", iq2_xxs_preview.get("decode_rule")),
            ("iq2_xxs_dequant_values_prefix", iq2_xxs_preview.get("dequant_values_prefix")),
            ("iq2_xxs_dequant_group_sums", iq2_xxs_preview.get("dequant_group_sums")),
            ("iq2_xxs_dequant_stats", iq2_xxs_preview.get("dequant_stats")),
            ("q1_0_d_fp32", q1_0_preview.get("d_fp32")),
            ("q1_0_packed_bit_bytes_prefix", q1_0_preview.get("packed_bit_bytes_prefix")),
            ("q1_0_bit_values_prefix", q1_0_preview.get("bit_values_prefix")),
            ("q1_0_dequant_rule", q1_0_preview.get("dequant_rule")),
            ("q1_0_dequant_values_prefix", q1_0_preview.get("dequant_values_prefix")),
            ("q1_0_dequant_stats", q1_0_preview.get("dequant_stats")),
            ("q2_k_scale_min_bytes_prefix", q2_k_preview.get("scale_min_bytes_prefix")),
            ("q2_k_packed_quant_bytes_prefix", q2_k_preview.get("packed_quant_bytes_prefix")),
            ("q2_k_d_fp32", q2_k_preview.get("d_fp32")),
            ("q2_k_dmin_fp32", q2_k_preview.get("dmin_fp32")),
            ("q2_k_scale_min_pairs", q2_k_preview.get("scale_min_pairs")),
            ("q2_k_dequant_rule", q2_k_preview.get("dequant_rule")),
            ("q2_k_dequant_group0_values_prefix", q2_k_preview.get("dequant_group0_values_prefix")),
            ("q2_k_dequant_group1_values_prefix", q2_k_preview.get("dequant_group1_values_prefix")),
            ("q2_k_dequant_values_prefix", q2_k_preview.get("dequant_values_prefix")),
            ("q2_k_dequant_group_sums", q2_k_preview.get("dequant_group_sums")),
            ("q2_k_dequant_stats", q2_k_preview.get("dequant_stats")),
            ("q3_k_high_mask_bytes_prefix", q3_k_preview.get("high_mask_bytes_prefix")),
            ("q3_k_packed_quant_bytes_prefix", q3_k_preview.get("packed_quant_bytes_prefix")),
            ("q3_k_scale_bytes_prefix", q3_k_preview.get("scale_bytes_prefix")),
            ("q3_k_d_fp32", q3_k_preview.get("d_fp32")),
            ("q3_k_scales", q3_k_preview.get("scales")),
            ("q3_k_dequant_rule", q3_k_preview.get("dequant_rule")),
            ("q3_k_dequant_group0_values_prefix", q3_k_preview.get("dequant_group0_values_prefix")),
            ("q3_k_dequant_group1_values_prefix", q3_k_preview.get("dequant_group1_values_prefix")),
            ("q3_k_dequant_values_prefix", q3_k_preview.get("dequant_values_prefix")),
            ("q3_k_dequant_group_sums", q3_k_preview.get("dequant_group_sums")),
            ("q3_k_dequant_stats", q3_k_preview.get("dequant_stats")),
            ("q4_0_d_fp32", q4_0_preview.get("d_fp32")),
            ("q4_0_packed_quant_bytes_prefix", q4_0_preview.get("packed_quant_bytes_prefix")),
            ("q4_0_quant_values_prefix", q4_0_preview.get("quant_values_prefix")),
            ("q4_0_dequant_rule", q4_0_preview.get("dequant_rule")),
            ("q4_0_dequant_values_prefix", q4_0_preview.get("dequant_values_prefix")),
            ("q4_0_dequant_stats", q4_0_preview.get("dequant_stats")),
            ("q4_1_d_fp32", q4_1_preview.get("d_fp32")),
            ("q4_1_m_fp32", q4_1_preview.get("m_fp32")),
            ("q4_1_packed_quant_bytes_prefix", q4_1_preview.get("packed_quant_bytes_prefix")),
            ("q4_1_quant_values_prefix", q4_1_preview.get("quant_values_prefix")),
            ("q4_1_dequant_rule", q4_1_preview.get("dequant_rule")),
            ("q4_1_dequant_values_prefix", q4_1_preview.get("dequant_values_prefix")),
            ("q4_1_dequant_stats", q4_1_preview.get("dequant_stats")),
            ("q5_0_d_fp32", q5_0_preview.get("d_fp32")),
            ("q5_0_high_bits_bytes_prefix", q5_0_preview.get("high_bits_bytes_prefix")),
            ("q5_0_packed_quant_bytes_prefix", q5_0_preview.get("packed_quant_bytes_prefix")),
            ("q5_0_quant_values_prefix", q5_0_preview.get("quant_values_prefix")),
            ("q5_0_dequant_rule", q5_0_preview.get("dequant_rule")),
            ("q5_0_dequant_values_prefix", q5_0_preview.get("dequant_values_prefix")),
            ("q5_0_dequant_stats", q5_0_preview.get("dequant_stats")),
            ("q5_1_d_fp32", q5_1_preview.get("d_fp32")),
            ("q5_1_m_fp32", q5_1_preview.get("m_fp32")),
            ("q5_1_high_bits_bytes_prefix", q5_1_preview.get("high_bits_bytes_prefix")),
            ("q5_1_packed_quant_bytes_prefix", q5_1_preview.get("packed_quant_bytes_prefix")),
            ("q5_1_quant_values_prefix", q5_1_preview.get("quant_values_prefix")),
            ("q5_1_dequant_rule", q5_1_preview.get("dequant_rule")),
            ("q5_1_dequant_values_prefix", q5_1_preview.get("dequant_values_prefix")),
            ("q5_1_dequant_stats", q5_1_preview.get("dequant_stats")),
            ("iq4_nl_d_fp32", iq4_nl_preview.get("d_fp32")),
            ("iq4_nl_packed_quant_bytes_prefix", iq4_nl_preview.get("packed_quant_bytes_prefix")),
            ("iq4_nl_quant_values_prefix", iq4_nl_preview.get("quant_values_prefix")),
            ("iq4_nl_codebook_table", iq4_nl_preview.get("codebook_table")),
            ("iq4_nl_codebook_values_prefix", iq4_nl_preview.get("codebook_values_prefix")),
            ("iq4_nl_dequant_rule", iq4_nl_preview.get("dequant_rule")),
            ("iq4_nl_dequant_values_prefix", iq4_nl_preview.get("dequant_values_prefix")),
            ("iq4_nl_dequant_stats", iq4_nl_preview.get("dequant_stats")),
            ("mxfp4_e", mxfp4_preview.get("e")),
            ("mxfp4_e_hex", mxfp4_preview.get("e_hex")),
            ("mxfp4_e_scale_fp32", mxfp4_preview.get("e_scale_fp32")),
            ("mxfp4_packed_quant_bytes_prefix", mxfp4_preview.get("packed_quant_bytes_prefix")),
            ("mxfp4_quant_values_prefix", mxfp4_preview.get("quant_values_prefix")),
            ("mxfp4_codebook_table", mxfp4_preview.get("codebook_table")),
            ("mxfp4_codebook_values_prefix", mxfp4_preview.get("codebook_values_prefix")),
            ("mxfp4_dequant_rule", mxfp4_preview.get("dequant_rule")),
            ("mxfp4_dequant_values_prefix", mxfp4_preview.get("dequant_values_prefix")),
            ("mxfp4_dequant_stats", mxfp4_preview.get("dequant_stats")),
            ("nvfp4_scale_bytes_prefix", nvfp4_preview.get("scale_bytes_prefix")),
            ("nvfp4_scales", nvfp4_preview.get("scales")),
            ("nvfp4_packed_quant_bytes_prefix", nvfp4_preview.get("packed_quant_bytes_prefix")),
            ("nvfp4_quant_values_prefix", nvfp4_preview.get("quant_values_prefix")),
            ("nvfp4_codebook_table", nvfp4_preview.get("codebook_table")),
            ("nvfp4_codebook_values_prefix", nvfp4_preview.get("codebook_values_prefix")),
            ("nvfp4_dequant_rule", nvfp4_preview.get("dequant_rule")),
            ("nvfp4_dequant_subblock0_values_prefix", nvfp4_preview.get("dequant_subblock0_values_prefix")),
            ("nvfp4_dequant_subblock1_values_prefix", nvfp4_preview.get("dequant_subblock1_values_prefix")),
            ("nvfp4_dequant_values_prefix", nvfp4_preview.get("dequant_values_prefix")),
            ("nvfp4_dequant_subblock_sums", nvfp4_preview.get("dequant_subblock_sums")),
            ("nvfp4_dequant_stats", nvfp4_preview.get("dequant_stats")),
            ("bf16_raw_values_hex_prefix", bf16_preview.get("raw_values_hex_prefix")),
            ("bf16_raw_values_prefix", bf16_preview.get("raw_values_prefix")),
            ("bf16_decode_rule", bf16_preview.get("decode_rule")),
            ("bf16_values_prefix", bf16_preview.get("values_prefix")),
            ("bf16_stats", bf16_preview.get("stats")),
            ("iq4_xs_d_fp32", iq4_xs_preview.get("d_fp32")),
            ("iq4_xs_scales_h", iq4_xs_preview.get("scales_h")),
            ("iq4_xs_scales_h_hex", iq4_xs_preview.get("scales_h_hex")),
            ("iq4_xs_scales_l_bytes_prefix", iq4_xs_preview.get("scales_l_bytes_prefix")),
            ("iq4_xs_packed_quant_bytes_prefix", iq4_xs_preview.get("packed_quant_bytes_prefix")),
            ("iq4_xs_scales", iq4_xs_preview.get("scales")),
            ("iq4_xs_quant_values_prefix", iq4_xs_preview.get("quant_values_prefix")),
            ("iq4_xs_codebook_table", iq4_xs_preview.get("codebook_table")),
            ("iq4_xs_codebook_values_prefix", iq4_xs_preview.get("codebook_values_prefix")),
            ("iq4_xs_dequant_rule", iq4_xs_preview.get("dequant_rule")),
            ("iq4_xs_dequant_subblock0_values_prefix", iq4_xs_preview.get("dequant_subblock0_values_prefix")),
            ("iq4_xs_dequant_subblock1_values_prefix", iq4_xs_preview.get("dequant_subblock1_values_prefix")),
            ("iq4_xs_dequant_values_prefix", iq4_xs_preview.get("dequant_values_prefix")),
            ("iq4_xs_dequant_subblock_sums", iq4_xs_preview.get("dequant_subblock_sums")),
            ("iq4_xs_dequant_stats", iq4_xs_preview.get("dequant_stats")),
            ("tq1_0_d_fp32", tq1_0_preview.get("d_fp32")),
            ("tq1_0_packed_qs_bytes_prefix", tq1_0_preview.get("packed_qs_bytes_prefix")),
            ("tq1_0_packed_qh_bytes_prefix", tq1_0_preview.get("packed_qh_bytes_prefix")),
            ("tq1_0_quant_values_prefix", tq1_0_preview.get("quant_values_prefix")),
            ("tq1_0_quant_counts", tq1_0_preview.get("quant_counts")),
            ("tq1_0_dequant_rule", tq1_0_preview.get("dequant_rule")),
            ("tq1_0_dequant_values_prefix", tq1_0_preview.get("dequant_values_prefix")),
            ("tq1_0_dequant_stats", tq1_0_preview.get("dequant_stats")),
            ("tq2_0_d_fp32", tq2_0_preview.get("d_fp32")),
            ("tq2_0_packed_quant_bytes_prefix", tq2_0_preview.get("packed_quant_bytes_prefix")),
            ("tq2_0_quant_values_prefix", tq2_0_preview.get("quant_values_prefix")),
            ("tq2_0_quant_counts", tq2_0_preview.get("quant_counts")),
            ("tq2_0_dequant_rule", tq2_0_preview.get("dequant_rule")),
            ("tq2_0_dequant_values_prefix", tq2_0_preview.get("dequant_values_prefix")),
            ("tq2_0_dequant_stats", tq2_0_preview.get("dequant_stats")),
            ("q4_k_scale_bytes_prefix", q4_k_preview.get("scale_bytes_prefix")),
            ("q4_k_packed_quant_bytes_prefix", q4_k_preview.get("packed_quant_bytes_prefix")),
            ("q4_k_d_fp32", q4_k_preview.get("d_fp32")),
            ("q4_k_dmin_fp32", q4_k_preview.get("dmin_fp32")),
            ("q4_k_scale_min_pairs", q4_k_preview.get("scale_min_pairs")),
            ("q4_k_dequant_rule", q4_k_preview.get("dequant_rule")),
            ("q4_k_dequant_subblock0_values_prefix", q4_k_preview.get("dequant_subblock0_values_prefix")),
            ("q4_k_dequant_subblock1_values_prefix", q4_k_preview.get("dequant_subblock1_values_prefix")),
            ("q4_k_dequant_values_prefix", q4_k_preview.get("dequant_values_prefix")),
            ("q4_k_dequant_subblock_sums", q4_k_preview.get("dequant_subblock_sums")),
            ("q4_k_dequant_stats", q4_k_preview.get("dequant_stats")),
            ("q4_k_unpacked_nibbles_prefix", q4_k_preview.get("unpacked_nibbles_prefix")),
            ("q5_k_scale_bytes_prefix", q5_k_preview.get("scale_bytes_prefix")),
            ("q5_k_high_bits_bytes_prefix", q5_k_preview.get("high_bits_bytes_prefix")),
            ("q5_k_packed_quant_bytes_prefix", q5_k_preview.get("packed_quant_bytes_prefix")),
            ("q5_k_d_fp32", q5_k_preview.get("d_fp32")),
            ("q5_k_dmin_fp32", q5_k_preview.get("dmin_fp32")),
            ("q5_k_scale_min_pairs", q5_k_preview.get("scale_min_pairs")),
            ("q5_k_dequant_rule", q5_k_preview.get("dequant_rule")),
            ("q5_k_dequant_subblock0_values_prefix", q5_k_preview.get("dequant_subblock0_values_prefix")),
            ("q5_k_dequant_subblock1_values_prefix", q5_k_preview.get("dequant_subblock1_values_prefix")),
            ("q5_k_dequant_values_prefix", q5_k_preview.get("dequant_values_prefix")),
            ("q5_k_dequant_subblock_sums", q5_k_preview.get("dequant_subblock_sums")),
            ("q5_k_dequant_stats", q5_k_preview.get("dequant_stats")),
            ("q6_k_low_bits_bytes_prefix", q6_k_preview.get("low_bits_bytes_prefix")),
            ("q6_k_high_bits_bytes_prefix", q6_k_preview.get("high_bits_bytes_prefix")),
            ("q6_k_scale_bytes_prefix", q6_k_preview.get("scale_bytes_prefix")),
            ("q6_k_d_fp32", q6_k_preview.get("d_fp32")),
            ("q6_k_scales", q6_k_preview.get("scales")),
            ("q6_k_dequant_rule", q6_k_preview.get("dequant_rule")),
            ("q6_k_dequant_group0_values_prefix", q6_k_preview.get("dequant_group0_values_prefix")),
            ("q6_k_dequant_group1_values_prefix", q6_k_preview.get("dequant_group1_values_prefix")),
            ("q6_k_dequant_values_prefix", q6_k_preview.get("dequant_values_prefix")),
            ("q6_k_dequant_group_sums", q6_k_preview.get("dequant_group_sums")),
            ("q6_k_dequant_stats", q6_k_preview.get("dequant_stats")),
            ("q8_0_d_fp32", q8_0_preview.get("d_fp32")),
            ("q8_0_quant_values_prefix", q8_0_preview.get("quant_values_prefix")),
            ("q8_0_dequant_values_prefix", q8_0_preview.get("dequant_values_prefix")),
            ("q8_0_quant_sum_first_block", q8_0_preview.get("quant_sum_first_block")),
            ("q8_0_dequant_stats", q8_0_preview.get("dequant_stats")),
            ("q8_1_d_fp32", q8_1_preview.get("d_fp32")),
            ("q8_1_s_fp32", q8_1_preview.get("s_fp32")),
            ("q8_1_expected_s_from_quant_sum", q8_1_preview.get("expected_s_from_quant_sum")),
            ("q8_1_quant_values_prefix", q8_1_preview.get("quant_values_prefix")),
            ("q8_1_dequant_values_prefix", q8_1_preview.get("dequant_values_prefix")),
            ("q8_1_quant_sum_first_block", q8_1_preview.get("quant_sum_first_block")),
            ("q8_1_dequant_stats", q8_1_preview.get("dequant_stats")),
            ("q8_k_d_fp32", q8_k_preview.get("d_fp32")),
            ("q8_k_quant_values_prefix", q8_k_preview.get("quant_values_prefix")),
            ("q8_k_block_sums_prefix", q8_k_preview.get("block_sums_prefix")),
            ("q8_k_block_sum_bytes_prefix", q8_k_preview.get("block_sum_bytes_prefix")),
            ("q8_k_dequant_rule", q8_k_preview.get("dequant_rule")),
            ("q8_k_dequant_values_prefix", q8_k_preview.get("dequant_values_prefix")),
            ("q8_k_quant_sum_first_block", q8_k_preview.get("quant_sum_first_block")),
            ("q8_k_dequant_stats", q8_k_preview.get("dequant_stats")),
            ("metadata_kv_count", get_path(data, root + ("metadata_kv_count",))),
            ("tensor_count", get_path(data, root + ("tensor_count",))),
            ("quantized_tensor_count", get_path(data, root + ("quantized_tensor_count",))),
            ("known_tensor_data_bytes", get_path(data, root + ("known_tensor_data_bytes",))),
            ("tensor_data_start_offset", get_path(data, root + ("tensor_data_start_offset",))),
            ("tensor_type_names", tensor_type_names),
            ("largest_tensor_data_bytes", max(data_sizes) if data_sizes else None),
            ("tokenizer_token_count", token_array_length),
            ("first_tensor_name", first_tensor.get("name") if isinstance(first_tensor, dict) else None),
            (
                "first_tensor_type",
                first_tensor.get("type_name") if isinstance(first_tensor, dict) else None,
            ),
            (
                "first_tensor_absolute_offset",
                first_tensor.get("absolute_offset") if isinstance(first_tensor, dict) else None,
            ),
            (
                "first_tensor_data_size_bytes",
                first_tensor.get("data_size_bytes") if isinstance(first_tensor, dict) else None,
            ),
            ("last_tensor_name", last_tensor.get("name") if isinstance(last_tensor, dict) else None),
            (
                "last_tensor_type",
                last_tensor.get("type_name") if isinstance(last_tensor, dict) else None,
            ),
            (
                "last_tensor_relative_offset",
                last_tensor.get("relative_offset") if isinstance(last_tensor, dict) else None,
            ),
            (
                "last_tensor_data_size_bytes",
                last_tensor.get("data_size_bytes") if isinstance(last_tensor, dict) else None,
            ),
        ]
    return build_section("GGUF Parser", "gguf_probe.json", data, metrics)


def gguf_fixture_comparison_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("reason", data.get("reason")),
            ("fixture_path", data.get("fixture_path")),
            ("expected_json", data.get("expected_json")),
            ("mismatch_count", data.get("mismatch_count")),
            ("metadata_kv_count", get_path(data, ("observed", "metadata_kv_count"))),
            ("tensor_count", get_path(data, ("observed", "tensor_count"))),
            (
                "quantized_tensor_count",
                get_path(data, ("observed", "quantized_tensor_count")),
            ),
            (
                "tensor_data_start_offset",
                get_path(data, ("observed", "tensor_data_start_offset")),
            ),
            (
                "known_tensor_data_bytes",
                get_path(data, ("observed", "known_tensor_data_bytes")),
            ),
            ("tensor_type_names", get_path(data, ("observed", "tensor_type_names"))),
            (
                "largest_tensor_data_bytes",
                get_path(data, ("observed", "largest_tensor_data_bytes")),
            ),
        ]
    return build_section(
        "GGUF Fixture Comparison",
        "gguf_fixture_comparison.json",
        data,
        metrics,
    )


def source_files_with_status(results_root: Path) -> tuple[List[str], Dict[str, Any]]:
    try:
        exists = results_root.exists()
    except OSError as exc:
        return [], {"status": "results_root_probe_failed", "path": path_label(results_root), "error": redact_path_error(str(exc), results_root)}
    if not exists:
        return [], {"status": "missing_results_root", "path": path_label(results_root)}
    try:
        paths = sorted(results_root.glob("*.json"))
    except OSError as exc:
        return [], {"status": "results_root_scan_failed", "path": path_label(results_root), "error": redact_path_error(str(exc), results_root)}
    files = [path.name for path in paths if path.name != "benchmark_summary.json"]
    return files, {"status": "ok", "path": path_label(results_root), "file_count": len(files)}


def source_files(results_root: Path) -> List[str]:
    files, _ = source_files_with_status(results_root)
    return files



def backend_quality_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        summary = data.get("summary") if isinstance(data.get("summary"), dict) else {}
        metrics = [
            ("record_count", summary.get("record_count")),
            ("pairwise_comparisons", summary.get("pairwise_comparisons")),
        ]
        backend_summary = summary.get("backend_summary") if isinstance(summary.get("backend_summary"), dict) else {}
        for backend, item in sorted(backend_summary.items()):
            if isinstance(item, dict):
                prefix = str(backend).replace(".", "_").replace("/", "_")
                metrics.extend(
                    [
                        (f"{prefix}_records", item.get("records")),
                        (f"{prefix}_with_full_text", item.get("with_full_text", item.get("with_text"))),
                        (f"{prefix}_with_redacted_preview", item.get("with_redacted_preview")),
                        (f"{prefix}_pass", item.get("pass")),
                        (f"{prefix}_fail", item.get("fail")),
                    ]
                )
    return build_section("Backend Quality Evaluation", "backend_quality_eval.json", data, metrics)


def profiling_artifacts_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        summary = data.get("summary") if isinstance(data.get("summary"), dict) else {}
        metrics = [
            ("file_count", summary.get("file_count")),
            ("total_bytes", summary.get("total_bytes")),
            ("metric_count", summary.get("metric_count")),
        ]
        categories = summary.get("categories") if isinstance(summary.get("categories"), dict) else {}
        for category, item in sorted(categories.items()):
            if isinstance(item, dict):
                metrics.extend(
                    [
                        (f"{category}_files", item.get("files")),
                        (f"{category}_metrics", item.get("metrics")),
                    ]
                )
    return build_section("Profiling Artifacts", "profiling_artifacts.json", data, metrics)


def backend_comparison_section(data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        backends = data.get("backends") if isinstance(data.get("backends"), list) else []
        metrics.append(("backend_count", len(backends)))
        for item in backends:
            if not isinstance(item, dict):
                continue
            name = str(item.get("backend", "unknown")).replace(".", "_").replace("/", "_")
            metrics.extend(
                [
                    (f"{name}_status", item.get("status")),
                    (f"{name}_readiness_score", item.get("readiness_score")),
                    (f"{name}_profiling_files", get_path(item, ("profiling", "files"))),
                    (
                        f"{name}_quality_with_full_text",
                        get_path(
                            item,
                            ("quality", "with_full_text"),
                            get_path(item, ("quality", "with_text")),
                        ),
                    ),
                    (
                        f"{name}_quality_with_redacted_preview",
                        get_path(item, ("quality", "with_redacted_preview")),
                    ),
                    (f"{name}_cost_total", get_path(item, ("cost", "total_cost"))),
                ]
            )
    return build_section("Backend Comparison", "backend_comparison.json", data, metrics)


def runtime_closure_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("executed", data.get("executed")),
            ("scenario_count", get_path(data, ("summary", "scenario_count"))),
            ("target_count", get_path(data, ("summary", "target_count"))),
            ("repository_count", get_path(data, ("summary", "repository_count"))),
            ("complete_count", get_path(data, ("summary", "complete_count"))),
            ("open_count", get_path(data, ("summary", "open_count"))),
            ("failed_checks", get_path(data, ("summary", "failed_checks"))),
            ("simpleperf_requested", get_path(data, ("summary", "simpleperf_requested"))),
            ("simpleperf_observed_count", get_path(data, ("summary", "simpleperf_observed_count"))),
            ("simpleperf_gap_count", get_path(data, ("summary", "simpleperf_gap_count"))),
        ]
    return build_section(title, source, data, metrics)



def advanced_serving_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("artifact_type", data.get("artifact_type") or get_path(data, ("schema", "name"))),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("cli_context_present", isinstance(data.get("cli_context"), dict)),
            ("cli_format", get_path(data, ("cli_context", "format"))),
            ("unsupported_format_rejected", get_path(data, ("cli_context", "unsupported_format_rejected"))),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("contract_count", get_path(data, ("summary", "contract_count"))),
            ("registry_contract_count", get_path(data, ("summary", "registry_contract_count"))),
            ("registry_consumed_contract_count", get_path(data, ("summary", "registry_consumed_contract_count"))),
            ("scenario_count", get_path(data, ("summary", "scenario_count"))),
            ("failed_records", get_path(data, ("summary", "failed_records"))),
            ("backend_count", get_path(data, ("summary", "backend_count"))),
            ("policy_count", get_path(data, ("summary", "policy_count"))),
            ("max_required_replicas", get_path(data, ("summary", "max_required_replicas"))),
        ]
    return build_section(title, source, data, metrics)


def product_profile_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        missing = get_path(data, ("environment_preflight", "missing"), [])
        collected = get_path(data, ("raw_metrics", "collected"), {})
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("replayed", data.get("replayed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("product", data.get("product")),
            ("category", data.get("category")),
            ("missing_preflight_count", len(missing) if isinstance(missing, list) else None),
            ("expected_metric_count", len(get_path(data, ("raw_metrics", "expected_metrics"), []))),
            (
                "replay_evidence_file_count",
                collected.get("evidence_file_count") if isinstance(collected, dict) else None,
            ),
            (
                "replay_parsed_metric_count",
                collected.get("parsed_metric_count") if isinstance(collected, dict) else None,
            ),
            (
                "replay_canonical_metric_count",
                collected.get("canonical_metric_count") if isinstance(collected, dict) else None,
            ),
            (
                "endpoint_success_count",
                collected.get("endpoint_success_count") if isinstance(collected, dict) else None,
            ),
            (
                "endpoint_check_count",
                collected.get("endpoint_check_count") if isinstance(collected, dict) else None,
            ),
            (
                "extra_command_success_count",
                collected.get("extra_command_success_count") if isinstance(collected, dict) else None,
            ),
            (
                "extra_command_count",
                collected.get("extra_command_count") if isinstance(collected, dict) else None,
            ),
        ]
    return build_section(title, source, data, metrics)


def product_profile_contract_check_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("artifact_type", data.get("artifact_type")),
            ("executed", data.get("executed")),
            ("replayed", data.get("replayed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("cli_context_present", isinstance(data.get("cli_context"), dict)),
            ("cli_format", get_path(data, ("cli_context", "format"))),
            ("unsupported_format_rejected", get_path(data, ("cli_context", "unsupported_format_rejected"))),
            ("strict_missing", data.get("strict_missing")),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("product_count", get_path(data, ("summary", "product_count"))),
            ("default_required_product_count", get_path(data, ("summary", "default_required_product_count"))),
            ("optional_on_demand_product_count", get_path(data, ("summary", "optional_on_demand_product_count"))),
            ("selected_product_count", get_path(data, ("summary", "selected_product_count"))),
            ("present_artifacts", get_path(data, ("summary", "present_artifacts"))),
            ("missing_artifacts", get_path(data, ("summary", "missing_artifacts"))),
            ("required_missing_artifacts", get_path(data, ("summary", "required_missing_artifacts"))),
            ("passed_records", get_path(data, ("summary", "passed_records"))),
            ("failed_records", get_path(data, ("summary", "failed_records"))),
            ("pending_records", get_path(data, ("summary", "pending_records"))),
            ("not_selected_optional_records", get_path(data, ("summary", "not_selected_optional_records"))),
            ("required_field_pass_rate", get_path(data, ("summary", "required_field_pass_rate"))),
            ("contract_coverage", get_path(data, ("summary", "contract_coverage"))),
            ("boundary_pass_count", get_path(data, ("summary", "boundary_pass_count"))),
            ("execute_gate_pass_count", get_path(data, ("summary", "execute_gate_pass_count"))),
            ("read_issue_count", get_path(data, ("summary", "read_issue_count"))),
            ("malformed_or_invalid_artifact_count", get_path(data, ("summary", "malformed_or_invalid_artifact_count"))),
            ("mode_flag_pass_count", get_path(data, ("summary", "mode_flag_pass_count"))),
            ("claim_scope_pass_count", get_path(data, ("summary", "claim_scope_pass_count"))),
            ("product_evidence_eligible_count", get_path(data, ("summary", "product_evidence_eligible_count"))),
            ("performance_claim_eligible_count", get_path(data, ("summary", "performance_claim_eligible_count"))),
            ("physical_artifact_read_count", get_path(data, ("summary", "physical_artifact_read_count"))),
        ]
    return build_section(title, source, data, metrics)


def product_integration_inventory_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("product_count", get_path(data, ("summary", "product_count"))),
            ("default_required_product_count", get_path(data, ("summary", "default_required_product_count"))),
            ("optional_on_demand_product_count", get_path(data, ("summary", "optional_on_demand_product_count"))),
            ("passed_records", get_path(data, ("summary", "passed_records"))),
            ("failed_records", get_path(data, ("summary", "failed_records"))),
            ("passed_checks", get_path(data, ("summary", "passed_checks"))),
            ("total_checks", get_path(data, ("summary", "total_checks"))),
            ("integration_score", get_path(data, ("summary", "integration_score"))),
        ]
    return build_section(title, source, data, metrics)


def claim_scope_contract_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("cli_context_present", isinstance(data.get("cli_context"), dict)),
            ("cli_format", get_path(data, ("cli_context", "format"))),
            ("unsupported_format_rejected", get_path(data, ("cli_context", "unsupported_format_rejected"))),
            ("strict_missing", data.get("strict_missing")),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("artifact_count", get_path(data, ("summary", "artifact_count"))),
            ("present_count", get_path(data, ("summary", "present_count"))),
            ("pass_count", get_path(data, ("summary", "pass_count"))),
            ("warn_count", get_path(data, ("summary", "warn_count"))),
            ("fail_count", get_path(data, ("summary", "fail_count"))),
            ("pending_count", get_path(data, ("summary", "pending_count"))),
            ("executed_true_count", get_path(data, ("summary", "executed_true_count"))),
            ("executed_false_count", get_path(data, ("summary", "executed_false_count"))),
            ("claim_scope_count", get_path(data, ("summary", "claim_scope_count"))),
            (
                "risky_executed_without_evidence_count",
                get_path(data, ("summary", "risky_executed_without_evidence_count")),
            ),
            ("claim_contract_score", get_path(data, ("summary", "claim_contract_score"))),
        ]
    return build_section(title, source, data, metrics)


def runtime_evidence_freshness_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("target_count", get_path(data, ("summary", "target_count"))),
            ("present_count", get_path(data, ("summary", "present_count"))),
            ("fresh_count", get_path(data, ("summary", "fresh_count"))),
            ("stale_count", get_path(data, ("summary", "stale_count"))),
            ("missing_count", get_path(data, ("summary", "missing_count"))),
            ("stale_or_missing_count", get_path(data, ("summary", "stale_or_missing_count"))),
            ("release_blocking_count", get_path(data, ("summary", "release_blocking_count"))),
            ("freshness_score", get_path(data, ("summary", "freshness_score"))),
            ("release_decision", get_path(data, ("summary", "release_decision"))),
            ("registry_freshness_target_count", get_path(data, ("summary", "registry_freshness_target_count"))),
            ("registry_consumed_freshness_target_count", get_path(data, ("summary", "registry_consumed_freshness_target_count"))),
            ("most_urgent_target", get_path(data, ("summary", "most_urgent_target"))),
        ]
    return build_section(title, source, data, metrics)


def product_replay_alignment_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("audit_type", data.get("audit_type")),
            ("cli_context_present", isinstance(data.get("cli_context"), dict)),
            ("cli_format", get_path(data, ("cli_context", "format"))),
            ("unsupported_format_rejected", get_path(data, ("cli_context", "unsupported_format_rejected"))),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("aligned_rule_count", get_path(data, ("summary", "aligned_rule_count"))),
            ("alignment_decision", get_path(data, ("summary", "alignment_decision"))),
            ("blocked_rule_count", get_path(data, ("summary", "blocked_rule_count"))),
            ("dry_run_rule_count", get_path(data, ("summary", "dry_run_rule_count"))),
            ("pending_rule_count", get_path(data, ("summary", "pending_rule_count"))),
            ("best_alignment_rule", get_path(data, ("summary", "best_alignment_rule"))),
            ("best_alignment_score", get_path(data, ("summary", "best_alignment_score"))),
            ("unique_product_artifact_count", get_path(data, ("summary", "unique_product_artifact_count"))),
            ("unique_native_artifact_count", get_path(data, ("summary", "unique_native_artifact_count"))),
            ("product_contract_ready_count", get_path(data, ("summary", "product_contract_ready_count"))),
            ("native_contract_ready_count", get_path(data, ("summary", "native_contract_ready_count"))),
            ("replay_evidence_product_count", get_path(data, ("summary", "replay_evidence_product_count"))),
            ("execute_evidence_product_count", get_path(data, ("summary", "execute_evidence_product_count"))),
            ("fully_aligned_rule_count", get_path(data, ("summary", "fully_aligned_rule_count"))),
            ("physical_artifact_read_count", get_path(data, ("summary", "physical_artifact_read_count"))),
            ("read_issue_count", get_path(data, ("summary", "read_issue_count"))),
            ("readiness_reverse_dependency_count", get_path(data, ("summary", "readiness_reverse_dependency_count"))),
        ]
    return build_section(title, source, data, metrics)


def deep_dive_contract_check_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        metrics = [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("executed", data.get("executed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("audit_type", data.get("audit_type")),
            ("cli_context_present", isinstance(data.get("cli_context"), dict)),
            ("cli_format", get_path(data, ("cli_context", "format"))),
            ("unsupported_format_rejected", get_path(data, ("cli_context", "unsupported_format_rejected"))),
            ("strict_missing", data.get("strict_missing")),
            ("record_count", len(data.get("records", [])) if isinstance(data.get("records"), list) else None),
            ("target_count", get_path(data, ("summary", "target_count"))),
            ("present_count", get_path(data, ("summary", "present_count"))),
            ("pass_count", get_path(data, ("summary", "pass_count"))),
            ("warn_count", get_path(data, ("summary", "warn_count"))),
            ("fail_count", get_path(data, ("summary", "fail_count"))),
            ("pending_count", get_path(data, ("summary", "pending_count"))),
            ("contract_score", get_path(data, ("summary", "contract_score"))),
            ("gate_decision", get_path(data, ("summary", "gate_decision"))),
        ]
    return build_section(title, source, data, metrics)


def evidence_governance_core_repair_metric_items(
    data: Dict[str, Any],
    evidence: Dict[str, Any],
) -> List[Metric]:
    return [
            ("status", data.get("status")),
            ("mode", data.get("mode")),
            ("artifact_type", data.get("artifact_type")),
            ("executed", data.get("executed")),
            ("replayed", data.get("replayed")),
            ("claim_scope", short_text(data.get("claim_scope"))),
            ("evidence_confidence", evidence["confidence"]),
            ("artifact_count", get_path(data, ("summary", "artifact_count"))),
            ("managed_artifact_count", get_path(data, ("summary", "managed_artifact_count"))),
            ("indexed_artifact_count", get_path(data, ("summary", "indexed_artifact_count"))),
            ("missing_report_count", get_path(data, ("summary", "missing_report_count"))),
            ("missing_matrix_count", get_path(data, ("summary", "missing_matrix_count"))),
            ("missing_summary_count", get_path(data, ("summary", "missing_summary_count"))),
            ("missing_prometheus_count", get_path(data, ("summary", "missing_prometheus_count"))),
            ("lineage_score", get_path(data, ("summary", "lineage_score"))),
            ("warehouse_decision", get_path(data, ("summary", "warehouse_decision"))),
            ("release_decision", get_path(data, ("summary", "release_decision"))),
            ("release_decision_reason", get_path(data, ("summary", "release_decision_reason"))),
            ("source_contract_gate_applied", get_path(data, ("summary", "source_contract_gate_applied"))),
            ("source_contract_gate_blocks_release", get_path(data, ("summary", "source_contract_gate_blocks_release"))),
            ("source_contract_issue_count", get_path(data, ("summary", "source_contract_issue_count"))),
            ("source_contract_blocking_source_count", get_path(data, ("summary", "source_contract_blocking_source_count"))),
            ("release_blocking_signal_count", get_path(data, ("summary", "release_blocking_signal_count"))),
            ("root_cause_count", get_path(data, ("summary", "root_cause_count"))),
            ("blocking_root_cause_count", get_path(data, ("summary", "blocking_root_cause_count"))),
            ("root_cause_repair_card_count", get_path(data, ("summary", "root_cause_repair_card_count"))),
            ("current_repo_repair_card_count", get_path(data, ("summary", "current_repo_repair_card_count"))),
            ("target_product_replay_repair_card_count", get_path(data, ("summary", "target_product_replay_repair_card_count"))),
            ("target_gpu_execute_repair_card_count", get_path(data, ("summary", "target_gpu_execute_repair_card_count"))),
            ("target_machine_repair_card_count", get_path(data, ("summary", "target_machine_repair_card_count"))),
            ("repair_environment_split_status", get_path(data, ("summary", "repair_environment_split_status"))),
            ("repair_environment_split_schema_gap", get_path(data, ("summary", "repair_environment_split_schema_gap"))),
            ("repair_environment_legacy_fallback_used", get_path(data, ("summary", "repair_environment_legacy_fallback_used"))),
            ("repair_environment_legacy_aggregate_retained", get_path(data, ("summary", "repair_environment_legacy_aggregate_retained"))),
            ("flow_check_repair_card_count", get_path(data, ("summary", "flow_check_repair_card_count"))),
            (
                "review_manifest_configured",
                get_path(data, ("summary", "review_manifest_configured")),
            ),
            (
                "review_manifest_present",
                get_path(data, ("summary", "review_manifest_present")),
            ),
            (
                "review_manifest_parse_ok",
                get_path(data, ("summary", "review_manifest_parse_ok")),
            ),
            (
                "review_manifest_override_count",
                get_path(data, ("summary", "review_manifest_override_count")),
            ),
            (
                "review_manifest_applied_override_count",
                get_path(data, ("summary", "review_manifest_applied_override_count")),
            ),
            (
                "review_manifest_invalid_override_count",
                get_path(data, ("summary", "review_manifest_invalid_override_count")),
            ),
            (
                "review_manifest_unmatched_override_count",
                get_path(data, ("summary", "review_manifest_unmatched_override_count")),
            ),
            (
                "review_manifest_closed_file_count",
                get_path(data, ("summary", "review_manifest_closed_file_count")),
            ),
            ("catalog_flow_root_cause_count", get_path(data, ("summary", "catalog_flow_root_cause_count"))),
            (
                "native_core_audit_root_cause_count",
                get_path(data, ("summary", "native_core_audit_root_cause_count")),
            ),
            (
                "native_core_audit_backlog_root_cause_count",
                get_path(data, ("summary", "native_core_audit_backlog_root_cause_count")),
            ),
            (
                "native_core_audit_scope_root_cause_count",
                get_path(data, ("summary", "native_core_audit_scope_root_cause_count")),
            ),
            *summary_domain_metric_pairs(
                data, NATIVE_CORE_AUDIT_CORE_FIELDS, "native_core_audit_"
            ),
            *summary_domain_metric_pairs(
                data,
                CATALOG_FLOW_GOVERNANCE_SUMMARY_FIELDS,
            ),
            ("top_root_cause", get_path(data, ("summary", "top_root_cause"))),
            ("operator_count", get_path(data, ("summary", "operator_count"))),
            ("selected_operator_count", get_path(data, ("summary", "selected_operator_count"))),
            ("selected_operator", get_path(data, ("summary", "selected_operator"))),
            ("execute_requested", get_path(data, ("summary", "execute_requested"))),
            ("benchmark_case_count", get_path(data, ("summary", "benchmark_case_count"))),
            ("correctness_result_count", get_path(data, ("summary", "correctness_result_count"))),
            ("timing_result_count", get_path(data, ("summary", "timing_result_count"))),
            ("google_benchmark_enabled", get_path(data, ("summary", "google_benchmark_enabled"))),
            ("cuda_enabled", get_path(data, ("summary", "cuda_enabled"))),
            ("nvtx_enabled", get_path(data, ("summary", "nvtx_enabled"))),
            ("microbench_decision", get_path(data, ("summary", "microbench_decision"))),
            ("dependency_count", get_path(data, ("summary", "dependency_count"))),
            ("fallback_ready_count", get_path(data, ("summary", "fallback_ready_count"))),
            ("dependency_gate_decision", get_path(data, ("summary", "dependency_gate_decision"))),
            ("sbom_gate_decision", get_path(data, ("summary", "sbom_gate_decision"))),
            ("product_runtime_dependency_count", get_path(data, ("summary", "product_runtime_dependency_count"))),
            ("correlation_count", get_path(data, ("summary", "correlation_count"))),
            ("ready_correlation_count", get_path(data, ("summary", "ready_correlation_count"))),
            ("blocked_correlation_count", get_path(data, ("summary", "blocked_correlation_count"))),
            ("correlation_decision", get_path(data, ("summary", "correlation_decision"))),
            ("stage_count", get_path(data, ("summary", "stage_count"))),
            ("required_stage_count", get_path(data, ("summary", "required_stage_count"))),
            ("ready_required_stage_count", get_path(data, ("summary", "ready_required_stage_count"))),
            ("blocking_stage_count", get_path(data, ("summary", "blocking_stage_count"))),
            ("executed_input_count", get_path(data, ("summary", "executed_input_count"))),
            ("execute_verified_input_count", get_path(data, ("summary", "execute_verified_input_count"))),
            ("non_gpu_review_signal_count", get_path(data, ("summary", "non_gpu_review_signal_count"))),
            ("non_gpu_repair_path_count", get_path(data, ("summary", "non_gpu_repair_path_count"))),
            ("non_gpu_current_repo_repair_count", get_path(data, ("summary", "non_gpu_current_repo_repair_count"))),
            ("non_gpu_target_machine_repair_count", get_path(data, ("summary", "non_gpu_target_machine_repair_count"))),
            (
                "non_gpu_repair_path_source_location_count",
                get_path(data, ("summary", "non_gpu_repair_path_source_location_count")),
            ),
            (
                "non_gpu_repair_path_source_location_gap_count",
                get_path(data, ("summary", "non_gpu_repair_path_source_location_gap_count")),
            ),
            (
                "non_gpu_repair_path_evidence_counter_count",
                get_path(data, ("summary", "non_gpu_repair_path_evidence_counter_count")),
            ),
            (
                "non_gpu_repair_path_group_count",
                get_path(data, ("summary", "non_gpu_repair_path_group_count")),
            ),
            (
                "non_gpu_repair_path_environment_group_count",
                get_path(data, ("summary", "non_gpu_repair_path_environment_group_count")),
            ),
            (
                "non_gpu_repair_path_category_group_count",
                get_path(data, ("summary", "non_gpu_repair_path_category_group_count")),
            ),
            (
                "non_gpu_repair_path_current_repo_action_group_count",
                get_path(
                    data,
                    ("summary", "non_gpu_repair_path_current_repo_action_group_count"),
                ),
            ),
            (
                "non_gpu_repair_path_target_or_validation_action_group_count",
                get_path(
                    data,
                    (
                        "summary",
                        "non_gpu_repair_path_target_or_validation_action_group_count",
                    ),
                ),
            ),
            (
                "non_gpu_repair_path_group_plan_step_count",
                get_path(data, ("summary", "non_gpu_repair_path_group_plan_step_count")),
            ),
            (
                "non_gpu_repair_path_group_plan_current_repo_step_count",
                get_path(
                    data,
                    (
                        "summary",
                        "non_gpu_repair_path_group_plan_current_repo_step_count",
                    ),
                ),
            ),
            (
                "non_gpu_repair_path_group_plan_target_or_validation_step_count",
                get_path(
                    data,
                    (
                        "summary",
                        "non_gpu_repair_path_group_plan_target_or_validation_step_count",
                    ),
                ),
            ),
            ("catalog_flow_repair_path_count", get_path(data, ("summary", "catalog_flow_repair_path_count"))),
            (
                "catalog_flow_repair_path_source_location_count",
                get_path(data, ("summary", "catalog_flow_repair_path_source_location_count")),
            ),
            (
                "catalog_flow_repair_path_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_repair_path_source_location_gap_count")),
            ),
            (
                "catalog_flow_repair_path_evidence_counter_count",
                get_path(data, ("summary", "catalog_flow_repair_path_evidence_counter_count")),
            ),
            (
                "catalog_flow_field_coverage_repair_path_count",
                get_path(data, ("summary", "catalog_flow_field_coverage_repair_path_count")),
            ),
            (
                "catalog_flow_field_coverage_repair_path_source_location_count",
                get_path(data, ("summary", "catalog_flow_field_coverage_repair_path_source_location_count")),
            ),
            (
                "catalog_flow_field_coverage_repair_path_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_field_coverage_repair_path_source_location_gap_count")),
            ),
            (
                "catalog_flow_field_coverage_repair_path_evidence_counter_count",
                get_path(data, ("summary", "catalog_flow_field_coverage_repair_path_evidence_counter_count")),
            ),
            (
                "catalog_flow_fallback_removal_shortest_path_repair_path_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_repair_path_count")),
            ),
            (
                "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count")),
            ),
            (
                "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count")),
            ),
            (
                "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count")),
            ),
            (
                "catalog_flow_fallback_removal_parity_gate_repair_path_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_repair_path_count")),
            ),
            (
                "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count")),
            ),
            (
                "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count")),
            ),
            (
                "catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_present",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_present")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_decision",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_decision")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_removal_allowed",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_removal_allowed")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_blocker_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_blocker_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_fallback_surface_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_current_repo_action_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_target_machine_action_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_source_location_gap_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count")),
            ),
            (
                "evidence_query_catalog_flow_fallback_removal_parity_gate_blocks_removal",
                get_path(data, ("summary", "evidence_query_catalog_flow_fallback_removal_parity_gate_blocks_removal")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_present",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_present")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_state",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_state")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_step_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_current_repo_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_target_machine_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_blocks_removal",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_shortest_path_blocks_removal")),
            ),
    ]


def evidence_governance_root_cause_product_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return _summary_metric_items(
        data,
        "root_cause_catalog_flow_fallback_removal_shortest_path_repair_path_count",
        "root_cause_catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_present",
        "root_cause_catalog_flow_fallback_removal_parity_gate_decision",
        "root_cause_catalog_flow_fallback_removal_parity_gate_removal_allowed",
        "root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal",
        "root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_count",
        "root_cause_catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
        "root_cause_product_replay_semantic_linkage_root_cause_count",
        "root_cause_product_replay_semantic_linkage_status",
        "root_cause_product_replay_semantic_linkage_blocker_count",
        "root_cause_product_replay_semantic_linkage_pending_event_count",
        "root_cause_product_replay_semantic_linkage_pending_rule_count",
        "root_cause_product_replay_semantic_linkage_blocked_correlation_count",
        "root_cause_product_replay_semantic_linkage_target_or_validation_action_count",
        "root_cause_product_replay_semantic_linkage_first_blocker",
        "root_cause_product_replay_semantic_linkage_next_action",
        "root_cause_product_replay_semantic_linkage_blocks_release",
        "root_cause_product_replay_family_gate_root_cause_count",
        "root_cause_product_replay_family_gate_status",
        "root_cause_product_replay_family_gate_blocker_count",
        "root_cause_product_replay_family_gate_semantic_linkage_blocker_count",
        "root_cause_product_replay_family_gate_replay_or_execute_missing_count",
        "root_cause_product_replay_family_gate_target_or_validation_action_count",
        "root_cause_product_replay_family_gate_target_product_replay_action_count",
        "root_cause_product_replay_family_gate_target_gpu_execute_action_count",
        "root_cause_product_replay_family_gate_typed_environment_split_present",
        "root_cause_product_replay_family_gate_source_artifact_count",
        "root_cause_product_replay_family_gate_first_source_artifact",
        "root_cause_product_replay_family_gate_next_action",
        "root_cause_product_replay_family_gate_blocks_release",
        "root_cause_product_replay_family_gate_repair_path_count",
        "root_cause_product_replay_family_gate_repair_path_source_location_gap_count",
        "root_cause_native_core_audit_review_coverage_gate_root_cause_count",
        "root_cause_native_core_audit_review_manifest_root_cause_count",
        "root_cause_native_core_audit_review_manifest_invalid_override_count",
        "root_cause_native_core_audit_review_manifest_unmatched_override_count",
        "root_cause_native_core_audit_review_manifest_repair_path_count",
        "root_cause_native_core_audit_review_coverage_state",
        "root_cause_native_core_audit_review_coverage_open_file_count",
        "root_cause_native_core_audit_review_coverage_high_priority_open_file_count",
        "root_cause_native_core_audit_review_coverage_source_location_gap_count",
        "root_cause_native_core_audit_review_coverage_primary_blocker",
        "root_cause_native_core_audit_review_coverage_first_open_file",
        "root_cause_native_core_audit_review_coverage_blocks_full_audit_closure",
        "root_cause_native_core_audit_review_coverage_repair_path_count",
        "root_cause_native_core_audit_review_coverage_repair_path_source_location_gap_count",
        "product_replay_semantic_linkage_repair_path_count",
        "product_replay_semantic_linkage_repair_path_source_location_gap_count",
        "evidence_query_product_replay_semantic_linkage_repair_path_count",
        "evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count",
        "root_cause_product_replay_semantic_linkage_repair_path_count",
        "root_cause_product_replay_semantic_linkage_repair_path_source_location_gap_count",
        "evidence_query_snapshot_diff_repair_path_count",
        "evidence_query_snapshot_diff_repair_path_source_location_count",
        "evidence_query_snapshot_diff_repair_path_source_location_gap_count",
        "evidence_query_snapshot_diff_repair_path_evidence_counter_count",
        "evidence_query_snapshot_diff_added_deep_path_count",
        "evidence_query_snapshot_diff_removed_deep_path_count",
        "evidence_query_snapshot_diff_changed_deep_value_count",
        "evidence_query_snapshot_diff_deep_semantic_truncated",
        "evidence_query_snapshot_diff_deep_semantic_decision",
        "evidence_query_snapshot_to_release_link_count",
        "evidence_query_snapshot_to_release_blocker_count",
        "evidence_query_snapshot_to_release_group_plan_step",
        "evidence_query_snapshot_to_release_current_repo_action_count",
        "blocker_repair_card_link_count",
        "blocker_repair_card_linked_count",
        "blocker_repair_card_unmatched_release_path_count",
        "blocker_repair_card_unmatched_root_cause_count",
        "blocker_repair_card_current_repo_action_count",
        "blocker_repair_card_target_or_validation_action_count",
        "blocker_repair_card_link_status",
    )


def evidence_governance_query_repair_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "evidence_query_repair_card_group_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_count")),
            ),
            (
                "evidence_query_repair_card_group_card_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_card_count")),
            ),
            (
                "evidence_query_repair_card_group_current_repo_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_current_repo_count")),
            ),
            (
                "evidence_query_repair_card_group_current_server_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_current_server_count")),
            ),
            (
                "evidence_query_repair_card_group_target_machine_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_target_machine_count")),
            ),
            (
                "evidence_query_repair_card_group_target_product_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_target_product_count")),
            ),
            (
                "evidence_query_repair_card_group_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_repair_card_group_source_location_gap_count")),
            ),
            (
                "evidence_query_repair_group_summary_present",
                get_path(data, ("summary", "evidence_query_repair_group_summary_present")),
            ),
            (
                "evidence_query_repair_group_exact_wiring_location_count",
                get_path(data, ("summary", "evidence_query_repair_group_exact_wiring_location_count")),
            ),
            (
                "evidence_query_repair_group_affected_claim_count",
                get_path(data, ("summary", "evidence_query_repair_group_affected_claim_count")),
            ),
            (
                "evidence_query_repair_narrative_present",
                get_path(data, ("summary", "evidence_query_repair_narrative_present")),
            ),
            (
                "evidence_query_repair_narrative_decision",
                get_path(data, ("summary", "evidence_query_repair_narrative_decision")),
            ),
            (
                "evidence_query_repair_narrative_readiness_state",
                get_path(data, ("summary", "evidence_query_repair_narrative_readiness_state")),
            ),
            (
                "evidence_query_repair_narrative_step_count",
                get_path(data, ("summary", "evidence_query_repair_narrative_step_count")),
            ),
            (
                "evidence_query_repair_narrative_current_repo_action_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_narrative_current_repo_action_count"),
                ),
            ),
            (
                "evidence_query_repair_narrative_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_narrative_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_narrative_source_location_gap_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_narrative_source_location_gap_count"),
                ),
            ),
            (
                "evidence_query_repair_narrative_readiness_blocker_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_narrative_readiness_blocker_count"),
                ),
            ),
            (
                "evidence_query_repair_narrative_affected_claim_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_narrative_affected_claim_count"),
                ),
            ),
            (
                "evidence_query_repair_narrative_exact_wiring_location_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_narrative_exact_wiring_location_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_group_primary_action_present",
                get_path(data, ("summary", "evidence_query_repair_group_primary_action_present")),
            ),
            (
                "evidence_query_repair_group_primary_source_location_present",
                get_path(data, ("summary", "evidence_query_repair_group_primary_source_location_present")),
            ),
            (
                "evidence_query_repair_group_primary_environment",
                get_path(data, ("summary", "evidence_query_repair_group_primary_environment")),
            ),
            (
                "evidence_query_repair_group_primary_category",
                get_path(data, ("summary", "evidence_query_repair_group_primary_category")),
            ),
            (
                "evidence_query_repair_group_primary_artifact",
                get_path(data, ("summary", "evidence_query_repair_group_primary_artifact")),
            ),
            (
                "evidence_query_repair_group_navigation_step_count",
                get_path(data, ("summary", "evidence_query_repair_group_navigation_step_count")),
            ),
            (
                "evidence_query_repair_group_navigation_current_repo_step_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_group_navigation_current_repo_step_count"),
                ),
            ),
            (
                "evidence_query_repair_group_navigation_target_or_validation_step_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_group_navigation_target_or_validation_step_count",
                    ),
                ),
            ),
            (
                "evidence_query_source_location_worklist_item_count",
                get_path(data, ("summary", "evidence_query_source_location_worklist_item_count")),
            ),
            (
                "evidence_query_source_location_worklist_current_repo_item_count",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_current_repo_item_count"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_target_or_validation_item_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_source_location_worklist_target_or_validation_item_count",
                    ),
                ),
            ),
            (
                "evidence_query_source_location_worklist_gap_count",
                get_path(data, ("summary", "evidence_query_source_location_worklist_gap_count")),
            ),
            (
                "evidence_query_source_location_worklist_focus_present",
                get_path(data, ("summary", "evidence_query_source_location_worklist_focus_present")),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_present",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_item_present"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_gap_present",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_gap_present"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_source_location_worklist_focus_first_item_current_repo_action_required",
                    ),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_source_location_worklist_focus_first_item_target_or_validation_action_required",
                    ),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_environment",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_item_environment"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_artifact",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_item_artifact"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_item_category",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_item_category"),
                ),
            ),
            (
                "evidence_query_source_location_worklist_focus_first_gap_category",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_worklist_focus_first_gap_category"),
                ),
            ),
    ]


def evidence_governance_query_source_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "evidence_query_source_location_category_count",
                get_path(data, ("summary", "evidence_query_source_location_category_count")),
            ),
            (
                "evidence_query_source_location_active_category_count",
                get_path(data, ("summary", "evidence_query_source_location_active_category_count")),
            ),
            (
                "evidence_query_source_location_exact_category_count",
                get_path(data, ("summary", "evidence_query_source_location_exact_category_count")),
            ),
            (
                "evidence_query_source_location_gap_category_count",
                get_path(data, ("summary", "evidence_query_source_location_gap_category_count")),
            ),
            (
                "evidence_query_source_location_current_repo_gap_category_count",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_current_repo_gap_category_count"),
                ),
            ),
            (
                "evidence_query_source_location_target_or_validation_gap_category_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_source_location_target_or_validation_gap_category_count",
                    ),
                ),
            ),
            (
                "evidence_query_source_location_gap_repair_card_count",
                get_path(data, ("summary", "evidence_query_source_location_gap_repair_card_count")),
            ),
            (
                "evidence_query_source_location_category_exactness_percent",
                get_path(
                    data,
                    ("summary", "evidence_query_source_location_category_exactness_percent"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_surface_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_surface_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_catalog_managed_surface_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_catalog_flow_consumer_field_coverage_catalog_managed_surface_count",
                    ),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_fallback_surface_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_fallback_surface_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_required_field_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_required_field_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_covered_field_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_covered_field_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_field_gap_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_field_gap_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_parity_refresh_required_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_catalog_flow_consumer_field_coverage_parity_refresh_required_count",
                    ),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_current_repo_action_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_current_repo_action_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_target_machine_action_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_target_machine_action_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_repair_card_count",
                get_path(
                    data,
                    ("summary", "evidence_query_catalog_flow_consumer_field_coverage_repair_card_count"),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_catalog_flow_consumer_field_coverage_current_repo_repair_card_count",
                    ),
                ),
            ),
            (
                "evidence_query_catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_catalog_flow_consumer_field_coverage_target_machine_repair_card_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_path_rank_count",
                get_path(data, ("summary", "evidence_query_repair_card_path_rank_count")),
            ),
            (
                "evidence_query_repair_card_path_current_repo_rank_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_card_path_current_repo_rank_count"),
                ),
            ),
            (
                "evidence_query_repair_card_path_target_or_validation_rank_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_path_target_or_validation_rank_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_path_source_location_gap_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_card_path_source_location_gap_count"),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_link_count",
                get_path(
                    data,
                    ("summary", "evidence_query_repair_card_rank_readiness_link_count"),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_current_repo_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_rank_readiness_current_repo_link_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_target_or_validation_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_rank_readiness_target_or_validation_link_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_report_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_rank_readiness_report_ready_link_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_replay_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_rank_readiness_replay_ready_link_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_rank_readiness_execute_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_repair_card_rank_readiness_execute_ready_link_count",
                    ),
                ),
            ),
            (
                "evidence_query_repair_card_source_exactness_card_count",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_card_count")),
            ),
            (
                "evidence_query_repair_card_source_exactness_exact_card_count",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_exact_card_count")),
            ),
            (
                "evidence_query_repair_card_source_exactness_gap_card_count",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_gap_card_count")),
            ),
            (
                "evidence_query_repair_card_source_exactness_current_repo_gap_count",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_current_repo_gap_count")),
            ),
            (
                "evidence_query_repair_card_source_exactness_target_or_validation_gap_count",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_target_or_validation_gap_count")),
            ),
            (
                "evidence_query_repair_card_source_exactness_percent",
                get_path(data, ("summary", "evidence_query_repair_card_source_exactness_percent")),
            ),
            (
                "evidence_query_explain_tree_node_count",
                get_path(data, ("summary", "evidence_query_explain_tree_node_count")),
            ),
            (
                "evidence_query_explain_tree_source_exact_node_count",
                get_path(data, ("summary", "evidence_query_explain_tree_source_exact_node_count")),
            ),
            (
                "evidence_query_explain_tree_source_gap_node_count",
                get_path(data, ("summary", "evidence_query_explain_tree_source_gap_node_count")),
            ),
            (
                "evidence_query_explain_tree_current_repo_source_gap_count",
                get_path(data, ("summary", "evidence_query_explain_tree_current_repo_source_gap_count")),
            ),
            (
                "evidence_query_explain_tree_target_or_validation_source_gap_count",
                get_path(data, ("summary", "evidence_query_explain_tree_target_or_validation_source_gap_count")),
            ),
            (
                "evidence_query_explain_tree_source_exactness_percent",
                get_path(data, ("summary", "evidence_query_explain_tree_source_exactness_percent")),
            ),
            (
                "native_core_audit_repair_path_evidence_counter_count",
                get_path(data, ("summary", "native_core_audit_repair_path_evidence_counter_count")),
            ),
            (
                "native_core_audit_scope_repair_path_evidence_counter_count",
                get_path(data, ("summary", "native_core_audit_scope_repair_path_evidence_counter_count")),
            ),
            (
                "native_core_audit_backlog_repair_path_evidence_counter_count",
                get_path(data, ("summary", "native_core_audit_backlog_repair_path_evidence_counter_count")),
            ),
    ]


def evidence_governance_query_operation_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "query_operation_selector_present",
                get_path(data, ("summary", "query_operation_selector_present")),
            ),
            (
                "query_operation_target_present",
                get_path(data, ("summary", "query_operation_target_present")),
            ),
            (
                "query_operation_snapshot_inputs_required",
                get_path(data, ("summary", "query_operation_snapshot_inputs_required")),
            ),
            (
                "query_operation_snapshot_inputs_configured",
                get_path(data, ("summary", "query_operation_snapshot_inputs_configured")),
            ),
            (
                "query_operation_selected_result_present",
                get_path(data, ("summary", "query_operation_selected_result_present")),
            ),
            (
                "query_operation_selected_result_input_ready",
                get_path(data, ("summary", "query_operation_selected_result_input_ready")),
            ),
            (
                "query_operation_selected_result_payload_count",
                get_path(data, ("summary", "query_operation_selected_result_payload_count")),
            ),
            (
                "query_operation_selected_result_kind",
                get_path(data, ("summary", "query_operation_selected_result_kind")),
            ),
            (
                "query_operation_show_artifact_candidate_count",
                get_path(data, ("summary", "query_operation_show_artifact_candidate_count")),
            ),
            (
                "query_operation_show_artifact_candidate_truncated",
                get_path(data, ("summary", "query_operation_show_artifact_candidate_truncated")),
            ),
            (
                "query_operation_show_artifact_exact_match",
                get_path(data, ("summary", "query_operation_show_artifact_exact_match")),
            ),
            (
                "query_operation_show_artifact_selector_ambiguous",
                get_path(data, ("summary", "query_operation_show_artifact_selector_ambiguous")),
            ),
            (
                "query_operation_show_artifact_selection_strategy",
                get_path(data, ("summary", "query_operation_show_artifact_selection_strategy")),
            ),
            (
                "query_operation_payload_selector_focus_present",
                get_path(data, ("summary", "query_operation_payload_selector_focus_present")),
            ),
            (
                "query_operation_payload_selector_focus_status",
                get_path(data, ("summary", "query_operation_payload_selector_focus_status")),
            ),
            (
                "query_operation_payload_selector_focus_match_count",
                get_path(data, ("summary", "query_operation_payload_selector_focus_match_count")),
            ),
            (
                "query_operation_payload_selector_focus_ambiguous",
                get_path(data, ("summary", "query_operation_payload_selector_focus_ambiguous")),
            ),
            (
                "query_operation_payload_selector_focus_source_location_gap_count",
                get_path(data, ("summary", "query_operation_payload_selector_focus_source_location_gap_count")),
            ),
            (
                "query_operation_selector_resolution_status",
                get_path(data, ("summary", "query_operation_selector_resolution_status")),
            ),
            (
                "query_operation_selector_kind",
                get_path(data, ("summary", "query_operation_selector_kind")),
            ),
            (
                "query_operation_selector_match_count",
                get_path(data, ("summary", "query_operation_selector_match_count")),
            ),
            (
                "query_operation_selector_ambiguous",
                get_path(data, ("summary", "query_operation_selector_ambiguous")),
            ),
            (
                "query_operation_selector_repair_card_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_count")),
            ),
            (
                "query_operation_selector_repair_card_emitted_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_emitted_count")),
            ),
            (
                "query_operation_selector_repair_card_truncated",
                get_path(data, ("summary", "query_operation_selector_repair_card_truncated")),
            ),
            (
                "query_operation_selector_repair_card_export_status",
                get_path(data, ("summary", "query_operation_selector_repair_card_export_status")),
            ),
            (
                "query_operation_selector_repair_card_embedded_limit",
                get_path(data, ("summary", "query_operation_selector_repair_card_embedded_limit")),
            ),
            (
                "query_operation_selector_repair_card_full_export_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_full_export_count")),
            ),
            (
                "query_operation_selector_repair_card_external_export_configured",
                get_path(data, ("summary", "query_operation_selector_repair_card_external_export_configured")),
            ),
            (
                "query_operation_selector_repair_card_external_export_written",
                get_path(data, ("summary", "query_operation_selector_repair_card_external_export_written")),
            ),
            (
                "query_operation_selector_repair_card_external_export_status",
                get_path(data, ("summary", "query_operation_selector_repair_card_external_export_status")),
            ),
            (
                "query_operation_selector_repair_card_current_repo_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_current_repo_count")),
            ),
            (
                "query_operation_selector_repair_card_current_server_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_current_server_count")),
            ),
            (
                "query_operation_selector_repair_card_target_machine_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_target_machine_count")),
            ),
            (
                "query_operation_selector_repair_card_target_product_count",
                get_path(data, ("summary", "query_operation_selector_repair_card_target_product_count")),
            ),
            (
                "query_operation_selector_repair_card_source_location_gap_card_count",
                get_path(
                    data,
                    ("summary", "query_operation_selector_repair_card_source_location_gap_card_count"),
                ),
            ),
            (
                "query_operation_selector_repair_card_export_review_status",
                get_path(data, ("summary", "query_operation_selector_repair_card_export_review_status")),
            ),
            (
                "query_operation_selector_repair_card_export_blocker_reason",
                get_path(data, ("summary", "query_operation_selector_repair_card_export_blocker_reason")),
            ),
            (
                "query_operation_selector_repair_card_external_file_required",
                get_path(data, ("summary", "query_operation_selector_repair_card_external_file_required")),
            ),
            (
                "query_operation_selector_source_location_count",
                get_path(data, ("summary", "query_operation_selector_source_location_count")),
            ),
            (
                "query_operation_selector_source_location_gap_count",
                get_path(data, ("summary", "query_operation_selector_source_location_gap_count")),
            ),
            (
                "query_operation_selector_current_repo_action_count",
                get_path(data, ("summary", "query_operation_selector_current_repo_action_count")),
            ),
            (
                "query_operation_selector_current_server_action_count",
                get_path(data, ("summary", "query_operation_selector_current_server_action_count")),
            ),
            (
                "query_operation_selector_target_machine_action_count",
                get_path(data, ("summary", "query_operation_selector_target_machine_action_count")),
            ),
            (
                "query_operation_selector_target_product_action_count",
                get_path(data, ("summary", "query_operation_selector_target_product_action_count")),
            ),
            (
                "query_operation_selector_action_plan_present",
                get_path(data, ("summary", "query_operation_selector_action_plan_present")),
            ),
            (
                "query_operation_selector_action_plan_decision",
                get_path(data, ("summary", "query_operation_selector_action_plan_decision")),
            ),
            (
                "query_operation_selector_action_plan_step_count",
                get_path(data, ("summary", "query_operation_selector_action_plan_step_count")),
            ),
            (
                "query_operation_selector_action_plan_current_repo_step_count",
                get_path(
                    data,
                    ("summary", "query_operation_selector_action_plan_current_repo_step_count"),
                ),
            ),
            (
                "query_operation_selector_action_plan_target_or_validation_step_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_operation_selector_action_plan_target_or_validation_step_count",
                    ),
                ),
            ),
            (
                "query_operation_selector_action_plan_source_location_gap_count",
                get_path(
                    data,
                    ("summary", "query_operation_selector_action_plan_source_location_gap_count"),
                ),
            ),
            (
                "query_operation_selector_action_plan_readiness_blocker_count",
                get_path(
                    data,
                    ("summary", "query_operation_selector_action_plan_readiness_blocker_count"),
                ),
            ),
            (
                "query_operation_selector_action_plan_readiness_state",
                get_path(data, ("summary", "query_operation_selector_action_plan_readiness_state")),
            ),
            (
                "operation_catalog_supported_count",
                get_path(data, ("summary", "operation_catalog_supported_count")),
            ),
            (
                "operation_catalog_read_only_count",
                get_path(data, ("summary", "operation_catalog_read_only_count")),
            ),
            (
                "operation_catalog_snapshot_required_count",
                get_path(data, ("summary", "operation_catalog_snapshot_required_count")),
            ),
            (
                "operation_catalog_selector_recommended_count",
                get_path(data, ("summary", "operation_catalog_selector_recommended_count")),
            ),
            (
                "operation_catalog_target_required_count",
                get_path(data, ("summary", "operation_catalog_target_required_count")),
            ),
            (
                "query_ux_repair_card_count",
                get_path(data, ("summary", "query_ux_repair_card_count")),
            ),
    ]


def evidence_governance_query_repair_ux_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "query_ux_repair_card_group_count",
                get_path(data, ("summary", "query_ux_repair_card_group_count")),
            ),
            (
                "query_ux_repair_card_group_card_count",
                get_path(data, ("summary", "query_ux_repair_card_group_card_count")),
            ),
            (
                "query_ux_repair_card_group_current_repo_count",
                get_path(data, ("summary", "query_ux_repair_card_group_current_repo_count")),
            ),
            (
                "query_ux_repair_card_group_current_server_count",
                get_path(data, ("summary", "query_ux_repair_card_group_current_server_count")),
            ),
            (
                "query_ux_repair_card_group_target_machine_count",
                get_path(data, ("summary", "query_ux_repair_card_group_target_machine_count")),
            ),
            (
                "query_ux_repair_card_group_target_product_count",
                get_path(data, ("summary", "query_ux_repair_card_group_target_product_count")),
            ),
            (
                "query_ux_repair_card_group_source_location_gap_count",
                get_path(data, ("summary", "query_ux_repair_card_group_source_location_gap_count")),
            ),
            (
                "query_ux_repair_group_summary_present",
                get_path(data, ("summary", "query_ux_repair_group_summary_present")),
            ),
            (
                "query_ux_repair_group_exact_wiring_location_count",
                get_path(data, ("summary", "query_ux_repair_group_exact_wiring_location_count")),
            ),
            (
                "query_ux_repair_group_affected_claim_count",
                get_path(data, ("summary", "query_ux_repair_group_affected_claim_count")),
            ),
            (
                "query_ux_repair_group_primary_action_present",
                get_path(data, ("summary", "query_ux_repair_group_primary_action_present")),
            ),
            (
                "query_ux_repair_group_primary_source_location_present",
                get_path(data, ("summary", "query_ux_repair_group_primary_source_location_present")),
            ),
            (
                "query_ux_repair_group_primary_environment",
                get_path(data, ("summary", "query_ux_repair_group_primary_environment")),
            ),
            (
                "query_ux_repair_group_primary_category",
                get_path(data, ("summary", "query_ux_repair_group_primary_category")),
            ),
            (
                "query_ux_repair_group_primary_artifact",
                get_path(data, ("summary", "query_ux_repair_group_primary_artifact")),
            ),
            (
                "query_ux_repair_narrative_present",
                get_path(data, ("summary", "query_ux_repair_narrative_present")),
            ),
            (
                "query_ux_repair_narrative_decision",
                get_path(data, ("summary", "query_ux_repair_narrative_decision")),
            ),
            (
                "query_ux_repair_narrative_readiness_state",
                get_path(data, ("summary", "query_ux_repair_narrative_readiness_state")),
            ),
            (
                "query_ux_repair_narrative_step_count",
                get_path(data, ("summary", "query_ux_repair_narrative_step_count")),
            ),
            (
                "query_ux_repair_narrative_current_repo_action_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_narrative_current_repo_action_count"),
                ),
            ),
            (
                "query_ux_repair_narrative_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_narrative_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_narrative_source_location_gap_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_narrative_source_location_gap_count"),
                ),
            ),
            (
                "query_ux_repair_narrative_readiness_blocker_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_narrative_readiness_blocker_count"),
                ),
            ),
            (
                "query_ux_repair_narrative_affected_claim_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_narrative_affected_claim_count"),
                ),
            ),
            (
                "query_ux_repair_narrative_exact_wiring_location_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_narrative_exact_wiring_location_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_group_navigation_step_count",
                get_path(data, ("summary", "query_ux_repair_group_navigation_step_count")),
            ),
            (
                "query_ux_repair_group_navigation_current_repo_step_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_group_navigation_current_repo_step_count"),
                ),
            ),
            (
                "query_ux_repair_group_navigation_target_or_validation_step_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_group_navigation_target_or_validation_step_count",
                    ),
                ),
            ),
    ]


def evidence_governance_query_navigation_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "query_ux_source_location_worklist_item_count",
                get_path(data, ("summary", "query_ux_source_location_worklist_item_count")),
            ),
            (
                "query_ux_source_location_worklist_current_repo_item_count",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_current_repo_item_count"),
                ),
            ),
            (
                "query_ux_source_location_worklist_target_or_validation_item_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_source_location_worklist_target_or_validation_item_count",
                    ),
                ),
            ),
            (
                "query_ux_source_location_worklist_gap_count",
                get_path(data, ("summary", "query_ux_source_location_worklist_gap_count")),
            ),
            (
                "query_ux_source_location_worklist_focus_present",
                get_path(data, ("summary", "query_ux_source_location_worklist_focus_present")),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_present",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_item_present"),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_gap_present",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_gap_present"),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_current_repo_action_required",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_source_location_worklist_focus_first_item_current_repo_action_required",
                    ),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required",
                    ),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_environment",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_item_environment"),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_artifact",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_item_artifact"),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_item_category",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_item_category"),
                ),
            ),
            (
                "query_ux_source_location_worklist_focus_first_gap_category",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_worklist_focus_first_gap_category"),
                ),
            ),
            (
                "query_ux_source_location_category_count",
                get_path(data, ("summary", "query_ux_source_location_category_count")),
            ),
            (
                "query_ux_source_location_active_category_count",
                get_path(data, ("summary", "query_ux_source_location_active_category_count")),
            ),
            (
                "query_ux_source_location_exact_category_count",
                get_path(data, ("summary", "query_ux_source_location_exact_category_count")),
            ),
            (
                "query_ux_source_location_gap_category_count",
                get_path(data, ("summary", "query_ux_source_location_gap_category_count")),
            ),
            (
                "query_ux_source_location_current_repo_gap_category_count",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_current_repo_gap_category_count"),
                ),
            ),
            (
                "query_ux_source_location_target_or_validation_gap_category_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_source_location_target_or_validation_gap_category_count",
                    ),
                ),
            ),
            (
                "query_ux_source_location_gap_repair_card_count",
                get_path(data, ("summary", "query_ux_source_location_gap_repair_card_count")),
            ),
            (
                "query_ux_source_location_category_exactness_percent",
                get_path(
                    data,
                    ("summary", "query_ux_source_location_category_exactness_percent"),
                ),
            ),
            (
                "query_ux_repair_card_path_rank_count",
                get_path(data, ("summary", "query_ux_repair_card_path_rank_count")),
            ),
            (
                "query_ux_repair_card_path_current_repo_rank_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_card_path_current_repo_rank_count"),
                ),
            ),
            (
                "query_ux_repair_card_path_target_or_validation_rank_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_path_target_or_validation_rank_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_path_source_location_gap_count",
                get_path(data, ("summary", "query_ux_repair_card_path_source_location_gap_count")),
            ),
            (
                "query_ux_repair_card_rank_readiness_link_count",
                get_path(
                    data,
                    ("summary", "query_ux_repair_card_rank_readiness_link_count"),
                ),
            ),
            (
                "query_ux_repair_card_rank_readiness_current_repo_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_rank_readiness_current_repo_link_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_rank_readiness_target_or_validation_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_rank_readiness_target_or_validation_link_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_rank_readiness_report_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_rank_readiness_report_ready_link_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_rank_readiness_replay_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_rank_readiness_replay_ready_link_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_rank_readiness_execute_ready_link_count",
                get_path(
                    data,
                    (
                        "summary",
                        "query_ux_repair_card_rank_readiness_execute_ready_link_count",
                    ),
                ),
            ),
            (
                "query_ux_repair_card_source_exactness_card_count",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_card_count")),
            ),
            (
                "query_ux_repair_card_source_exactness_exact_card_count",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_exact_card_count")),
            ),
            (
                "query_ux_repair_card_source_exactness_gap_card_count",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_gap_card_count")),
            ),
            (
                "query_ux_repair_card_source_exactness_current_repo_gap_count",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_current_repo_gap_count")),
            ),
            (
                "query_ux_repair_card_source_exactness_target_or_validation_gap_count",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_target_or_validation_gap_count")),
            ),
            (
                "query_ux_repair_card_source_exactness_percent",
                get_path(data, ("summary", "query_ux_repair_card_source_exactness_percent")),
            ),
            (
                "query_ux_explain_tree_node_count",
                get_path(data, ("summary", "query_ux_explain_tree_node_count")),
            ),
            (
                "query_ux_explain_tree_source_exact_node_count",
                get_path(data, ("summary", "query_ux_explain_tree_source_exact_node_count")),
            ),
            (
                "query_ux_explain_tree_source_gap_node_count",
                get_path(data, ("summary", "query_ux_explain_tree_source_gap_node_count")),
            ),
            (
                "query_ux_explain_tree_current_repo_source_gap_count",
                get_path(data, ("summary", "query_ux_explain_tree_current_repo_source_gap_count")),
            ),
            (
                "query_ux_explain_tree_target_or_validation_source_gap_count",
                get_path(data, ("summary", "query_ux_explain_tree_target_or_validation_source_gap_count")),
            ),
            (
                "query_ux_explain_tree_source_exactness_percent",
                get_path(data, ("summary", "query_ux_explain_tree_source_exactness_percent")),
            ),
            (
                "query_ux_blocker_drilldown_record_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_record_count")),
            ),
            (
                "query_ux_blocker_drilldown_category_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_category_count")),
            ),
            (
                "query_ux_blocker_drilldown_source_location_gap_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_source_location_gap_count")),
            ),
            (
                "query_ux_blocker_drilldown_affected_claim_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_affected_claim_count")),
            ),
            (
                "query_ux_blocker_drilldown_exact_wiring_location_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_exact_wiring_location_count")),
            ),
            (
                "query_ux_blocker_drilldown_current_repo_action_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_current_repo_action_count")),
            ),
            (
                "query_ux_blocker_drilldown_current_server_action_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_current_server_action_count")),
            ),
            (
                "query_ux_blocker_drilldown_target_machine_action_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_target_machine_action_count")),
            ),
            (
                "query_ux_blocker_drilldown_target_product_action_count",
                get_path(data, ("summary", "query_ux_blocker_drilldown_target_product_action_count")),
            ),
            (
                "query_ux_blocker_category_count",
                get_path(data, ("summary", "query_ux_blocker_category_count")),
            ),
            (
                "query_ux_exact_wiring_location_count",
                get_path(data, ("summary", "query_ux_exact_wiring_location_count")),
            ),
            (
                "query_ux_report_ready_blocker_count",
                get_path(data, ("summary", "query_ux_report_ready_blocker_count")),
            ),
            (
                "query_ux_replay_ready_blocker_count",
                get_path(data, ("summary", "query_ux_replay_ready_blocker_count")),
            ),
            (
                "query_ux_execute_ready_blocker_count",
                get_path(data, ("summary", "query_ux_execute_ready_blocker_count")),
            ),
            (
                "query_ux_readiness_path_target",
                get_path(data, ("summary", "query_ux_readiness_path_target")),
            ),
            (
                "query_ux_readiness_path_state",
                get_path(data, ("summary", "query_ux_readiness_path_state")),
            ),
            (
                "query_ux_readiness_path_blocking_repair_card_count",
                get_path(data, ("summary", "query_ux_readiness_path_blocking_repair_card_count")),
            ),
            (
                "query_ux_readiness_path_step_count",
                get_path(data, ("summary", "query_ux_readiness_path_step_count")),
            ),
            (
                "query_ux_readiness_path_current_repo_step_count",
                get_path(data, ("summary", "query_ux_readiness_path_current_repo_step_count")),
            ),
            (
                "query_ux_readiness_path_target_or_validation_step_count",
                get_path(data, ("summary", "query_ux_readiness_path_target_or_validation_step_count")),
            ),
            (
                "query_ux_readiness_path_source_location_gap_count",
                get_path(data, ("summary", "query_ux_readiness_path_source_location_gap_count")),
            ),
            (
                "query_ux_readiness_path_catalog_flow_discrepancy_count",
                get_path(data, ("summary", "query_ux_readiness_path_catalog_flow_discrepancy_count")),
            ),
            (
                "query_ux_readiness_path_fallback_removal_blocked_count",
                get_path(data, ("summary", "query_ux_readiness_path_fallback_removal_blocked_count")),
            ),
            (
                "query_ux_readiness_path_field_coverage_gap_count",
                get_path(data, ("summary", "query_ux_readiness_path_field_coverage_gap_count")),
            ),
            (
                "query_ux_readiness_path_actionability_status",
                get_path(data, ("summary", "query_ux_readiness_path_actionability_status")),
            ),
            (
                "query_ux_readiness_path_primary_environment",
                get_path(data, ("summary", "query_ux_readiness_path_primary_environment")),
            ),
            (
                "query_ux_readiness_path_blocker_reason",
                get_path(data, ("summary", "query_ux_readiness_path_blocker_reason")),
            ),
            (
                "query_ux_readiness_path_current_repo_actionable",
                get_path(data, ("summary", "query_ux_readiness_path_current_repo_actionable")),
            ),
            (
                "query_ux_readiness_path_validation_or_target_required",
                get_path(data, ("summary", "query_ux_readiness_path_validation_or_target_required")),
            ),
            (
                "query_ux_readiness_path_source_location_metadata_required",
                get_path(data, ("summary", "query_ux_readiness_path_source_location_metadata_required")),
            ),
            (
                "query_ux_readiness_path_next_action_present",
                get_path(data, ("summary", "query_ux_readiness_path_next_action_present")),
            ),
    ]


def evidence_governance_cross_artifact_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "cross_artifact_blocker_explanation_present",
                get_path(data, ("summary", "cross_artifact_blocker_explanation_present")),
            ),
            (
                "cross_artifact_blocker_explanation_status",
                get_path(data, ("summary", "cross_artifact_blocker_explanation_status")),
            ),
            (
                "cross_artifact_blocker_family_count",
                get_path(data, ("summary", "cross_artifact_blocker_family_count")),
            ),
            (
                "cross_artifact_active_family_count",
                get_path(data, ("summary", "cross_artifact_active_family_count")),
            ),
            (
                "cross_artifact_total_blocker_count",
                get_path(data, ("summary", "cross_artifact_total_blocker_count")),
            ),
            (
                "cross_artifact_current_repo_action_count",
                get_path(data, ("summary", "cross_artifact_current_repo_action_count")),
            ),
            (
                "cross_artifact_target_or_validation_action_count",
                get_path(
                    data,
                    ("summary", "cross_artifact_target_or_validation_action_count"),
                ),
            ),
            (
                "cross_artifact_source_location_gap_count",
                get_path(data, ("summary", "cross_artifact_source_location_gap_count")),
            ),
            (
                "cross_artifact_primary_family",
                get_path(data, ("summary", "cross_artifact_primary_family")),
            ),
            (
                "cross_artifact_primary_blocker_reason",
                get_path(data, ("summary", "cross_artifact_primary_blocker_reason")),
            ),
            (
                "cross_artifact_primary_environment",
                get_path(data, ("summary", "cross_artifact_primary_environment")),
            ),
            (
                "cross_artifact_release_focus_present",
                get_path(data, ("summary", "cross_artifact_release_focus_present")),
            ),
            (
                "cross_artifact_release_focus_decision",
                get_path(data, ("summary", "cross_artifact_release_focus_decision")),
            ),
            (
                "cross_artifact_release_focus_repair_lane",
                get_path(data, ("summary", "cross_artifact_release_focus_repair_lane")),
            ),
            (
                "cross_artifact_release_focus_affected_claim_count",
                get_path(
                    data,
                    ("summary", "cross_artifact_release_focus_affected_claim_count"),
                ),
            ),
            (
                "cross_artifact_release_focus_repair_path_count",
                get_path(
                    data,
                    ("summary", "cross_artifact_release_focus_repair_path_count"),
                ),
            ),
            (
                "cross_artifact_release_focus_first_repair_family",
                get_path(
                    data,
                    ("summary", "cross_artifact_release_focus_first_repair_family"),
                ),
            ),
            (
                "cross_artifact_release_focus_first_repair_environment",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_release_focus_first_repair_environment",
                    ),
                ),
            ),
            (
                "cross_artifact_release_focus_first_repair_action",
                get_path(
                    data,
                    ("summary", "cross_artifact_release_focus_first_repair_action"),
                ),
            ),
            (
                "cross_artifact_release_focus_current_repo_actionable",
                get_path(
                    data,
                    ("summary", "cross_artifact_release_focus_current_repo_actionable"),
                ),
            ),
            (
                "cross_artifact_release_focus_target_or_validation_required",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_release_focus_target_or_validation_required",
                    ),
                ),
            ),
            (
                "cross_artifact_release_focus_source_location_metadata_required",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_release_focus_source_location_metadata_required",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_present",
                get_path(
                    data,
                    ("summary", "cross_artifact_product_replay_family_present"),
                ),
            ),
            (
                "cross_artifact_product_replay_family_status",
                get_path(
                    data,
                    ("summary", "cross_artifact_product_replay_family_status"),
                ),
            ),
            (
                "cross_artifact_product_replay_family_blocker_count",
                get_path(
                    data,
                    ("summary", "cross_artifact_product_replay_family_blocker_count"),
                ),
            ),
            (
                "cross_artifact_product_replay_family_semantic_linkage_blocker_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_semantic_linkage_blocker_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_replay_or_execute_missing_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_replay_or_execute_missing_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_target_product_replay_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_target_product_replay_action_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_target_gpu_execute_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_target_gpu_execute_action_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_source_artifact_count",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_source_artifact_count",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_first_source_artifact",
                get_path(
                    data,
                    (
                        "summary",
                        "cross_artifact_product_replay_family_first_source_artifact",
                    ),
                ),
            ),
            (
                "cross_artifact_product_replay_family_next_action",
                get_path(
                    data,
                    ("summary", "cross_artifact_product_replay_family_next_action"),
                ),
            ),
            (
                "product_replay_semantic_linkage_present",
                get_path(data, ("summary", "product_replay_semantic_linkage_present")),
            ),
            (
                "product_replay_semantic_linkage_status",
                get_path(data, ("summary", "product_replay_semantic_linkage_status")),
            ),
            (
                "product_replay_semantic_events_present",
                get_path(data, ("summary", "product_replay_semantic_events_present")),
            ),
            (
                "product_replay_alignment_present",
                get_path(data, ("summary", "product_replay_alignment_present")),
            ),
            (
                "product_replay_semantic_linkage_read_issue_count",
                get_path(
                    data,
                    ("summary", "product_replay_semantic_linkage_read_issue_count"),
                ),
            ),
            (
                "product_replay_semantic_linkage_pending_event_count",
                get_path(
                    data,
                    ("summary", "product_replay_semantic_linkage_pending_event_count"),
                ),
            ),
            (
                "product_replay_semantic_linkage_sample_replay_present",
                get_path(
                    data,
                    ("summary", "product_replay_semantic_linkage_sample_replay_present"),
                ),
            ),
            (
                "product_replay_semantic_linkage_replay_supported_event_count",
                get_path(
                    data,
                    (
                        "summary",
                        "product_replay_semantic_linkage_replay_supported_event_count",
                    ),
                ),
            ),
            (
                "product_replay_semantic_linkage_aligned_rule_count",
                get_path(
                    data,
                    ("summary", "product_replay_semantic_linkage_aligned_rule_count"),
                ),
            ),
            (
                "product_replay_semantic_linkage_correlation_count",
                get_path(
                    data,
                    ("summary", "product_replay_semantic_linkage_correlation_count"),
                ),
            ),
            (
                "product_replay_semantic_linkage_blocker_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_blocker_count")),
            ),
            (
                "product_replay_semantic_linkage_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "product_replay_semantic_linkage_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "product_replay_semantic_linkage_first_blocker",
                get_path(data, ("summary", "product_replay_semantic_linkage_first_blocker")),
            ),
            (
                "product_replay_semantic_linkage_next_action",
                get_path(data, ("summary", "product_replay_semantic_linkage_next_action")),
            ),
    ]


def evidence_governance_snapshot_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "snapshot_diff_query_present",
                get_path(data, ("summary", "snapshot_diff_query_present")),
            ),
            (
                "snapshot_diff_configured",
                get_path(data, ("summary", "snapshot_diff_configured")),
            ),
            (
                "snapshot_diff_before_configured",
                get_path(data, ("summary", "snapshot_diff_before_configured")),
            ),
            (
                "snapshot_diff_after_configured",
                get_path(data, ("summary", "snapshot_diff_after_configured")),
            ),
            (
                "snapshot_diff_before_present",
                get_path(data, ("summary", "snapshot_diff_before_present")),
            ),
            (
                "snapshot_diff_after_present",
                get_path(data, ("summary", "snapshot_diff_after_present")),
            ),
            (
                "snapshot_diff_path_issue_count",
                get_path(data, ("summary", "snapshot_diff_path_issue_count")),
            ),
            (
                "snapshot_diff_read_issue_count",
                get_path(data, ("summary", "snapshot_diff_read_issue_count")),
            ),
            (
                "snapshot_diff_content_compared",
                get_path(data, ("summary", "snapshot_diff_content_compared")),
            ),
            (
                "snapshot_diff_content_changed",
                get_path(data, ("summary", "snapshot_diff_content_changed")),
            ),
            (
                "snapshot_diff_semantic_compared",
                get_path(data, ("summary", "snapshot_diff_semantic_compared")),
            ),
            (
                "snapshot_diff_deep_semantic_compared",
                get_path(data, ("summary", "snapshot_diff_deep_semantic_compared")),
            ),
            (
                "snapshot_diff_deep_semantic_truncated",
                get_path(data, ("summary", "snapshot_diff_deep_semantic_truncated")),
            ),
            (
                "snapshot_diff_added_top_level_key_count",
                get_path(data, ("summary", "snapshot_diff_added_top_level_key_count")),
            ),
            (
                "snapshot_diff_removed_top_level_key_count",
                get_path(data, ("summary", "snapshot_diff_removed_top_level_key_count")),
            ),
            (
                "snapshot_diff_changed_top_level_value_count",
                get_path(data, ("summary", "snapshot_diff_changed_top_level_value_count")),
            ),
            (
                "snapshot_diff_added_deep_path_count",
                get_path(data, ("summary", "snapshot_diff_added_deep_path_count")),
            ),
            (
                "snapshot_diff_removed_deep_path_count",
                get_path(data, ("summary", "snapshot_diff_removed_deep_path_count")),
            ),
            (
                "snapshot_diff_changed_deep_value_count",
                get_path(data, ("summary", "snapshot_diff_changed_deep_value_count")),
            ),
            (
                "snapshot_diff_before_size_bytes",
                get_path(data, ("summary", "snapshot_diff_before_size_bytes")),
            ),
            (
                "snapshot_diff_after_size_bytes",
                get_path(data, ("summary", "snapshot_diff_after_size_bytes")),
            ),
            (
                "snapshot_diff_size_delta_bytes",
                get_path(data, ("summary", "snapshot_diff_size_delta_bytes")),
            ),
            (
                "snapshot_diff_semantic_decision",
                get_path(data, ("summary", "snapshot_diff_semantic_decision")),
            ),
            (
                "snapshot_diff_deep_semantic_decision",
                get_path(data, ("summary", "snapshot_diff_deep_semantic_decision")),
            ),
            (
                "snapshot_diff_decision",
                get_path(data, ("summary", "snapshot_diff_decision")),
            ),
            (
                "snapshot_to_release_focus_present",
                get_path(data, ("summary", "snapshot_to_release_focus_present")),
            ),
            (
                "snapshot_to_release_focus_link_status",
                get_path(data, ("summary", "snapshot_to_release_focus_link_status")),
            ),
            (
                "snapshot_to_release_focus_blocker_reason",
                get_path(data, ("summary", "snapshot_to_release_focus_blocker_reason")),
            ),
            (
                "snapshot_to_release_focus_review_required",
                get_path(data, ("summary", "snapshot_to_release_focus_review_required")),
            ),
            (
                "snapshot_to_release_focus_current_repo_action_count",
                get_path(data, ("summary", "snapshot_to_release_focus_current_repo_action_count")),
            ),
            (
                "snapshot_to_release_focus_changed_deep_path_count",
                get_path(data, ("summary", "snapshot_to_release_focus_changed_deep_path_count")),
            ),
            ("repair_path_query_present", get_path(data, ("summary", "repair_path_query_present"))),
            ("root_cause_repair_card_count", get_path(data, ("summary", "root_cause_repair_card_count"))),
            ("root_cause_current_repo_repair_card_count", get_path(data, ("summary", "root_cause_current_repo_repair_card_count"))),
            ("root_cause_target_product_replay_repair_card_count", get_path(data, ("summary", "root_cause_target_product_replay_repair_card_count"))),
            ("root_cause_target_gpu_execute_repair_card_count", get_path(data, ("summary", "root_cause_target_gpu_execute_repair_card_count"))),
            ("root_cause_target_machine_repair_card_count", get_path(data, ("summary", "root_cause_target_machine_repair_card_count"))),
            ("root_cause_repair_environment_split_status", get_path(data, ("summary", "root_cause_repair_environment_split_status"))),
            ("root_cause_repair_environment_split_schema_gap", get_path(data, ("summary", "root_cause_repair_environment_split_schema_gap"))),
            ("root_cause_repair_environment_legacy_fallback_used", get_path(data, ("summary", "root_cause_repair_environment_legacy_fallback_used"))),
            ("root_cause_flow_check_repair_card_count", get_path(data, ("summary", "root_cause_flow_check_repair_card_count"))),
            (
                "root_cause_catalog_flow_root_cause_count",
                get_path(data, ("summary", "root_cause_catalog_flow_root_cause_count")),
            ),
            (
                "root_cause_catalog_flow_discrepancy_count",
                get_path(data, ("summary", "root_cause_catalog_flow_discrepancy_count")),
            ),
            (
                "root_cause_catalog_flow_input_gap_count",
                get_path(data, ("summary", "root_cause_catalog_flow_input_gap_count")),
            ),
            (
                "root_cause_catalog_flow_single_source_repair_card_count",
                get_path(data, ("summary", "root_cause_catalog_flow_single_source_repair_card_count")),
            ),
            (
                "root_cause_catalog_flow_single_source_parity_refresh_required_count",
                get_path(
                    data,
                    ("summary", "root_cause_catalog_flow_single_source_parity_refresh_required_count"),
                ),
            ),
            (
                "root_cause_catalog_flow_surface_migration_ready_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_migration_ready_count")),
            ),
            (
                "root_cause_catalog_flow_surface_migration_fallback_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_migration_fallback_count")),
            ),
            (
                "root_cause_catalog_flow_surface_parity_refresh_required_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_parity_refresh_required_count")),
            ),
            (
                "root_cause_catalog_flow_surface_current_repo_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_current_repo_action_count")),
            ),
            (
                "root_cause_catalog_flow_surface_target_machine_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_target_machine_action_count")),
            ),
            (
                "root_cause_catalog_flow_surface_consumer_migration_plan_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_consumer_migration_plan_count")),
            ),
            (
                "root_cause_catalog_flow_surface_consumer_fallback_removal_blocked_count",
                get_path(
                    data,
                    ("summary", "root_cause_catalog_flow_surface_consumer_fallback_removal_blocked_count"),
                ),
            ),
            (
                "root_cause_catalog_flow_surface_consumer_parity_refresh_required_count",
                get_path(
                    data,
                    ("summary", "root_cause_catalog_flow_surface_consumer_parity_refresh_required_count"),
                ),
            ),
            (
                "root_cause_catalog_flow_surface_consumer_current_repo_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_consumer_current_repo_action_count")),
            ),
            (
                "root_cause_catalog_flow_surface_consumer_target_machine_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_surface_consumer_target_machine_action_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_queue_item_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_queue_item_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_queue_blocked_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_queue_blocked_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_queue_parity_refresh_required_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_queue_parity_refresh_required_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_queue_current_repo_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_queue_current_repo_action_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_queue_target_machine_action_count",
                get_path(data, ("summary", "root_cause_catalog_flow_fallback_removal_queue_target_machine_action_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_root_cause_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_root_cause_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_present",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_present")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_state",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_state")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_step_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_current_repo_step_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_current_repo_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_target_machine_step_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_target_machine_step_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_source_location_gap_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_primary_environment",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_primary_environment")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_first_surface",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_first_surface")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_shortest_path_blocks_removal",
                get_path(data, ("summary", "catalog_flow_fallback_removal_shortest_path_blocks_removal")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_root_cause_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_root_cause_count")),
            ),
    ]


def evidence_governance_root_cause_review_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_present",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_present")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_decision",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_decision")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_removal_allowed",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_removal_allowed")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_blocker_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_blocker_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_blocking_surface_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_fallback_surface_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_fallback_surface_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_parity_refresh_required_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_current_repo_action_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_current_repo_action_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_target_machine_action_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_target_machine_action_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_source_location_gap_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_blocking_surface_source_location_gap_count")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_first_surface",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_first_surface")),
            ),
            (
                "root_cause_catalog_flow_fallback_removal_parity_gate_blocks_removal",
                get_path(data, ("summary", "catalog_flow_fallback_removal_parity_gate_blocks_removal")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_root_cause_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_root_cause_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_status",
                get_path(data, ("summary", "product_replay_semantic_linkage_status")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_blocker_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_blocker_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_pending_event_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_pending_event_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_pending_rule_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_pending_rule_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_blocked_correlation_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_blocked_correlation_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_target_or_validation_action_count",
                get_path(data, ("summary", "product_replay_semantic_linkage_target_or_validation_action_count")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_first_blocker",
                get_path(data, ("summary", "product_replay_semantic_linkage_first_blocker")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_next_action",
                get_path(data, ("summary", "product_replay_semantic_linkage_next_action")),
            ),
            (
                "root_cause_product_replay_semantic_linkage_blocks_release",
                get_path(data, ("summary", "product_replay_semantic_linkage_blocks_release")),
            ),
            (
                "root_cause_product_replay_family_gate_root_cause_count",
                get_path(data, ("summary", "product_replay_family_gate_root_cause_count")),
            ),
            (
                "root_cause_product_replay_family_gate_status",
                get_path(data, ("summary", "product_replay_family_gate_status")),
            ),
            (
                "root_cause_product_replay_family_gate_blocker_count",
                get_path(data, ("summary", "product_replay_family_gate_blocker_count")),
            ),
            (
                "root_cause_product_replay_family_gate_semantic_linkage_blocker_count",
                get_path(data, ("summary", "product_replay_family_gate_semantic_linkage_blocker_count")),
            ),
            (
                "root_cause_product_replay_family_gate_replay_or_execute_missing_count",
                get_path(data, ("summary", "product_replay_family_gate_replay_or_execute_missing_count")),
            ),
            (
                "root_cause_product_replay_family_gate_target_or_validation_action_count",
                get_path(data, ("summary", "product_replay_family_gate_target_or_validation_action_count")),
            ),
            (
                "root_cause_product_replay_family_gate_target_product_replay_action_count",
                get_path(data, ("summary", "product_replay_family_gate_target_product_replay_action_count")),
            ),
            (
                "root_cause_product_replay_family_gate_target_gpu_execute_action_count",
                get_path(data, ("summary", "product_replay_family_gate_target_gpu_execute_action_count")),
            ),
            (
                "root_cause_product_replay_family_gate_typed_environment_split_present",
                get_path(data, ("summary", "product_replay_family_gate_typed_environment_split_present")),
            ),
            (
                "root_cause_product_replay_family_gate_source_artifact_count",
                get_path(data, ("summary", "product_replay_family_gate_source_artifact_count")),
            ),
            (
                "root_cause_product_replay_family_gate_first_source_artifact",
                get_path(data, ("summary", "product_replay_family_gate_first_source_artifact")),
            ),
            (
                "root_cause_product_replay_family_gate_next_action",
                get_path(data, ("summary", "product_replay_family_gate_next_action")),
            ),
            (
                "root_cause_product_replay_family_gate_blocks_release",
                get_path(data, ("summary", "product_replay_family_gate_blocks_release")),
            ),
            (
                "root_cause_product_replay_family_gate_repair_path_count",
                get_path(data, ("summary", "product_replay_family_gate_repair_path_count")),
            ),
            (
                "root_cause_product_replay_family_gate_repair_path_source_location_gap_count",
                get_path(data, ("summary", "product_replay_family_gate_repair_path_source_location_gap_count")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_gate_root_cause_count",
                get_path(data, ("summary", "native_core_audit_review_coverage_gate_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_root_cause_count",
                get_path(data, ("summary", "native_core_audit_review_manifest_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_configured",
                get_path(data, ("summary", "native_core_audit_review_manifest_configured")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_parse_ok",
                get_path(data, ("summary", "native_core_audit_review_manifest_parse_ok")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_invalid_override_count",
                get_path(data, ("summary", "native_core_audit_review_manifest_invalid_override_count")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_unmatched_override_count",
                get_path(data, ("summary", "native_core_audit_review_manifest_unmatched_override_count")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_state",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_state")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_open_file_count",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_open_file_count")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_high_priority_open_file_count",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_high_priority_open_file_count")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_source_location_gap_count",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_source_location_gap_count")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_primary_blocker",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_primary_blocker")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_first_open_file",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_first_open_file")),
            ),
            (
                "root_cause_native_core_audit_review_coverage_blocks_full_audit_closure",
                get_path(data, ("summary", "native_core_audit_audit_review_coverage_blocks_full_audit_closure")),
            ),
            (
                "root_cause_catalog_metadata_policy_gap_root_cause_count",
                get_path(data, ("summary", "root_cause_catalog_metadata_policy_gap_root_cause_count")),
            ),
            (
                "root_cause_catalog_flow_metadata_policy_card_count",
                get_path(data, ("summary", "root_cause_catalog_flow_metadata_policy_card_count")),
            ),
            (
                "root_cause_catalog_flow_metadata_derived_policy_artifact_count",
                get_path(data, ("summary", "root_cause_catalog_flow_metadata_derived_policy_artifact_count")),
            ),
            (
                "root_cause_catalog_flow_metadata_derived_policy_field_count",
                get_path(data, ("summary", "root_cause_catalog_flow_metadata_derived_policy_field_count")),
            ),
            (
                "root_cause_native_core_audit_root_cause_count",
                get_path(data, ("summary", "root_cause_native_core_audit_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_backlog_root_cause_count",
                get_path(data, ("summary", "root_cause_native_core_audit_backlog_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_scope_root_cause_count",
                get_path(data, ("summary", "root_cause_native_core_audit_scope_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_review_manifest_root_cause_count",
                get_path(data, ("summary", "root_cause_native_core_audit_review_manifest_root_cause_count")),
            ),
            (
                "root_cause_native_core_audit_blocker_count",
                get_path(data, ("summary", "root_cause_native_core_audit_blocker_count")),
            ),
            (
                "root_cause_native_core_audit_source_unreadable_count",
                get_path(data, ("summary", "root_cause_native_core_audit_source_unreadable_count")),
            ),
            (
                "root_cause_native_core_audit_high_priority_review_item_count",
                get_path(data, ("summary", "native_core_audit_high_priority_review_item_count")),
            ),
            (
                "root_cause_native_core_audit_top_review_priority",
                get_path(data, ("summary", "native_core_audit_top_review_priority")),
            ),
            ("root_cause_source_location_card_count", get_path(data, ("summary", "root_cause_source_location_card_count"))),
            ("repair_path_source_gap_count", get_path(data, ("summary", "repair_path_source_gap_count"))),
            ("repair_path_source_location_count", get_path(data, ("summary", "repair_path_source_location_count"))),
            ("repair_path_source_location_gap_count", get_path(data, ("summary", "repair_path_source_location_gap_count"))),
            ("repair_path_count", get_path(data, ("summary", "repair_path_count"))),
            ("repair_path_first_action_present", get_path(data, ("summary", "repair_path_first_action_present"))),
            ("catalog_flow_filter_query_present", get_path(data, ("summary", "catalog_flow_filter_query_present"))),
            (
                "catalog_flow_catalog_artifact_readable",
                get_path(data, ("summary", "catalog_flow_catalog_artifact_readable")),
            ),
            (
                "catalog_flow_catalog_artifact_read_status",
                get_path(data, ("summary", "catalog_flow_catalog_artifact_read_status")),
            ),
            ("catalog_flow_filter_decision", get_path(data, ("summary", "catalog_flow_filter_decision"))),
            ("registered_artifact_count", get_path(data, ("summary", "registered_artifact_count"))),
            ("registered_schema_file_count", get_path(data, ("summary", "registered_schema_file_count"))),
            (
                "selector_repair_card_export_schema_registered",
                get_path(data, ("summary", "selector_repair_card_export_schema_registered")),
            ),
            ("schema_registry_decision", get_path(data, ("summary", "schema_registry_decision"))),
            ("registry_decision", get_path(data, ("summary", "registry_decision"))),
            ("registry_consumer_surface_count", get_path(data, ("summary", "registry_consumer_surface_count"))),
            ("registry_consumed_surface_count", get_path(data, ("summary", "registry_consumed_surface_count"))),
            ("hardcoded_duplicate_location_count", get_path(data, ("summary", "hardcoded_duplicate_location_count"))),
            ("duplicate_registration_count", get_path(data, ("summary", "duplicate_registration_count"))),
            ("missing_wiring_policy_count", get_path(data, ("summary", "missing_wiring_policy_count"))),
            (
                "downstream_wiring_matrix_entry_count",
                get_path(data, ("summary", "downstream_wiring_matrix_entry_count")),
            ),
            (
                "downstream_wiring_surface_count",
                get_path(data, ("summary", "downstream_wiring_surface_count")),
            ),
            (
                "catalog_drift_missing_surface_count",
                get_path(data, ("summary", "catalog_drift_missing_surface_count")),
            ),
            (
                "catalog_fallback_retained_surface_count",
                get_path(data, ("summary", "catalog_fallback_retained_surface_count")),
            ),
            (
                "catalog_surface_migration_ready_count",
                get_path(data, ("summary", "catalog_surface_migration_ready_count")),
            ),
            (
                "catalog_surface_migration_fallback_count",
                get_path(data, ("summary", "catalog_surface_migration_fallback_count")),
            ),
            (
                "catalog_surface_parity_refresh_required_count",
                get_path(data, ("summary", "catalog_surface_parity_refresh_required_count")),
            ),
            (
                "catalog_surface_current_repo_action_count",
                get_path(data, ("summary", "catalog_surface_current_repo_action_count")),
            ),
            (
                "catalog_surface_target_machine_action_count",
                get_path(data, ("summary", "catalog_surface_target_machine_action_count")),
            ),
    ]


def evidence_governance_catalog_delivery_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "fallback_removal_queue_item_count",
                get_path(data, ("summary", "fallback_removal_queue_item_count")),
            ),
            (
                "fallback_removal_queue_blocked_count",
                get_path(data, ("summary", "fallback_removal_queue_blocked_count")),
            ),
            (
                "fallback_removal_queue_parity_refresh_required_count",
                get_path(data, ("summary", "fallback_removal_queue_parity_refresh_required_count")),
            ),
            (
                "fallback_removal_queue_current_repo_action_count",
                get_path(data, ("summary", "fallback_removal_queue_current_repo_action_count")),
            ),
            (
                "fallback_removal_queue_target_machine_action_count",
                get_path(data, ("summary", "fallback_removal_queue_target_machine_action_count")),
            ),
            (
                "fallback_removal_queue_state",
                get_path(data, ("summary", "fallback_removal_queue_state")),
            ),
            (
                "fallback_removal_shortest_path_present",
                get_path(data, ("summary", "fallback_removal_shortest_path_present")),
            ),
            (
                "fallback_removal_shortest_path_state",
                get_path(data, ("summary", "fallback_removal_shortest_path_state")),
            ),
            (
                "fallback_removal_shortest_path_step_count",
                get_path(data, ("summary", "fallback_removal_shortest_path_step_count")),
            ),
            (
                "fallback_removal_shortest_path_current_repo_step_count",
                get_path(data, ("summary", "fallback_removal_shortest_path_current_repo_step_count")),
            ),
            (
                "fallback_removal_shortest_path_target_machine_step_count",
                get_path(data, ("summary", "fallback_removal_shortest_path_target_machine_step_count")),
            ),
            (
                "fallback_removal_shortest_path_source_location_gap_count",
                get_path(data, ("summary", "fallback_removal_shortest_path_source_location_gap_count")),
            ),
            (
                "fallback_removal_shortest_path_primary_environment",
                get_path(data, ("summary", "fallback_removal_shortest_path_primary_environment")),
            ),
            (
                "fallback_removal_shortest_path_first_surface",
                get_path(data, ("summary", "fallback_removal_shortest_path_first_surface")),
            ),
            (
                "fallback_removal_parity_gate_present",
                get_path(data, ("summary", "fallback_removal_parity_gate_present")),
            ),
            (
                "fallback_removal_parity_gate_decision",
                get_path(data, ("summary", "fallback_removal_parity_gate_decision")),
            ),
            (
                "fallback_removal_parity_gate_removal_allowed",
                get_path(data, ("summary", "fallback_removal_parity_gate_removal_allowed")),
            ),
            (
                "fallback_removal_parity_gate_blocker_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_blocker_count")),
            ),
            (
                "fallback_removal_parity_gate_blocking_surface_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_blocking_surface_count")),
            ),
            (
                "fallback_removal_parity_gate_fallback_surface_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_fallback_surface_count")),
            ),
            (
                "fallback_removal_parity_gate_parity_refresh_required_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_parity_refresh_required_count")),
            ),
            (
                "fallback_removal_parity_gate_current_repo_action_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_current_repo_action_count")),
            ),
            (
                "fallback_removal_parity_gate_target_machine_action_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_target_machine_action_count")),
            ),
            (
                "fallback_removal_parity_gate_source_location_gap_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_source_location_gap_count")),
            ),
            (
                "fallback_removal_parity_gate_blocking_surface_source_location_gap_count",
                get_path(data, ("summary", "fallback_removal_parity_gate_blocking_surface_source_location_gap_count")),
            ),
            (
                "fallback_removal_parity_gate_first_surface",
                get_path(data, ("summary", "fallback_removal_parity_gate_first_surface")),
            ),
            (
                "consumer_field_coverage_surface_count",
                get_path(data, ("summary", "consumer_field_coverage_surface_count")),
            ),
            (
                "consumer_field_coverage_catalog_managed_surface_count",
                get_path(data, ("summary", "consumer_field_coverage_catalog_managed_surface_count")),
            ),
            (
                "consumer_field_coverage_fallback_surface_count",
                get_path(data, ("summary", "consumer_field_coverage_fallback_surface_count")),
            ),
            (
                "consumer_field_coverage_required_field_count",
                get_path(data, ("summary", "consumer_field_coverage_required_field_count")),
            ),
            (
                "consumer_field_coverage_covered_field_count",
                get_path(data, ("summary", "consumer_field_coverage_covered_field_count")),
            ),
            (
                "consumer_field_coverage_field_gap_count",
                get_path(data, ("summary", "consumer_field_coverage_field_gap_count")),
            ),
            (
                "consumer_field_coverage_parity_refresh_required_count",
                get_path(data, ("summary", "consumer_field_coverage_parity_refresh_required_count")),
            ),
            (
                "consumer_field_coverage_current_repo_action_count",
                get_path(data, ("summary", "consumer_field_coverage_current_repo_action_count")),
            ),
            (
                "consumer_field_coverage_target_machine_action_count",
                get_path(data, ("summary", "consumer_field_coverage_target_machine_action_count")),
            ),
            (
                "single_source_repair_card_count",
                get_path(data, ("summary", "single_source_repair_card_count")),
            ),
            (
                "single_source_fallback_repair_card_count",
                get_path(data, ("summary", "single_source_fallback_repair_card_count")),
            ),
            (
                "single_source_parity_refresh_required_count",
                get_path(data, ("summary", "single_source_parity_refresh_required_count")),
            ),
            (
                "registry_explicit_policy_complete_artifact_count",
                get_path(data, ("summary", "registry_explicit_policy_complete_artifact_count")),
            ),
            (
                "registry_derived_policy_artifact_count",
                get_path(data, ("summary", "registry_derived_policy_artifact_count")),
            ),
            (
                "registry_derived_policy_field_count",
                get_path(data, ("summary", "registry_derived_policy_field_count")),
            ),
            (
                "registry_metadata_policy_card_count",
                get_path(data, ("summary", "registry_metadata_policy_card_count")),
            ),
            ("check_count", get_path(data, ("summary", "check_count"))),
            ("ready_check_count", get_path(data, ("summary", "ready_check_count"))),
            ("missing_wiring_count", get_path(data, ("summary", "missing_wiring_count"))),
            ("non_gpu_acceptance_decision", get_path(data, ("summary", "non_gpu_acceptance_decision"))),
            ("supported_command_count", get_path(data, ("summary", "supported_command_count"))),
            ("selected_artifact_count", get_path(data, ("summary", "selected_artifact_count"))),
            ("repair_card_count", get_path(data, ("summary", "repair_card_count"))),
            ("source_location_gap_card_count", get_path(data, ("summary", "source_location_gap_card_count"))),
            (
                "catalog_flow_discrepancy_card_count",
                get_path(data, ("summary", "catalog_flow_discrepancy_card_count")),
            ),
            (
                "action_source_filter_card_count",
                get_path(data, ("summary", "action_source_filter_card_count")),
            ),
            (
                "review_bundle_filter_card_count",
                get_path(data, ("summary", "review_bundle_filter_card_count")),
            ),
            (
                "action_source_filter_query_present",
                get_path(data, ("summary", "action_source_filter_query_present")),
            ),
            (
                "action_source_filter_source_gap_count",
                get_path(data, ("summary", "action_source_filter_source_gap_count")),
            ),
            (
                "action_source_filter_source_location_gap_count",
                get_path(data, ("summary", "action_source_filter_source_location_gap_count")),
            ),
            (
                "action_source_filter_decision",
                get_path(data, ("summary", "action_source_filter_decision")),
            ),
            (
                "review_bundle_filter_query_present",
                get_path(data, ("summary", "review_bundle_filter_query_present")),
            ),
            (
                "review_bundle_filter_manifest_present",
                get_path(data, ("summary", "review_bundle_filter_manifest_present")),
            ),
            (
                "review_bundle_filter_item_count",
                get_path(data, ("summary", "review_bundle_filter_item_count")),
            ),
            (
                "review_bundle_filter_present_item_count",
                get_path(data, ("summary", "review_bundle_filter_present_item_count")),
            ),
            (
                "review_bundle_filter_missing_required_item_count",
                get_path(data, ("summary", "review_bundle_filter_missing_required_item_count")),
            ),
            (
                "review_bundle_filter_report_export_item_count",
                get_path(data, ("summary", "review_bundle_filter_report_export_item_count")),
            ),
            (
                "review_bundle_filter_decision",
                get_path(data, ("summary", "review_bundle_filter_decision")),
            ),
            (
                "native_core_audit_filter_card_count",
                get_path(data, ("summary", "native_core_audit_filter_card_count")),
            ),
            (
                "native_core_audit_filter_query_present",
                get_path(data, ("summary", "native_core_audit_filter_query_present")),
            ),
            (
                "native_core_audit_source_root_count",
                get_path(data, ("summary", "native_core_audit_source_root_count")),
            ),
            (
                "native_core_audit_source_root_issue_count",
                get_path(data, ("summary", "native_core_audit_source_root_issue_count")),
            ),
            (
                "native_core_audit_scope_blocker_count",
                get_path(data, ("summary", "native_core_audit_scope_blocker_count")),
            ),
            (
                "native_core_audit_empty_scan",
                get_path(data, ("summary", "native_core_audit_empty_scan")),
            ),
            (
                "native_core_audit_in_scope_file_count",
                get_path(data, ("summary", "native_core_audit_in_scope_file_count")),
            ),
            (
                "native_core_audit_deferred_with_reason_file_count",
                get_path(
                    data,
                    ("summary", "native_core_audit_deferred_with_reason_file_count"),
                ),
            ),
            (
                "native_core_audit_issue_count",
                get_path(data, ("summary", "native_core_audit_issue_count")),
            ),
            (
                "native_core_audit_high_priority_review_item_count",
                get_path(data, ("summary", "native_core_audit_high_priority_review_item_count")),
            ),
            (
                "native_core_audit_top_review_priority",
                get_path(data, ("summary", "native_core_audit_top_review_priority")),
            ),
            (
                "native_core_audit_filter_read_issue_count",
                get_path(data, ("summary", "native_core_audit_filter_read_issue_count")),
            ),
    ]


def evidence_governance_evidence_query_operation_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "evidence_query_operation_catalog_present",
                get_path(data, ("summary", "evidence_query_operation_catalog_present")),
            ),
            (
                "evidence_query_operation_catalog_supported_count",
                get_path(data, ("summary", "evidence_query_operation_catalog_supported_count")),
            ),
            (
                "evidence_query_operation_catalog_read_only_count",
                get_path(data, ("summary", "evidence_query_operation_catalog_read_only_count")),
            ),
            (
                "evidence_query_operation_catalog_snapshot_required_count",
                get_path(data, ("summary", "evidence_query_operation_catalog_snapshot_required_count")),
            ),
            (
                "evidence_query_operation_catalog_selector_recommended_count",
                get_path(data, ("summary", "evidence_query_operation_catalog_selector_recommended_count")),
            ),
            (
                "evidence_query_operation_catalog_target_required_count",
                get_path(data, ("summary", "evidence_query_operation_catalog_target_required_count")),
            ),
            (
                "evidence_query_selected_result_present",
                get_path(data, ("summary", "evidence_query_selected_result_present")),
            ),
            (
                "evidence_query_selected_result_input_ready",
                get_path(data, ("summary", "evidence_query_selected_result_input_ready")),
            ),
            (
                "evidence_query_selected_result_payload_count",
                get_path(data, ("summary", "evidence_query_selected_result_payload_count")),
            ),
            (
                "evidence_query_selected_result_kind",
                get_path(data, ("summary", "evidence_query_selected_result_kind")),
            ),
            (
                "evidence_query_show_artifact_candidate_count",
                get_path(data, ("summary", "evidence_query_show_artifact_candidate_count")),
            ),
            (
                "evidence_query_show_artifact_candidate_truncated",
                get_path(data, ("summary", "evidence_query_show_artifact_candidate_truncated")),
            ),
            (
                "evidence_query_show_artifact_exact_match",
                get_path(data, ("summary", "evidence_query_show_artifact_exact_match")),
            ),
            (
                "evidence_query_show_artifact_selector_ambiguous",
                get_path(data, ("summary", "evidence_query_show_artifact_selector_ambiguous")),
            ),
            (
                "evidence_query_show_artifact_selection_strategy",
                get_path(data, ("summary", "evidence_query_show_artifact_selection_strategy")),
            ),
            (
                "evidence_query_payload_selector_focus_present",
                get_path(data, ("summary", "evidence_query_payload_selector_focus_present")),
            ),
            (
                "evidence_query_payload_selector_focus_status",
                get_path(data, ("summary", "evidence_query_payload_selector_focus_status")),
            ),
            (
                "evidence_query_payload_selector_focus_match_count",
                get_path(data, ("summary", "evidence_query_payload_selector_focus_match_count")),
            ),
            (
                "evidence_query_payload_selector_focus_ambiguous",
                get_path(data, ("summary", "evidence_query_payload_selector_focus_ambiguous")),
            ),
            (
                "evidence_query_payload_selector_focus_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_payload_selector_focus_source_location_gap_count")),
            ),
            (
                "evidence_query_selector_resolution_present",
                get_path(data, ("summary", "evidence_query_selector_resolution_present")),
            ),
            (
                "evidence_query_selector_resolution_status",
                get_path(data, ("summary", "evidence_query_selector_resolution_status")),
            ),
            (
                "evidence_query_selector_kind",
                get_path(data, ("summary", "evidence_query_selector_kind")),
            ),
            (
                "evidence_query_selector_match_count",
                get_path(data, ("summary", "evidence_query_selector_match_count")),
            ),
            (
                "evidence_query_selector_ambiguous",
                get_path(data, ("summary", "evidence_query_selector_ambiguous")),
            ),
            (
                "evidence_query_selector_repair_card_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_count")),
            ),
            (
                "evidence_query_selector_repair_card_emitted_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_emitted_count")),
            ),
            (
                "evidence_query_selector_repair_card_truncated",
                get_path(data, ("summary", "evidence_query_selector_repair_card_truncated")),
            ),
            (
                "evidence_query_selector_repair_card_export_status",
                get_path(data, ("summary", "evidence_query_selector_repair_card_export_status")),
            ),
            (
                "evidence_query_selector_repair_card_embedded_limit",
                get_path(data, ("summary", "evidence_query_selector_repair_card_embedded_limit")),
            ),
            (
                "evidence_query_selector_repair_card_full_export_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_full_export_count")),
            ),
            (
                "evidence_query_selector_repair_card_external_export_configured",
                get_path(data, ("summary", "evidence_query_selector_repair_card_external_export_configured")),
            ),
            (
                "evidence_query_selector_repair_card_external_export_written",
                get_path(data, ("summary", "evidence_query_selector_repair_card_external_export_written")),
            ),
            (
                "evidence_query_selector_repair_card_external_export_status",
                get_path(data, ("summary", "evidence_query_selector_repair_card_external_export_status")),
            ),
            (
                "evidence_query_selector_repair_card_current_repo_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_current_repo_count")),
            ),
            (
                "evidence_query_selector_repair_card_current_server_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_current_server_count")),
            ),
            (
                "evidence_query_selector_repair_card_target_machine_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_target_machine_count")),
            ),
            (
                "evidence_query_selector_repair_card_target_product_count",
                get_path(data, ("summary", "evidence_query_selector_repair_card_target_product_count")),
            ),
            (
                "evidence_query_selector_repair_card_source_location_gap_card_count",
                get_path(
                    data,
                    ("summary", "evidence_query_selector_repair_card_source_location_gap_card_count"),
                ),
            ),
            (
                "evidence_query_selector_repair_card_export_review_status",
                get_path(data, ("summary", "evidence_query_selector_repair_card_export_review_status")),
            ),
            (
                "evidence_query_selector_repair_card_export_blocker_reason",
                get_path(data, ("summary", "evidence_query_selector_repair_card_export_blocker_reason")),
            ),
            (
                "evidence_query_selector_repair_card_external_file_required",
                get_path(data, ("summary", "evidence_query_selector_repair_card_external_file_required")),
            ),
            (
                "evidence_query_selector_source_location_count",
                get_path(data, ("summary", "evidence_query_selector_source_location_count")),
            ),
            (
                "evidence_query_selector_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_selector_source_location_gap_count")),
            ),
            (
                "evidence_query_selector_current_repo_action_count",
                get_path(data, ("summary", "evidence_query_selector_current_repo_action_count")),
            ),
            (
                "evidence_query_selector_current_server_action_count",
                get_path(data, ("summary", "evidence_query_selector_current_server_action_count")),
            ),
            (
                "evidence_query_selector_target_machine_action_count",
                get_path(data, ("summary", "evidence_query_selector_target_machine_action_count")),
            ),
            (
                "evidence_query_selector_target_product_action_count",
                get_path(data, ("summary", "evidence_query_selector_target_product_action_count")),
            ),
            (
                "evidence_query_selector_action_plan_decision",
                get_path(data, ("summary", "evidence_query_selector_action_plan_decision")),
            ),
            (
                "evidence_query_selector_action_plan_step_count",
                get_path(data, ("summary", "evidence_query_selector_action_plan_step_count")),
            ),
            (
                "evidence_query_selector_action_plan_current_repo_step_count",
                get_path(data, ("summary", "evidence_query_selector_action_plan_current_repo_step_count")),
            ),
            (
                "evidence_query_selector_action_plan_target_or_validation_step_count",
                get_path(
                    data,
                    ("summary", "evidence_query_selector_action_plan_target_or_validation_step_count"),
                ),
            ),
            (
                "evidence_query_selector_action_plan_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_selector_action_plan_source_location_gap_count")),
            ),
            (
                "evidence_query_selector_action_plan_readiness_blocker_count",
                get_path(data, ("summary", "evidence_query_selector_action_plan_readiness_blocker_count")),
            ),
            (
                "evidence_query_selector_action_plan_readiness_state",
                get_path(data, ("summary", "evidence_query_selector_action_plan_readiness_state")),
            ),
            (
                "evidence_query_readiness_path_target",
                get_path(data, ("summary", "evidence_query_readiness_path_target")),
            ),
            (
                "evidence_query_readiness_path_state",
                get_path(data, ("summary", "evidence_query_readiness_path_state")),
            ),
            (
                "evidence_query_readiness_path_blocking_repair_card_count",
                get_path(data, ("summary", "evidence_query_readiness_path_blocking_repair_card_count")),
            ),
            (
                "evidence_query_readiness_path_step_count",
                get_path(data, ("summary", "evidence_query_readiness_path_step_count")),
            ),
            (
                "evidence_query_readiness_path_current_repo_step_count",
                get_path(data, ("summary", "evidence_query_readiness_path_current_repo_step_count")),
            ),
            (
                "evidence_query_readiness_path_target_or_validation_step_count",
                get_path(data, ("summary", "evidence_query_readiness_path_target_or_validation_step_count")),
            ),
            (
                "evidence_query_readiness_path_source_location_gap_count",
                get_path(data, ("summary", "evidence_query_readiness_path_source_location_gap_count")),
            ),
            (
                "evidence_query_readiness_path_catalog_flow_discrepancy_count",
                get_path(data, ("summary", "evidence_query_readiness_path_catalog_flow_discrepancy_count")),
            ),
            (
                "evidence_query_readiness_path_fallback_removal_blocked_count",
                get_path(data, ("summary", "evidence_query_readiness_path_fallback_removal_blocked_count")),
            ),
            (
                "evidence_query_readiness_path_field_coverage_gap_count",
                get_path(data, ("summary", "evidence_query_readiness_path_field_coverage_gap_count")),
            ),
            (
                "evidence_query_readiness_path_actionability_status",
                get_path(data, ("summary", "evidence_query_readiness_path_actionability_status")),
            ),
            (
                "evidence_query_readiness_path_primary_environment",
                get_path(data, ("summary", "evidence_query_readiness_path_primary_environment")),
            ),
            (
                "evidence_query_readiness_path_blocker_reason",
                get_path(data, ("summary", "evidence_query_readiness_path_blocker_reason")),
            ),
            (
                "evidence_query_readiness_path_current_repo_actionable",
                get_path(data, ("summary", "evidence_query_readiness_path_current_repo_actionable")),
            ),
            (
                "evidence_query_readiness_path_validation_or_target_required",
                get_path(data, ("summary", "evidence_query_readiness_path_validation_or_target_required")),
            ),
            (
                "evidence_query_readiness_path_source_location_metadata_required",
                get_path(data, ("summary", "evidence_query_readiness_path_source_location_metadata_required")),
            ),
            (
                "evidence_query_readiness_path_next_action_present",
                get_path(data, ("summary", "evidence_query_readiness_path_next_action_present")),
            ),
    ]


def evidence_governance_evidence_query_cross_artifact_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
            (
                "evidence_query_cross_artifact_blocker_explanation_status",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_blocker_explanation_status"),
                ),
            ),
            (
                "evidence_query_cross_artifact_family_count",
                get_path(data, ("summary", "evidence_query_cross_artifact_family_count")),
            ),
            (
                "evidence_query_cross_artifact_active_family_count",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_active_family_count"),
                ),
            ),
            (
                "evidence_query_cross_artifact_total_blocker_count",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_total_blocker_count"),
                ),
            ),
            (
                "evidence_query_cross_artifact_current_repo_action_count",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_current_repo_action_count"),
                ),
            ),
            (
                "evidence_query_cross_artifact_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_source_location_gap_count",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_source_location_gap_count"),
                ),
            ),
            (
                "evidence_query_cross_artifact_primary_family",
                get_path(data, ("summary", "evidence_query_cross_artifact_primary_family")),
            ),
            (
                "evidence_query_cross_artifact_primary_blocker_reason",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_primary_blocker_reason"),
                ),
            ),
            (
                "evidence_query_cross_artifact_primary_environment",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_primary_environment"),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_present",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_release_focus_present"),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_decision",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_release_focus_decision"),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_repair_lane",
                get_path(
                    data,
                    ("summary", "evidence_query_cross_artifact_release_focus_repair_lane"),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_affected_claim_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_affected_claim_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_repair_path_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_repair_path_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_first_repair_family",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_first_repair_family",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_first_repair_environment",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_first_repair_environment",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_first_repair_action",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_first_repair_action",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_current_repo_actionable",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_current_repo_actionable",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_target_or_validation_required",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_target_or_validation_required",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_release_focus_source_location_metadata_required",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_release_focus_source_location_metadata_required",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_present",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_present",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_status",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_status",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_blocker_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_blocker_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_semantic_linkage_blocker_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_replay_or_execute_missing_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_target_product_replay_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_target_gpu_execute_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_source_artifact_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_source_artifact_count",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_first_source_artifact",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_first_source_artifact",
                    ),
                ),
            ),
            (
                "evidence_query_cross_artifact_product_replay_family_next_action",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_cross_artifact_product_replay_family_next_action",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_present",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_present"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_status",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_status"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_events_present",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_events_present"),
                ),
            ),
            (
                "evidence_query_product_replay_alignment_present",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_alignment_present"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_read_issue_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_read_issue_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_pending_event_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_pending_event_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_sample_replay_present",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_sample_replay_present",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_replay_supported_event_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_replay_supported_event_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_aligned_rule_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_aligned_rule_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_correlation_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_correlation_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_blocker_count",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_blocker_count"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_target_or_validation_action_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_target_or_validation_action_count",
                    ),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_first_blocker",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_first_blocker"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_next_action",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_next_action"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_repair_path_count",
                get_path(
                    data,
                    ("summary", "evidence_query_product_replay_semantic_linkage_repair_path_count"),
                ),
            ),
            (
                "evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count",
                get_path(
                    data,
                    (
                        "summary",
                        "evidence_query_product_replay_semantic_linkage_repair_path_source_location_gap_count",
                    ),
                ),
            ),
    ]


def evidence_governance_native_audit_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
        *summary_domain_metric_pairs(data, NATIVE_CORE_AUDIT_CORE_FIELDS),
        (
            "audit_register_refresh_command",
            get_path(data, ("audit_register_refresh_command",)),
        ),
        (
            "review_manifest_refresh_command",
            get_path(data, ("review_manifest_refresh_command",)),
        ),
    ]


def evidence_governance_handoff_metric_items(
    data: Dict[str, Any],
) -> List[Metric]:
    return [
        *_summary_metric_items(
            data,
            "repair_card_count",
            "root_cause_repair_card_count",
            "root_cause_current_repo_repair_card_count",
            "root_cause_target_product_replay_repair_card_count",
            "root_cause_target_gpu_execute_repair_card_count",
            "root_cause_target_machine_repair_card_count",
            "root_cause_repair_environment_split_status",
            "root_cause_repair_environment_split_schema_gap",
            "root_cause_repair_environment_legacy_fallback_used",
            "root_cause_flow_check_repair_card_count",
            "root_cause_native_core_audit_root_cause_count",
            "root_cause_native_core_audit_backlog_root_cause_count",
            "root_cause_native_core_audit_scope_root_cause_count",
            "root_cause_native_core_audit_blocker_count",
            "root_cause_native_core_audit_source_unreadable_count",
        ),
        (
            "root_cause_native_core_audit_high_priority_review_item_count",
            get_path(data, ("summary", "native_core_audit_high_priority_review_item_count")),
        ),
        (
            "root_cause_native_core_audit_top_review_priority",
            get_path(data, ("summary", "native_core_audit_top_review_priority")),
        ),
        *_summary_metric_items(
            data,
            "root_cause_source_location_review_present",
            "root_cause_source_location_card_count",
            "root_cause_first_card_source_location_count",
        ),
        (
            "root_cause_first_source_location",
            get_path(data, ("root_cause_repair_review", "first_source_location")),
        ),
        *_summary_metric_items(
            data,
            "evidence_query_repair_path_review_present",
            "evidence_query_repair_path_count",
            "evidence_query_first_action_present",
            "evidence_query_repair_path_source_gap_count",
            "evidence_query_repair_path_source_location_count",
            "evidence_query_repair_path_source_location_gap_count",
            "native_core_audit_backlog_action_source_count",
            "native_core_audit_backlog_source_location_gap_count",
            "native_core_audit_scope_action_source_count",
            "native_core_audit_scope_source_location_gap_count",
            "native_core_audit_review_coverage_action_source_count",
        ),
        (
            "action_source_review_decision",
            get_path(data, ("action_source_review", "review_decision")),
        ),
        *_summary_metric_items(
            data,
            "snapshot_diff_review_present",
            "snapshot_baseline_present",
            "snapshot_baseline_gap_count",
            "snapshot_entry_count",
            "snapshot_added_artifact_count",
            "snapshot_missing_artifact_count",
            "snapshot_changed_artifact_count",
            "snapshot_unchanged_artifact_count",
            "snapshot_action_required_count",
            "snapshot_review_decision",
            "review_bundle_decision",
        ),
        (
            "evidence_query_first_target",
            get_path(data, ("evidence_query_repair_path_review", "first_target")),
        ),
        (
            "evidence_query_first_state",
            get_path(data, ("evidence_query_repair_path_review", "first_state")),
        ),
        (
            "evidence_query_first_action",
            short_text(
                get_path(data, ("evidence_query_repair_path_review", "first_action"))
            ),
        ),
        (
            "evidence_query_review_decision",
            get_path(data, ("evidence_query_repair_path_review", "review_decision")),
        ),
        *_summary_metric_items(
            data,
            "not_run_count",
            "not_run_item_count",
            "handoff_checklist_count",
            "current_repo_handoff_count",
            "target_machine_handoff_count",
            "required_handoff_count",
            "not_run_handoff_count",
            "review_decision",
            "managed_artifact_count",
            "artifact_ready_count",
            "artifact_with_gap_count",
            "required_flow_step_count",
            "passed_flow_step_count",
            "missing_flow_step_count",
            "source_control_handoff_required_count",
            "source_control_handoff_ready_count",
            "source_control_handoff_blocker_count",
            "source_control_handoff_status",
            "flow_check_decision",
        ),
    ]


def evidence_governance_section(title: str, source: str, data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    metrics: List[Metric] = []
    if data is not None:
        evidence = artifact_evidence_projection(data, source)
        metrics = [
            *evidence_governance_core_repair_metric_items(data, evidence),
            *evidence_governance_root_cause_product_metric_items(data),
            *evidence_governance_query_repair_metric_items(data),
            *evidence_governance_query_source_metric_items(data),
            *evidence_governance_query_operation_metric_items(data),
            *evidence_governance_query_repair_ux_metric_items(data),
            *evidence_governance_query_navigation_metric_items(data),
            *evidence_governance_cross_artifact_metric_items(data),
            *evidence_governance_snapshot_metric_items(data),
            *evidence_governance_root_cause_review_metric_items(data),
            *evidence_governance_catalog_delivery_metric_items(data),
            *evidence_governance_evidence_query_operation_metric_items(data),
            *evidence_governance_evidence_query_cross_artifact_metric_items(data),
            *evidence_governance_native_audit_metric_items(data),
            *evidence_governance_handoff_metric_items(data),
        ]
    return build_section(title, source, data, metrics)


def build_summary(results_root: Path) -> Dict[str, Any]:
    catalog_resolution = resolve_catalog_sources(results_root, EXPECTED_SOURCES)
    source_names = catalog_resolution.names
    sources = SourcePayloads(
        {name: load_json(results_root / name) for name in source_names}
    )
    sections = [
        system_section(sources["system_info.json"]),
        cuda_section(sources["cuda_environment.json"]),
        vllm_serving_section(sources["vllm_serving_benchmark.json"]),
        llama_cpp_section(sources["llama_cpp_benchmark.json"]),
        trtllm_section(sources["trtllm_benchmark.json"]),
        native_probe_run_section(sources["native_probe_run.json"]),
        shape_section("Python Shape Probe", "shape_probe.json", sources["shape_probe.json"]),
        mini_transformer_cache_section(sources["mini_transformer_cache_compare.json"]),
        mini_transformer_workload_section(sources["mini_transformer_workload_profile.json"]),
        mini_transformer_bottleneck_section(sources["mini_transformer_bottleneck_advisor.json"]),
        mini_transformer_scenario_sweep_section(sources["mini_transformer_scenario_sweep.json"]),
        shape_section("Native Shape Probe", "native_shape_probe.json", sources["native_shape_probe.json"]),
        scheduler_section(sources["scheduler_trace.json"]),
        scheduler_pressure_sweep_section(sources["scheduler_pressure_sweep.json"]),
        scheduler_workload_sweep_section(sources["scheduler_workload_sweep.json"]),
        scheduler_capacity_sweep_section(sources["scheduler_capacity_sweep.json"]),
        scheduler_admission_policy_sweep_section(sources["scheduler_admission_policy_sweep.json"]),
        scheduler_runtime_policy_sweep_section(sources["scheduler_runtime_policy_sweep.json"]),
        scheduler_fairness_sweep_section(sources["scheduler_fairness_sweep.json"]),
        kv_allocation_section(sources["kv_allocation_compare.json"]),
        prefix_cache_section(sources["prefix_cache_compare.json"]),
        kv_fragmentation_section(sources["kv_fragmentation_probe.json"]),
        paged_kv_locality_section(sources["paged_kv_locality_probe.json"]),
        quantization_section(sources["quantization_probe.json"]),
        quantization_sweep_section(sources["quantization_sweep.json"]),
        quantization_channel_section(sources["quantization_channel_compare.json"]),
        attention_section(sources["attention_probe.json"]),
        matrix_ops_profile_section(sources["matrix_ops_profile.json"]),
        decoder_section(sources["decoder_probe.json"]),
        decoder_kv_section(sources["decoder_kv_probe.json"]),
        gguf_section(sources["gguf_probe.json"]),
        gguf_fixture_comparison_section(sources["gguf_fixture_comparison.json"]),
        backend_quality_section(sources["backend_quality_eval.json"]),
        profiling_artifacts_section(sources["profiling_artifacts.json"]),
        backend_comparison_section(sources["backend_comparison.json"]),
        runtime_closure_section("CUDA Profiling Matrix", "cuda_profile_matrix.json", sources["cuda_profile_matrix.json"]),
        runtime_closure_section("llama.cpp Matrix", "llama_cpp_matrix.json", sources["llama_cpp_matrix.json"]),
        runtime_closure_section("Android Edge Benchmark", "android_edge_benchmark.json", sources["android_edge_benchmark.json"]),
        runtime_closure_section("Source Trace Verification", "source_trace_verification.json", sources["source_trace_verification.json"]),
        runtime_closure_section("Evidence Checklist", "evidence_checklist.json", sources["evidence_checklist.json"]),
        advanced_serving_section("Structured Outputs", "structured_outputs.json", sources["structured_outputs.json"]),
        advanced_serving_section("Pooling Benchmark", "pooling_benchmark.json", sources["pooling_benchmark.json"]),
        advanced_serving_section("Kserve Llm D Plan", "kserve_llm_d_plan.json", sources["kserve_llm_d_plan.json"]),
        advanced_serving_section("Native Advanced Kv Router", "native_advanced_kv_router.json", sources["native_advanced_kv_router.json"]),
        advanced_serving_section("Native Disaggregated Serving", "native_disaggregated_serving.json", sources["native_disaggregated_serving.json"]),
        advanced_serving_section("Native Slo Capacity Plan", "native_slo_capacity_plan.json", sources["native_slo_capacity_plan.json"]),
        advanced_serving_section("Native Long Context Matrix", "native_long_context_matrix.json", sources["native_long_context_matrix.json"]),
        advanced_serving_section("Native Speculative Decode Plan", "native_speculative_decode_plan.json", sources["native_speculative_decode_plan.json"]),
        advanced_serving_section("Native Observability Evidence", "native_observability_evidence.json", sources["native_observability_evidence.json"]),
        advanced_serving_section("Native Streaming Sse Plan", "native_streaming_sse_plan.json", sources["native_streaming_sse_plan.json"]),
        advanced_serving_section("Native Radix Prefix Cache Plan", "native_radix_prefix_cache_plan.json", sources["native_radix_prefix_cache_plan.json"]),
        advanced_serving_section("Native Tensor Parallel Comm Plan", "native_tensor_parallel_comm_plan.json", sources["native_tensor_parallel_comm_plan.json"]),
        advanced_serving_section("Native Lora Adapter Cache Plan", "native_lora_adapter_cache_plan.json", sources["native_lora_adapter_cache_plan.json"]),
        advanced_serving_section("Native Chunked Prefill Scheduler Plan", "native_chunked_prefill_scheduler_plan.json", sources["native_chunked_prefill_scheduler_plan.json"]),
        advanced_serving_section("Native Cost Accounting Plan", "native_cost_accounting_plan.json", sources["native_cost_accounting_plan.json"]),
        advanced_serving_section("Native Kv Offload Hierarchy Plan", "native_kv_offload_hierarchy_plan.json", sources["native_kv_offload_hierarchy_plan.json"]),
        advanced_serving_section("Native Engine Memory Planner", "native_engine_memory_planner.json", sources["native_engine_memory_planner.json"]),
        advanced_serving_section("Native Gateway Policy Matrix Plan", "native_gateway_policy_matrix_plan.json", sources["native_gateway_policy_matrix_plan.json"]),
        advanced_serving_section("Native Replica Placement Plan", "native_replica_placement_plan.json", sources["native_replica_placement_plan.json"]),
        product_profile_contract_check_section("Native Product Profile Contract Check", "native_product_profile_contract_check.json", sources["native_product_profile_contract_check.json"]),
        product_integration_inventory_section("Native Product Integration Inventory", "native_product_integration_inventory.json", sources["native_product_integration_inventory.json"]),
        claim_scope_contract_section("Native Claim Scope Contract", "native_claim_scope_contract.json", sources["native_claim_scope_contract.json"]),
        runtime_evidence_freshness_section("Native Runtime Evidence Freshness Gate", "native_runtime_evidence_freshness_gate.json", sources["native_runtime_evidence_freshness_gate.json"]),
        advanced_serving_section("Native KV PagedAttention Deep Dive", "native_kv_paged_attention_deep_dive.json", sources["native_kv_paged_attention_deep_dive.json"]),
        advanced_serving_section("Native KV Lifecycle Deep Dive", "native_kv_lifecycle_deep_dive.json", sources["native_kv_lifecycle_deep_dive.json"]),
        advanced_serving_section("Native Scheduler Batching Deep Dive", "native_scheduler_batching_deep_dive.json", sources["native_scheduler_batching_deep_dive.json"]),
        advanced_serving_section("Native CUDA Quant Microkernel Deep Dive", "native_cuda_quant_microkernel_deep_dive.json", sources["native_cuda_quant_microkernel_deep_dive.json"]),
        advanced_serving_section("Native KV Tier Decision Deep Dive", "native_kv_tier_decision_deep_dive.json", sources["native_kv_tier_decision_deep_dive.json"]),
        advanced_serving_section("Native PD Split Crossover Deep Dive", "native_pd_split_crossover_deep_dive.json", sources["native_pd_split_crossover_deep_dive.json"]),
        advanced_serving_section("Native Scheduler Root Cause Deep Dive", "native_scheduler_root_cause_deep_dive.json", sources["native_scheduler_root_cause_deep_dive.json"]),
        advanced_serving_section("Native Kernel Compatibility Matrix", "native_kernel_compatibility_matrix.json", sources["native_kernel_compatibility_matrix.json"]),
        advanced_serving_section("Product Replay Semantic Events", "product_replay_semantic_events.json", sources["product_replay_semantic_events.json"]),
        advanced_serving_section("Native Streaming Backpressure Deep Dive", "native_streaming_backpressure_deep_dive.json", sources["native_streaming_backpressure_deep_dive.json"]),
        advanced_serving_section("Native Structured Output Parser Contract", "native_structured_output_parser_contract.json", sources["native_structured_output_parser_contract.json"]),
        advanced_serving_section("Native Autoscaler Cost SLO Deep Dive", "native_autoscaler_cost_slo_deep_dive.json", sources["native_autoscaler_cost_slo_deep_dive.json"]),
        advanced_serving_section("Native LoRA Adapter Cache Deep Dive", "native_lora_adapter_cache_deep_dive.json", sources["native_lora_adapter_cache_deep_dive.json"]),
        advanced_serving_section("Native Grammar Backend Matrix", "native_grammar_backend_matrix.json", sources["native_grammar_backend_matrix.json"]),
        advanced_serving_section("Native Edge Compiler Artifact Matrix", "native_edge_compiler_artifact_matrix.json", sources["native_edge_compiler_artifact_matrix.json"]),
        advanced_serving_section("Native KV Transport Timeline Deep Dive", "native_kv_transport_timeline_deep_dive.json", sources["native_kv_transport_timeline_deep_dive.json"]),
        advanced_serving_section("Native Radix Prefix Cache Deep Dive", "native_radix_prefix_cache_deep_dive.json", sources["native_radix_prefix_cache_deep_dive.json"]),
        advanced_serving_section("Native KV Compression Blend Risk", "native_kv_compression_blend_risk.json", sources["native_kv_compression_blend_risk.json"]),
        advanced_serving_section("Native Chunked Prefill Stall Free Deep Dive", "native_chunked_prefill_stall_free_deep_dive.json", sources["native_chunked_prefill_stall_free_deep_dive.json"]),
        advanced_serving_section("Native Tenant Quota Admission Deep Dive", "native_tenant_quota_admission_deep_dive.json", sources["native_tenant_quota_admission_deep_dive.json"]),
        advanced_serving_section("Native Live Request Migration Deep Dive", "native_live_request_migration_deep_dive.json", sources["native_live_request_migration_deep_dive.json"]),
        advanced_serving_section("Native Quant Correctness Contract", "native_quant_correctness_contract.json", sources["native_quant_correctness_contract.json"]),
        advanced_serving_section("Native Paged KV Gather Kernel Contract", "native_paged_kv_gather_kernel_contract.json", sources["native_paged_kv_gather_kernel_contract.json"]),
        advanced_serving_section("Native FlashInfer Replay Alignment", "native_flashinfer_replay_alignment.json", sources["native_flashinfer_replay_alignment.json"]),
        advanced_serving_section("Native TensorRT-LLM Engine Quant Alignment", "native_trtllm_engine_quant_alignment.json", sources["native_trtllm_engine_quant_alignment.json"]),
        advanced_serving_section("Native Gateway Policy Matrix", "native_gateway_policy_matrix.json", sources["native_gateway_policy_matrix.json"]),
        advanced_serving_section("Native Deployment Readiness Matrix", "native_deployment_readiness_matrix.json", sources["native_deployment_readiness_matrix.json"]),
        advanced_serving_section("Native OpenAI API Conformance Matrix", "native_openai_api_conformance_matrix.json", sources["native_openai_api_conformance_matrix.json"]),
        product_replay_alignment_section("Native Product Replay Alignment", "native_product_replay_alignment.json", sources["native_product_replay_alignment.json"]),
        deep_dive_contract_check_section("Native Deep Dive Contract Check", "native_deep_dive_contract_check.json", sources["native_deep_dive_contract_check.json"]),
        evidence_governance_section("Native Evidence Warehouse Index", "native_evidence_warehouse_index.json", sources["native_evidence_warehouse_index.json"]),
        evidence_governance_section("Native Kernel Microbench Harness", "native_kernel_microbench_harness.json", sources["native_kernel_microbench_harness.json"]),
        evidence_governance_section("Native Third-Party Dependency Gate", "native_third_party_dependency_gate.json", sources["native_third_party_dependency_gate.json"]),
        evidence_governance_section("Native Release Evidence Pipeline", "native_release_evidence_pipeline.json", sources["native_release_evidence_pipeline.json"]),
        evidence_governance_section("Native Evidence Query Result", "native_evidence_query_result.json", sources["native_evidence_query_result.json"]),
        evidence_governance_section("Native Evidence Query Selector Repair Card Export", "native_evidence_query_selector_repair_card_export.json", sources["native_evidence_query_selector_repair_card_export.json"]),
        evidence_governance_section("Native Dependency SBOM Gate", "native_dependency_sbom_gate.json", sources["native_dependency_sbom_gate.json"]),
        evidence_governance_section("Native Artifact Schema Registry", "native_artifact_schema_registry.json", sources["native_artifact_schema_registry.json"]),
        evidence_governance_section("Native Artifact Catalog Single Source", "native_artifact_catalog_single_source.json", sources["native_artifact_catalog_single_source.json"]),
        evidence_governance_section("Native Core Audit Register", "native_core_audit_register.json", sources["native_core_audit_register.json"]),
        product_profile_section("Product vLLM Capability Profile", "product_vllm_capability_profile.json", sources["product_vllm_capability_profile.json"]),
        product_profile_section("Product SGLang Capability Profile", "product_sglang_capability_profile.json", sources["product_sglang_capability_profile.json"]),
        product_profile_section("Product TensorRT-LLM Runtime Profile", "product_tensorrtllm_runtime_profile.json", sources["product_tensorrtllm_runtime_profile.json"]),
    ]
    handled_sources = {section["source"] for section in sections}
    for source in source_names:
        if source in handled_sources:
            continue
        sections.append(evidence_governance_section(source_title_from_name(source), source, sources.get(source)))
    missing_source_files = [
        section["source"] for section in sections if section["status"] == "missing"
    ]
    invalid_sources = [
        section["source"]
        for section in sections
        if section["status"] in {"invalid_artifact", "invalid_encoding", "invalid_json"}
    ]
    unreadable_sources = [
        section["source"] for section in sections if section["status"] in {"path_probe_failed", "unreadable_json"}
    ]
    unavailable_sources = [
        section["source"] for section in sections if section["status"] != "ok"
    ]
    available_ok_sources = [
        section["source"] for section in sections if section["status"] == "ok"
    ]
    discovered_source_files, source_file_scan = source_files_with_status(results_root)
    summary_status = (
        "ok"
        if not unavailable_sources and source_file_scan.get("status") == "ok"
        else "incomplete"
    )
    return {
        "schema_version": 1,
        "status": summary_status,
        **path_payload(results_root, "results_root", "results_root_path_redacted"),
        "source_files": discovered_source_files,
        "overview": {
            "expected_sources": len(source_names),
            "available_sources": len(available_ok_sources),
            "available_ok_sources": available_ok_sources,
            "unavailable_sources": unavailable_sources,
            "missing_sources": missing_source_files,
            "missing_source_files": missing_source_files,
            "invalid_sources": invalid_sources,
            "unreadable_sources": unreadable_sources,
            "source_file_scan": source_file_scan,
            "source_file_scan_status": source_file_scan.get("status", "unknown"),
            "source_file_scan_error": source_file_scan.get("error", ""),
            "catalog_source_resolution": catalog_resolution.to_dict(),
        },
        "sections": sections,
    }


def write_summary(summary: Dict[str, Any], output: Path) -> None:
    write_text_file(output, json.dumps(summary, indent=2, sort_keys=True) + "\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output", default="results/raw/benchmark_summary.json")
    parser.add_argument(
        "--allow-non-ok-status",
        action="store_true",
        help="Return zero after writing an incomplete diagnostic summary.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    results_root = resolve_repo_path(repo_root, args.results)
    output = resolve_repo_path(repo_root, args.output)
    summary = build_summary(results_root)
    write_summary(summary, output)
    print(path_label(output))
    return artifact_status_exit_code(summary, allow_non_ok=args.allow_non_ok_status)


if __name__ == "__main__":
    raise SystemExit(main())
