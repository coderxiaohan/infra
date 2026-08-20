# Product Kernel And Hybrid Runtime Profile

本章节覆盖 FlashInfer 和 KTransformers。InferLab 不复制 FlashInfer kernels，也不重写 KTransformers heterogeneous runtime。

## 产品已有能力

- FlashInfer: attention、GEMM、MoE、cascade inference、paged KV APIs。
- KTransformers: CPU/GPU heterogeneous inference、MoE expert placement、hybrid memory。

## InferLab 接入方式

| 产品 | Runner | Config | Artifact |
|---|---|---|---|
| FlashInfer | `scripts/product_profiles/profile_flashinfer.py` | `configs/product_profiles/flashinfer.example.yaml` | `results/raw/product_flashinfer_kernel_profile.json` |
| KTransformers | `scripts/product_profiles/profile_ktransformers.py` | `configs/product_profiles/ktransformers.example.yaml` | `results/raw/product_ktransformers_hybrid_profile.json` |

```bash
python3 -m scripts.product_profiles.profile_flashinfer --repo-root . --config configs/product_profiles/flashinfer.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_flashinfer --repo-root . --config configs/product_profiles/flashinfer.example.yaml --replay --evidence-dir profiling/flashinfer
python3 -m scripts.product_profiles.profile_ktransformers --repo-root . --config configs/product_profiles/ktransformers.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_ktransformers --repo-root . --config configs/product_profiles/ktransformers.example.yaml --replay --evidence-dir profiling/ktransformers
```

## 采集 Artifact

| 字段 | FlashInfer | KTransformers |
|---|---|---|
| version_capture | flashinfer、torch、CUDA | ktransformers、runtime args |
| config_snapshot | shape matrix、dtype、kernel family | model path、placement config、memory tier |
| raw_metrics | kernel availability、shape coverage、fallback | load time、CPU/GPU memory、tokens/s、expert placement |
| replay_evidence | capability logs、kernel CSV/JSON、Nsight export | placement logs、memory/latency metrics、runtime JSON/CSV |
| claim_scope | dry-run/execute/replay 区分 | dry-run/execute/replay 区分 |
| not_implemented_by_inferlab | attention/GEMM/MoE kernels | heterogeneous runtime、MoE placement runtime |

## 验收

- FlashInfer 只记录 capability/fallback，不把 kernel 复制进 InferLab。
- KTransformers 只记录 placement 和 runtime artifact，不重写运行时。
- replay 模式可导入目标机器的 kernel/profile 日志，但仍必须标记哪些 shape 需要真实 benchmark 才能确认。
