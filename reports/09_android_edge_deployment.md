# 09 Android Edge Deployment

## Purpose

This report tracks the ARM/Android offline inference milestone for InferLab.
The code scaffold is present even when the current development server does not
have Android NDK, adb, an ARM device, or a legal GGUF model.

## Current Implementation

- `android/native_hello/` provides a minimal C++17 executable for Android NDK
  cross-compilation and device execution checks.
- `android/llama_runner/` records the llama.cpp Android build and deployment
  flow without vendoring llama.cpp into this repository.
- `scripts/plan_android_deploy.py` emits Markdown and JSON deployment plans
  with toolchain, adb, llama.cpp, model, and device-path preflight state.
- `scripts/run_android_edge_benchmark.py` remains the single ADB execution
  producer for `android_edge_benchmark.json`. It now requires device model,
  Android API and ABI identity before model push or timing (API 24+ and
  `arm64-v8a` by default) and records structured SoC/ABI,
  CPU topology/cpufreq, before/after thermal and battery, runtime/model file
  size and optional installed split-APK observations.
- `scripts/android_device_evidence.py` owns bounded device probe schemas and
  typed projections. `scripts/android_benchmark_output.py` owns llama.cpp
  timing/backend-log projection and the Android report view.
- `source_notes/android_ndk_llama_trace.md` documents the cross-compile
  boundary and llama.cpp call path to inspect.

The runner distinguishes `execution_requested` from actual `executed` scenario
attempts. A failed setup or missing required device identity cannot be counted
as execute evidence. Timing/backend evidence and retained diagnostics come only
from stderr; generated stdout text is not parsed or retained. Resource-window
duration encloses the before snapshots, attempted scenarios and after
snapshots. Artifact/report destinations are checked for model-input aliases
before ADB is invoked.

When `--simpleperf` is enabled, the same stderr owner emits unique canonical
counter values, explicit not-counted events, total test time and issue counts.
Missing or ambiguous requested counters downgrade the scenario to partial.
These are external process-window observations, not operator-level attribution,
NEON/Vulkan delegation proof or workload-exclusive energy.

ADB execution now uses the repository's shared bounded-process owner rather
than unbounded `capture_output`. Identity/resource probes retain at most 64 KiB
per stream; model push and llama.cpp/simpleperf scenarios retain at most 16 MiB
per stream. Timeout or overflow terminates the process group and fails the
command. This is source behavior until exercised against a real adb/device
process tree.

## Experiment Matrix

| Environment | Model | Quant | Threads | Prompt tokens | Output tokens | Status |
|---|---|---:|---:|---:|---:|---|
| Android arm64-v8a | TBD | TBD | TBD | TBD | TBD | Not run |
| ARM Linux | TBD | TBD | TBD | TBD | TBD | Not run |
| x86 Linux baseline | TBD | TBD | TBD | TBD | TBD | Not run |

## Metrics To Fill After Migration

| Metric | Android | ARM Linux | x86 Linux |
|---|---:|---:|---:|
| Prompt eval tokens/s | TBD | TBD | TBD |
| Decode eval tokens/s | TBD | TBD | TBD |
| Peak RSS MiB | TBD | TBD | TBD |
| CPU frequency range | TBD | TBD | TBD |
| Thermal zone before/after | TBD | TBD | TBD |
| simpleperf cycles/instructions | TBD | TBD | TBD |
| simpleperf cache/branch misses | TBD | TBD | TBD |
| simpleperf task clock/test time | TBD | TBD | TBD |
| Battery counter method/change | TBD | TBD | N/A |
| Runtime/model/package archive bytes | TBD | TBD | TBD |

Battery `energy_now` or `charge_counter` changes are signed whole-device
counter observations. They are not model-exclusive energy, instantaneous
power, efficiency or attribution evidence. The standalone llama.cpp CLI path
also does not validate the InferLab JNI/AAR lifecycle; that remains a separate
on-device application task.

## Interpretation Questions

- Does Android use the expected `arm64-v8a` binary and NEON path?
- Is the workload memory-bandwidth limited or compute limited on the device?
- Does throughput change after warmup because of thermal throttling?
- Which deployment step is the highest-friction part: toolchain, packaging,
  model transfer, runtime permissions, or performance measurement?
