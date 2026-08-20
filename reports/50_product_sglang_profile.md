# Product SGLang Profile

本章节记录 InferLab 对 SGLang 的产品化接入方式。SGLang 是成熟 serving engine；InferLab 不重写 SGLang runtime、TBO、RadixAttention、HiCache 或 model gateway。

## 产品已有能力

- TBO、RadixAttention/HiCache、PD/EPD disaggregation。
- expert parallelism、structured outputs、reasoning/tool parser、model gateway。

## InferLab 接入方式

- Profile runner: `scripts/product_profiles/profile_sglang.py`
- Config: `configs/product_profiles/sglang.example.yaml`
- Artifact: `results/raw/product_sglang_capability_profile.json`
- Report: `reports/50_product_sglang_profile.md`

```bash
python3 -m scripts.product_profiles.profile_sglang --repo-root . --config configs/product_profiles/sglang.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_sglang --repo-root . --config configs/product_profiles/sglang.example.yaml --execute --endpoint http://127.0.0.1:30000
python3 -m scripts.product_profiles.profile_sglang --repo-root . --config configs/product_profiles/sglang.example.yaml --replay --evidence-dir profiling/sglang
```

## 采集 Artifact

| 字段 | 内容 |
|---|---|
| official_sources | SGLang server arguments 文档 |
| environment_preflight | SGLang module、GPU 工具、endpoint |
| version_capture | SGLang / torch / nvidia-smi 版本 |
| config_snapshot | server args、gateway、cache、parser 配置 |
| raw_metrics | TTFT、TPOT、throughput、HiCache hit、parser success、P99 |
| replay_evidence | 已采集 SGLang server logs、gateway/cache metrics、JSON/CSV/Prometheus 证据 |
| claim_scope | 区分 dry-run 计划、execute 在线采集、replay 离线证据回放 |
| not_implemented_by_inferlab | SGLang runtime、TBO、RadixAttention、HiCache、gateway |

## 验收

- dry-run 只描述接入计划和 schema。
- execute 必须从真实 SGLang endpoint、日志或 metrics 采集证据。
- replay 只读取目标机器导出的 SGLang 日志和指标，不启动服务或依赖本机 GPU。
