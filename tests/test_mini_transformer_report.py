from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scripts.render_mini_transformer_report import render_report


class MiniTransformerReportTest(unittest.TestCase):
    def test_report_renders_available_sources_and_missing_list(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            (root / "mini_transformer_cache_compare.json").write_text(
                json.dumps(
                    {
                        "generated_ids_match": True,
                        "prompt_ids": [1, 2],
                        "max_new_tokens": 2,
                        "totals": {
                            "input_token_savings": 3,
                            "input_token_savings_ratio": 0.5,
                            "attention_score_element_savings": 8,
                            "attention_score_element_savings_ratio": 0.25,
                            "cached_max_kv_cache_total_bytes": 128,
                            "recompute_max_kv_cache_total_bytes": 0,
                        },
                    }
                ),
                encoding="utf-8",
            )

            report = render_report(root)

        self.assertIn("# Mini Transformer Optimization Report", report)
        self.assertIn("Cache Compare", report)
        self.assertIn("input_token_savings", report)
        self.assertIn("mini_transformer_workload_profile.json", report)


if __name__ == "__main__":
    unittest.main()
