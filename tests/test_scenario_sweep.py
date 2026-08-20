from __future__ import annotations

import unittest

from mini_llm.scenario_sweep import Scenario, build_scenario_sweep
from mini_llm.transformer import MiniTransformerConfig


class MiniTransformerScenarioSweepTest(unittest.TestCase):
    def test_scenario_sweep_ranks_multiple_workloads(self) -> None:
        config = MiniTransformerConfig(
            vocab_size=32,
            max_seq_len=32,
            hidden_size=16,
            num_layers=2,
            num_heads=4,
            ffn_hidden_size=32,
        )
        report = build_scenario_sweep(
            config,
            [
                Scenario("short", 4, 2),
                Scenario("decode_heavy", 3, 8),
            ],
            peak_tflops=1.0,
            bandwidth_gbps=100.0,
        )

        self.assertEqual(report["kind"], "mini_transformer_scenario_sweep")
        self.assertEqual(report["scenario_count"], 2)
        self.assertEqual(len(report["rankings"]["by_flops_saved_ratio"]), 2)
        self.assertTrue(report["recommendations"])


if __name__ == "__main__":
    unittest.main()
