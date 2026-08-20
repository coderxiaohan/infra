# InferLab Monitoring

This directory contains portable observability assets for machines that later
run InferLab benchmarks or serving workloads.

## Prometheus Textfile Export

Generate metrics from existing JSON artifacts:

```bash
python3 scripts/export_prometheus_metrics.py \
  --results results/raw \
  --output results/tables/inferlab_metrics.prom
```

To expose the file through node_exporter:

```bash
mkdir -p /opt/inferlab/textfile
cp results/tables/inferlab_metrics.prom /opt/inferlab/textfile/
node_exporter --collector.textfile.directory=/opt/inferlab/textfile
```

The exporter records:

- expected artifact presence
- source status labels
- numeric benchmark-summary metrics
- bounded raw numeric JSON metrics for deeper inspection

## Grafana

Import `grafana_dashboard.json` and select the Prometheus datasource that
scrapes the node_exporter textfile collector. The dashboard includes artifact
coverage, source status, CUDA readiness, latency, throughput, and a raw metric
explorer.
