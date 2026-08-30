#!/usr/bin/env python3
"""Build the dynamic-input TensorRT-LLM engine used by the showcase."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Mapping, Sequence


DEFAULT_CONFIG = "configs/trtllm_showcase_engine.json"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def read_json_object(path: Path) -> Dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"unable to read JSON file: {exc}") from exc
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"invalid JSON file: {exc}") from exc
    if not isinstance(payload, dict):
        raise ValueError("JSON root must be an object")
    return payload


def write_json_object(path: Path, payload: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(dict(payload), indent=2, sort_keys=True) + "\n", encoding="utf-8")


def require_object(payload: Mapping[str, Any], key: str) -> Dict[str, Any]:
    value = payload.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"{key} must be an object")
    return value


def require_string(payload: Mapping[str, Any], key: str, prefix: str) -> str:
    value = payload.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{prefix}.{key} must be a nonempty string")
    return value


def require_int(
    payload: Mapping[str, Any],
    key: str,
    prefix: str,
    minimum: int,
    maximum: int,
) -> int:
    value = payload.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{prefix}.{key} must be an integer in [{minimum}, {maximum}]")
    return value


def validate_config(config: Any) -> Dict[str, Any]:
    if not isinstance(config, dict):
        raise ValueError("config root must be an object")
    if config.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")
    model = require_object(config, "model")
    for key in ("name", "path", "dtype"):
        require_string(model, key, "model")
    if model["dtype"] not in {"float16", "bfloat16"}:
        raise ValueError("model.dtype must be float16 or bfloat16")

    engine = require_object(config, "engine")
    for key in ("output_dir", "workspace"):
        require_string(engine, key, "engine")
    for key in (
        "tensor_parallel_size",
        "pipeline_parallel_size",
        "max_input_len",
        "max_seq_len",
        "opt_batch_size",
        "max_batch_size",
        "opt_num_tokens",
        "max_num_tokens",
    ):
        require_int(engine, key, "engine", 1, 1_000_000)
    for key in ("multiple_profiles", "use_paged_context_fmha"):
        if type(engine.get(key)) is not bool:
            raise ValueError(f"engine.{key} must be a boolean")
    if engine["multiple_profiles"]:
        raise ValueError("engine.multiple_profiles must be false for a dynamic token range")
    if engine["max_input_len"] > engine["max_seq_len"]:
        raise ValueError("engine.max_input_len must not exceed max_seq_len")
    if engine["opt_batch_size"] > engine["max_batch_size"]:
        raise ValueError("engine.opt_batch_size must not exceed max_batch_size")
    if engine["opt_num_tokens"] > engine["max_num_tokens"]:
        raise ValueError("engine.opt_num_tokens must not exceed max_num_tokens")
    if engine["max_num_tokens"] > engine["max_input_len"] * engine["max_batch_size"]:
        raise ValueError("engine.max_num_tokens exceeds the configured input capacity")
    return config


def expand_path(repo_root: Path, value: str) -> Path:
    expanded = Path(os.path.expandvars(os.path.expanduser(value)))
    candidate = expanded if expanded.is_absolute() else repo_root / expanded
    return Path(os.path.abspath(candidate))


def expected_token_profile(config: Mapping[str, Any]) -> list[int]:
    engine = require_object(config, "engine")
    return [1, int(engine["opt_num_tokens"]), int(engine["max_num_tokens"])]


def validate_engine_config(
    engine_payload: Mapping[str, Any],
    config: Mapping[str, Any],
) -> list[str]:
    requested = require_object(config, "engine")
    build = engine_payload.get("build_config")
    if not isinstance(build, dict):
        return ["engine config is missing build_config"]
    plugin = build.get("plugin_config")
    if not isinstance(plugin, dict):
        return ["engine config is missing plugin_config"]
    checks = {
        "max_input_len": requested["max_input_len"],
        "max_seq_len": requested["max_seq_len"],
        "opt_batch_size": requested["opt_batch_size"],
        "max_batch_size": requested["max_batch_size"],
        "opt_num_tokens": requested["opt_num_tokens"],
        "max_num_tokens": requested["max_num_tokens"],
    }
    errors = [
        f"{key}: expected {expected}, got {build.get(key)}"
        for key, expected in checks.items()
        if build.get(key) != expected
    ]
    if plugin.get("multiple_profiles") is not False:
        errors.append("plugin_config.multiple_profiles must be false")
    if plugin.get("use_paged_context_fmha") is not requested["use_paged_context_fmha"]:
        errors.append("plugin_config.use_paged_context_fmha does not match the request")
    return errors


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def fingerprint_directory(path: Path) -> Dict[str, Any]:
    digest = hashlib.sha256()
    files = 0
    total_bytes = 0
    for child in sorted(item for item in path.rglob("*") if item.is_file()):
        relative = child.relative_to(path).as_posix()
        size = child.stat().st_size
        child_hash = sha256_file(child)
        digest.update(f"{relative}\0{size}\0{child_hash}\n".encode("utf-8"))
        files += 1
        total_bytes += size
    return {
        "files": files,
        "bytes": total_bytes,
        "sha256": digest.hexdigest(),
    }


def relative_or_name(path: Path, repo_root: Path) -> str:
    try:
        return path.relative_to(repo_root).as_posix()
    except ValueError:
        return f"external/{path.name}"


def build_manifest(
    *,
    status: str,
    config: Mapping[str, Any],
    config_path: Path,
    repo_root: Path,
    model_path: Path,
    output_dir: Path,
    workspace: Path,
    engine_payload: Mapping[str, Any],
    tensorrt_llm_version: str,
) -> Dict[str, Any]:
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        "status": status,
        "config": relative_or_name(config_path, repo_root),
        "model": {
            "name": require_object(config, "model")["name"],
            "path": relative_or_name(model_path, repo_root),
            "dtype": require_object(config, "model")["dtype"],
        },
        "engine": {
            "path": relative_or_name(output_dir, repo_root),
            "workspace": relative_or_name(workspace, repo_root),
            "input_ids_profile": expected_token_profile(config),
            "multiple_profiles": False,
            "fingerprint": fingerprint_directory(output_dir),
            "build_config": engine_payload.get("build_config", {}),
        },
        "runtime": {"tensorrt_llm_version": tensorrt_llm_version},
    }


def build_engine(config: Mapping[str, Any], config_path: Path, repo_root: Path) -> int:
    model = require_object(config, "model")
    engine = require_object(config, "engine")
    model_path = expand_path(repo_root, str(model["path"]))
    output_dir = expand_path(repo_root, str(engine["output_dir"]))
    workspace = expand_path(repo_root, str(engine["workspace"]))
    manifest_path = output_dir.parent / f"{output_dir.name}.build.json"
    if not model_path.is_dir():
        raise ValueError("configured local Hugging Face model directory is missing")

    os.environ.setdefault("HF_HUB_OFFLINE", "1")
    os.environ.setdefault("TRANSFORMERS_OFFLINE", "1")
    try:
        import tensorrt_llm  # type: ignore
        from tensorrt_llm._tensorrt_engine import LLM  # type: ignore
        from tensorrt_llm.bench.build.build import get_model_config  # type: ignore
        from tensorrt_llm.bench.utils.data import initialize_tokenizer  # type: ignore
        from tensorrt_llm.builder import BuildConfig  # type: ignore
        from tensorrt_llm.llmapi.llm_utils import QuantConfig  # type: ignore
    except ImportError as exc:
        raise ValueError(f"TensorRT-LLM build imports failed: {exc}") from exc

    if output_dir.exists():
        engine_payload = read_json_object(output_dir / "config.json")
        errors = validate_engine_config(engine_payload, config)
        if errors:
            raise ValueError("existing engine does not match requested config: " + "; ".join(errors))
        manifest = build_manifest(
            status="reused",
            config=config,
            config_path=config_path,
            repo_root=repo_root,
            model_path=model_path,
            output_dir=output_dir,
            workspace=workspace,
            engine_payload=engine_payload,
            tensorrt_llm_version=str(tensorrt_llm.__version__),
        )
        write_json_object(manifest_path, manifest)
        print(f"existing engine matches: {relative_or_name(output_dir, repo_root)}")
        return 0

    workspace.mkdir(parents=True, exist_ok=True)
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    pending_dir = workspace / f"{output_dir.name}.pending-{os.getpid()}"
    if pending_dir.exists():
        raise ValueError("pending engine directory already exists")

    model_config = get_model_config(str(model["name"]), model_path)
    tokenizer = initialize_tokenizer(model_path)
    build_config = BuildConfig(
        max_input_len=int(engine["max_input_len"]),
        max_seq_len=int(engine["max_seq_len"]),
        opt_batch_size=int(engine["opt_batch_size"]),
        max_batch_size=int(engine["max_batch_size"]),
        opt_num_tokens=int(engine["opt_num_tokens"]),
        max_num_tokens=int(engine["max_num_tokens"]),
    )
    build_config.plugin_config.dtype = str(model["dtype"])
    build_config.plugin_config.multiple_profiles = False
    build_config.plugin_config.use_paged_context_fmha = bool(engine["use_paged_context_fmha"])

    llm = None
    try:
        llm = LLM(
            model_path,
            tokenizer,
            dtype=str(model["dtype"]),
            tensor_parallel_size=int(engine["tensor_parallel_size"]),
            pipeline_parallel_size=int(engine["pipeline_parallel_size"]),
            build_config=build_config,
            quant_config=QuantConfig(quant_algo=None),
            workspace=str(workspace),
            load_format="auto",
            trust_remote_code=False,
        )
        llm.save(pending_dir)
    finally:
        if llm is not None:
            llm.shutdown()

    engine_payload = read_json_object(pending_dir / "config.json")
    errors = validate_engine_config(engine_payload, config)
    if errors:
        raise ValueError("new engine failed contract validation: " + "; ".join(errors))
    pending_dir.rename(output_dir)
    manifest = build_manifest(
        status="built",
        config=config,
        config_path=config_path,
        repo_root=repo_root,
        model_path=model_path,
        output_dir=output_dir,
        workspace=workspace,
        engine_payload=engine_payload,
        tensorrt_llm_version=str(tensorrt_llm.__version__),
    )
    write_json_object(manifest_path, manifest)
    print(f"built engine: {relative_or_name(output_dir, repo_root)}")
    print(f"input_ids profile: {expected_token_profile(config)}")
    return 0


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--config", default=DEFAULT_CONFIG)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        repo_root = Path(args.repo_root).expanduser().resolve()
        config_path = expand_path(repo_root, args.config)
        config = validate_config(read_json_object(config_path))
        return build_engine(config, config_path, repo_root)
    except (OSError, ValueError) as exc:
        print(f"TensorRT-LLM engine build failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
