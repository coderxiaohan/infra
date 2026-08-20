# Advanced Serving Research

This report chapter tracks InferLab's deep integration plan for production-grade inference systems.

## Implemented Entrypoints

| Area | Script | Artifact |
|---|---|---|
| Structured outputs | `advanced_serving/structured_outputs.py` | `results/raw/structured_outputs.json` |
| Pooling/rerank | `advanced_serving/pooling_benchmark.py` | `results/raw/pooling_benchmark.json` |
| KServe/llm-d | `build/inferlab_kserve_llm_d_plan` | `results/raw/kserve_llm_d_plan.json` |
| Native KV-aware routing | `build/inferlab_advanced_kv_router` | `results/raw/native_advanced_kv_router.json` |
| Native disaggregated serving model | `build/inferlab_disaggregated_serving` | `results/raw/native_disaggregated_serving.json` |
| Native SLO capacity model | `build/inferlab_slo_capacity_plan` | `results/raw/native_slo_capacity_plan.json` |
| Native long context model | `build/inferlab_long_context_plan` | `results/raw/native_long_context_matrix.json` |
| Native speculative decode model | `build/inferlab_speculative_decode_plan` | `results/raw/native_speculative_decode_plan.json` |
| Native observability evidence | `build/inferlab_observability_evidence` | `results/raw/native_observability_evidence.json` |
| Native streaming/SSE model | `build/inferlab_streaming_sse_plan` | `results/raw/native_streaming_sse_plan.json` |
| Native radix prefix cache model | `build/inferlab_radix_prefix_cache_plan` | `results/raw/native_radix_prefix_cache_plan.json` |
| Native tensor parallel communication model | `build/inferlab_tensor_parallel_comm_plan` | `results/raw/native_tensor_parallel_comm_plan.json` |
| Native KServe/llm-d deployment package | `build/inferlab_kserve_llm_d_plan` | `results/raw/kserve_llm_d_plan.json` |
| Native LoRA adapter cache model | `build/inferlab_lora_adapter_cache_plan` | `results/raw/native_lora_adapter_cache_plan.json` |

