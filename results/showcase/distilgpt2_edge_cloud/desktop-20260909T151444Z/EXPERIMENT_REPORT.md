# InferLab 成熟产品 Profile 对照与端云实验实录

**完成时间：2026-09-09，Windows + WSL2 本地环境。** 本次完成了可验收的真实端云实验，以及仓库已有样例的产品 Profile 回放与严格契约检查。两类结果分开保存。

| 验收内容 | 本次结果 |
| --- | --- |
| 完整 Showcase | 退出码 0，`run_manifest.json` 为 `passed`，10/10 阶段通过 |
| CUDA / CPU 构建测试 | Release/CUDA 构建完成，22/22 CTest 通过，含 7 项 CUDA 测试 |
| Native Probes | 本轮严格执行 20/20，无 failed、partial、skipped |
| llama.cpp / vLLM / TensorRT-LLM | 三份真实 Benchmark JSON 均为 `ok` |
| 独立验收 | 32 项交叉核验通过，整个成功运行目录严格 JSON 校验退出 0 |
| 临时服务回收 | vLLM 收到 SIGINT 后退出，18000 端口释放，本次临时容器全部消失 |
| 产品 Profile | 82 条矩阵计划；9 个产品样例回放通过，360/360 契约检查通过 |

本轮 Showcase 从北京时间 **23:14:44 至 23:16:17**，实际编排耗时 **93.224 秒**。这个时间不包含此前模型下载、Engine 构建和本轮编译准备。

**结论范围：** 本次证明所列推理和验收链路可以在这台电脑运行。vLLM 与 TensorRT-LLM 本轮都生成了 **8 个换行符**，并没有给出有用的 KV Cache 解释。因此“执行成功”不表示回答质量达标，也不表示获得了产品性能排名。

## 1. 实验实际运行在哪里

项目根目录：`/home/anonymous/Projects/infra`。

| 组件 | 本次使用方式 |
| --- | --- |
| Windows / WSL | Windows 上的 Ubuntu WSL2；宿主 Python 3.14.4 负责轻量编排和 HTTP 客户端 |
| GPU | NVIDIA GeForce RTX 4060 Ti，16 GB；Windows 驱动 591.86 |
| Native 构建 | 在现有 TensorRT-LLM CUDA 开发镜像内构建，产物写回 `build-final-cuda`；CPU 原生 Probes 在 WSL 中运行 |
| llama.cpp | WSL CPU 执行，4 线程，`ngl=0`；固定源码 Commit `11cd98842874cc1b87ac274bd2d5cceb38102bb2` |
| vLLM | 已有 `inferlab-vllm:0.27.1-local` 镜像，vLLM 0.27.1；每轮创建临时容器 |
| TensorRT-LLM | 官方 1.2.1 镜像，每轮创建临时 Benchmark 容器；复用已完成的 Engine |
| 模型 | 同一 DistilGPT2 模型家族：端侧 F16 GGUF，云侧 Hugging Face 权重，TRT 使用编译后的 FP16 Engine |

“端侧”和“云侧”在本次是运行角色：端侧使用本机 CPU，两个云侧后端依次使用本机 GPU。没有另连远程云服务器，也没有使用手机或 ARM 设备；没有验证跨机器网络、动态端云路由或请求迁移。

模型目录与 Engine：

```text
/home/anonymous/Projects/infra/models/distilgpt2-f16.gguf
/home/anonymous/Projects/infra/models/distilgpt2_clean_hf
/home/anonymous/Projects/infra/tmp/inferlab-trtllm.dakte9sa/engine
```

模型和 Engine 的本轮指纹保存在 [model_artifacts.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/model_artifacts.json)，已重新计算并核对。不同文件格式的 SHA256 本来就不同；这里核验的是各自文件完整性与模型家族身份。

## 2. 从 README 默认环境到本机配置

README 默认使用两个本机虚拟环境。本机已经有可工作的 Docker 镜像，因此没有再向 Ubuntu 的 Python 3.14 安装另一套 vLLM / TensorRT-LLM。

本次配置在 [showcase.config.json](/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/showcase.config.json)。项目主编排器 `scripts/run_edge_cloud_showcase.py` 保持原样，通过它已有的可执行路径配置调用容器适配器。

