import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.validate_outputs import parse_allowed_status_rules, validate_results


class ValidateOutputsTest(unittest.TestCase):
    def test_valid_json_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "ok.json").write_text(json.dumps({"hello": "world"}), encoding="utf-8")
            self.assertEqual(validate_results(root), [])

    def test_invalid_json_reports_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "bad.json").write_text("{not json", encoding="utf-8")
            errors = validate_results(root)
            self.assertEqual(len(errors), 1)
            self.assertIn("invalid JSON", errors[0])

    def test_non_success_status_fails_strict_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "skipped.json").write_text(
                json.dumps({"status": "skipped"}), encoding="utf-8"
            )

            errors = validate_results(root)

            self.assertEqual(len(errors), 1)
            self.assertIn("non-success artifact status", errors[0])
            self.assertEqual(validate_results(root, check_status=False), [])

    def test_exact_file_status_exceptions_preserve_strict_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, status in (
                ("llama_cpp_benchmark.json", "skipped"),
                ("trtllm_benchmark.json", "skipped"),
                ("benchmark_summary.json", "incomplete"),
                ("unexpected.json", "skipped"),
            ):
                (root / name).write_text(
                    json.dumps({"status": status}), encoding="utf-8"
                )

            errors = validate_results(
                root,
                allowed_non_success_statuses={
                    "llama_cpp_benchmark.json": {"skipped"},
                    "trtllm_benchmark.json": {"skipped"},
                    "benchmark_summary.json": {"incomplete"},
                },
            )

            self.assertEqual(len(errors), 1)
            self.assertIn("non-success artifact status: skipped", errors[0])

    def test_file_status_exception_does_not_allow_other_failure_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "llama_cpp_benchmark.json").write_text(
                json.dumps({"status": "failed"}), encoding="utf-8"
            )

            errors = validate_results(
                root,
                allowed_non_success_statuses={
                    "llama_cpp_benchmark.json": {"skipped"}
                },
            )

            self.assertEqual(len(errors), 1)
            self.assertIn("non-success artifact status: failed", errors[0])

    def test_allow_status_rules_reject_broad_or_success_exceptions(self):
        invalid_rules = (
            "missing-separator",
            "../outside.json=skipped",
            "/absolute.json=skipped",
            "artifact.json=ok",
        )
        for rule in invalid_rules:
            with self.subTest(rule=rule), self.assertRaises(ValueError):
                parse_allowed_status_rules([rule])

    def test_run_all_keeps_final_gate_strict_with_exact_exceptions(self):
        script = (
            Path(__file__).resolve().parents[1]
            / "scripts"
            / "run_all_benchmarks.sh"
        ).read_text(encoding="utf-8")
        final_gate = script.rsplit(
            "python3 scripts/validate_outputs.py", 1
        )[1].split("python3 -m unittest", 1)[0]

        self.assertNotIn("--allow-non-ok-status", final_gate)
        self.assertIn(
            "--allow-status llama_cpp_benchmark.json=skipped", final_gate
        )
        self.assertIn(
            "--allow-status trtllm_benchmark.json=skipped", final_gate
        )
        self.assertIn(
            "--allow-status benchmark_summary.json=incomplete", final_gate
        )

    def test_cli_allows_only_the_declared_file_status_pairs(self):
        repo_root = Path(__file__).resolve().parents[1]
        validator = repo_root / "scripts" / "validate_outputs.py"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, status in (
                ("llama_cpp_benchmark.json", "skipped"),
                ("trtllm_benchmark.json", "skipped"),
                ("benchmark_summary.json", "incomplete"),
            ):
                (root / name).write_text(
                    json.dumps({"status": status}), encoding="utf-8"
                )
            command = [
                sys.executable,
                str(validator),
                "--results",
                str(root),
                "--allow-status",
                "llama_cpp_benchmark.json=skipped",
                "--allow-status",
                "trtllm_benchmark.json=skipped",
                "--allow-status",
                "benchmark_summary.json=incomplete",
            ]

            allowed = subprocess.run(
                command,
                cwd=repo_root,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(allowed.returncode, 0, allowed.stdout + allowed.stderr)

            (root / "llama_cpp_benchmark.json").write_text(
                json.dumps({"status": "failed"}), encoding="utf-8"
            )
            rejected = subprocess.run(
                command,
                cwd=repo_root,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(rejected.returncode, 1)
            self.assertIn("non-success artifact status: failed", rejected.stdout)


if __name__ == "__main__":
    unittest.main()
