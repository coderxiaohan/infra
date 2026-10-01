#!/usr/bin/env bash
set -euo pipefail
cd /home/anonymous/Projects/infra
cmake -S third_party/llama.cpp -B /home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=ON -DLLAMA_BUILD_UI=OFF -DLLAMA_USE_PREBUILT_UI=OFF
cmake --build /home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build --target llama-cli llama-bench --parallel 6
/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build/bin/llama-cli --version
/home/anonymous/Projects/infra/tmp/inferlab-showcase-setup.2wwq42qb/llama-build/bin/llama-bench --help
