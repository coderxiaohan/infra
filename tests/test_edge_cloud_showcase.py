import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from scripts.run_edge_cloud_showcase import (
    build_trtllm_command,
    expand_config_path,
    finalize_run,
    main,
    stop_vllm_service,
    validate_config,
    validate_trtllm_runtime_contract,
    write_trtllm_dataset,
)


def valid_config():
    return {
        "schema_version": 1,
        "experiment": {
            "id": "test_showcase",
            "title": "Test edge-cloud showcase",
            "model_family": "distilgpt2",
            "prompt_id": "prompt_1",
            "prompt": "Explain KV cache.",
            "output_tokens": 8,
            "claim_scope": "Single-request integration evidence only.",
        },
        "paths": {
            "build_dir": "build",
            "llama_cli": "tools/llama-cli",
            "llama_bench": "tools/llama-bench",
            "llama_model": "models/model.gguf",
            "vllm_python": "venvs/vllm/python",
            "vllm_model": "models/hf",
            "trtllm_python": "venvs/trt/python",
            "trtllm_bench": "venvs/trt/trtllm-bench",
            "trtllm_engine_dir": "engines/distilgpt2",
        },
        "native": {"timeout_seconds": 30},
        "llama_cpp": {
            "threads": 4,
            "ctx_size": 512,
            "batch_size": 128,
            "ubatch_size": 128,
            "ngl": 0,
            "timeout_seconds": 30,
        },
        "vllm": {
            "host": "127.0.0.1",
            "port": 18000,
            "served_model_name": "inferlab-distilgpt2",
            "dtype": "half",
            "max_model_len": 512,
            "gpu_memory_utilization": 0.35,
            "startup_timeout_seconds": 30,
            "request_timeout_seconds": 30,
            "shutdown_timeout_seconds": 10,
            "enforce_eager": True,
            "use_v2_model_runner": False,
        },
        "trtllm": {
            "model": "distilgpt2",
            "batch_size": 1,
            "max_num_tokens": 64,
            "warmup": 0,
            "timeout_seconds": 30,
            "mpi_interface": "eth0",
        },
    }


