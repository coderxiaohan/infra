from __future__ import annotations

import unittest

from mini_llm.transformer import MiniTransformerConfig
from mini_llm.workload_profile import build_workload_report


class MiniTransformerWorkloadProfileTest(unittest.TestCase):
    def test_cached_decode_profile_saves_work_but_uses_persistent_kv(self) -> None:
        config = MiniTransformerConfig(
            vocab_size=32,
            max_seq_len=16,
            hidden_size=16,
            num_layers=2,
            num_heads=4,
            ffn_hidden_size=32,
        )
        report = build_workload_report(config, prompt_tokens=4, max_new_tokens=3)

        comparison = report["comparison"]
        self.assertGreater(comparison["input_tokens_saved"], 0)
        self.assertGreater(comparison["attention_score_elements_saved"], 0)
        self.assertGreater(comparison["flops_saved"], 0)
        self.assertGreater(comparison["cached_persistent_kv_cache_bytes"], 0)
        self.assertEqual(comparison["recompute_persistent_kv_cache_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