| 配置项 | 本机值 / 调整 |
| --- | --- |
| `paths.build_dir` | `build-final-cuda`，新建 Release/CUDA 构建 |
| `paths.llama_cli` | `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build/bin/llama-cli` |
| `paths.llama_bench` | `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build/bin/llama-bench` |
| `paths.vllm_python` | `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/bin/vllm-python`，容器监督器入口 |
| `paths.trtllm_python` | `/usr/bin/python3`，仅运行项目的 Benchmark 包装脚本 |
| `paths.trtllm_bench` | `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/bin/trtllm-bench`，实际调用官方镜像的 CLI |
| `paths.trtllm_engine_dir` | 复用前述已经构建并验证的 Engine |
| vLLM | 端口 18000，FP16，最大长度 512，GPU 显存比例 0.35，eager 模式，V2 runner 关闭 |
| TRT | batch=1、max_num_tokens=64、warmup=0；容器适配器额外设置 concurrency=1、KV cache fraction=0.3 |
| 临时文件 | 宿主保存在项目 `tmp`；容器映射为较短的 `/inferlab-tmp` |

适配器的行为是实际启动进程，不生成模拟结果：

1. 输入准备：启动临时 vLLM 镜像，仅加载本地 Tokenizer，离线生成输入文件。
2. vLLM 启动：临时容器内监听 `0.0.0.0:18000`，只发布到宿主 `127.0.0.1:18000`。项目继续在宿主轮询 `/health`。
3. API 请求：宿主标准库 HTTP 客户端向上述端口发送真实 Completion 请求。
4. 回收：监督器转发 SIGINT / SIGTERM，并等待容器结束；外层执行器最终按本次实验标签检查残留。
5. TensorRT-LLM：在另一个临时容器中运行官方 `trtllm-bench`，挂载同一绝对项目路径，使用真实 Engine。

本轮没有复用此前长期 Compose vLLM 服务，也没有创建 TRT 长期 API。已有 Grafana、Prometheus、Node Exporter、`one-api` 和原 TensorRT 环境容器保留。

完整 Docker argv、镜像标识、时间、信号和回收结果位于 [reproduction](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/reproduction) 内的 `inferlab-showcase-*.json`。这些记录也列出了快照配置以外追加的参数，便于追踪最终执行方式。

## 3. 编译与预检查

CUDA 构建使用已下载的 TensorRT-LLM 镜像，其镜像摘要固定为：

```text
nvcr.io/nvidia/tensorrt-llm/release@sha256:33cd085b772947bd22b7273886539331420404e5d2a4a039945241945ff927b9
```

容器内执行的核心命令：

```bash
cmake -S . -B build-final-cuda -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DINFERLAB_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build-final-cuda --parallel 6
ctest --test-dir build-final-cuda --output-on-failure --parallel 1
```

结果为 22/22。对应 [build-native.log](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/logs/build-native.log)。宿主 WSL 未安装 nvcc，重建时应继续使用下文容器命令。

llama.cpp 在宿主 WSL 构建：

```bash
cmake -S third_party/llama.cpp \
  -B "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=OFF \
  -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=ON \
  -DLLAMA_BUILD_UI=OFF -DLLAMA_USE_PREBUILT_UI=OFF
cmake --build "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build" \
  --target llama-cli llama-bench --parallel 6
```

该固定版本的 `llama-cli` 构建目标受 `LLAMA_BUILD_SERVER` 开关控制，因而需要 ON；这只是编译依赖，不会启动 HTTP 服务。UI 构建和预制 UI 下载关闭。子模块源码没有修改。

之后实际运行 `run_native_probes.py --strict`，预检查得到 20/20；正式 Showcase 内再次执行一轮 20/20，以产生属于本轮的原始结果。

## 4. 计划、失败记录与最终执行

先生成本机适配后的 Dry Run：

```bash
cd /home/anonymous/Projects/infra
python3 scripts/run_edge_cloud_showcase.py \
  --config "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/showcase.config.json" \
  --run-id desktop-dryrun-20260909
```

计划目录为 `results/showcase/distilgpt2_edge_cloud/desktop-dryrun-20260909`，状态为 planned。Dry Run 不启动后端，也不会替换成功指针。

第一次真实执行目录为 `desktop-20260909T151039Z`。输入、Native、llama.cpp 已通过，但两个 GPU 后端遇到同一问题：长临时路径加上 UUID 后，ZeroMQ IPC socket 路径超过 107 字符。首次 manifest 如实保留为 failed，三个临时容器均回收，既有 `latest.json` 未被覆盖。

