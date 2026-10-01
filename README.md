# InferLab

InferLab 是一个以 C++/CUDA 为核心、面向端侧与云侧场景的大模型推理平台。它把可独立检查的推理机制实现、成熟推理产品的适配器，以及一整套"证据"生成/校验体系组合在一起，让实验过程与结果都能保存为可复现、机器可读的产物。

InferLab 不是聊天机器人演示项目，也不尝试替代 llama.cpp、vLLM、TensorRT-LLM、SGLang、KServe/llm-d 等生产级推理运行时。它的目标是让推理行为可以被测量、比较和解释：

```text
配置实验
    -> 运行原生机制或推理后端
    -> 收集指标与环境证据
    -> 校验并比较产物
    -> 保存可复现的 JSON 结果
```

## 阅读导航

| 目标 | 建议入口 |
| --- | --- |
| 第一次运行项目 | [最短验证路径](#最短验证路径) |
| 理解 C++/CUDA 与 Python 的职责 | [架构总览](#架构总览)、[实现边界](#实现边界) |
| 查找模块和脚本 | [代码结构](#代码结构) |
| 分别验证 Python、CPU 或完整基础流程 | [分项运行](#分项运行) |
| 接入端侧、云侧运行时 | [端侧使用方式](#端侧使用方式)、[云侧使用方式](#云侧使用方式) |



## 最短验证路径

以下命令在仓库根目录构建 CPU 目标、运行 CTest，并将本次基础流程写入独立临时目录。本文示例统一在项目的 `tmp/` 下创建临时目录；如果仓库位于 `~/Projects/infra`，则输出保存在 `~/Projects/infra/tmp/`。每次运行会生成带随机后缀的子目录，避免混入历史结果。`tmp/` 已由 `.gitignore` 排除，结果会保留在本地，可在确认不再需要后自行清理：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_CUDA=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure

mkdir -p "$PWD/tmp"
BASIC_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-basic.XXXXXX")"
INFERLAB_RESULTS_DIR="$BASIC_RESULTS" bash scripts/run_all_benchmarks.sh
printf 'results: %s\n' "$BASIC_RESULTS"
```

`run_all_benchmarks.sh` 会执行基础环境采集、可选后端预检、Native Probes、结果汇总和 Python 回归测试。
## 架构总览

InferLab 分三层：

```text
┌───────────────────────────────────────────────────────────┐
│ Native 层（C++17 / 可选 CUDA C++，native_core/ + cuda_kernels/）│
│  ├─ 推理机制核心：KV Cache、Paged KV、调度、Decoder、矩阵/归一化/│
│  │   RoPE/Attention/量化 内核，CPU 与 CUDA 双实现              │
│  ├─ 端侧运行时契约：嵌入式 Runtime Matrix、llama.cpp/ExecuTorch │
│  │   Adapter、Android JNI 桥接、端云路由                        │
│  └─ 证据与治理层：审计台账、证据仓库、Schema/Catalog 校验、       │
│      Serving 机制研究（调度 sweep、投机解码、前缀复用等）的        │
│      独立 CLI 可执行程序，以 JSON 输出结构化证据                  │
└───────────────────────┬───────────────────────────────────┘
                         │ 命令行调用 + JSON 产物
┌───────────────────────▼───────────────────────────────────┐
│ Python 编排层（scripts/、mini_llm/、advanced_serving/ 等）      │
│  ├─ 实验配置、环境预检、进程控制（受限执行、输出脱敏）             │
│  ├─ 后端测试工具（llama.cpp / vLLM / TensorRT-LLM）适配          │
│  ├─ 结果校验、对比、汇总、上游源码版本核对                        │
│  └─ mini_llm/ 轻依赖 PyTorch 参考实现（用于解释模型级行为，        │
│      不是生产运行时核心）                                        │
└───────────────────────┬───────────────────────────────────┘
                         │ 写入
┌───────────────────────▼───────────────────────────────────┐
│ 证据产物层（results/、configs/ 中的 Schema/Fixture）             │
│  JSON / JSONL / CSV，带状态字段（ok/planned/skipped/failed…）   │
└───────────────────────────────────────────────────────────┘
```



## 实现边界

| 层 | 负责 | 不负责 |
| --- | --- | --- |
| C++17 / CUDA C++ | KV Cache、Paged KV、调度、Decoder、CPU/CUDA 内核、嵌入式运行时契约，以及原生诊断和证据 CLI | 外部推理服务生命周期、跨产品结果汇总 |
| Python | 实验配置、环境预检、受限进程控制、后端适配、产物解析/校验/比较、部署计划 | 生产推理热路径、改写原生计算结果 |

`mini_llm/` 是轻依赖的参考实现层，用于解释模型级行为。

## 代码结构

### 顶层目录

```text
InferLab/
├── CMakeLists.txt           C++17 主构建入口；注册原生核心、CLI、CTest 和可选 CUDA/端侧集成
├── native_core/             原生机制与运行时主体
│   ├── include/inferlab/    对外 C++ 接口和运行时契约
│   ├── src/                 推理机制、端云策略、诊断/证据模型及各类原生 CLI
│   └── tests/               KV Cache、Scheduler、Attention、Decoder、量化等 CPU 单元测试
├── cuda_kernels/            矩阵运算、归一化、RoPE、Attention、量化和 Paged KV 的可选 CUDA 实现与测试
├── mini_llm/                PyTorch 参考模型、KV 容量估算、工作负载画像和瓶颈分析
├── advanced_serving/        Pooling、结构化输出、工具调用解析等 Python Serving 实验
├── quantization/            依赖无关的量化参考实现与误差分析
├── benchmarks/              面向单项机制的轻量 Benchmark 入口
├── scripts/                 实验编排、后端适配、进程控制、结果校验、汇总和部署规划
│   └── product_profiles/    vLLM、TensorRT-LLM、SGLang 等产品 Profile 解析器
├── configs/                 实验输入、矩阵配置、Schema、Fixture 和产品 Profile
│   ├── schemas/             机器可读产物与证据契约
│   ├── evidence_fixtures/   校验器和证据流程的固定样本
│   └── product_profiles/    外部推理产品的能力描述与 Replay 样本
├── edge/                    GGUF 二进制解析、检查和 llama.cpp 辅助入口
├── android/                 Android JNI/AAR 集成与最小 Native 冒烟工程
├── examples/reference/      可独立运行的机制参考案例
├── docker/                  开发环境和 vLLM 镜像定义
├── deploy/                  Docker Compose、KServe/llm-d 与运行时部署资源
├── monitoring/              Prometheus 采集配置和 Grafana Dashboard
├── profiling/               Nsight 等性能分析记录
├── tests/                   Python 编排、参考实现、安全和产物契约回归测试
├── results/                 生成的 JSON/JSONL/CSV 证据、对比结果和端云案例
│   ├── raw/                 Benchmark、Probe 和汇总原始产物
│   ├── showcase/            代表性端云实验运行记录
│   ├── verification/        用于版本控制的轻量验证状态快照
│   └── tables/              表格与指标导出
├── requirements-pytorch.txt Python 参考层的可选 PyTorch 依赖
└── README.md                项目入口、构建方式、能力边界与验收说明
```

各目录之间的主数据流如下：

```text
configs/ ──> scripts/ ──> native_core/、cuda_kernels/ 或外部推理后端
                              │
                              └──> results/ ──> 校验、对比与汇总
```

`native_core/` 和 `cuda_kernels/` 承担需要确定性、性能或贴近运行时的机制实现；`mini_llm/`、`advanced_serving/`、`quantization/` 与 `examples/reference/` 提供便于解释和对照的 Python 参考实现；`scripts/` 负责把配置、原生可执行程序、外部后端和机器可读产物连接成完整实验流程，不承载推理热路径。

llama.cpp、ExecuTorch、vLLM、TensorRT-LLM 以及模型文件不复制进本仓库。它们通过 CMake 路径、命令行参数、Endpoint 或配置文件接入；`configs/product_profiles/` 中的描述和 Replay 样本只表示集成契约，不等同于对应产品已经安装或完成真实执行。

`results/` 是生成产物目录，不是算法源码。`README.md` 是项目唯一维护的 Markdown 文档，实验报告和汇总统一保存为 JSON、JSONL 或 CSV。

### `native_core/` 内部结构

`native_core/src` 下约 700 个 `.cpp`/`.hpp` 文件，`CMakeLists.txt` 中共声明 87 个可执行目标（含 7 个 CUDA 测试、15 个 CPU 单元测试）。按职责分组：

| 分组 | 代表模块 | 说明 |
| --- | --- | --- |
| 推理机制核心 | `kv_cache`、`static_kv_cache`、`paged_kv_cache`、`paged_kv_locality`、`attention`、`paged_attention`、`continuous_batch_scheduler`、`decoder_block`、`decoder_kv`、`matrix_ops`、`normalization`、`rope`、`quantization`、`gguf` | 实际推理热路径，`native_core/tests/` 中 15 个 CTest 用例覆盖 |
| 端侧运行时 | `edge_runtime`、`edge_runtime_matrix*`、`edge_llama_cpp_runtime` | 嵌入式 Runtime Matrix、llama.cpp/ExecuTorch Adapter 契约 |
| Serving 机制研究 CLI | `scheduler_*_sweep`（准入/公平性/压力/容量/工作负载）、`speculative_decode_plan`、`radix_prefix_cache_plan`、`lora_adapter_cache_plan`、`chunked_prefill_scheduler_plan`、`tensor_parallel_comm_plan`、`slo_capacity_plan`、`replica_placement_plan`、`streaming_sse_plan` | 每个都是独立的 `inferlab_*` 可执行程序，输出 JSON 证据，不是推理服务本身 |
| 证据与治理层 | `native_core_audit_register`、`evidence_warehouse_index`、`release_evidence_pipeline*`、`repository_evidence_dag`、`runtime_evidence_freshness_gate`、`claim_scope_contract`、`dependency_governance`、`artifact_catalog_single_source`、`artifact_schema_registry`、`evidence_query*` | 审计台账、证据仓库索引、发布前校验门禁；不参与推理计算 |
| 成熟产品对照 | `mature_product_*`、`product_profile_contract_check`、`product_replay_alignment`、`product_replay_semantic_events`、`product_integration_inventory`、`kernel_compatibility_matrix` | 依据 `configs/product_profiles/` 的 Profile/Replay 样本做契约级比对，不代表真实跑过对应产品 |
| Deep Dive 诊断程序 | `scheduler_batching_deep_dive`、`scheduler_root_cause_deep_dive`、`kv_paged_attention_deep_dive`、`kv_tier_decision_deep_dive`、`pd_split_crossover_deep_dive`、`cuda_quant_microkernel_deep_dive`、`deep_dive_contract_check` | 针对单一主题的深入诊断可执行程序 |

完整目标列表以 `CMakeLists.txt` 中的 `add_executable` 声明为准。

### `scripts/` 内部结构

| 分组 | 代表文件 |
| --- | --- |
| 编排/执行入口 | `run_all_benchmarks.sh`、`run_experiment_matrix.py`、`run_edge_cloud_showcase.py`、`run_native_probes.py` |
| 后端测试工具 | `run_llama_cpp_benchmark.py`、`run_llama_cpp_matrix.py`、`run_vllm_benchmark.py`、`run_trtllm_benchmark.py`、`build_trtllm_showcase_engine.py` |
| 校验与门禁 | `validate_outputs.py`、`verify_source_traces.py`、`render_evidence_checklist.py`、`compare_gguf_fixture.py` |
| 证据处理/汇总 | `summarize_benchmarks.py`、`compare_backends.py`、`evaluate_backend_quality.py`、`evidence_projection.py`、`artifact_catalog_glue.py`、`release_audit_projection.py` |
| 部署规划 | `plan_runtime_deploy.py`、`plan_android_deploy.py`、`plan_profiling_runs.py` |
| Android 端侧 | `run_android_edge_benchmark.py`、`android_device_evidence.py`、`android_benchmark_output.py` |
| 环境/系统信息 | `collect_system_info.py`、`check_cuda_environment.py`、`run_cuda_profile_matrix.py`、`parse_profiling_artifacts.py`、`export_prometheus_metrics.py` |
| 公共工具库 | `bounded_process.py`/`bounded_http.py`（受限子进程/HTTP 执行）、`command_redaction.py`（敏感信息脱敏）、`artifact_json.py`/`strict_json_document.py`（原子写入 JSON）、`repo_paths.py`/`path_status.py`/`time_utils.py`/`cli_validation.py`/`text_output.py`/`json_projection.py`/`quiet_cli_output.py` |
| GGUF 工具 | `generate_gguf_fixture.py`、`compare_gguf_fixture.py` |
| 报表导出 | `export_mini_transformer_tables.py` |

### Python 参考/研究层

| 目录 | 内容 |
| --- | --- |
| `mini_llm/` | `shape_probe.py`（Prefill/Decode 形状与 KV 显存追踪）、`transformer.py`（PyTorch Mini Transformer 参考实现）、`kv_cache.py`（KV 显存估算）、`workload_profile.py`（静态负载画像）、`bottleneck_advisor.py`（Roofline 瓶颈建议）、`cache_compare.py`（缓存解码 vs 全量重算对比）、`scenario_sweep.py`（场景扫描）、`generate.py`（生成 CLI） |
| `advanced_serving/` | `pooling_benchmark.py`（Embedding/Rerank Pooling）、`structured_outputs.py`（结构化输出/受限解码检查）、`tool_call_parser_benchmark.py`（工具调用/推理解析器鲁棒性）、`common.py`（共享工具函数） |
| `quantization/` | `toy_quantizer.py`（依赖无关的对称/非对称分组量化器）、`error_analysis.py`（量化误差分布汇总） |
| `benchmarks/` | `bench_quantization.py`（调用玩具量化器的 Benchmark 入口） |
| `edge/` | `gguf_binary_parser.py`（依赖无关的 GGUF 二进制解析）、`gguf_inspector.py`（友好包装的检查工具） |
| `examples/reference/` | `speculative_decode.py`（投机解码参考实现与验证轮数期望进度计算） |


## 环境要求

基础 CPU 流程需要：

- Linux 或 WSL2
- Python 3
- CMake 3.20 或更高版本
- 支持 C++17 的编译器

可选流程只需要安装对应工具：

| 流程 | 附加要求 |
| --- | --- |
| CUDA Probes | NVIDIA GPU、兼容驱动、CUDA Toolkit，并启用 CUDA 构建 |
| Mini Transformer | 安装 `requirements-pytorch.txt` 中的依赖（`numpy>=2.4`、`torch>=2.12`；GPU 机器应改装对应 CUDA 版 torch wheel） |
| llama.cpp 端侧 | 与 CMake 声明版本一致的干净源码仓库（固定在 `11cd988`），以及合法取得的 GGUF 模型 |
| ExecuTorch 端侧 | 固定在 `0b0e2c5` 的 ExecuTorch v1.2.0 SDK（含 LLM Runner 与 XNNPACK 目标） |
| vLLM 云侧 | 独立 Python 环境、模型访问权限和兼容的 GPU/Runtime |
| TensorRT-LLM 云侧 | TensorRT-LLM 环境、兼容 CUDA 栈，以及模型或 Engine 产物 |
| Android ARM64 | Android NDK、CMake、ADB、arm64-v8a 设备和端侧模型/二进制文件 |
| 容器 | Docker；GPU Profile 还需要 NVIDIA Container Toolkit |

InferLab 默认不会在构建过程中隐式下载第三方运行时或模型。启用嵌入式集成前，CMake 会检查配置的源码目录和固定版本（版本不匹配会直接配置失败，而不是静默使用错误版本）。

### 模型准备

首次克隆时初始化固定版本的 `llama.cpp` 子模块，并显式下载模型：

```bash
git submodule update --init --recursive
bash scripts/download_models.sh --download-only
```

`--download-only` 会把固定 revision 的 DistilGPT2 Hugging Face 文件下载到 `models/distilgpt2_clean_hf/`，并逐文件校验 SHA256。若还需要生成端侧 F16 GGUF，请使用独立转换环境，避免与项目主 Python 环境的依赖版本互相影响。请先准备 Python 3.12；固定转换依赖中的 NumPy 1.26.4 不支持 Python 3.14。以下命令使用 CPU 版 PyTorch，转换无需 CUDA：

```bash
python3.12 -m venv .venv-llama-convert
.venv-llama-convert/bin/python -m pip install \
  --index-url https://download.pytorch.org/whl/cpu "torch==2.11.0"
.venv-llama-convert/bin/python -m pip install \
  -r third_party/llama.cpp/requirements/requirements-convert_hf_to_gguf.txt
INFERLAB_CONVERT_PYTHON=.venv-llama-convert/bin/python \
  bash scripts/download_models.sh
```

脚本固定上游模型 revision、`llama.cpp` Commit 和最终 GGUF SHA256。转换时会在临时副本中去掉 6 个 GPT2 可重建的注意力掩码，并校验清理后权重的 SHA256；下载的原始文件和学习参数保持不变。下载或转换结果不一致时会返回非零退出码。模型目录由 `.gitignore` 排除，不会进入主仓库。

## 构建配置项

`CMakeLists.txt` 中的关键 `option()`：

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `INFERLAB_ENABLE_CUDA` | OFF | 构建可选 CUDA 内核实验 |
| `INFERLAB_ENABLE_THIRD_PARTY_JSON` | ON | 启用 nlohmann/json 做 Artifact 解析 |
| `INFERLAB_ENABLE_EVIDENCE_SQLITE` | OFF | 启用 SQLite 证据仓库后端 |
| `INFERLAB_ENABLE_GOOGLE_BENCHMARK` | OFF | 启用 google/benchmark 微基准框架 |
| `INFERLAB_ENABLE_NVTX` | OFF | CUDA 启用时附加 NVTX Profiling Range |
| `INFERLAB_FETCH_THIRD_PARTY_DEPS` | OFF | 允许 CMake 在依赖缺失时联网下载（默认禁止） |
| `INFERLAB_ENABLE_EDGE_LLAMA_CPP` | OFF | 启用内嵌 llama.cpp 端侧运行时集成 |
| `INFERLAB_ENABLE_EDGE_EXECUTORCH` | OFF | 启用 ExecuTorch/XNNPACK 端侧运行时集成 |
| `INFERLAB_REQUIRE_EDGE_RUNTIME_LINKED` | OFF | 强制要求至少链接一个端侧运行时，否则配置失败 |
| `INFERLAB_ENABLE_ANDROID_JNI` | OFF | 构建统一端侧运行时的 Android JNI 桥接 |
| `INFERLAB_ENABLE_EDGE_CLOUD_CURL` | OFF | 启用基于 libcurl 的 OpenAI 兼容端云传输 |
| `INFERLAB_ENABLE_ARM_NEON` | AArch64 上默认 ON | 构建 ARM NEON 端侧微内核 |
| `INFERLAB_ENABLE_ARM_SVE` | OFF | 预留的 ARM SVE/SVE2 端侧后端（尚未实现） |

配合使用的路径型 Cache 变量：`INFERLAB_LLAMA_CPP_SOURCE_DIR`、`INFERLAB_EXECUTORCH_SOURCE_DIR`、`INFERLAB_EXECUTORCH_INSTALL_DIR`、`INFERLAB_EXECUTORCH_BUILD_REVISION_FILE`——均要求指向已固定版本的本地检出/安装，不接受隐式下载。

## 分项运行

以下命令均在仓库根目录执行。

### Python Smoke 流程

```bash
mkdir -p "$PWD/tmp"
SMOKE_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-smoke.XXXXXX")"

python3 -m mini_llm.shape_probe \
  --batch-size 1 \
  --seq-len 128 \
  --decode-steps 16 \
  --output "$SMOKE_RESULTS/shape_probe.json"

python3 scripts/validate_outputs.py --results "$SMOKE_RESULTS"
python3 -m unittest discover -s tests -p "test_*.py"
```

独立输出目录让校验器只检查本次 Smoke 产物，不会被 `results/raw` 中保留的历史状态干扰。

### 原生 CPU 构建

```bash
mkdir -p "$PWD/tmp"
CPU_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-cpu.XXXXXX")"

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_CUDA=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 scripts/run_native_probes.py \
  --repo-root . \
  --build-dir build \
  --results "$CPU_RESULTS" \
  --output "$CPU_RESULTS/native_probe_run.json"

python3 scripts/summarize_benchmarks.py \
  --results "$CPU_RESULTS" \
  --output "$CPU_RESULTS/benchmark_summary.json" \
  --allow-non-ok-status

python3 scripts/validate_outputs.py \
  --results "$CPU_RESULTS" \
  --allow-status benchmark_summary.json=incomplete
```

只运行 Native Probes 时，汇总缺少外部后端类别，因此 `benchmark_summary.json` 为 `incomplete` 是声明过的范围边界，不代表 Native Probe 失败。构建产物不会保留在仓库中；需要执行原生程序时，重新生成 `build/` 即可。

### 基础完整流程

```bash
mkdir -p "$PWD/tmp"
BASIC_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-basic.XXXXXX")"
INFERLAB_RESULTS_DIR="$BASIC_RESULTS" bash scripts/run_all_benchmarks.sh
```

该流程会收集环境证据，执行当前可用后端的预检和 Native Probes，校验结果，在 `$BASIC_RESULTS/benchmark_summary.json` 生成汇总，并运行 Python 回归测试。可选后端不可用时，会在产物中记录对应状态，不会被静默转换为成功执行证据。
示例默认使用空的临时目录；此设置只改变本次产物位置，不会删除或放宽历史证据的状态校验。脚本要求 `build/` 中已经存在本机可执行的 Native 目标，可先执行上一节的 CMake 构建。

## 端侧使用方式

### 嵌入式 Linux 端侧运行时

llama.cpp Adapter 默认关闭，并且要求使用精确固定版本（`11cd988`）的源码仓库：

```bash
cmake -S . -B build-edge \
  -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_EDGE_LLAMA_CPP=ON \
  -DINFERLAB_LLAMA_CPP_SOURCE_DIR=/path/to/llama.cpp
cmake --build build-edge --parallel

./build-edge/inferlab_edge_runtime_matrix \
  --repo-root . \
  --config configs/edge_runtime_matrix.distilgpt2_x86_64.json \
  --mode plan \
  --output results/raw/native_edge_runtime_matrix.json
```

只有在目标平台、运行时二进制文件和模型都存在时，才能使用 `--mode execute`。ExecuTorch/XNNPACK 与 ARM NEON 使用独立的 CMake 选项（`INFERLAB_ENABLE_EDGE_EXECUTORCH`、`INFERLAB_ENABLE_ARM_NEON`）。能够生成 AArch64 计划，不代表 x86 主机真实执行了 AArch64 代码。

GGUF 相关的依赖无关工具位于 `edge/`：

```bash
python3 edge/gguf_inspector.py /path/to/model.gguf
```

### Android ARM64

先生成不会修改设备的部署计划：

```bash
python3 scripts/plan_android_deploy.py \
  --output results/raw/android_deploy_plan.json

python3 scripts/run_android_edge_benchmark.py \
  --output results/raw/android_edge_benchmark.json
```

选定设备、二进制文件和模型后，通过 ADB 执行：

```bash
python3 scripts/run_android_edge_benchmark.py \
  --execute \
  --serial DEVICE_SERIAL \
  --model /path/to/model.gguf \
  --push-model \
  --output results/raw/android_edge_benchmark.json
```

`android/edge_runtime_jni/` 模块把统一 Native Edge Runtime 封装为 Android AAR 契约，构建需要 `INFERLAB_ENABLE_ANDROID_JNI=ON`；`android/native_hello` 是不依赖模型的最小冒烟示例。

## 云侧使用方式

### 直接运行后端测试工具

以下示例用于批量收集环境预检诊断，因此显式允许工具在写出非 `ok` 产物后继续：

```bash
mkdir -p "$PWD/tmp"
BACKEND_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-backends.XXXXXX")"

python3 scripts/run_llama_cpp_benchmark.py \
  --allow-non-ok-status \
  --output "$BACKEND_RESULTS/llama_cpp_benchmark.json"

python3 scripts/run_vllm_benchmark.py \
  --allow-non-ok-status \
  --output "$BACKEND_RESULTS/vllm_serving_benchmark.json"

python3 scripts/run_trtllm_benchmark.py \
  --allow-non-ok-status \
  --output "$BACKEND_RESULTS/trtllm_benchmark.json"

python3 scripts/compare_backends.py \
  --results "$BACKEND_RESULTS" \
  --output "$BACKEND_RESULTS/backend_comparison.json"
```

未提供真实执行参数时，这些工具只执行受限的环境预检。`--allow-non-ok-status` 只改变进程退出码，不会把产物状态改成成功；判断真实后端是否完成时，应去掉该参数并要求产物状态为 `ok`。使用各命令的 `--help` 查看 Binary、Endpoint、Model、Engine 或 Command Mode 的真实执行参数。

### 云侧部署执行路线

云侧闭环不是以生成部署计划为终点。当前仓库可直接执行的主路径是单机 Docker Compose vLLM，完整顺序如下；TensorRT-LLM 和 KServe/llm-d 分别作为 Engine 执行路径和外部集群路径在后续小节说明。

| 阶段 | 执行动作 | 成功证据 |
| --- | --- | --- |
| 1. 环境准备 | 检查 NVIDIA GPU、Docker、Compose 和 NVIDIA Container Runtime | `nvidia-smi`、`docker info` 和 `docker compose version` 均成功 |
| 2. 模型与版本固定 | 指定宿主机模型目录、容器内模型路径和固定 vLLM 版本 | Compose 展开配置中的挂载、模型和版本与预期一致 |
| 3. 构建与启动 | 构建 `vllm-server` 镜像并启动服务 | 容器保持运行，日志中没有模型加载错误 |
| 4. API 验证 | 请求 OpenAI 兼容的 `/v1/models` 和 Completion API | HTTP 请求成功并返回目标模型和生成结果 |
| 5. Benchmark | 使用 `run_vllm_benchmark.py` 发起真实请求 | `vllm_serving_benchmark.json` 顶层状态为 `ok` |
| 6. 严格验收 | 使用 `validate_outputs.py` 校验本次独立结果目录 | 严格模式返回零，不使用 `--allow-non-ok-status` |
| 7. 指标与回收 | 按需启动可观测性服务，实验结束后停止 Compose | 指标可查询，运行容器和网络按预期回收 |

下面的部署预检是可选步骤；需要直接执行时，可以从“单机 vLLM Docker 部署”开始，并按其中的 1 至 6 步完成真实闭环。

### 可选：生成云侧部署预检

以下命令只生成 Docker/Runtime 部署计划，不会构建镜像或启动服务：

```bash
python3 scripts/plan_runtime_deploy.py \
  --profile full \
  --output results/raw/runtime_deploy_plan.json \
  --env-output deploy/inferlab.runtime.env
```

规划产物用于检查镜像、端口、模型目录和 GPU 数量等配置。仓库当前可直接执行的主路径是单机 Docker Compose vLLM；TensorRT-LLM 以 Engine/Benchmark 为主，KServe/llm-d 需要外部 Kubernetes 集群。

### 单机 vLLM Docker 部署（当前主路径）

`docker/Dockerfile.vllm` 会按 `VLLM_PACKAGE` 安装指定版本，`deploy/docker-compose.inferlab.yml` 会挂载本地模型并启动 OpenAI 兼容 API。以下命令均在仓库根目录执行。

#### 1. 检查环境

```bash
nvidia-smi
docker --version
docker compose version
docker info
```

`docker info` 应能看到 NVIDIA Runtime 或 `nvidia.com/gpu` 设备。目标机器还需要能够拉取 CUDA 基础镜像和 Python 包。

#### 2. 选择模型和运行时版本

模型根目录必须使用宿主机绝对路径，目录内应包含 Transformers 格式的配置、Tokenizer 和权重文件：

```bash
export INFERLAB_MODEL_ROOT=/absolute/path/to/models
export INFERLAB_MODEL=/models/<transformers-model-directory>
export VLLM_PACKAGE='vllm==0.27.1'
```

`INFERLAB_MODEL_ROOT` 是宿主机目录，Compose 会把它只读挂载为容器内的 `/models`；`INFERLAB_MODEL` 必须使用容器内路径。不要依赖 `deploy/inferlab.runtime.env` 中的相对默认值 `./models`，它会相对于 `deploy/` 解析。vLLM 版本应与 `configs/source_trace_targets.example.json` 的固定版本一致。

#### 3. 解析配置并构建镜像

```bash
docker compose \
  --project-name inferlab-runtime \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.inferlab.yml \
  --profile gpu \
  config

docker compose \
  --project-name inferlab-runtime \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.inferlab.yml \
  --profile gpu \
  build vllm-server
```

`config` 的展开结果应显示正确的宿主机模型目录、容器内模型路径和固定的 `VLLM_PACKAGE`。首次构建需要下载 CUDA 基础镜像和 vLLM 依赖。

#### 4. 启动并检查服务

```bash
docker compose \
  --project-name inferlab-runtime \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.inferlab.yml \
  --profile gpu \
  up -d vllm-server

docker compose \
  --project-name inferlab-runtime \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.inferlab.yml \
  --profile gpu \
  logs --tail=100 vllm-server

curl --fail http://127.0.0.1:8000/v1/models
```

模型加载完成前，健康检查可能暂时失败；先通过容器日志确认 API Server 已开始监听。仅看到容器处于 `running` 状态，不代表模型已经可请求。

#### 5. 执行真实 Benchmark

```bash
mkdir -p "$PWD/tmp"
VLLM_RESULTS="$(mktemp -d "$PWD/tmp/inferlab-vllm.XXXXXX")"

python3 scripts/run_vllm_benchmark.py \
  --base-url http://127.0.0.1:8000 \
  --model "$INFERLAB_MODEL" \
  --request-kind completion \
  --output "$VLLM_RESULTS/vllm_serving_benchmark.json"

python3 scripts/validate_outputs.py --results "$VLLM_RESULTS"
```

真实部署验证不要传 `--allow-non-ok-status`。只有请求实际完成、产物状态为 `ok` 且严格校验返回零，才能把本次 vLLM 云侧执行记录为成功。

#### 6. 停止服务

```bash
docker compose \
  --project-name inferlab-runtime \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.inferlab.yml \
  --profile gpu \
  down
```

部署完成判据：

| 检查项 | 成功条件 |
| --- | --- |
| 容器 | `vllm-server` 未退出，日志没有模型加载错误 |
| API | `/v1/models` 返回成功响应 |
| 推理 | Benchmark 至少完成一个真实请求 |
| 产物 | `vllm_serving_benchmark.json` 顶层状态为 `ok` |
| 门禁 | `validate_outputs.py` 严格模式返回零 |

### TensorRT-LLM Engine 执行

TensorRT-LLM 当前提供固定 Engine 构建配置和真实 Benchmark 入口，不提供与 vLLM 等价的长期 API 服务 Compose。目标环境需要 TensorRT-LLM 1.2.1、兼容 CUDA 栈和 `trtllm-build`/`trtllm-bench`：

```bash
python3 scripts/build_trtllm_showcase_engine.py \
  --config configs/trtllm_showcase_engine.json

python3 scripts/run_trtllm_benchmark.py \
  --mode bench \
  --engine-dir results/raw/trtllm_distilgpt2_dynamic_engine \
  --model distilgpt2 \
  --tokenizer models/distilgpt2_clean_hf \
  --output results/raw/trtllm_benchmark.json
```

Engine 构建成功不等同于推理完成；必须继续运行 Benchmark，并要求结果状态为 `ok`。需要长期 TensorRT-LLM API 服务时，应在目标环境单独启动 `trtllm-serve`，当前仓库不宣称已经提供该服务的容器生命周期管理。

### KServe/llm-d 集群部署

`deploy/kserve_llm_d.example.yaml` 是可修改的集群资源样例，不是单机部署脚本。执行前必须已经具备安装了 KServe/llm-d CRD 的 Kubernetes 集群、GPU 节点、模型 PVC 和可用 Ingress/Gateway：

```bash
kubectl api-resources | grep -i LLMInferenceService
kubectl get nodes
kubectl create namespace inferlab --dry-run=client -o yaml | kubectl apply -f -
kubectl apply -f deploy/kserve_llm_d.example.yaml
kubectl -n inferlab get llminferenceservice inferlab-vllm
```

样例中的 `pvc://models/model` 要求 `inferlab` 命名空间中已经存在名为 `models` 的 PVC，并包含目标模型。只有 Pod Ready、路由可访问并完成真实推理请求后，才能记录集群部署成功。不同 KServe/llm-d 版本的 CRD 字段可能不同，应用前应按目标集群已安装版本调整样例。

移除样例资源时执行：

```bash
kubectl delete -f deploy/kserve_llm_d.example.yaml
```

### 可观测性服务

已有 Benchmark 结果后，可以导出 Prometheus Textfile 指标并启动 Node Exporter、Prometheus 和 Grafana：

```bash
python3 scripts/export_prometheus_metrics.py \
  --results results/raw \
  --output results/tables/inferlab_metrics.prom

export GRAFANA_ADMIN_PASSWORD='<set-a-private-password>'

docker compose \
  --project-name inferlab-observability \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.observability.yml \
  up -d
```

默认访问地址为 Prometheus `http://127.0.0.1:9090`、Grafana `http://127.0.0.1:3000`。端口默认仅绑定本机回环地址；若 3000 已被占用，可在启动前设置 `export GRAFANA_PORT=3001`。密码只应通过私有环境变量或未提交的环境文件提供。可观测性服务属于运维辅助能力，不替代 Benchmark JSON 和严格结果校验。

Compose 会自动配置 Grafana 的 Prometheus 数据源和 InferLab 看板；Prometheus 通过服务名 `node-exporter:9100` 抓取指标。导出脚本生成的是已完成测试的快照，不会自动重跑 Benchmark。结果位于 `tmp/` 等其他目录时，须用 `--results`、`--source` 显式选择来源；`--source` 会替换默认来源列表，目录不会递归扫描。多次测试可通过 `--summary` 提供含 `sections` 数组的统一摘要，配合 `--no-raw` 展示经核对的指标。纯输出吞吐量与含输入的总吞吐量应分别标注；测试参数不同的结果不能直接作为性能排名。

停止可观测性服务时执行：

```bash
docker compose \
  --project-name inferlab-observability \
  --env-file deploy/inferlab.runtime.env \
  -f deploy/docker-compose.observability.yml \
  down
```

### 成熟产品 Profile 对照

```bash
python3 scripts/run_experiment_matrix.py \
  --matrix configs/benchmark_matrix.yaml \
  --output results/raw/experiment_matrix_run.json
```

`configs/product_profiles/` 下有 vLLM、TensorRT-LLM、SGLang、FlashInfer、Mooncake、KServe/llm-d、Ray Serve、TGI、LMDeploy、KTransformers、Dynamo、KV Offload 等 Profile 与对应的 Replay 样本（`*_replay_sample.jsonl`），供 `inferlab_product_profile_contract_check` 等原生工具做比对。

## 端云协同实验

`scripts/run_edge_cloud_showcase.py` 是一次性端云实验编排器，会组合 Native Probes、llama.cpp 端侧执行、vLLM Serving、TensorRT-LLM Engine 执行、指标采集和后端对比。它用于完成一轮可验收实验，不负责长期驻留的生产服务部署。

### 仅生成执行计划

不传 `--execute` 时，只创建带时间戳的实验目录、脱敏配置快照和 `planned` 状态的阶段清单，不启动任何后端进程，也不会更新成功运行指针：

```bash
python3 scripts/run_edge_cloud_showcase.py \
  --config configs/showcase_edge_cloud_distilgpt2.json
```

Dry Run 用于检查最终将执行的命令、模型路径、端口和产物位置；它不是端侧推理、云侧服务或 TensorRT-LLM Engine 已经成功执行的证据。

### 真实执行前置条件

默认配置 `configs/showcase_edge_cloud_distilgpt2.json` 使用同一 DistilGPT2 模型家族对齐端侧与云侧实验身份。执行前应确认：

| 组件 | 默认配置 | 执行要求 |
| --- | --- | --- |
| Native Probes | `build-final-cuda/` | 已完成 Release/CUDA 构建，`run_native_probes.py --strict` 能达到 `20/20` |
| llama.cpp | `third_party/llama.cpp/build/bin/llama-cli`、`llama-bench` | 二进制可执行，源码版本与 Source Trace 固定版本一致 |
| 端侧模型 | `models/distilgpt2-f16.gguf` | GGUF 文件存在且不是占位文件，Tokenizer 已嵌入模型 |
| vLLM | `${HOME}/.venvs/inferlab-vllm/bin/python` | vLLM 环境可导入，GPU 可用，配置端口 `18000` 未被占用 |
| 云侧模型 | `models/distilgpt2_clean_hf` | Transformers 配置、Tokenizer 和权重完整，可离线加载 |
| TensorRT-LLM | `${HOME}/.venvs/inferlab-trtllm/bin/trtllm-bench` | TensorRT-LLM 环境与 CUDA/GPU 兼容 |
| TensorRT-LLM Engine | `results/raw/trtllm_distilgpt2_dynamic_engine` | `config.json` 存在，输入、输出和最大 Token 边界满足本次请求 |

仓库不保留构建产物。清理过 `build-final-cuda/` 或 TensorRT-LLM Engine 后，应先重新生成：

```bash
cmake -S . -B build-final-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_CUDA=ON
cmake --build build-final-cuda --parallel

"$HOME/.venvs/inferlab-trtllm/bin/python" \
  scripts/build_trtllm_showcase_engine.py \
  --config configs/trtllm_showcase_engine.json
```

如果目标机器的二进制、模型、虚拟环境或 Engine 路径不同，应修改 Showcase 配置，不要通过伪造结果产物绕过路径和状态检查。

### 执行完整端云实验

准备好上述依赖后，显式传入 `--execute`：

```bash
python3 scripts/run_edge_cloud_showcase.py \
  --config configs/showcase_edge_cloud_distilgpt2.json \
  --execute
```

真实执行按以下顺序运行，任一必要阶段失败都会使整轮实验失败：

| 阶段 | 实际动作 | 通过条件 |
| --- | --- | --- |
| 输入准备 | 使用 vLLM Tokenizer 生成统一 Prompt JSONL 和 TensorRT-LLM Dataset | `runtime_inputs.json` 状态为 `ok` |
| 模型身份 | 记录 GGUF、Transformers 模型和 Engine 的受限指纹 | 所有必需模型产物可读取 |
| Engine 契约 | 检查 TensorRT-LLM Engine 的输入、输出和 Token 上限 | Engine `config.json` 与本次请求兼容 |
| Native 机制 | 严格执行 Native Probes | 通过数为 `20/20`，无 failed、partial 或 skipped |
| llama.cpp 端侧 | 对 DistilGPT2 GGUF 发起单请求生成 | `llama_cpp_benchmark.json` 状态为 `ok` |
| vLLM 启动 | 启动本地 vLLM 子进程并轮询 `/health` | 在超时前返回 HTTP 2xx |
| vLLM 云侧 | 请求 `/v1/completions` | `vllm_serving_benchmark.json` 状态为 `ok` |
| vLLM 回收 | 依次尝试 SIGINT、SIGTERM 和 SIGKILL，并等待端口释放 | 进程退出且端口不再监听 |
| TensorRT-LLM 云侧 | 使用已构建 Engine 执行 `trtllm-bench` | `trtllm_benchmark.json` 状态为 `ok` |
| 后端对比 | 汇总 Native、llama.cpp、vLLM 和 TensorRT-LLM 结果 | `backend_comparison.json` 成功生成 |

### 服务生命周期边界

Showcase 启动的是实验期间的本地 vLLM 临时进程，不复用前面 Docker Compose 的长期服务。启动前如果配置端口已经被占用，实验会直接失败；无论请求成功还是异常退出，编排器都会在 `finally` 路径执行 vLLM 回收并检查端口释放。

TensorRT-LLM 在该流程中仍以 Engine Benchmark 方式执行，不启动 `trtllm-serve`。因此这轮实验能够验证端侧生成、云侧在线请求、TensorRT-LLM Engine 执行和统一对比，但不代表提供了 TensorRT-LLM 长期 API 服务。

### 成功判据与产物

每次运行都会在 `results/showcase/<experiment-id>/<run-id>/` 下创建独立目录，其中包含：

| 产物 | 内容 |
| --- | --- |
| `config.snapshot.json` | 已脱敏的本次配置快照 |
| `run_manifest.json` | 所有阶段的命令元数据、状态、耗时和失败原因 |
| `model_artifacts.json` | 模型与 Engine 的受限指纹和可读状态 |
| `raw/*.json` | Native、llama.cpp、vLLM、TensorRT-LLM 的原始结构化结果 |
| `logs/*.log` | 已限制范围的阶段日志和 vLLM 服务日志 |
| `backend_comparison.json` | 本轮端云后端对比结果 |

只有脚本返回零、`run_manifest.json` 顶层状态为 `passed` 且全部阶段均为 `passed`，本轮实验才算成功。`results/showcase/latest.json` 只会指向最近一次完整成功运行；Dry Run 或失败运行会保留各自产物，但不会覆盖该指针。

## 实验矩阵

首先以 Dry Run 方式检查实验矩阵，准备好环境后再显式执行选定实验：

```bash
python3 scripts/run_experiment_matrix.py \
  --matrix configs/benchmark_matrix.yaml \
  --select kv_shape_probe_small \
  --execute \
  --output results/raw/experiment_matrix_run.json
```

## 上游源码版本验证

Source Trace 工具会检查配置的 Python Distribution 或源码仓库是否符合固定版本，并确认关键实现位置存在：

```bash
python3 scripts/verify_source_traces.py \
  --config configs/source_trace_targets.example.json \
  --repo llama_cpp=/path/to/llama.cpp \
  --output results/raw/source_trace_verification.json
```

默认检查范围包括 Transformers、vLLM、TensorRT-LLM 和 llama.cpp。不同机器应通过参数覆盖本地路径，不应直接修改证据来掩盖版本不一致。

## 产物规范

规范实验汇总使用结构化格式：

| 格式 | 用途 |
| --- | --- |
| JSON | 单次运行证据、计划、汇总、对比、Schema 和 Manifest |
| JSONL | Prompt 集合与 Replay 事件流 |
| CSV | 供外部分析使用的可选导出表格 |

真实执行还可能保留 `.log` 日志、`.nsys-rep` 等 Profiler 文件、可选 Prometheus `.prom` 输出或部署 `.env` 文件。这些属于运维输入或原始证据，不是生成的 Markdown 报告。

重要结果入口：

- `results/verification/latest.json`：用于版本控制的最近验证状态和边界摘要
- `results/raw/benchmark_summary.json`
- `results/raw/backend_comparison.json`
- `results/raw/evidence_checklist.json`
- `results/raw/source_trace_verification.json`
- `results/showcase/latest.json`

`results/verification/latest.json` 是简短索引，不复制原始 Benchmark 数据。`results/raw/` 和 `results/showcase/` 保存运行时生成的详细证据；需要发布或长期归档时，应把选定的隔离运行目录连同源码版本一起固化。

产物状态本身也是证据的一部分。`planned`、`skipped`、`partial`、`unavailable` 和 `failed` 不能被描述为已成功完成真实执行。`validate_outputs.py` 默认同时校验 JSON 结构和顶层状态；`--allow-non-ok-status` 只用于中间阶段的结构校验，不能替代最终严格门禁。对于明确声明为可选的后端，可使用可重复的 `--allow-status FILE=STATUS` 增加精确到结果相对路径和状态的例外；该规则不会放行同一文件的其他失败状态，也不会影响其他产物。基础完整流程只允许 llama.cpp 与 TensorRT-LLM 预检产物为 `skipped`，并允许覆盖基础流程范围的汇总产物为 `incomplete`，其余结果仍按严格模式验收。

## 许可说明

知识星球：“奔跑中cpp / c++” 所有，

阿甘微信：LLqueww

商业使用前请联系我方授权 一旦发现侵权行为，将依法追究法律责任

（对于公司法律事务已有对接律师，敬请告知）
