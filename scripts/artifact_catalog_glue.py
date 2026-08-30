#!/usr/bin/env python3
"""Shared catalog-consumption glue for InferLab report/export scripts."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Tuple

from command_redaction import path_was_redacted, redact_path_error, redacted_path_label


CATALOG_FILENAME = "native_artifact_catalog_single_source.json"

# Canonical non-catalog evidence inventory shared by report/export consumers.
# These sources have independent producers and are extensions, not a fallback
# copy of the high-value catalog.
COMMON_EVIDENCE_SOURCE_EXTENSIONS = (
    "system_info.json",
    "cuda_environment.json",
    "vllm_serving_benchmark.json",
    "llama_cpp_benchmark.json",
    "trtllm_benchmark.json",
    "native_probe_run.json",
    "shape_probe.json",
    "mini_transformer_cache_compare.json",
    "mini_transformer_workload_profile.json",
    "mini_transformer_bottleneck_advisor.json",
    "mini_transformer_scenario_sweep.json",
    "native_shape_probe.json",
    "scheduler_trace.json",
    "scheduler_pressure_sweep.json",
    "scheduler_workload_sweep.json",
    "scheduler_capacity_sweep.json",
    "scheduler_admission_policy_sweep.json",
    "scheduler_runtime_policy_sweep.json",
    "scheduler_fairness_sweep.json",
    "kv_allocation_compare.json",
    "prefix_cache_compare.json",
    "kv_fragmentation_probe.json",
    "paged_kv_locality_probe.json",
    "quantization_probe.json",
    "quantization_sweep.json",
    "quantization_channel_compare.json",
    "attention_probe.json",
    "matrix_ops_profile.json",
    "decoder_probe.json",
    "decoder_kv_probe.json",
    "gguf_probe.json",
    "gguf_fixture_comparison.json",
    "backend_quality_eval.json",
    "profiling_artifacts.json",
    "backend_comparison.json",
    "cuda_profile_matrix.json",
    "llama_cpp_matrix.json",
    "android_edge_benchmark.json",
    "source_trace_verification.json",
    "evidence_checklist.json",
    "structured_outputs.json",
    "pooling_benchmark.json",
    "kserve_llm_d_plan.json",
    "native_disaggregated_serving.json",
    "native_slo_capacity_plan.json",
    "native_long_context_matrix.json",
    "native_speculative_decode_plan.json",
    "native_observability_evidence.json",
    "native_streaming_sse_plan.json",
    "native_radix_prefix_cache_plan.json",
    "native_tensor_parallel_comm_plan.json",
    "native_lora_adapter_cache_plan.json",
    "native_chunked_prefill_scheduler_plan.json",
    "native_cost_accounting_plan.json",
    "native_kv_offload_hierarchy_plan.json",
    "native_engine_memory_planner.json",
    "native_product_profile_contract_check.json",
    "native_product_integration_inventory.json",
    "native_claim_scope_contract.json",
    "native_runtime_evidence_freshness_gate.json",
    "native_kv_lifecycle_deep_dive.json",
    "native_kv_tier_decision_deep_dive.json",
    "native_pd_split_crossover_deep_dive.json",
    "native_scheduler_root_cause_deep_dive.json",
    "native_kernel_compatibility_matrix.json",
    "product_replay_semantic_events.json",
    "native_streaming_backpressure_deep_dive.json",
    "native_structured_output_parser_contract.json",
    "native_autoscaler_cost_slo_deep_dive.json",
    "native_lora_adapter_cache_deep_dive.json",
    "native_grammar_backend_matrix.json",
    "native_edge_compiler_artifact_matrix.json",
    "native_kv_transport_timeline_deep_dive.json",
    "native_radix_prefix_cache_deep_dive.json",
    "native_kv_compression_blend_risk.json",
    "native_chunked_prefill_stall_free_deep_dive.json",
    "native_tenant_quota_admission_deep_dive.json",
    "native_live_request_migration_deep_dive.json",
    "native_quant_correctness_contract.json",
    "native_paged_kv_gather_kernel_contract.json",
    "native_flashinfer_replay_alignment.json",
    "native_trtllm_engine_quant_alignment.json",
    "native_gateway_policy_matrix.json",
    "native_deployment_readiness_matrix.json",
    "native_openai_api_conformance_matrix.json",
    "native_product_replay_alignment.json",
    "native_deep_dive_contract_check.json",
    "product_vllm_capability_profile.json",
    "product_sglang_capability_profile.json",
    "product_tensorrtllm_runtime_profile.json",
)


@dataclass(frozen=True)
class CatalogSourceResolution:
    names: Tuple[str, ...]
    status: str
    catalog_path: str
    catalog_path_redacted: bool
    catalog_error: str
    source_policy: str
    extension_count: int
    fallback_count: int
    catalog_count: int
    resolved_count: int
    extra_catalog_count: int
    extension_only_count: int
    fallback_only_count: int
    shared_count: int

    def to_dict(self) -> dict:
        return {
            "status": self.status,
            "catalog_path": self.catalog_path,
            "catalog_path_redacted": self.catalog_path_redacted,
            "catalog_error": self.catalog_error,
            "source_policy": self.source_policy,
            "extension_count": self.extension_count,
            "extension_only_count": self.extension_only_count,
            "fallback_fields_status": "deprecated_zero_only",
            "fallback_count": self.fallback_count,
            "catalog_count": self.catalog_count,
            "resolved_count": self.resolved_count,
            "extra_catalog_count": self.extra_catalog_count,
            "fallback_only_count": self.fallback_only_count,
            "shared_count": self.shared_count,
        }


def source_title_from_name(name: str) -> str:
    stem = Path(name).stem
    return stem.replace("_", " ").replace("-", " ").strip().title()


def compose_catalog_extensions(
    *,
    insertions: Iterable[Tuple[str, str]] = (),
    relocations: Iterable[Tuple[str, str]] = (),
) -> Tuple[str, ...]:
    """Compose canonical non-catalog sources in stable historical order.

    Placement tuples are `(source, after_source)`. Relocations must name an
    existing common source; insertions must introduce a consumer-only source.
    """

    names = list(COMMON_EVIDENCE_SOURCE_EXTENSIONS)

    def place_after(source: str, anchor: str, *, relocate: bool) -> None:
        if relocate:
            try:
                names.remove(source)
            except ValueError as exc:
                raise ValueError(
                    f"extension source relocation is not registered: {source}"
                ) from exc
        elif source in names:
            raise ValueError(f"extension source insertion already exists: {source}")
        try:
            anchor_index = names.index(anchor)
        except ValueError as exc:
            raise ValueError(f"extension source anchor is missing: {anchor}") from exc
        names.insert(anchor_index + 1, source)

    for source, anchor in relocations:
        place_after(source, anchor, relocate=True)
    for source, anchor in insertions:
        place_after(source, anchor, relocate=False)
    return tuple(names)


def _unique_json_sources(values: Iterable[str]) -> Tuple[str, ...]:
    seen = set()
    unique = []
    for value in values:
        if not isinstance(value, str):
            continue
        name = Path(value).name.strip()
        if not name.endswith(".json") or name in seen:
            continue
        seen.add(name)
        unique.append(name)
    return tuple(unique)


def load_catalog_artifact_names(results_dir: Path) -> Tuple[Tuple[str, ...], str, str]:
    catalog_path = results_dir / CATALOG_FILENAME
    try:
        catalog_exists = catalog_path.exists()
    except OSError as exc:
        return (), "catalog_probe_failed", redact_path_error(exc, catalog_path)
    if not catalog_exists:
        return (), "catalog_missing", ""
    try:
        payload = json.loads(catalog_path.read_text(encoding="utf-8"))
    except OSError as exc:
        return (), "catalog_unreadable", redact_path_error(exc, catalog_path)
    except UnicodeDecodeError as exc:
        return (), "catalog_invalid_encoding", redact_path_error(exc, catalog_path)
    except json.JSONDecodeError as exc:
        return (), "catalog_invalid_json", redact_path_error(exc, catalog_path)
    if not isinstance(payload, dict):
        return (), "catalog_invalid", ""
    raw_artifacts = payload.get("artifacts")
    if not isinstance(raw_artifacts, list):
        return (), "catalog_invalid", ""

    artifacts = []
    for item in raw_artifacts:
        if not isinstance(item, dict):
            continue
        artifact = item.get("artifact")
        if isinstance(artifact, str):
            artifacts.append(artifact)
    names = _unique_json_sources(artifacts)
    if not names:
        return (), "catalog_empty", ""
    return names, "catalog_consumed", ""


def resolve_catalog_sources(
    results_dir: Path, source_extensions: Iterable[str]
) -> CatalogSourceResolution:
    catalog_path = results_dir / CATALOG_FILENAME
    catalog_path_label = redacted_path_label(catalog_path)
    extensions = _unique_json_sources(source_extensions)
    catalog, status, catalog_error = load_catalog_artifact_names(results_dir)
    overlap = tuple(sorted(set(extensions) & set(catalog)))
    if overlap:
        preview = ", ".join(overlap[:8])
        suffix = "" if len(overlap) <= 8 else f", ... ({len(overlap)} total)"
        raise ValueError(
            "catalog-managed artifacts must not be registered as source "
            f"extensions: {preview}{suffix}"
        )
    names = _unique_json_sources((*extensions, *catalog)) if catalog else extensions
    extension_set = set(extensions)
    catalog_set = set(catalog)
    extra_catalog_count = sum(1 for name in catalog if name not in extension_set)
    extension_only_count = sum(1 for name in extensions if name not in catalog_set)
    shared_count = sum(1 for name in catalog if name in extension_set)
    if not catalog:
        source_policy = "canonical_extensions_only"
    elif extension_only_count:
        source_policy = "catalog_plus_canonical_extensions"
    else:
        source_policy = "catalog_only"
    return CatalogSourceResolution(
        names=names,
        status=status,
        catalog_path=catalog_path_label,
        catalog_path_redacted=path_was_redacted(catalog_path, catalog_path_label),
        catalog_error=catalog_error,
        source_policy=source_policy,
        extension_count=len(extensions),
        fallback_count=0,
        catalog_count=len(catalog),
        resolved_count=len(names),
        extra_catalog_count=extra_catalog_count,
        extension_only_count=extension_only_count,
        fallback_only_count=0,
        shared_count=shared_count,
    )
