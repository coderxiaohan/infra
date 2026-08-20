# InferLab Docker Images

The Docker assets are migration scaffolds. They are committed before the
current server has Docker, GPU access, or model weights so a later CPU/GPU
machine has a reproducible starting point.

## Images

- `Dockerfile.dev`: CPU-oriented development image with Python, CMake, Ninja,
  Git, and native build tools. Mount the repository into `/workspace/InferLab`.
- `Dockerfile.vllm`: CUDA/vLLM serving image scaffold. It expects a GPU-capable
  Docker host, NVIDIA Container Toolkit, and a legal model mounted at runtime.

## Example Commands

```bash
docker build -f docker/Dockerfile.dev -t inferlab-dev:local .
docker run --rm -it -v "$PWD":/workspace/InferLab inferlab-dev:local bash
```

```bash
docker build -f docker/Dockerfile.vllm -t inferlab-vllm:local .
docker run --rm --gpus all \
  -p 8000:8000 \
  -v /models:/models:ro \
  -e INFERLAB_MODEL=/models/model \
  inferlab-vllm:local
```

Generate a machine-specific runtime plan without building or running Docker:

```bash
python3 scripts/plan_runtime_deploy.py --profile full
```
