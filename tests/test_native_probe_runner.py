import json
import tempfile
import unittest
from pathlib import Path

from scripts.run_native_probes import run_all, run_probe


class NativeProbeRunnerTest(unittest.TestCase):
    def test_missing_probe_is_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result = run_probe(root / "missing_probe", root / "out.json")

            self.assertEqual(result["status"], "skipped")
            self.assertEqual(result["reason"], "executable missing")

    def test_run_all_reports_skipped_counts_without_build_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = run_all(root / "build", root / "raw")

            self.assertEqual(payload["counts"]["passed"], 0)
            self.assertGreater(payload["counts"]["skipped"], 0)
            self.assertEqual(payload["counts"]["failed"], 0)

    def test_probe_execution_writes_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            probe = root / "probe"
            output = root / "result.json"
            probe.write_text(
                "#!/usr/bin/env python3\n"
                "import json, sys\n"
                "path = sys.argv[sys.argv.index('--output') + 1]\n"
                "open(path, 'w', encoding='utf-8').write(json.dumps({'ok': True}))\n",
                encoding="utf-8",
            )
            probe.chmod(0o755)

            result = run_probe(probe, output)

            self.assertEqual(result["status"], "passed")
            self.assertIn("elapsed_ms", result)
            self.assertGreaterEqual(result["elapsed_ms"], 0.0)
            self.assertTrue(json.loads(output.read_text(encoding="utf-8"))["ok"])

    def test_run_all_reports_timing_for_executed_probe(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            build = root / "build"
            raw = root / "raw"
            build.mkdir()
            probe = build / "inferlab_shape_probe"
            probe.write_text(
                "#!/usr/bin/env python3\n"
                "import json, sys\n"
                "path = sys.argv[sys.argv.index('--output') + 1]\n"
                "open(path, 'w', encoding='utf-8').write(json.dumps({'shape': True}))\n",
                encoding="utf-8",
            )
            probe.chmod(0o755)

            payload = run_all(build, raw)

            self.assertEqual(payload["counts"]["passed"], 1)
            self.assertGreaterEqual(payload["timing"]["total_elapsed_ms"], 0.0)
            self.assertEqual(payload["timing"]["slowest_probe"], "inferlab_shape_probe")

    def test_run_all_supplies_generated_gguf_fixture(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            build = root / "build"
            raw = root / "raw"
            build.mkdir()
            probe = build / "inferlab_gguf_probe"
            probe.write_text(
                "#!/usr/bin/env python3\n"
                "import json, sys\n"
                "input_path = sys.argv[sys.argv.index('--input') + 1]\n"
                "output_path = sys.argv[sys.argv.index('--output') + 1]\n"
                "payload = open(input_path, 'rb').read()\n"
                "open(output_path, 'w', encoding='utf-8').write(json.dumps({\n"
                "    'input_size': len(payload),\n"
                "    'magic': payload[:4].decode('ascii'),\n"
                "}))\n",
                encoding="utf-8",
            )
            probe.chmod(0o755)

            payload = run_all(build, raw)
            result = json.loads((raw / "gguf_probe.json").read_text(encoding="utf-8"))

            self.assertEqual(payload["counts"]["passed"], 1)
            self.assertEqual(result["magic"], "GGUF")
            fixture = raw / "inferlab_fixture.gguf"
            self.assertTrue(fixture.is_file())
            self.assertEqual(result["input_size"], fixture.stat().st_size)
            self.assertGreater(result["input_size"], 0)


if __name__ == "__main__":
    unittest.main()
