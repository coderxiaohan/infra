import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from examples.reference.speculative_decode import SpeculativeScenario, main, scenario_record


class SpeculativeDecodeReferenceTest(unittest.TestCase):
    def test_all_accepted_window_includes_target_bonus_token(self) -> None:
        record = scenario_record(
            SpeculativeScenario("all_accepted", 1.0, 4),
            output_tokens=100,
            target_ms_per_token=10.0,
            draft_ms_per_token=1.0,
            target_verify_overhead_ms=0.0,
        )

        self.assertEqual(record["speculative"]["expected_progress_per_round"], 5.0)
        self.assertEqual(record["speculative"]["rounds"], 20.0)

    def test_cli_imports_and_writes_report(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "speculative.json"
            self.assertEqual(main(["--output", str(output)]), 0)
            self.assertTrue(output.is_file())

    def test_file_entrypoint_runs_from_repo_root(self) -> None:
        repo_root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "speculative-direct.json"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(repo_root / "examples/reference/speculative_decode.py"),
                    "--output",
                    str(output),
                ],
                cwd=repo_root,
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertTrue(output.is_file())


if __name__ == "__main__":
    unittest.main()
