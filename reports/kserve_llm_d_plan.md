# KServe/llm-d 原生部署计划

本报告对应唯一正式入口 `build/inferlab_kserve_llm_d_plan`。该 C++ owner 同时输出部署场景、manifest 字段映射和发布资源对账，替代旧 `inferlab_kserve_llmd_manifest_plan` 与 Python 同路径 producer。当前机器不要求安装 Kubernetes、KServe 或 llm-d；模块只输出 dry-run 计划和迁移命令，真实部署结果必须在目标集群生成 execute artifact 后再写入最终结论。

## 已实现范围

| 能力 | 状态 | 产物 |
|---|---|---|
| single-node serving 场景 | 已实现 | `results/raw/kserve_llm_d_plan.json` |
| multi-node tensor parallel 场景 | 已实现 | `results/raw/kserve_llm_d_plan.json` |
| prefill/decode separation 场景 | 已实现 | `results/raw/kserve_llm_d_plan.json` |
| KV-cache-aware routing 字段映射 | 已实现 | `records[].field_metric_mapping` |
| autoscaling placeholder 与 SLO 关联 | 已实现 | `records[].slo_fit_score` |
| Gateway/API ingress 迁移命令 | 已实现 | `records[].migration_commands` |
| Docker compose 与 KServe/llm-d 选择建议 | 已实现 | `records[].docker_compose_guidance` |
| GPU/replica/TP/PP 发布资源对账 | 已实现 | `release_deployment_resource_profile` |

## C++ 模块

- 源码：`native_core/src/kserve_llm_d_plan_main.cpp`
- 公共模型：`native_core/include/inferlab/advanced_serving.hpp`
- 估算逻辑：`native_core/src/advanced_serving_deployment_planning.cpp`
- 构建目标：`inferlab_kserve_llm_d_plan`
- benchmark matrix：`configs/benchmark_matrix.yaml`

## 输出 Schema

`results/raw/kserve_llm_d_plan.json` 包含：

- `schema_version`
- `status`
- `executed`
- `dry_run`
- `summary.best_slo_fit_scenario`
- `summary.lowest_complexity_scenario`
- `summary.max_gpu_request`
- `release_deployment_resource_profile`
- `records[].scenario`
- `records[].deployment_mode`
- `records[].routing_readiness_score`
- `records[].slo_fit_score`
- `records[].deployment_complexity_score`
- `records[].field_metric_mapping`
- `records[].migration_commands`

## 迁移执行命令

```bash
./build/inferlab_kserve_llm_d_plan \
  --repo-root . \
  --output results/raw/kserve_llm_d_plan.json \
  --report reports/kserve_llm_d_plan.md
```

目标集群侧再执行模块输出的 `migration_commands`，例如 namespace dry-run、server-side apply dry-run、资源查询、Prometheus 指标导出和 gateway smoke check。

## 验收标准

- 不要求当前机器 apply YAML。
- 必须能区分 single-pool、tensor parallel、prefill/decode split、KV-aware decode pool。
- 必须给出 Docker compose 与 KServe/llm-d 的选择边界。
- 必须把 YAML/CRD 字段映射到 InferLab 指标：TTFT、TPOT、QPS、replicas、GPU request、routing readiness 和 SLO fit。
- 最终报告只能引用 `results/raw/kserve_llm_d_plan.json` 中存在的真实字段。
- 不得恢复第二个 manifest executable、第二个 artifact identity 或 Python 同路径 writer。
