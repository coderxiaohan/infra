import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts.artifact_json import try_write_json_file
from scripts.text_output import atomic_write_text, try_write_text_file


class AtomicOutputTest(unittest.TestCase):
    def test_atomic_text_write_replaces_complete_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifact.txt"
            path.write_text("old", encoding="utf-8")

            atomic_write_text(path, "new")

            self.assertEqual(path.read_text(encoding="utf-8"), "new")

    def test_failed_replace_preserves_existing_text_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "artifact.txt"
            path.write_text("old", encoding="utf-8")

            with patch("scripts.text_output.os.replace", side_effect=OSError("replace failed")):
                result = try_write_text_file(path, "new")

            self.assertEqual(result["status"], "write_failed")
            self.assertEqual(path.read_text(encoding="utf-8"), "old")
            self.assertEqual(list(root.glob(".artifact.txt.*.tmp")), [])

    def test_failed_replace_preserves_existing_json_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "artifact.json"
            path.write_text('{"old": true}\n', encoding="utf-8")

            with patch("scripts.text_output.os.replace", side_effect=OSError("replace failed")):
                result = try_write_json_file(path, {"new": True})

            self.assertEqual(result["status"], "write_failed")
            self.assertEqual(path.read_text(encoding="utf-8"), '{"old": true}\n')
            self.assertEqual(list(root.glob(".artifact.json.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
