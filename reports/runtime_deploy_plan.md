# InferLab Runtime Deployment Plan

- Generated: `2026-06-15T02:23:20Z`
- Profile: `full`
- Host: `Linux-6.8.0-71-generic-x86_64-with-glibc2.39` / `x86_64`

## Preflight

| Item | Value | Ready |
|---|---|---:|
| docker | `/usr/bin/docker` | `True` |
| nvidia-smi | `` | `False` |
| model root | `./models` | `False` |
| textfile dir | `../results/tables` | `False` |
| inferlab compose | `deploy/docker-compose.inferlab.yml` | `True` |
| observability compose | `deploy/docker-compose.observability.yml` | `True` |

## Build Commands

```bash
docker build -f docker/Dockerfile.dev -t inferlab-dev:local .
docker build -f docker/Dockerfile.vllm -t inferlab-vllm:local .
```

## Dev Commands

```bash
docker run --rm -it -v '$PWD:/workspace/InferLab' inferlab-dev:local bash
```

## Vllm Commands

```bash
docker compose --env-file deploy/inferlab.runtime.env -f deploy/docker-compose.inferlab.yml --profile gpu up vllm-server
```

## Observability Commands

```bash
python3 scripts/export_prometheus_metrics.py --repo-root . --results results/raw --output results/tables/inferlab_metrics.prom
docker compose --env-file deploy/inferlab.runtime.env -f deploy/docker-compose.observability.yml up -d
```

## Benchmark Commands

```bash
python3 scripts/run_vllm_benchmark.py --repo-root . --base-url http://127.0.0.1:8000/v1 --model model --output results/raw/vllm_serving_benchmark.json
python3 scripts/summarize_benchmarks.py --repo-root . --results results/raw --output results/tables/benchmark_summary.md --json-output results/raw/benchmark_summary.json
```

## Notes

- This planner writes commands only; it does not build or run containers.
- Use the dev image on CPU-only machines for editing and native builds.
- Use the vLLM service profile only on a GPU machine with NVIDIA Container Toolkit.
- Generate inferlab_metrics.prom before starting the observability stack if no benchmark pipeline has run yet.
