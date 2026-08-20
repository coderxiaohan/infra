from __future__ import annotations

import unittest

try:
    import torch
except ModuleNotFoundError:  # pragma: no cover - optional dependency.
    torch = None

from mini_llm.cache_compare import compare_cache_strategies
from mini_llm.transformer import MiniTransformerConfig


@unittest.skipIf(torch is None, "PyTorch is not installed")
class MiniTransformerCacheCompareTest(unittest.TestCase):
    def test_cached_decode_saves_attention_score_elements(self) -> None:
        config = MiniTransformerConfig(
            vocab_size=32,
            max_seq_len=16,
            hidden_size=16,
            num_layers=2,
            num_heads=4,
            ffn_hidden_size=32,
        )
        comparison = compare_cache_strategies(config, [1, 2, 3], max_new_tokens=3, seed=123)

        self.assertTrue(comparison["generated_ids_match"])
        self.assertGreater(comparison["totals"]["input_token_savings"], 0)
        self.assertGreater(comparison["totals"]["attention_score_element_savings"], 0)
        self.assertGreater(comparison["totals"]["cached_max_kv_cache_total_bytes"], 0)
        self.assertEqual(comparison["totals"]["recompute_max_kv_cache_total_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
