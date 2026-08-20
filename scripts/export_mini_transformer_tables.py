#!/usr/bin/env python3
"""Export Mini Transformer planning JSON files into CSV tables."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence

from artifact_json import load_json_dict as load_json
from command_redaction import artifact_path_payload as path_payload, redact_path_error, redacted_path_label as path_label
from json_projection import get_path
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


def _stringify(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (dict, list)):
        return json.dumps(value, sort_keys=True)
    return str(value)


def write_csv(path: Path, rows: Sequence[Dict[str, Any]], fieldnames: Sequence[str]) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
            writer.writeheader()
            for row in rows:
                writer.writerow({name: _stringify(row.get(name)) for name in fieldnames})
    except OSError as exc:
        raise SystemExit(f"failed to write {path_label(path)}: {redact_path_error(str(exc), path)}") from exc


def cache_summary_rows(data: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if data is None:
        return []
    totals = data.get("totals") if isinstance(data.get("totals"), dict) else {}
    return [
        {
            "source": "cache_compare",
            "generated_ids_match": data.get("generated_ids_match"),
            "prompt_tokens": len(data.get("prompt_ids", [])),
            "max_new_tokens": data.get("max_new_tokens"),
            "cached_input_tokens": totals.get("cached_input_tokens"),
            "recompute_input_tokens": totals.get("recompute_input_tokens"),
            "input_token_savings": totals.get("input_token_savings"),
            "input_token_savings_ratio": totals.get("input_token_savings_ratio"),
            "cached_attention_score_elements": totals.get("cached_attention_score_elements"),
            "recompute_attention_score_elements": totals.get("recompute_attention_score_elements"),
            "attention_score_element_savings": totals.get("attention_score_element_savings"),
            "attention_score_element_savings_ratio": totals.get("attention_score_element_savings_ratio"),
            "cached_max_kv_cache_total_bytes": totals.get("cached_max_kv_cache_total_bytes"),
            "recompute_max_kv_cache_total_bytes": totals.get("recompute_max_kv_cache_total_bytes"),
        }
    ]


def workload_summary_rows(data: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if data is None:
        return []
    comparison = data.get("comparison") if isinstance(data.get("comparison"), dict) else {}
    return [
        {
            "source": "workload_profile",
            "prompt_tokens": data.get("prompt_tokens"),
            "max_new_tokens": data.get("max_new_tokens"),
            "cached_total_flops": get_path(data, ("cached_decode", "total_flops")),
            "recompute_total_flops": get_path(data, ("full_recompute", "total_flops")),
            "flops_saved": comparison.get("flops_saved"),
            "flops_saved_ratio": comparison.get("flops_saved_ratio"),
            "cached_total_bytes": get_path(data, ("cached_decode", "total_bytes")),
            "recompute_total_bytes": get_path(data, ("full_recompute", "total_bytes")),
            "bytes_saved": comparison.get("bytes_saved"),
            "bytes_saved_ratio": comparison.get("bytes_saved_ratio"),
            "cached_persistent_kv_cache_bytes": comparison.get("cached_persistent_kv_cache_bytes"),
        }
    ]


def scenario_rows(data: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if data is None:
        return []
    scenarios = data.get("scenarios")
    if not isinstance(scenarios, list):
        return []
    rows: List[Dict[str, Any]] = []
    for item in scenarios:
        if not isinstance(item, dict):
            continue
        rows.append(
            {
                "scenario": item.get("name"),
                "prompt_tokens": item.get("prompt_tokens"),
                "max_new_tokens": item.get("max_new_tokens"),
                "total_tokens": item.get("total_tokens"),
                "cached_top_bottleneck_group": item.get("cached_top_bottleneck_group"),
                "cached_top_roofline_limiter": item.get("cached_top_roofline_limiter"),
                "input_tokens_saved": item.get("input_tokens_saved"),
                "input_tokens_saved_ratio": item.get("input_tokens_saved_ratio"),
                "attention_score_elements_saved": item.get("attention_score_elements_saved"),
                "attention_score_elements_saved_ratio": item.get("attention_score_elements_saved_ratio"),
                "flops_saved": item.get("flops_saved"),
                "flops_saved_ratio": item.get("flops_saved_ratio"),
                "bytes_saved": item.get("bytes_saved"),
                "bytes_saved_ratio": item.get("bytes_saved_ratio"),
                "cached_persistent_kv_cache_bytes": item.get("cached_persistent_kv_cache_bytes"),
            }
        )
    return rows


def bottleneck_rows(data: Optional[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if data is None:
        return []
    rows: List[Dict[str, Any]] = []
    for strategy_name in ("cached_decode", "full_recompute"):
        rankings = get_path(data, (strategy_name, "group_rankings"), [])
        if not isinstance(rankings, list):
            continue
        for index, item in enumerate(rankings):
            if not isinstance(item, dict):
                continue
            rows.append(
                {
                    "strategy": strategy_name,
                    "rank": index + 1,
                    "group": item.get("group"),
                    "roofline_limiter": item.get("roofline_limiter"),
                    "arithmetic_intensity_flops_per_byte": item.get("arithmetic_intensity_flops_per_byte"),
                    "ridge_point_flops_per_byte": item.get("ridge_point_flops_per_byte"),
                    "estimated_roofline_time_ms": item.get("estimated_roofline_time_ms"),
                    "flops": item.get("flops"),
                    "bytes": item.get("bytes"),
                    "flops_share": item.get("flops_share"),
                    "bytes_share": item.get("bytes_share"),
                    "optimization_advice": item.get("optimization_advice"),
                }
            )
    return rows


def export_tables(results_root: Path, output_dir: Path) -> Dict[str, Any]:
    cache = load_json(results_root / "mini_transformer_cache_compare.json")
    workload = load_json(results_root / "mini_transformer_workload_profile.json")
    bottleneck = load_json(results_root / "mini_transformer_bottleneck_advisor.json")
    scenarios = load_json(results_root / "mini_transformer_scenario_sweep.json")

    table_specs = [
        (
            "mini_transformer_cache_summary.csv",
            cache_summary_rows(cache),
            (
                "source",
                "generated_ids_match",
                "prompt_tokens",
                "max_new_tokens",
                "cached_input_tokens",
                "recompute_input_tokens",
                "input_token_savings",
                "input_token_savings_ratio",
                "cached_attention_score_elements",
                "recompute_attention_score_elements",
                "attention_score_element_savings",
                "attention_score_element_savings_ratio",
                "cached_max_kv_cache_total_bytes",
                "recompute_max_kv_cache_total_bytes",
            ),
        ),
        (
            "mini_transformer_workload_summary.csv",
            workload_summary_rows(workload),
            (
                "source",
                "prompt_tokens",
                "max_new_tokens",
                "cached_total_flops",
                "recompute_total_flops",
                "flops_saved",
                "flops_saved_ratio",
                "cached_total_bytes",
                "recompute_total_bytes",
                "bytes_saved",
                "bytes_saved_ratio",
                "cached_persistent_kv_cache_bytes",
            ),
        ),
        (
            "mini_transformer_scenarios.csv",
            scenario_rows(scenarios),
            (
                "scenario",
                "prompt_tokens",
                "max_new_tokens",
                "total_tokens",
                "cached_top_bottleneck_group",
                "cached_top_roofline_limiter",
                "input_tokens_saved",
                "input_tokens_saved_ratio",
                "attention_score_elements_saved",
                "attention_score_elements_saved_ratio",
                "flops_saved",
                "flops_saved_ratio",
                "bytes_saved",
                "bytes_saved_ratio",
                "cached_persistent_kv_cache_bytes",
            ),
        ),
        (
            "mini_transformer_bottlenecks.csv",
            bottleneck_rows(bottleneck),
            (
                "strategy",
                "rank",
                "group",
                "roofline_limiter",
                "arithmetic_intensity_flops_per_byte",
                "ridge_point_flops_per_byte",
                "estimated_roofline_time_ms",
                "flops",
                "bytes",
                "flops_share",
                "bytes_share",
                "optimization_advice",
            ),
        ),
    ]

    exports = []
    for filename, rows, fieldnames in table_specs:
        output_path = output_dir / filename
        write_csv(output_path, rows, fieldnames)
        exports.append({**path_payload(output_path), "rows": len(rows), "columns": list(fieldnames)})
    index = {
        "schema_version": 1,
        "kind": "mini_transformer_table_exports",
        **path_payload(results_root, "results_root", "results_root_path_redacted"),
        **path_payload(output_dir, "output_dir", "output_dir_path_redacted"),
        "exports": exports,
        "missing_sources": [
            name
            for name, payload in (
                ("mini_transformer_cache_compare.json", cache),
                ("mini_transformer_workload_profile.json", workload),
                ("mini_transformer_bottleneck_advisor.json", bottleneck),
                ("mini_transformer_scenario_sweep.json", scenarios),
            )
            if payload is None
        ],
    }
    write_text_file(
        output_dir / "mini_transformer_table_exports.json",
        json.dumps(index, indent=2, sort_keys=True) + "\n",
    )
    return index


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output-dir", default="results/tables")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    index = export_tables(
        resolve_repo_path(repo_root, args.results),
        resolve_repo_path(repo_root, args.output_dir),
    )
    print(json.dumps(index, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
