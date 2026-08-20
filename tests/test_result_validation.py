import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_outputs import validate_results


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


if __name__ == "__main__":
    unittest.main()
