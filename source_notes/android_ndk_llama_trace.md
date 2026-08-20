# Android NDK and llama.cpp Trace Notes

## Cross-Compile Boundary

The Android path has three separate boundaries:

- Host build tools: CMake, Android NDK, Ninja or Make, and adb.
- Target runtime: Android `arm64-v8a`, Bionic libc, device filesystem, and
  executable permissions.
- Inference runtime: llama.cpp model load, GGUF metadata/tensor mapping, ggml
  graph build, backend dispatch, sampler, and decode loop.

`scripts/plan_android_deploy.py` keeps these boundaries explicit by recording
toolchain paths, device paths, model path, and generated commands before any
runtime work is attempted.

## Expected llama.cpp Call Path

For the CLI path, the high-level call chain to inspect in llama.cpp is:

```text
main
  llama_backend_init
  llama_model_load_from_file
  llama_new_context_with_model
  llama_tokenize
  llama_decode prefill
  sampler accept/sample loop
  llama_decode one or more decode steps
  llama_perf_context_print
```

The Android-specific concern is not only whether the binary runs. The important
question is which backend path ggml selects, whether NEON kernels are enabled,
whether model pages are mmap-backed, and whether thermal throttling changes the
decode loop after warmup.

## Metrics To Preserve

- ABI, Android platform, NDK version, compiler target, and sysroot path.
- Device model, SoC, Android version, CPU governor, and thermal zone readings.
- GGUF model name, quantization type, file size, prompt tokens, and generated
  tokens.
- Prompt eval time, eval time, tokens/s, peak RSS if available, and simpleperf
  counters such as instructions, cycles, cache misses, and context switches.

## Failure Triage

- Build fails before compile: check `CMAKE_TOOLCHAIN_FILE`, `ANDROID_ABI`, and
  `ANDROID_PLATFORM`.
- Binary pushes but cannot execute: check `chmod +x`, device path, executable
  bit, and ABI mismatch.
- Model load fails: check model path, file transfer completeness, permissions,
  and available storage.
- Throughput degrades over time: check thermal zone readings, CPU frequency,
  thread count, and background load.