修复方法是把项目下的 `scratch` 目录再挂载到容器内 `/inferlab-tmp`，并使用该短路径作为 `TMPDIR`。宿主文件仍位于项目 `tmp`，没有通过改写结果绕过检查。

修复后实际执行：

```bash
python3 scripts/run_edge_cloud_showcase.py \
  --config "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/showcase.config.json" \
  --run-id desktop-20260909T151444Z \
  --execute
```

实际执行由额外的轻量外层脚本记录退出码并复核容器回收。上述命令和其结果保存在 [execution.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/reproduction/execution.json)。最终成功目录是：

```text
/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z
```

## 5. 正式运行的十个阶段

| 阶段 | 状态 | 阶段耗时 |
| --- | --- | ---: |
| 输入准备 | passed | 2.930 秒 |
| 模型指纹 | passed | 0.415 秒 |
| Engine 契约 | passed | 未独立计时 |
| Native 机制 | passed | 0.269 秒 |
| llama.cpp 端侧 | passed | 0.534 秒 |
| vLLM 启动 | passed | 70.422 秒 |
| vLLM API 请求 | passed | 0.247 秒 |
| vLLM 回收 | passed | 0.829 秒 |
| TRT Engine 请求 | passed | 21.000 秒 |
| 后端对比 | passed | 0.051 秒 |

Engine 契约检查确认本次 **9 输入 Token + 8 输出 Token** 在 Engine 的 `max_input_len=64`、`max_seq_len=96`、`max_num_tokens=64` 范围内，且 `multiple_profiles=false`。

时间含义需要区分：vLLM 启动约 70.4 秒，之后 HTTP 请求约 129.9 毫秒；TRT 整个包装命令约 21.0 秒，而官方报告中的单请求时间约 205.3 毫秒。不能把单请求时间当成从启动容器到结束的总时间。

## 6. 真实结果与能支持的结论

统一的文本 Prompt 为：

```text
Explain KV cache in one sentence.
```

vLLM Tokenizer 将其编码为 9 Token，TRT Dataset 使用这些 Token ID。实际 vLLM 回包和 TRT 官方报告都确认输入为 9、输出为 8。llama.cpp 使用同一文本和同一模型家族，但本次日志没有提供足够信息证明其内部 Token ID 与两个 GPU 后端逐个一致。

| 后端 | 实测结果 | 指标含义 |
| --- | --- | --- |
| llama.cpp CPU | `ok`；Generation 282.5 Token/s；进程约 267.721 ms | CLI 输出的生成阶段速率；进程时间包含其他开销；未提供可靠的分段耗时或实际 Token 计数 |
| vLLM API | HTTP 200；1 请求成功，0 失败；9 输入 / 8 输出；129.901 ms；61.477 输出 Token/s | 客户端请求延迟；输出吞吐采用整轮客户端测量区间 |
| TensorRT-LLM Engine | 1 请求成功，error_tokens=0；9 输入 / 8 输出；205.326 ms；38.963 输出 Token/s | 官方请求时间戳和报告；warmup=0，包含首次请求影响 |
| Native | 20/20，0 failed/partial/skipped | 合成机制与正确性实验，不是 DistilGPT2 文本生成吞吐 |

这张表用于展示结果口径，不据此宣布某后端更快：每个模型后端只有一个请求，llama.cpp 为 CPU CLI，vLLM 为 HTTP API，TRT 为 Engine Benchmark，预热及计时边界也不同。不能从单样本得出稳定 P90/P99、并发能力或产品 SLA。

本轮 GPU 输出为 `"\\n\\n\\n\\n\\n\\n\\n\\n"`。TRT 官方输出文件保留了原文；vLLM 按项目默认策略只保存字符数与 SHA256，其哈希与 TRT 原文重新计算的哈希一致。llama.cpp 混合日志中能看到短文本生成，但其 JSON 的 `generated_text` 留空是因为包装脚本未从混合 stdout 提取纯文本。

### 指标解析与已有工具的限制

