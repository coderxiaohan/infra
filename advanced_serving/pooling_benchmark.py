#!/usr/bin/env python3
"""Plan or execute embedding/rerank pooling benchmarks."""

from __future__ import annotations

if __package__ in (None, ""):
    from _bootstrap import bootstrap_package

    bootstrap_package()


from typing import Any, Dict, List

from advanced_serving.common import (
    DEFAULT_BACKENDS,
    api_key_metadata,
    api_key_from_env,
    artifact_safe_value,
    base_parser,
    command_record,
    config_int_list,
    config_string_list,
    config_string_value,
    path_label,
    post_json,
    post_json_ok,
    read_json,
    read_jsonl_with_status,
    status_from_execution,
    utc_now,
    write_json,
    write_report,
)


DEFAULT_CONFIG = {"backends": DEFAULT_BACKENDS, "dataset": "configs/rag_pairs.example.jsonl", "tasks": ["embedding", "rerank"], "top_k": [1, 5, 10]}
FALLBACK_ROWS = [{"id": "kv_cache", "query": "What is KV cache?", "positive": "KV cache stores prior keys and values.", "negatives": ["A compiler builds binaries."]}]
KNOWN_TASKS = {"embedding", "rerank"}


def row_documents(row: Dict[str, Any]) -> List[str]:
    positive = str(row.get("positive", ""))
    negatives = row.get("negatives", [])
    if not isinstance(negatives, list):
        negatives = []
    return [positive] + [str(item) for item in negatives]


def build_payload(args) -> Dict[str, Any]:
    cfg = validate_config(read_json(args.config, DEFAULT_CONFIG))
    rows, dataset_read_status = read_jsonl_with_status(cfg.get("dataset", ""), FALLBACK_ROWS)
    api_key = api_key_from_env(args.api_key_env)
    records: List[Dict[str, Any]] = []
    commands = []
    failures = 0
    for backend_name, backend in cfg["backends"].items():
        base_url = str(backend.get("base_url", "")).rstrip("/")
        model = str(backend.get("model", "model"))
        for task in cfg["tasks"]:
            url = f"{base_url}/embeddings" if task == "embedding" else f"{base_url}/rerank"
            top_k_values = cfg["top_k"] if task == "rerank" else [None]
            for top_k in top_k_values:
                command_name = f"{backend_name}:{task}" if top_k is None else f"{backend_name}:{task}:top_{top_k}"
                commands.append(command_record(command_name, ["POST", url, "<payload>"]))
                for row in rows:
                    record: Dict[str, Any] = {
                        "backend": backend_name,
                        "task": task,
                        "top_k": top_k,
                        "id": row.get("id"),
                        "status": "planned",
                    }
                    if args.execute:
                        payload = (
                            {"model": model, "input": row.get("query", "")}
                            if task == "embedding"
                            else {
                                "model": model,
                                "query": row.get("query", ""),
                                "documents": row_documents(row),
                                "top_n": top_k,
                            }
                        )
                        result = post_json(url, payload, args.timeout_seconds, api_key)
                        record["result"] = result
                        record["status"] = "ok" if post_json_ok(result) else "failed"
                        failures += 0 if record["status"] == "ok" else 1
                    records.append(record)
    return {"schema_version": 1, "timestamp_utc": utc_now(), "status": status_from_execution(args.execute, failures, len(records)), "executed": bool(args.execute), "config": artifact_safe_value(cfg), "api_key": api_key_metadata(args.api_key_env, api_key), "dataset_read_status": dataset_read_status, "commands": commands, "records": records, "summary": {"task_count": len(cfg["tasks"]), "top_k_count": len(cfg["top_k"]), "pair_count": len(rows), "record_count": len(records), "failed_records": failures}, "next_actions": ["Attach Recall@k/nDCG labels for real RAG evaluation.", "Compare generation serving metrics with pooling/rerank latency."]}


def validate_config(config: Dict[str, Any]) -> Dict[str, Any]:
    raw_backends = config.get("backends", DEFAULT_BACKENDS)
    if not isinstance(raw_backends, dict) or not raw_backends:
        raise SystemExit("config.backends must be a non-empty object")
    backends: Dict[str, Dict[str, str]] = {}
    for name, backend in raw_backends.items():
        backend_name = config_string_value("config.backends.<name>", name)
        if not isinstance(backend, dict):
            raise SystemExit(f"config.backends.{backend_name} must be an object")
        backends[backend_name] = {
            "base_url": config_string_value(
                f"config.backends.{backend_name}.base_url",
                backend.get("base_url", ""),
            ),
            "model": config_string_value(
                f"config.backends.{backend_name}.model",
                backend.get("model", "model"),
            ),
        }
    config["backends"] = backends
    config["dataset"] = config_string_value("config.dataset", config.get("dataset", DEFAULT_CONFIG["dataset"]))
    config["tasks"] = config_string_list(config, "tasks", KNOWN_TASKS)
    config["top_k"] = config_int_list(config, "top_k", 1, 1_000_000)
    return config


def main() -> int:
    parser = base_parser(__doc__ or "")
    parser.add_argument("--config", default="configs/pooling_benchmark.example.json")
    parser.add_argument("--api-key-env", default="", help="Environment variable containing an API key for endpoint probes.")
    parser.add_argument("--output", default="results/raw/pooling_benchmark.json")
    parser.add_argument("--report", default="reports/pooling_benchmark.md")
    args = parser.parse_args()
    payload = build_payload(args)
    write_json(args.output, payload)
    write_report(args.report, "Pooling/Rerank Benchmark", payload, ((r["backend"], r["task"], r.get("top_k") or "", r["id"], r["status"]) for r in payload["records"]), ("Backend", "Task", "Top K", "ID", "Status"), payload["next_actions"])
    print(f"wrote {path_label(args.output)}")
    print(f"wrote {path_label(args.report)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
