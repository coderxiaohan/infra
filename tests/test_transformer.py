from __future__ import annotations

import unittest

try:
    import torch
except ModuleNotFoundError:  # pragma: no cover - optional dependency.
    torch = None

from mini_llm.transformer import (
    MiniTransformerConfig,
    RMSNorm,
    build_model,
    generate_greedy,
)


@unittest.skipIf(torch is None, "PyTorch is not installed")
class MiniTransformerTest(unittest.TestCase):
    def _config(self) -> MiniTransformerConfig:
        return MiniTransformerConfig(
            vocab_size=32,
            max_seq_len=16,
            hidden_size=16,
            num_layers=2,
            num_heads=4,
            ffn_hidden_size=32,
        )

    def test_forward_returns_logits_and_kv_cache(self) -> None:
        model = build_model(self._config(), seed=123)
        input_ids = torch.tensor([[1, 2, 3, 4]], dtype=torch.long)
        output = model(input_ids)
        self.assertEqual(list(output.logits.shape), [1, 4, 32])
        self.assertEqual(len(output.kv_cache), 2)
        self.assertEqual(output.kv_cache[0].sequence_length, 4)
        self.assertGreater(output.kv_cache[0].byte_size, 0)

    def test_cached_decode_matches_full_forward_last_token(self) -> None:
        model = build_model(self._config(), seed=123)
        full_ids = torch.tensor([[1, 2, 3, 4, 5]], dtype=torch.long)
        prompt_ids = full_ids[:, :4]
        next_id = full_ids[:, 4:]

        full = model(full_ids)
        prefill = model(prompt_ids)
        cached = model(next_id, past_kv=prefill.kv_cache)

        max_diff = (full.logits[:, -1, :] - cached.logits[:, -1, :]).abs().max().item()
        self.assertLess(max_diff, 1e-5)
        self.assertEqual(cached.kv_cache[0].sequence_length, 5)

    def test_greedy_generation_extends_prompt_and_cache(self) -> None:
        model = build_model(self._config(), seed=123)
        generated, cache = generate_greedy(model, [1, 2, 3], max_new_tokens=3)
        self.assertEqual(len(generated), 6)
        self.assertEqual(len(cache), 2)
        self.assertEqual(cache[0].sequence_length, 6)

    def test_rmsnorm_accumulates_low_precision_inputs_in_float32(self) -> None:
        norm = RMSNorm(2).half()
        hidden = torch.tensor([[60000.0, -60000.0]], dtype=torch.float16)

        output = norm(hidden)

        self.assertEqual(output.dtype, torch.float16)
        self.assertTrue(torch.isfinite(output).all().item())
        self.assertTrue(torch.allclose(output.float(), torch.tensor([[1.0, -1.0]]), atol=2e-3))


if __name__ == "__main__":
    unittest.main()