- 原 TRT 文本解析器容易将“输入 + 输出总吞吐”当作输出吞吐。本次适配器在官方命令成功、请求数和长度验证通过后，从 `system_output_throughput_tok_s` 提取输出吞吐，写入附加的规范化日志行。原始官方报告保持原样，来源 SHA256 保存在 [trtllm_metric_normalization.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/raw/trtllm_metric_normalization.json)。本轮总 Token 吞吐是 82.795，输出 Token 吞吐是 38.963，最终比较使用后者。
- TRT 本次不是流式测量，`first_token_timestamp=-1`；原始请求记录中的 TTFT、intertoken latency 和某些速率字段不可用于性能结论。本次只使用已交叉核对的开始/结束时间戳和官方汇总字段。
- vLLM / TRT 包装脚本在宿主 Python 中采集环境，因此 JSON 可能显示本机对应模块不可导入；这不是容器内后端缺失。真实容器命令、GPU 日志、HTTP 回包及 Engine 报告才是本次运行证据。`backend_comparison.json` 的 readiness 分数及自动建议应结合这个部署方式解读，不用来做产品优劣结论。
- 固定版 `llama-bench` 不支持 `--version`。项目通用环境探测因此把它标为 unavailable；另用 `--help` 验证了二进制可执行且退出 0，实际 Showcase 使用的 `llama-cli` 也成功执行。证据见 [llama_bench_help.log](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/logs/llama_bench_help.log)。
- llama JSON 中 `config.prompt_tokens=128` 是未使用的 bench 默认参数，不是 CLI 本次实际输入长度。缺失的计数和耗时均保留为空，没有补造数值。

## 7. 成熟产品 Profile 对照

本轮首先执行用户给出的矩阵计划命令，得到 **82 条 planned**。随后另建只含样例 Replay 的矩阵，实际调用 9 个 Profile 适配器：

vLLM、SGLang、TensorRT-LLM、KV Offload（LMCache 样例）、Mooncake、FlashInfer、KServe/llm-d、Ray Serve、TGI。

默认必需的 vLLM、SGLang、TensorRT-LLM 全部覆盖。LMDeploy、KTransformers、Dynamo 没有对应的内置 Replay 样例，本轮未选择这三个可选项，也未为其补造日志。

成功结果为：9/9 Profile，46 行样例事件、93 个解析数值项；原生严格契约 360/360，189/189 必需字段完整，9 pass / 0 fail。严格 JSON 校验和 11 项相关既有测试通过。

首次检查暴露 Python 生成器与 C++ 契约的两处描述不同。仅修改了 `scripts/product_profiles/common.py` 的两个字符串，补上实际已有的 `/report` 职责；没有改指标或校验规则。修复后重新运行生成器，原始失败产物保持不变。

**这些都是仓库样例回放。** 没有把样例数字作为本机性能数据；也没有因此安装或启动上述九个产品。部分原有 schema 字段会把带指标的 Replay 标成“可用于性能声明”，但它并不认证来源真实性；本轮样例可支持的真实性能结论仍为零。

详细过程、逐产品来源和命令见 [PROFILE_WORKFLOW_REPORT.md](/home/anonymous/Projects/infra/tmp/inferlab-product-profiles.7rb9mysw/PROFILE_WORKFLOW_REPORT.md)。最终结果在 `/home/anonymous/Projects/infra/tmp/inferlab-product-profiles.7rb9mysw/profiles-fixed`，严格契约在 [native_product_profile_contract_check.json](/home/anonymous/Projects/infra/tmp/inferlab-product-profiles.7rb9mysw/audit-fixed/native_product_profile_contract_check.json)。首次失败结果另存 `profiles/`、`audit/`。

## 8. 文件索引与指标快照

| 文件 / 目录 | 用途 |
| --- | --- |
| [run_manifest.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/run_manifest.json) | 本轮 10 阶段状态、耗时、命令元数据、日志位置 |
| [config.snapshot.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/config.snapshot.json) | 编排器自动生成的脱敏配置 |
| [model_artifacts.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/model_artifacts.json) | GGUF、HF 权重和 Engine 指纹 |
| [raw](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/raw) | 本轮 Native、llama.cpp、vLLM、TRT 原始结构化结果 |
| [trtllm_report.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/raw/trtllm_report.json) | 官方 Engine 性能报告 |
| [trtllm_output.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/raw/trtllm_output.json) | 官方生成原文 |
| [trtllm_request_details.raw](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/raw/trtllm_request_details.raw) | 官方逐请求原始记录，JSON 数组 |
| [backend_comparison.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/backend_comparison.json) | 项目原有脚本生成的四后端汇总，需按第 6 节解释 |
| [verification.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/verification.json) | 32 项补充验收、严格校验退出码与结论范围 |
| [benchmark_summary.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/benchmark_summary.json) | 从本轮真实结果提取的指标摘要 |
| [inferlab_metrics.prom](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/inferlab_metrics.prom) | 本轮独立 Prometheus Textfile 快照 |
| [logs](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/logs) | 阶段日志、vLLM 服务日志、构建与校验日志 |
| [reproduction](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/reproduction) | 本机配置、适配器、构建命令、容器执行记录、源脚本快照 |
| [ARTIFACTS_SHA256.json](/home/anonymous/Projects/infra/results/showcase/distilgpt2_edge_cloud/desktop-20260909T151444Z/ARTIFACTS_SHA256.json) | 本轮结果文件的大小与 SHA256 |

