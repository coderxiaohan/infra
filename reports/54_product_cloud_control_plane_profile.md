# Product Cloud Control Plane Profile

本章节覆盖 NVIDIA Dynamo、KServe/llm-d 和 Ray Serve。InferLab 不重写控制面、scheduler、gateway、CRD reconciliation 或 autoscaler。

## 产品已有能力

- Dynamo: disaggregated inference runtime、router、worker pool、KV transfer、control plane。
- KServe/llm-d: LLMInferenceService、KV-cache-aware scheduling、prefill-decode separation、gateway routing。
- Ray Serve: Serve DAG、autoscaling、OpenAI ingress、KV offload deployment。

## InferLab 接入方式

| 产品 | Runner | Config | Artifact |
|---|---|---|---|
| Dynamo | `scripts/product_profiles/profile_dynamo.py` | `configs/product_profiles/dynamo.example.yaml` | `results/raw/product_dynamo_serving_profile.json` |
| KServe/llm-d | `scripts/product_profiles/profile_kserve_llmd.py` | `configs/product_profiles/kserve_llmd.example.yaml` | `results/raw/product_kserve_llmd_profile.json` |
| Ray Serve | `scripts/product_profiles/profile_ray_serve.py` | `configs/product_profiles/ray_serve.example.yaml` | `results/raw/product_ray_serve_profile.json` |

```bash
python3 -m scripts.product_profiles.profile_dynamo --repo-root . --config configs/product_profiles/dynamo.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_dynamo --repo-root . --config configs/product_profiles/dynamo.example.yaml --replay --evidence-dir profiling/dynamo
python3 -m scripts.product_profiles.profile_kserve_llmd --repo-root . --config configs/product_profiles/kserve_llmd.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_kserve_llmd --repo-root . --config configs/product_profiles/kserve_llmd.example.yaml --replay --evidence-dir profiling/kserve_llmd
python3 -m scripts.product_profiles.profile_ray_serve --repo-root . --config configs/product_profiles/ray_serve.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_ray_serve --repo-root . --config configs/product_profiles/ray_serve.example.yaml --replay --evidence-dir profiling/ray_serve
```

## 采集 Artifact

| 字段 | Dynamo | KServe/llm-d | Ray Serve |
|---|---|---|---|
| version_capture | deployment image、worker/router version | CRD、controller、image tag | ray、serve version |
| config_snapshot | topology、router、worker pools | LLMInferenceService YAML | Serve DAG、autoscaling policy |
| raw_metrics | router latency、queue depth、KV transfer | CRD status、ready replicas、gateway | deployment ready、replicas、P99 |
| replay_evidence | router/worker logs、KV transfer metrics、JSON/Prometheus | kubectl/CRD/gateway metrics、YAML/status dump | Ray Serve logs、deployment graph、metrics dump |
| claim_scope | dry-run/execute/replay 区分 | dry-run/execute/replay 区分 | dry-run/execute/replay 区分 |
| not_implemented_by_inferlab | router/control plane/KV transfer | controller/scheduler/gateway | Serve controller/autoscaler |

## 验收

- Manifest generated、manifest applied、service ready、traffic measured 四个阶段必须分开。
- 云原生产品的真实性能结论必须来自 Kubernetes/Ray/Dynamo 真实环境 artifact。
- replay 模式适合导入目标集群导出的 CRD status、gateway metrics、router logs 和 Prometheus dump，不会访问当前机器的 Kubernetes。
