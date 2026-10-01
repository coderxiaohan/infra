#!/usr/bin/env bash
set -euo pipefail
cd /home/anonymous/Projects/infra
cmake -S . -B build-final-cuda -G Ninja -DCMAKE_BUILD_TYPE=Release -DINFERLAB_ENABLE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89
cmake --build build-final-cuda --parallel 6
ctest --test-dir build-final-cuda --output-on-failure --parallel 1
