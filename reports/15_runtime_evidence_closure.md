# Runtime Evidence Closure

This chapter ties the original InferLab plan to migration-ready execution
scripts. The current development server can keep these as planned artifacts;
target machines can execute them without changing repository structure.

## Closure Scripts

| Area | Script | Output |
|---|---|---|
| CUDA correctness and Nsight profiling | `scripts/run_cuda_profile_matrix.py` | `results/raw/cuda_profile_matrix.json` |
| llama.cpp CPU/GPU/offload matrix | `scripts/run_llama_cpp_matrix.py` | `results/raw/llama_cpp_matrix.json` |
| Android/ARM adb/simpleperf run | `scripts/run_android_edge_benchmark.py` | `results/raw/android_edge_benchmark.json` |
| Upstream source trace verification | `scripts/verify_source_traces.py` | `results/raw/source_trace_verification.json` |
| Plan evidence checklist | `scripts/render_evidence_checklist.py` | `results/raw/evidence_checklist.json` |
| Native runtime evidence freshness gate | `build/inferlab_runtime_evidence_freshness_gate` | `results/raw/native_runtime_evidence_freshness_gate.json` |

## Migration Rule

Run the scripts first without `--execute` to review commands, then run with
`--execute` only on a machine that has the required CUDA, vLLM,
TensorRT-LLM, llama.cpp, Android, or monitoring environment.

## Freshness Gate

`inferlab_runtime_evidence_freshness_gate` is a C++ native evidence gate for
release and report hygiene. It reads artifact timestamps under `results/raw`
and emits freshness state, maximum allowed age, stale policy, refresh command,
hardware/execute requirements, and release-blocking status for runtime evidence
such as CUDA profiles, vLLM/TensorRT-LLM serving runs, llama.cpp matrices,
Android evidence, profiling imports, source trace verification, benchmark
summary, and the evidence checklist.

The gate does not run benchmarks or external products. Stale or missing target
environment evidence keeps performance claims in pending or blocked state until
the appropriate refresh command is run on a capable machine.

The native gate accepts JSON-only output mode via `--format json`. Unsupported
formats and unsupported positional forms are rejected by shared CLI validation.
The emitted artifact includes `cli_context` with strict CLI flags,
`reference_age_hours`, supported formats, and redacted repo/results/output path
status. This context is input-contract metadata only; it does not refresh
artifacts or create benchmark, replay, execute, CUDA, product, summary,
Prometheus, or final-report evidence.

The freshness gate also consumes catalog-managed artifacts from
`high_value_artifact_catalog.hpp`. Registry-derived targets are merged with the
legacy runtime/product target list, so catalog artifacts receive freshness
coverage without losing explicit benchmark, profiler, and product execute
fallbacks.

The selector repair-card export artifact
`native_evidence_query_selector_repair_card_export.json` is included in this
freshness surface as read-only Evidence Query handoff evidence. Missing or stale
exports can block review handoff, but they do not imply failed benchmark,
replay, or execute evidence.

## Refresh Command

```bash
./build/inferlab_runtime_evidence_freshness_gate --repo-root . --results results/raw --output results/raw/native_runtime_evidence_freshness_gate.json --format json
```
