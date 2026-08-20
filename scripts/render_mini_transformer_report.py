#!/usr/bin/env python3
"""Render Mini Transformer analysis JSON files into a Markdown report."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

from artifact_json import load_json_dict as load_json
from command_redaction import redacted_path_label as path_label
from json_projection import get_path
from markdown_output import escape_markdown_cell as escape_cell
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


REPORT_SOURCES = (
    "mini_transformer_cache_compare.json",
    "mini_transformer_workload_profile.json",
    "mini_transformer_bottleneck_advisor.json",
    "mini_transformer_scenario_sweep.json",
)

Metric = Tuple[str, Any]


def percent(value: Any) -> Any:
    if isinstance(value, (int, float)):
        return round(float(value) * 100.0, 4)
    return value


def table(headers: Sequence[str], rows: Iterable[Sequence[Any]]) -> List[str]:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(escape_cell(item) for item in row) + " |")
    return lines


def metrics_section(title: str, metrics: Iterable[Metric]) -> List[str]:
    lines = ["", f"## {title}", ""]
    lines.extend(table(("metric", "value"), metrics))
    return lines


def cache_metrics(data: Optional[Dict[str, Any]]) -> List[Metric]:
    if data is None:
        return [("status", "missing source file")]
    return [
        ("generated_ids_match", data.get("generated_ids_match")),
        ("prompt_tokens", len(data.get("prompt_ids", []))),
        ("max_new_tokens", data.get("max_new_tokens")),
        ("input_token_savings", get_path(data, ("totals", "input_token_savings"))),
        ("input_token_savings_percent", percent(get_path(data, ("totals", "input_token_savings_ratio")))),
        (
            "attention_score_element_savings",
            get_path(data, ("totals", "attention_score_element_savings")),
        ),
        (
            "attention_score_element_savings_percent",
            percent(get_path(data, ("totals", "attention_score_element_savings_ratio"))),
        ),
        ("cached_max_kv_cache_total_bytes", get_path(data, ("totals", "cached_max_kv_cache_total_bytes"))),
        (
            "recompute_max_kv_cache_total_bytes",
            get_path(data, ("totals", "recompute_max_kv_cache_total_bytes")),
        ),
    ]


def workload_metrics(data: Optional[Dict[str, Any]]) -> List[Metric]:
    if data is None:
        return [("status", "missing source file")]
    return [
        ("prompt_tokens", data.get("prompt_tokens")),
        ("max_new_tokens", data.get("max_new_tokens")),
        ("cached_total_flops", get_path(data, ("cached_decode", "total_flops"))),
        ("recompute_total_flops", get_path(data, ("full_recompute", "total_flops"))),
        ("flops_saved", get_path(data, ("comparison", "flops_saved"))),
        ("flops_saved_percent", percent(get_path(data, ("comparison", "flops_saved_ratio")))),
        ("cached_total_bytes", get_path(data, ("cached_decode", "total_bytes"))),
        ("recompute_total_bytes", get_path(data, ("full_recompute", "total_bytes"))),
        ("bytes_saved", get_path(data, ("comparison", "bytes_saved"))),
        ("bytes_saved_percent", percent(get_path(data, ("comparison", "bytes_saved_ratio")))),
        (
            "cached_persistent_kv_cache_bytes",
            get_path(data, ("comparison", "cached_persistent_kv_cache_bytes")),
        ),
    ]


def bottleneck_metrics(data: Optional[Dict[str, Any]]) -> List[Metric]:
    if data is None:
        return [("status", "missing source file")]
    cached_top = get_path(data, ("cached_decode", "group_rankings", 0), {})
    recompute_top = get_path(data, ("full_recompute", "group_rankings", 0), {})
    return [
        ("peak_tflops", get_path(data, ("hardware_model", "peak_tflops"))),
        ("bandwidth_gbps", get_path(data, ("hardware_model", "bandwidth_gbps"))),
        ("ridge_point_flops_per_byte", get_path(data, ("hardware_model", "ridge_point_flops_per_byte"))),
        ("cached_top_group", get_path(data, ("cached_decode", "top_bottleneck_group"))),
        ("cached_top_limiter", cached_top.get("roofline_limiter") if isinstance(cached_top, dict) else None),
        (
            "cached_top_estimated_roofline_time_ms",
            cached_top.get("estimated_roofline_time_ms") if isinstance(cached_top, dict) else None,
        ),
        ("recompute_top_group", get_path(data, ("full_recompute", "top_bottleneck_group"))),
        (
            "recompute_top_limiter",
            recompute_top.get("roofline_limiter") if isinstance(recompute_top, dict) else None,
        ),
        (
            "recompute_top_estimated_roofline_time_ms",
            recompute_top.get("estimated_roofline_time_ms") if isinstance(recompute_top, dict) else None,
        ),
    ]


def scenario_rows(data: Optional[Dict[str, Any]]) -> List[Sequence[Any]]:
    if data is None:
        return [("missing source file", None, None, None, None, None)]
    scenarios = data.get("scenarios")
    if not isinstance(scenarios, list):
        return [("invalid scenarios", None, None, None, None, None)]
    rows: List[Sequence[Any]] = []
    for item in scenarios:
        if not isinstance(item, dict):
            continue
        rows.append(
            (
                item.get("name"),
                item.get("prompt_tokens"),
                item.get("max_new_tokens"),
                percent(item.get("flops_saved_ratio")),
                item.get("cached_persistent_kv_cache_bytes"),
                item.get("cached_top_bottleneck_group"),
            )
        )
    return rows


def recommendation_lines(
    bottleneck: Optional[Dict[str, Any]],
    scenario_sweep: Optional[Dict[str, Any]],
) -> List[str]:
    seen: set[str] = set()
    recommendations: List[str] = []
    for source in (bottleneck, scenario_sweep):
        if not isinstance(source, dict):
            continue
        values = source.get("recommendations")
        if not isinstance(values, list):
            continue
        for value in values:
            if isinstance(value, str) and value not in seen:
                recommendations.append(value)
                seen.add(value)
    if not recommendations:
        recommendations = [
            "Generate Mini Transformer analysis JSON files before using this report for planning.",
            "Treat static estimates as planning guidance until hardware measurements are available.",
        ]
    return recommendations


def render_report(results_root: Path) -> str:
    sources = {name: load_json(results_root / name) for name in REPORT_SOURCES}
    available = [name for name, payload in sources.items() if payload is not None]
    missing = [name for name, payload in sources.items() if payload is None]
    scenario_sweep = sources["mini_transformer_scenario_sweep.json"]
    bottleneck = sources["mini_transformer_bottleneck_advisor.json"]

    lines = [
        "# Mini Transformer Optimization Report",
        "",
        "This report summarizes static Mini Transformer inference-planning outputs. It does not claim measured latency.",
        "",
    ]
    lines.extend(
        table(
            ("field", "value"),
            (
                ("results_root", path_label(results_root)),
                ("available_sources", len(available)),
                ("missing_sources", len(missing)),
            ),
        )
    )
    lines.extend(metrics_section("Cache Compare", cache_metrics(sources["mini_transformer_cache_compare.json"])))
    lines.extend(metrics_section("Workload Profile", workload_metrics(sources["mini_transformer_workload_profile.json"])))
    lines.extend(metrics_section("Bottleneck Advisor", bottleneck_metrics(bottleneck)))

    lines.extend(["", "## Scenario Sweep", ""])
    lines.extend(
        table(
            (
                "scenario",
                "prompt_tokens",
                "max_new_tokens",
                "flops_saved_percent",
                "persistent_kv_bytes",
                "top_bottleneck",
            ),
            scenario_rows(scenario_sweep),
        )
    )

    lines.extend(["", "## Recommendations", ""])
    lines.extend(f"- {item}" for item in recommendation_lines(bottleneck, scenario_sweep))
    if missing:
        lines.extend(["", "## Missing Sources", ""])
        lines.extend(f"- `{name}`" for name in missing)
    return "\n".join(lines) + "\n"


def write_report(results_root: Path, output: Path) -> None:
    write_text_file(output, render_report(results_root))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--results", default="results/raw")
    parser.add_argument("--output", default="results/tables/mini_transformer_report.md")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_report(resolve_repo_path(repo_root, args.results), output)
    print(path_label(output))


if __name__ == "__main__":
    main()