Observability SLO, OTel Span Budget, OTel Sampling Budget, and Failure Drill now
expose one shared compiled release surface for TTFT/TPOT, error-budget,
trace-join, collector-backpressure, and metric/trace/alert/runbook declarations.
The shared alignment layer owns only cross-artifact consistency; telemetry,
sampling, capacity, SLO risk, and drill analysis remain with their existing
typed domain owners.
| Native chunked prefill scheduler model | `build/inferlab_chunked_prefill_scheduler_plan` | `results/raw/native_chunked_prefill_scheduler_plan.json` |
| Native cost accounting model | `build/inferlab_cost_accounting_plan` | `results/raw/native_cost_accounting_plan.json` |
| Native KV offload hierarchy model | `build/inferlab_kv_offload_hierarchy_plan` | `results/raw/native_kv_offload_hierarchy_plan.json` |
| Native engine memory planner | `build/inferlab_engine_memory_planner` | `results/raw/native_engine_memory_planner.json` |
| Native gateway policy matrix plan | `build/inferlab_gateway_policy_matrix_plan` | `results/raw/native_gateway_policy_matrix_plan.json` |
| Native replica placement plan | `build/inferlab_replica_placement_plan` | `results/raw/native_replica_placement_plan.json` |
| Native KV/PagedAttention deep dive | `build/inferlab_kv_paged_attention_deep_dive` | `results/raw/native_kv_paged_attention_deep_dive.json` |
| Native KV lifecycle deep dive | `build/inferlab_kv_lifecycle_deep_dive` | `results/raw/native_kv_lifecycle_deep_dive.json` |
| Native scheduler/batching deep dive | `build/inferlab_scheduler_batching_deep_dive` | `results/raw/native_scheduler_batching_deep_dive.json` |
| Native CUDA/Quant microkernel deep dive | `build/inferlab_cuda_quant_microkernel_deep_dive` | `results/raw/native_cuda_quant_microkernel_deep_dive.json` |
| Native KV transport timeline deep dive | `build/inferlab_kv_transport_timeline_deep_dive` | `results/raw/native_kv_transport_timeline_deep_dive.json` |
| Native radix prefix cache deep dive | `build/inferlab_radix_prefix_cache_deep_dive` | `results/raw/native_radix_prefix_cache_deep_dive.json` |
| Native KV compression blend risk | `build/inferlab_kv_compression_blend_risk` | `results/raw/native_kv_compression_blend_risk.json` |
| Native chunked prefill stall-free deep dive | `build/inferlab_chunked_prefill_stall_free_deep_dive` | `results/raw/native_chunked_prefill_stall_free_deep_dive.json` |
| Native tenant quota admission deep dive | `build/inferlab_tenant_quota_admission_deep_dive` | `results/raw/native_tenant_quota_admission_deep_dive.json` |
| Native live request migration deep dive | `build/inferlab_live_request_migration_deep_dive` | `results/raw/native_live_request_migration_deep_dive.json` |
| Native quant correctness contract | `build/inferlab_quant_correctness_contract` | `results/raw/native_quant_correctness_contract.json` |
| Native Paged KV gather kernel contract | `build/inferlab_paged_kv_gather_kernel_contract` | `results/raw/native_paged_kv_gather_kernel_contract.json` |
| Native FlashInfer replay alignment | `build/inferlab_flashinfer_replay_alignment` | `results/raw/native_flashinfer_replay_alignment.json` |
| Native TensorRT-LLM engine quant alignment | `build/inferlab_trtllm_engine_quant_alignment` | `results/raw/native_trtllm_engine_quant_alignment.json` |
| Native gateway policy matrix | `build/inferlab_gateway_policy_matrix` | `results/raw/native_gateway_policy_matrix.json` |
| Native deployment readiness matrix | `build/inferlab_deployment_readiness_matrix` | `results/raw/native_deployment_readiness_matrix.json` |
| Native OpenAI API conformance matrix | `build/inferlab_openai_api_conformance_matrix` | `results/raw/native_openai_api_conformance_matrix.json` |
| Native product replay alignment | `build/inferlab_product_replay_alignment` | `results/raw/native_product_replay_alignment.json` |
| Native deep-dive contract check | `build/inferlab_deep_dive_contract_check` | `results/raw/native_deep_dive_contract_check.json` |
| Native trace candidate join | `build/inferlab_evidence_query` | `results/raw/native_evidence_query_result.json` field `native_trace_correlation` |
| Native evidence warehouse index | `build/inferlab_evidence_warehouse_index` | `results/raw/native_evidence_warehouse_index.json` |
| Native release root-cause query | `build/inferlab_release_root_cause_query` | `results/raw/native_release_root_cause_query.json` |
| Native kernel microbench harness | `build/inferlab_kernel_microbench_harness` | `results/raw/native_kernel_microbench_harness.json` |
| Native third-party dependency gate | `build/inferlab_third_party_dependency_gate` | `results/raw/native_third_party_dependency_gate.json` |
| Native release evidence pipeline | `build/inferlab_release_evidence_pipeline` | `results/raw/native_release_evidence_pipeline.json` |
| Native evidence query | `build/inferlab_evidence_query` | `results/raw/native_evidence_query_result.json` |
| Native dependency SBOM gate | `build/inferlab_dependency_sbom_gate` | `results/raw/native_dependency_sbom_gate.json` |
| Native artifact schema registry | `build/inferlab_artifact_schema_registry` | `results/raw/native_artifact_schema_registry.json` |
| Native non-GPU acceptance checklist | `build/inferlab_non_gpu_acceptance_checklist` | `results/raw/native_non_gpu_acceptance_checklist.json` |
| Native artifact catalog single source | `build/inferlab_artifact_catalog_single_source` | `results/raw/native_artifact_catalog_single_source.json` |
| Native evidence explain repair | `build/inferlab_evidence_explain_repair` | `results/raw/native_evidence_explain_repair.json` |
| Native new artifact flow check | `build/inferlab_new_artifact_flow_check` | `results/raw/native_new_artifact_flow_check.json` |

## Operating Model

InferLab does not replace vLLM, SGLang, TensorRT-LLM, TGI, llama.cpp, LMDeploy, or KServe. It integrates them as measurable backends, then implements small simulators for the mechanisms that explain routing, KV locality, disaggregated serving, and capacity trade-offs.

All scripts default to planned or simulated output. Use `--execute` only after migrating to a target environment with the relevant serving backend.

## Native Advanced Serving Layer

The C++ layer now mirrors the most execution-sensitive planning surfaces so the project is not Python-only:

- `native_core/include/inferlab/advanced_serving.hpp` defines portable structs and estimators for KV-aware routing, prefill/decode disaggregation, and SLO capacity planning.
- `native_core/src/advanced_serving_*.cpp` specialty owners implement deterministic, dependency-free analytical slices suitable for CPU-only hosts and later native profiling.
- Continuous Batching and Tensor Parallel Communication keep their analytical formulas in `advanced_serving_runtime_communication.cpp`, reject invalid resources instead of silently normalizing them, and use target-local typed plan/integrity plus projection-only view owners. Their artifacts remain compiled dry-run fixtures, not observed scheduler, NCCL, product, or performance evidence.
- Native KV-aware routing keeps request selection and cache/load state in `advanced_serving_routing.cpp`; its artifact model anonymizes request/prefix identities, reconciles every policy decision, and optionally binds the shared native trace sidecar without turning the simulation into product telemetry.
- The active native targets listed above emit JSON artifacts wired into benchmark summary, Prometheus export, final report coverage, and `configs/benchmark_matrix.yaml`. Product replay semantic correlation is now an Evidence Query projection over the Catalog rule registry and current native artifacts; the former standalone pseudo-SQL target is retired.
