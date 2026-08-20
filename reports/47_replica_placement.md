# Replica Placement Plan

This report slot documents the native C++ analytical planner for model
replicas, LoRA adapter residency, hot-prefix placement, worker-pool layout,
and read-only migration preparation.

## Scope

- Compare spread, prefix-colocated, LoRA-colocated, SLO-tiered,
  drain/rebalance, and remote-overflow policies.
- Model per-GPU-group replica fit before aggregate capacity, then reconcile
  memory capacity, GPU inventory, resident replicas, and missing replicas.
- Model QPS and tenant capacity with explicit availability plus uncapped and
  bounded pressure; positive demand without capacity fails closed.
- Separate binary-GiB model/runtime, adapter, prefix, and rebalance facts.
- Keep placement isolation explicit across model, adapter, tenant, tokenizer
  template, and prefix hash.
- Produce typed prefill/decode/remote-KV pool placement without fabricating a
  replica or cache residency when capacity is zero.

## Canonical Owners

- Public contract:
  native_core/include/inferlab/advanced_serving_deployment_planning.hpp
- Validation, analysis, policy, decision, ranking, and summary:
  native_core/src/advanced_serving_replica_placement.cpp
- Six compiled scenarios:
  native_core/src/advanced_serving_replica_placement_fixture.cpp
- Projection-only artifact:
  native_core/src/replica_placement_plan_view.hpp/.cpp
- CLI request/output:
  native_core/src/replica_placement_plan_main.cpp
- Rebalance transfer time:
  advanced_serving_runtime_communication_internal.hpp::transfer_milliseconds
- Mature-product display names:
  mature_product_profile_registry.hpp

Gateway policy, request-trace routing, control-plane admission/autoscaling,
KServe/llm-d deployment descriptors, and live product schedulers remain
separate owners.

## Artifact

- Binary: build/inferlab_replica_placement_plan
- Output: results/raw/native_replica_placement_plan.json
- Reference config: configs/replica_placement.example.json
- Reference config consumption: false
- Mode: dry_run_plan
- Evidence confidence: schema_only
- Metric evidence level: modeled_not_measured

## Canonical Metrics

- replica_footprint_fits_gpu_group
- replica_footprint_gib
- usable_gpu_group_memory_gib
- resident_capacity_from_memory
- resident_capacity_from_gpu_inventory
- resident_replicas
- missing_replicas
- warm_adapter_slots
- cold_adapter_count
- prefix_shard_gib
- gpu_memory_used_gib
- gpu_memory_pressure
- modeled_qps_capacity
- qps_pressure_uncapped
- qps_pressure
- modeled_tenant_capacity
- tenant_pressure_uncapped
- tenant_pressure
- replica_coverage
- cold_model_load_ms
- adapter_fault_ms
- rebalance_gib
- rebalance_window_ms
- slo_overrun_ratio_uncapped
- placement_score
- p99_risk_score

Legacy *_gb fields contain binary GiB and remain only as explicitly scoped
compatibility aliases. Legacy observed_p99_ttft_ms is an alias for the
compiled modeled_p99_ttft_ms; no P99 observation occurred.

## Safety And Commands

- Migration guidance is read-only or server-side dry-run.
- The old mutating kubectl cordon plus kubectl drain command is not retained.
- Worker-pool GPU, replica, adapter-slot, and prefix-shard totals derive from
  one typed result and are reconciled by the model/view contract.
- Ranking uses stable first-tie selection among active compiled fixture
  records only.

## Claim Boundary

The artifact proves only that the C++17 analytical mechanism slice, strict
input contract, typed policy/decision model, compiled fixture, artifact
projection, CMake target wiring, and migration metadata are implemented.

It does not prove live scheduler placement, cache residency, gateway routing,
Kubernetes mutation, observed P99, product behavior, product replay/execute,
benchmark performance, release readiness, or mature-product core
implementation.

## Validation Status

Not run per user request: CMake configure/build, compiler/header/linker/static
initialization, native binary, malformed/duplicate/non-finite/zero-resource/
extreme/threshold/tie matrices, exact JSON or numerical parity, artifact
refresh, Summary, Prometheus, Final Report, product replay/execute, benchmark,
Kubernetes commands, and GPU validation.

## Refresh Command

    ./build/inferlab_replica_placement_plan --repo-root . --output results/raw/native_replica_placement_plan.json
