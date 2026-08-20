from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scripts.export_mini_transformer_tables import export_tables


class MiniTransformerTableExportTest(unittest.TestCase):
    def test_export_tables_writes_csvs_and_index(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            output = root / "tables"
            (root / "mini_transformer_scenario_sweep.json").write_text(
                json.dumps(
                    {
                        "scenarios": [
                            {
                                "name": "short",
                                "prompt_tokens": 4,
                                "max_new_tokens": 2,
                                "total_tokens": 6,
                                "cached_top_bottleneck_group": "ffn",
                                "flops_saved_ratio": 0.25,
                                "cached_persistent_kv_cache_bytes": 128,
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )

            index = export_tables(root, output)

            self.assertEqual(index["kind"], "mini_transformer_table_exports")
            self.assertTrue((output / "mini_transformer_scenarios.csv").exists())
            self.assertTrue((output / "mini_transformer_table_exports.json").exists())


if __name__ == "__main__":
    unittest.main()