本轮指标导出使用 `--no-raw` 和显式来源清单，避免将不适用的原始字段混入统一指标。`.prom` 是已经完成的实验快照；没有覆盖此前 Grafana 展示的 Benchmark 快照，也没有把静态结果当成实时推理服务负载。

`results/showcase/latest.json` 现已指向本轮正式成功目录。旧成功指针备份、失败尝试与编译准备记录均保存在 `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb` 或各自运行目录。

## 9. 在同一台电脑上复现

先进入 WSL Ubuntu，并切到仓库目录。以下命令在 Ubuntu 中运行；Docker Desktop 需处于运行状态，Ubuntu WSL Integration 已开启。

```bash
cd /home/anonymous/Projects/infra
nvidia-smi
docker info
```

保留模型、两个 Docker 镜像、`build-final-cuda`、本次 setup 目录和 Engine 即可复用。正常再次执行不需要重复下载或构建：

```bash
python3 "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/run_showcase.py"
```

这个外层脚本会生成新的 UTC run ID，调用未修改的项目 Showcase，记录退出码并复核本次容器回收。不要同时运行同一 setup 的两轮实验。端口被其他服务占用时，项目会判定失败，不能通过修改 manifest 绕过。

手动生成新计划：

```bash
python3 scripts/run_edge_cloud_showcase.py \
  --config "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/showcase.config.json" \
  --run-id "plan-$(date -u +%Y%m%dT%H%M%SZ)"
```

查看最新成功结果并严格校验：

```bash
RUN_DIR="$(python3 -c 'import json; print(json.load(open("results/showcase/latest.json"))["run_directory"])')"
python3 scripts/validate_outputs.py --results "$RUN_DIR"
python3 -c 'import json,sys; m=json.load(open(sys.argv[1])); print(m["status"]); print([(s["name"],s["status"]) for s in m["stages"]])' \
  "$RUN_DIR/run_manifest.json"
```

如果新一轮失败，`latest.json` 仍指向之前成功结果。应从脚本输出或 `/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/latest-attempt.txt` 找到失败目录检查，不能仅看旧 latest 误判新一轮成功。

重新执行 Profile 样例流程：

```bash
bash "/home/anonymous/Projects/infra/tmp/inferlab-product-profiles.7rb9mysw/run_replay_fixed.sh"
```

该 Profile 脚本重写其本次工作目录中的最终回放产物；首次失败记录保留。需要另存一轮时，应先使用新的输出目录和对应独立矩阵。

若清理了 Native 构建目录，可在已有 CUDA 镜像内重建：

```bash
docker run --rm --network none --gpus all --user 1000:1000 \
  --workdir /home/anonymous/Projects/infra \
  --mount type=bind,source=/home/anonymous/Projects/infra,target=/home/anonymous/Projects/infra \
  --env HOME=/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/home \
  --entrypoint bash \
  nvcr.io/nvidia/tensorrt-llm/release@sha256:33cd085b772947bd22b7273886539331420404e5d2a4a039945241945ff927b9 \
  "/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/build-native.sh"
```

若清理了 llama 二进制，执行 `bash /home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/build-llama.sh`。若清理了 TRT Engine，需按此前 Engine 构建报告重建，并在本机配置中更新路径。重建产物后应重新执行整轮 Showcase 获取新的指纹和实际结果。

## 10. 本轮改动与保存范围

- 主源码只有 Profile 生成器的两处边界字符串修复；本轮未修改 Showcase、Benchmark 或对比脚本的逻辑。
- 新增本机实验配置、容器适配器、构建产物、独立运行目录和本说明文档。
- 保留此前 vLLM / TensorRT Benchmark、观测服务与用户已有其他修改。
- 没有提交或推送 Git。模型与大型 Engine 不打进文档结果包；原始模型和编译产物仍在前述 WSL 路径中。

详细 Profile 报告与本报告共同说明了本次已验证内容、未执行的可选项以及所有实测结果的使用边界。
