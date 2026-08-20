# InferLab Android and ARM Edge Deployment

This directory contains portable scaffolding for the ARM/Android stage of
InferLab. It is intentionally safe to keep in the repository before an Android
NDK, adb, llama.cpp checkout, device, or legal GGUF model is available.

## Modules

- `native_hello/`: minimal C++ executable for validating Android NDK cross
  compilation, ABI macros, NEON availability, adb push, and device execution.
- `llama_runner/`: llama.cpp Android build and deployment notes for a legal
  GGUF model.
- `edge_runtime_jni/`: optional JNI shared library and Java lifecycle wrapper
  over the canonical in-process llama.cpp/ExecuTorch `IEdgeRuntime` adapters.

## Plan Generation

Generate a reproducible command plan without executing it:

```bash
python3 scripts/plan_android_deploy.py \
  --ndk /path/to/android-ndk \
  --llama-cpp-dir /path/to/llama.cpp \
  --model /path/to/legal/model.gguf \
  --output reports/android_deploy_plan.md \
  --json-output results/raw/android_deploy_plan.json
```

The generated plan records missing tools and paths instead of failing. Move the
repository to an Android/ARM machine, install the required tools, then run the
commands from the plan.
