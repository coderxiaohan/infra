# Gateway Policy Matrix

This report slot documents the native C++ gateway policy planner for production LLM serving control planes.

## Scope

- Compare round-robin, least-loaded, prefix/LoRA-aware, SLO rescue, overload shedding, and canary/shadow routing policies.
- Keep routing-key isolation explicit: base model, adapter, tokenizer template, prefix hash, and tenant tier must all be part of cache-safe routing state.
- Treat the module as a compiled analytical fixture until real gateway traces are imported.
- Do not describe `compiled_ttft_ms`, backend-support scores, policy scores, or expected TTFT deltas as observations or measured performance.

## Implementation Ownership

- Public typed contract: `native_core/include/inferlab/advanced_serving_scheduler.hpp`
- Gateway policy profiles, strict input validation, analytical scoring, decision, recommendation, and stable ranking: `native_core/src/advanced_serving_gateway_policy.cpp`
- Projection-only artifact renderer: `native_core/src/gateway_policy_matrix_plan_view.hpp/.cpp`
- CLI/path/output only: `native_core/src/gateway_policy_matrix_plan_main.cpp`

The existing `simulate_kv_aware_routing()` remains the canonical request-trace
mechanism simulator. This matrix has aggregate hit-rate inputs, so it does not
pretend to invoke that request-level simulator. The mature-product
`native_gateway_policy_matrix.json` artifact is a separate borrowed-gap
explainer and is not an alias for this native compiled fixture.

## Artifact

- Binary: `build/inferlab_gateway_policy_matrix_plan`
- Output: `results/raw/native_gateway_policy_matrix_plan.json`
- Config reference: `configs/gateway_policy_matrix.example.json`

The config is reference-only (`consumed_by_current_cli: false`). The current
target uses `inferlab::default_gateway_policy_scenarios()` so its scenario and
assumption provenance is explicit rather than silently implying config parsing.

## Metrics

- `prefix_locality_score`
- `lora_affinity_score`
- `load_balance_score`
- `tenant_fairness_score`
- `slo_rescue_score`
- `admission_drop_rate`
- `routed_qps`
- `shed_qps`
- `expected_ttft_delta_ms`
- `control_plane_overhead_ms`
- `p99_risk_score`

## Acceptance

- The artifact must include at least six gateway policies.
- Each policy must state required production metrics, routing-key isolation, recommendation, and migration commands.
- The report must not claim real p99 gains until the numbers come from live gateway traces.
- Policy identity is typed; substring matching in the CLI is not allowed.
- Counts, token sums, finite scalar inputs, normalized scores, policy coverage, and active-workload ranking must fail closed.
- `mode=dry_run_plan`, `executed=false`, and `replayed=false` remain explicit.

## Claim Boundary

This artifact demonstrates a C++17 mechanism slice for strict gateway-policy
analysis and migration planning. It does not execute a gateway, enforce tenant
quota or overload shedding, replay product logs, run the listed migration
commands, or produce measured TTFT/routing quality. External product names are
research alignment only and are not InferLab self-developed product cores.

## Validation Status

Not run per user request. CMake, compilation, native execution, product replay,
gateway execute, benchmark, summary, Prometheus export, and final report were
not run on the current server.

## Migration Command

```bash
./build/inferlab_gateway_policy_matrix_plan --repo-root . --output results/raw/native_gateway_policy_matrix_plan.json
```
