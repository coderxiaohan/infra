import unittest

from mini_llm.kv_cache import KVCacheConfig, estimate_kv_cache
from mini_llm.shape_probe import TransformerShapeConfig, build_trace


class KVCacheEstimateTest(unittest.TestCase):
    def test_estimate_matches_formula(self):
        config = KVCacheConfig(
            batch_size=2,
            sequence_length=4,
            num_layers=3,
            num_heads=5,
            head_dim=8,
            dtype="fp16",
        )
        estimate = estimate_kv_cache(config)
        expected = 3 * 2 * 2 * 5 * 4 * 8 * 2
        self.assertEqual(estimate.total_bytes, expected)
        self.assertEqual(estimate.key_shape, (2, 5, 4, 8))
        self.assertEqual(estimate.value_shape, (2, 5, 4, 8))

    def test_invalid_dtype_raises(self):
        config = KVCacheConfig(1, 1, 1, 1, 1, dtype="unknown")
        with self.assertRaises(ValueError):
            estimate_kv_cache(config)

    def test_int4_uses_packed_half_byte_storage(self):
        common = dict(
            batch_size=1,
            sequence_length=7,
            num_layers=3,
            num_heads=5,
            head_dim=9,
        )
        int8 = estimate_kv_cache(KVCacheConfig(**common, dtype="int8"))
        int4 = estimate_kv_cache(KVCacheConfig(**common, dtype="int4"))

        self.assertEqual(int4.bytes_per_token_per_layer * 2, int8.bytes_per_token_per_layer)
        self.assertEqual(int4.total_bytes * 2, int8.total_bytes)


class ShapeProbeTest(unittest.TestCase):
    def test_decode_grows_kv_length(self):
        trace = build_trace(
            TransformerShapeConfig(
                batch_size=1,
                seq_len=8,
                decode_steps=3,
                hidden_size=16,
                num_layers=2,
                num_heads=4,
                head_dim=4,
                dtype="fp16",
            )
        )
        lengths = [row["kv_sequence_length"] for row in trace["decode"]]
        self.assertEqual(lengths, [9, 10, 11])

    def test_hidden_size_must_match_heads(self):
        with self.assertRaises(ValueError):
            build_trace(
                TransformerShapeConfig(
                    batch_size=1,
                    seq_len=8,
                    decode_steps=1,
                    hidden_size=15,
                    num_layers=1,
                    num_heads=4,
                    head_dim=4,
                    dtype="fp16",
                )
            )


if __name__ == "__main__":
    unittest.main()
