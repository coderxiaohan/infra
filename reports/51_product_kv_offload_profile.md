# Product KV Offload Profile

本章节记录 InferLab 对 LMCache、Ray Serve KV offload 和 Mooncake 的接入方式。InferLab 不重写 KV storage/offload/connector，只统一采集和解释证据。`kv_offload` 保留为聚合产品族 profile；Mooncake 另有显式 profile surface，便于单独采集远端 KV 复用、传输和一致性风险证据。

## 产品已有能力

- LMCache connector、CPU/disk KV reuse、prefix cache/offload。
- Ray Serve LLM deployment 和 KV cache offloading。
- Mooncake distributed KV cache、remote KV reuse、transfer path。

## InferLab 接入方式

- Profile runner: `scripts/product_profiles/profile_kv_offload.py`
- Config: `configs/product_profiles/kv_offload.example.yaml`
- Artifact: `results/raw/product_lmcache_kv_offload_profile.json`
- Mooncake runner: `scripts/product_profiles/profile_mooncake.py`
- Mooncake config: `configs/product_profiles/mooncake.example.yaml`
- Mooncake artifact: `results/raw/product_mooncake_kv_profile.json`
- Report: `reports/51_product_kv_offload_profile.md`

```bash
python3 -m scripts.product_profiles.profile_kv_offload --repo-root . --config configs/product_profiles/kv_offload.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_kv_offload --repo-root . --config configs/product_profiles/kv_offload.example.yaml --execute --endpoint http://127.0.0.1:8000
python3 -m scripts.product_profiles.profile_kv_offload --repo-root . --config configs/product_profiles/kv_offload.example.yaml --replay --evidence-dir profiling/kv_offload
python3 -m scripts.product_profiles.profile_mooncake --repo-root . --config configs/product_profiles/mooncake.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_mooncake --repo-root . --config configs/product_profiles/mooncake.example.yaml --execute --endpoint http://127.0.0.1:8000
python3 -m scripts.product_profiles.profile_mooncake --repo-root . --config configs/product_profiles/mooncake.example.yaml --replay --evidence-dir profiling/mooncake
```

## 采集 Artifact

| 字段 | 内容 |
|---|---|
| official_sources | LMCache、Ray KV offload、Mooncake |
| environment_preflight | connector module、serving endpoint、cache path |
| version_capture | LMCache/Ray/product version |
| config_snapshot | cache tier、connector、storage path、serving engine |
| raw_metrics | cache hit、offload bytes、restore latency、HBM saved、TTFT delta |
| replay_evidence | 已采集 cache hit/miss、KV transfer、restore latency、Prometheus/日志/JSON/CSV 证据 |
| claim_scope | 区分 dry-run 计划、execute 在线采集、replay 离线证据回放 |
| not_implemented_by_inferlab | LMCache connector、Ray KV engine、Mooncake storage、KV correctness policy |

## 验收

- 严格前缀复用和非前缀/语义复用必须分开标记。
- 非前缀/语义复用必须写 correctness risk，不能只报性能收益。
- Mooncake 真实收益必须来自 `product_mooncake_kv_profile.json` 中的版本、配置、远端 KV 传输、restore latency 和环境证据。
- replay 模式可把目标机器上的 LMCache/Ray/Mooncake 日志和 metrics 转成统一 artifact，但不会替代正确性风险标注。
