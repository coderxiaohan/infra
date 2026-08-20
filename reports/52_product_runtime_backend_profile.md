# Product Runtime Backend Profile

本章节覆盖 TensorRT-LLM/Triton、LMDeploy 和 Hugging Face TGI 的产品化接入。InferLab 不重写 compiled runtime、Triton backend、LMDeploy TurboMind、persistent batching、blocked KV cache、TGI server、low precision kernel 或 continuous batching runtime。

## 产品已有能力

- TensorRT-LLM: engine build、compiled runtime、FP8/FP4/NVFP4、Triton backend、in-flight batching。
- LMDeploy: TurboMind runtime、persistent batching、blocked KV cache、KV cache quantization、AWQ、multi-model serving。
- TGI: production server、SSE streaming、Prometheus/OpenTelemetry、continuous batching、quantization。

## InferLab 接入方式

| 产品 | Runner | Config | Artifact |
|---|---|---|---|
| TensorRT-LLM/Triton | `scripts/product_profiles/profile_tensorrtllm.py` | `configs/product_profiles/tensorrtllm.example.yaml` | `results/raw/product_tensorrtllm_runtime_profile.json` |
| LMDeploy | `scripts/product_profiles/profile_lmdeploy.py` | `configs/product_profiles/lmdeploy.example.yaml` | `results/raw/product_lmdeploy_runtime_profile.json` |
| TGI | `scripts/product_profiles/profile_tgi.py` | `configs/product_profiles/tgi.example.yaml` | `results/raw/product_tgi_profile.json` |

```bash
python3 -m scripts.product_profiles.profile_tensorrtllm --repo-root . --config configs/product_profiles/tensorrtllm.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_tensorrtllm --repo-root . --config configs/product_profiles/tensorrtllm.example.yaml --replay --evidence-dir profiling/tensorrtllm
python3 -m scripts.product_profiles.profile_lmdeploy --repo-root . --config configs/product_profiles/lmdeploy.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_lmdeploy --repo-root . --config configs/product_profiles/lmdeploy.example.yaml --replay --evidence-dir profiling/lmdeploy
python3 -m scripts.product_profiles.profile_tgi --repo-root . --config configs/product_profiles/tgi.example.yaml --dry-run
python3 -m scripts.product_profiles.profile_tgi --repo-root . --config configs/product_profiles/tgi.example.yaml --replay --evidence-dir profiling/tgi
```

## 采集 Artifact

| 字段 | TensorRT-LLM/Triton | LMDeploy | TGI |
|---|---|---|---|
| version_capture | TensorRT-LLM、TensorRT、Triton、CUDA | lmdeploy、TurboMind、CUDA、model config | launcher、server、image tag |
| config_snapshot | build flags、engine dir、Triton repo | engine、persistent batching、blocked KV、KV cache quant、model path | model id、launcher args、streaming |
| raw_metrics | build time、engine size、TTFT、TPOT、GPU memory | TTFT、TPOT、tokens/s、P99、GPU memory、KV cache quant、batching state | first chunk、stream gap、tokens/s、Prometheus metrics |
| replay_evidence | build/runtime logs、Triton metrics、Nsight/CSV/JSON | LMDeploy logs、metrics、runtime JSON/CSV | TGI logs、SSE timing、Prometheus/OTel export |
| claim_scope | dry-run/execute/replay 区分 | dry-run/execute/replay 区分 | dry-run/execute/replay 区分 |
| not_implemented_by_inferlab | builder、plugins、Triton backend、kernels | TurboMind runtime、persistent batching、blocked KV runtime、KV quant kernels | TGI server、runtime、attention backend |

## 验收

- TensorRT-LLM engine 证据必须绑定 engine metadata 和 GPU 架构。
- LMDeploy 真实结论必须绑定 model、TurboMind/runtime config、版本、GPU 环境和 endpoint/metrics artifact。
- TGI streaming 结论必须来自真实 SSE/metrics artifact。
- replay 模式只规范化已采集证据，不构建 engine、不启动 Triton/LMDeploy/TGI，也不把本机环境当成目标环境。
