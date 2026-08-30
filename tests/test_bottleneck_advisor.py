from __future__ import annotations

import unittest

from mini_llm.bottleneck_advisor import build_bottleneck_report
from mini_llm.transformer import MiniTransformerConfig


class MiniTransformerBottleneckAdvisorTest(unittest.TestCase):
    def test_bottleneck_report_ranks_groups_and_keeps_recommendations(self) -> None:
        config = MiniTransformerConfig(
            vocab_size=32,
            max_seq_len=16,
            hidden_size=16,
            num_layers=2,
            num_heads=4,
            ffn_hidden_size=32,
        )
        report = build_bottleneck_report(
            config,
            prompt_tokens=4,
            max_new_tokens=3,
            peak_tflops=1.0,
            bandwidth_gbps=100.0,
        )

        self.assertEqual(report["kind"], "mini_transformer_bottleneck_advisor")
        self.assertGreater(report["hardware_model"]["ridge_point_flops_per_byte"], 0)
        self.assertTrue(report["cached_decode"]["group_rankings"])
        self.assertTrue(report["full_recompute"]["group_rankings"])
        self.assertTrue(report["recommendations"])
        for strategy_name in ("cached_decode", "full_recompute"):
            strategy = report[strategy_name]
            self.assertAlmostEqual(
                sum(float(item["flops_share"] or 0.0) for item in strategy["group_rankings"]),
                1.0,
            )
            self.assertAlmostEqual(
                sum(float(item["bytes_share"] or 0.0) for item in strategy["group_rankings"]),
                1.0,
            )
            self.assertEqual(strategy["attribution"]["unattributed_flops"], 0)
            self.assertEqual(strategy["attribution"]["unattributed_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
