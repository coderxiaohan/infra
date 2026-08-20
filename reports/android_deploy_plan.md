# InferLab Android Deployment Plan

- Generated: `2026-06-15T02:23:20Z`
- ABI: `arm64-v8a`
- Android platform: `android-24`
- Device directory: `/data/local/tmp/inferlab`

## Preflight

| Item | Value | Ready |
|---|---:|---:|
| NDK root | `` | `False` |
| CMake toolchain | `` | `False` |
| clang++ | `` | `False` |
| sysroot | `` | `False` |
| adb | `` | `False` |
| cmake | `/usr/bin/cmake` | `True` |
| llama.cpp dir | `../llama.cpp` | `False` |
| GGUF model | `` | `False` |

## Native Hello

```bash
cmake -S android/native_hello -B build/android-native-hello -DCMAKE_TOOLCHAIN_FILE= -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-native-hello --config Release -j 4
adb push build/android-native-hello/inferlab_android_hello /data/local/tmp/inferlab/
adb shell 'chmod +x /data/local/tmp/inferlab/inferlab_android_hello'
adb shell /data/local/tmp/inferlab/inferlab_android_hello
```

## Llama Cpp

```bash
cmake -S ../llama.cpp -B build/llama.cpp-android -DCMAKE_TOOLCHAIN_FILE= -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release -DGGML_OPENMP=OFF -DGGML_NATIVE=OFF -DGGML_LLAMAFILE=OFF
cmake --build build/llama.cpp-android --config Release -j 4
adb shell 'mkdir -p /data/local/tmp/inferlab'
adb push /path/to/model.gguf /data/local/tmp/inferlab/model.gguf
adb push build/llama.cpp-android/bin/llama-cli /data/local/tmp/inferlab/
adb shell 'chmod +x /data/local/tmp/inferlab/llama-cli'
adb shell '/data/local/tmp/inferlab/llama-cli -m /data/local/tmp/inferlab/model.gguf -p '\''InferLab edge inference'\'' -n 32 -t 4'
```

## Perf

```bash
adb shell 'simpleperf stat --app '\'''\'' /data/local/tmp/inferlab/inferlab_android_hello'
adb shell 'simpleperf stat /data/local/tmp/inferlab/llama-cli -m /data/local/tmp/inferlab/model.gguf -p '\''InferLab edge inference'\'' -n 32 -t 4'
adb shell 'cat /proc/cpuinfo | head -80'
adb shell 'cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | head'
```

## Notes

- Commands are not executed by this planner.
- Run native_hello first to verify ABI, adb push, and executable permissions.
- Run llama.cpp only with a legal GGUF model path and a compatible device.
- Record thermal and CPU-frequency state before comparing x86, ARM Linux, and Android throughput.