class EdgeCloudShowcaseTest(unittest.TestCase):
    def test_validate_config_rejects_non_object_root(self):
        with self.assertRaisesRegex(ValueError, "config root"):
            validate_config([])

    def test_validate_config_rejects_non_loopback_service(self):
        config = valid_config()
        config["vllm"]["host"] = "0.0.0.0"

        with self.assertRaisesRegex(ValueError, "loopback"):
            validate_config(config)

    def test_expand_config_path_supports_relative_and_home_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            home = root / "home"
            with patch.dict(os.environ, {"HOME": str(home)}):
                self.assertEqual(expand_config_path(root, "models/a.gguf"), root / "models/a.gguf")
                self.assertEqual(expand_config_path(root, "${HOME}/bin/python"), home / "bin/python")

    def test_expand_config_path_preserves_virtualenv_interpreter_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            system_python = root / "system-python"
            system_python.write_text("", encoding="utf-8")
            venv_python = root / "venv-python"
            venv_python.symlink_to(system_python)

            expanded = expand_config_path(root, str(venv_python))

            self.assertEqual(expanded, venv_python)
            self.assertTrue(expanded.is_symlink())

    def test_trtllm_command_uses_runtime_contract_without_build_only_flags(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            config = valid_config()
            paths = {
                key: expand_config_path(root, value)
                for key, value in config["paths"].items()
            }
            command = build_trtllm_command(
                config,
                paths,
                root / "dataset.json",
                root / "report.json",
                root / "output.json",
            )

            self.assertIn("throughput", command)
            self.assertIn("--model_path", command)
            self.assertIn("--engine_dir", command)
            self.assertIn("--report_json", command)
            self.assertIn("--output_json", command)
            self.assertNotIn("--max_seq_len", command)
            self.assertNotIn("--request_json", command)

    def test_trtllm_dataset_is_one_json_object_on_one_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            dataset = Path(tmp) / "dataset.json"

            write_trtllm_dataset(dataset, [1, 2, 3], 8)

            content = dataset.read_text(encoding="utf-8")
            self.assertEqual(len(content.splitlines()), 1)
            self.assertEqual(
                json.loads(content),
                {"task_id": 0, "input_ids": [1, 2, 3], "output_tokens": 8},
            )

    def test_trtllm_runtime_contract_accepts_full_prompt_dynamic_engine(self):
        engine = {
            "build_config": {
                "max_input_len": 64,
                "max_seq_len": 96,
                "max_num_tokens": 64,
                "plugin_config": {"multiple_profiles": False},
            }
        }

        self.assertEqual(
            validate_trtllm_runtime_contract(
                engine,
                input_tokens=9,
                output_tokens=8,
                runtime_max_tokens=64,
            ),
            [],
        )

    def test_trtllm_runtime_contract_rejects_old_fixed_engine(self):
        engine = {
            "build_config": {
                "max_input_len": 96,
                "max_seq_len": 96,
                "max_num_tokens": 64,
                "plugin_config": {"multiple_profiles": True},
            }
        }

        errors = validate_trtllm_runtime_contract(
            engine,
            input_tokens=9,
            output_tokens=8,
            runtime_max_tokens=64,
        )

        self.assertIn(
            "engine must use a dynamic token profile (multiple_profiles=false)",
            errors,
        )

    def test_dry_run_writes_json_manifest_without_markdown(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            config_path = root / "config.json"
            config = valid_config()
            config["hf_token"] = "hf-sensitive-value"
            config["security"] = {
                "private_key": "private-sensitive-value",
                "client_secret_key": "client-sensitive-value",
            }
            config_path.write_text(json.dumps(config), encoding="utf-8")

            returncode = main(
                [
                    "--repo-root",
                    str(root),
                    "--config",
                    str(config_path),
                    "--run-id",
                    "dry_run",
                ]
            )

            run_dir = root / "results/showcase/test_showcase/dry_run"
            manifest = json.loads((run_dir / "run_manifest.json").read_text(encoding="utf-8"))
            snapshot = json.loads((run_dir / "config.snapshot.json").read_text(encoding="utf-8"))
            self.assertEqual(returncode, 0)
            self.assertEqual(manifest["status"], "planned")
            self.assertTrue(all(stage["status"] == "planned" for stage in manifest["stages"]))
            trt_stage = next(stage for stage in manifest["stages"] if stage["name"] == "trtllm_cloud")
            input_index = trt_stage["command"].index("--input-tokens") + 1
            self.assertEqual(
                trt_stage["command"][input_index],
                "{runtime_inputs.trtllm_input_tokens}",
            )
            compare_stage = next(stage for stage in manifest["stages"] if stage["name"] == "backend_comparison")
            self.assertNotIn("--report", compare_stage["command"])
            self.assertEqual(list(root.rglob("*.md")), [])
            self.assertEqual(snapshot["hf_token"], "<redacted>")
            self.assertEqual(snapshot["security"]["private_key"], "<redacted>")
            self.assertEqual(snapshot["security"]["client_secret_key"], "<redacted>")

    def test_shutdown_timeout_after_sigkill_is_reported_not_raised(self):
        process = MagicMock()
        process.pid = 123
        process.poll.return_value = None
        process.wait.side_effect = [
            subprocess.TimeoutExpired("vllm", 1),
            subprocess.TimeoutExpired("vllm", 5),
            subprocess.TimeoutExpired("vllm", 5),
        ]
        log_handle = MagicMock()

        with patch("scripts.run_edge_cloud_showcase.os.killpg"), patch(
            "scripts.run_edge_cloud_showcase.port_is_open", return_value=False
        ):
            stage = stop_vllm_service(process, log_handle, valid_config())

        self.assertEqual(stage["status"], "failed")
        self.assertIn("SIGKILL", stage["error"])
        log_handle.close.assert_called_once()

    def test_finalize_run_updates_json_only_stable_index(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            run_dir = root / "results/showcase/test_showcase/passed_run"
            run_dir.mkdir(parents=True)
            manifest = {"schema_version": 1, "status": "passed"}

            finalize_run(
                manifest,
                run_dir,
                root,
                experiment_id="test_showcase",
                update_stable=True,
            )

            latest = json.loads(
                (root / "results/showcase/latest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(latest["experiment_id"], "test_showcase")
            self.assertEqual(latest["status"], "passed")
            self.assertTrue(latest["manifest"].endswith("run_manifest.json"))
            self.assertTrue(latest["comparison"].endswith("backend_comparison.json"))
            self.assertNotIn("report", latest)


if __name__ == "__main__":
    unittest.main()
