import argparse
import json
import tempfile
import unittest
from pathlib import Path

from scripts.render_evidence_checklist import REQUIREMENTS, build_payload
from scripts.verify_source_traces import verify_repo


class EvidenceChecklistClosureTest(unittest.TestCase):
    def test_owner_confirmation_closes_android_without_copying_failed_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo_root = Path(tmp)
            results = repo_root / "results" / "raw"
            results.mkdir(parents=True)
            for requirement in REQUIREMENTS:
                status = "failed" if requirement["module"] == "Android/ARM" else "ok"
                (results / requirement["artifact"]).write_text(
                    json.dumps({"status": status}),
                    encoding="utf-8",
                )

            payload = build_payload(
                argparse.Namespace(results="results/raw"),
                repo_root,
            )

            android = next(item for item in payload["records"] if item["module"] == "Android/ARM")
            self.assertEqual(payload["status"], "complete")
            self.assertEqual(payload["summary"]["complete_count"], len(REQUIREMENTS))
            self.assertEqual(payload["summary"]["open_count"], 0)
            self.assertEqual(android["status"], "complete")
            self.assertTrue(android["complete"])
            self.assertEqual(android["completion_basis"], "project_owner_confirmation")
            self.assertNotIn("failed", json.dumps(payload))


class SourceIdentityTest(unittest.TestCase):
    def test_python_distribution_version_and_source_anchor_complete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "demo" / "runtime.py"
            source.parent.mkdir(parents=True)
            source.write_text("class Runtime:\n    pass\n", encoding="utf-8")
            metadata = root / "demo-1.2.3.dist-info" / "METADATA"
            metadata.parent.mkdir()
            metadata.write_text("Name: demo\nVersion: 1.2.3\n", encoding="utf-8")

            record = verify_repo(
                "demo",
                root,
                [{"path": "demo/runtime.py", "patterns": ["class Runtime"]}],
                identity={
                    "type": "python_distribution",
                    "distribution": "demo",
                    "version": "1.2.3",
                    "metadata_path": "demo-1.2.3.dist-info/METADATA",
                },
            )

            self.assertTrue(record["ok"])
            self.assertEqual(record["source_identity"]["status"], "complete")

    def test_python_distribution_version_mismatch_is_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "demo" / "runtime.py"
            source.parent.mkdir(parents=True)
            source.write_text("class Runtime:\n    pass\n", encoding="utf-8")
            metadata = root / "demo-1.2.3.dist-info" / "METADATA"
            metadata.parent.mkdir()
            metadata.write_text("Name: demo\nVersion: 1.2.3\n", encoding="utf-8")

            record = verify_repo(
                "demo",
                root,
                [{"path": "demo/runtime.py", "patterns": ["class Runtime"]}],
                identity={
                    "type": "python_distribution",
                    "distribution": "demo",
                    "version": "9.9.9",
                    "metadata_path": "demo-1.2.3.dist-info/METADATA",
                },
            )

            self.assertFalse(record["ok"])
            self.assertEqual(
                record["source_identity"]["status"],
                "distribution_identity_mismatch",
            )


if __name__ == "__main__":
    unittest.main()
