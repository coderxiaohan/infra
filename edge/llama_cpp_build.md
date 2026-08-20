# llama.cpp Build Notes

Recommended CPU build:

```bash
git clone https://github.com/ggml-org/llama.cpp external/llama.cpp
cmake -S external/llama.cpp -B external/llama.cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build external/llama.cpp/build -j$(nproc)
```

Optional accelerated builds should record CUDA/Vulkan/Metal/SYCL flags, compiler version, backend availability, and the exact model path. Keep model files out of Git.

Benchmark through the portable harness:

```bash
python3 scripts/run_llama_cpp_benchmark.py --binary external/llama.cpp/build/bin/llama-cli --model /path/to/model.gguf --output results/raw/llama_cpp_benchmark.json
```
