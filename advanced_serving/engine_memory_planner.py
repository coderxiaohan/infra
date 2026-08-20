#!/usr/bin/env python3
"""Compatibility glue for the native engine memory planner."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


from advanced_serving.native_compat_runner import (
    NativeCompatibilitySpec,
    run_native_compatibility,
)


SPEC = NativeCompatibilitySpec(
    entry_id="advanced_serving.engine_memory_planner",
    report_title="Engine Memory Planner Glue",
    output="results/raw/engine_memory_planner.json",
    report="reports/21_engine_memory_planner.md",
    core_source="native_core/src/engine_memory_planner_main.cpp",
    native_artifact="results/raw/native_engine_memory_planner.json",
    main_report="reports/21_engine_memory_planner.md",
    extra_payload={
        "compat_target": "inferlab_engine_memory_plan",
        "compat_target_source": "native_core/src/engine_memory_planner_main.cpp",
    },
)


def main() -> int:
    return run_native_compatibility(SPEC, description=__doc__ or "")


if __name__ == "__main__":
    raise SystemExit(main())
