#!/usr/bin/env python3
"""Compatibility glue for the native streaming SSE planner."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


from advanced_serving.native_compat_runner import (
    NativeCompatibilitySpec,
    run_native_compatibility,
)


SPEC = NativeCompatibilitySpec(
    entry_id="advanced_serving.streaming_sse_benchmark",
    report_title="Streaming SSE Benchmark Glue",
    output="results/raw/streaming_sse_benchmark.json",
    report="reports/20_streaming_sse_backpressure.md",
    core_source="native_core/src/streaming_sse_plan_main.cpp",
    native_artifact="results/raw/native_streaming_sse_plan.json",
    main_report="reports/20_streaming_sse_backpressure.md",
)


def main() -> int:
    return run_native_compatibility(SPEC, description=__doc__ or "")


if __name__ == "__main__":
    raise SystemExit(main())
