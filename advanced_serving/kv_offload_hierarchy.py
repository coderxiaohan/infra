#!/usr/bin/env python3
"""Compatibility glue for the native KV offload hierarchy planner."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


from advanced_serving.native_compat_runner import (
    NativeCompatibilitySpec,
    run_native_compatibility,
)


SPEC = NativeCompatibilitySpec(
    entry_id="advanced_serving.kv_offload_hierarchy",
    report_title="KV Offload Hierarchy Glue",
    output="results/raw/kv_offload_hierarchy.json",
    report="reports/19_kv_offload_hierarchy.md",
    core_source="native_core/src/advanced_serving_kv_offload_hierarchy.cpp",
    native_artifact="results/raw/native_kv_offload_hierarchy_plan.json",
    main_report="reports/19_kv_offload_hierarchy.md",
)


def main() -> int:
    return run_native_compatibility(SPEC, description=__doc__ or "")


if __name__ == "__main__":
    raise SystemExit(main())
