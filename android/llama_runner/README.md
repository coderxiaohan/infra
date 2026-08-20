# llama.cpp Android Runner Notes

This directory captures the expected Android deployment path for llama.cpp
without vendoring llama.cpp into InferLab.

## Build Shape

Use `scripts/plan_android_deploy.py` to generate commands for the target
machine. The expected flow is:

1. Build `android/native_hello` with the Android NDK and run it on the device.
2. Build llama.cpp with the same `ANDROID_ABI` and `ANDROID_PLATFORM`.
3. Push a legal GGUF model and the `llama-cli` binary to the device.
4. Run a fixed prompt with fixed thread count and output length.
5. Record prompt eval time, decode eval time, memory, CPU frequency, and
   thermal state.

## Recommended Defaults

- ABI: `arm64-v8a`
- Platform: `android-24` or higher
- Device path: `/data/local/tmp/inferlab`
- llama.cpp flags: `GGML_OPENMP=OFF`, `GGML_NATIVE=OFF`,
  `GGML_LLAMAFILE=OFF`

## Measurement Notes

Run `simpleperf stat` around both `inferlab_android_hello` and `llama-cli` when
available. Record device model, SoC, OS version, CPU governor, thermal state,
thread count, quantization type, prompt tokens, generated tokens, and whether
the model uses mmap or copied buffers.
