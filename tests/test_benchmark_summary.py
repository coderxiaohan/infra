import json
import tempfile
import unittest
from pathlib import Path

from scripts.summarize_benchmarks import build_summary, write_summary


class BenchmarkSummaryTest(unittest.TestCase):
    def write_json(self, root: Path, name: str, payload: dict) -> None:
        (root / name).write_text(json.dumps(payload), encoding="utf-8")

    def test_build_summary_extracts_native_probe_metrics(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "kv_allocation_compare.json",
                {
                    "static": {"reserved_bytes": 32768, "utilization": 0.25},
                    "paged": {"reserved_bytes": 8192, "utilization": 1.0},
                    "savings": {"reserved_bytes_delta": 24576, "reserved_bytes_ratio": 0.25},
                },
            )
            self.write_json(
                root,
                "decoder_kv_probe.json",
                {
                    "decoder_kv": {
                        "prompt_tokens": 5,
                        "decode_steps": 3,
                        "final_tokens": 8,
                        "used_blocks": 2,
                        "block_size_tokens": 4,
                        "used_block_table_gather": True,
                        "last_token_output_checksum": -0.1,
                    }
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}

            self.assertEqual(sections["Static vs Paged KV"]["status"], "ok")
            kv_metrics = {metric["name"]: metric["value"] for metric in sections["Static vs Paged KV"]["metrics"]}
            self.assertEqual(kv_metrics["reserved_bytes_saved"], 24576)
            self.assertEqual(kv_metrics["paged_reserved_ratio_percent"], 25.0)

            decoder_metrics = {
                metric["name"]: metric["value"] for metric in sections["Decoder KV Decode"]["metrics"]
            }
            self.assertTrue(decoder_metrics["used_block_table_gather"])
            self.assertEqual(decoder_metrics["final_tokens"], 8)

    def test_missing_expected_sources_are_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            summary = build_summary(Path(tmp))

            self.assertEqual(summary["overview"]["available_sources"], 0)
            self.assertEqual(summary["status"], "incomplete")
            self.assertIn("cuda_environment.json", summary["overview"]["missing_sources"])
            self.assertTrue(all(section["status"] == "missing" for section in summary["sections"]))

    def test_write_summary_emits_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            summary = build_summary(root)
            json_path = root / "raw" / "benchmark_summary.json"

            write_summary(summary, json_path)

            loaded = json.loads(json_path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["schema_version"], 1)
            self.assertIn("cuda_environment.json", loaded["overview"]["missing_sources"])

    def test_non_success_source_status_is_not_silently_promoted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(root, "vllm_serving_benchmark.json", {"status": "partial"})

            summary = build_summary(root)
            section = next(
                item for item in summary["sections"] if item["source"] == "vllm_serving_benchmark.json"
            )

            self.assertEqual(section["status"], "partial")
            self.assertIn("vllm_serving_benchmark.json", summary["overview"]["unavailable_sources"])
            self.assertEqual(summary["status"], "incomplete")

    def test_summary_contains_extracted_metrics(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "quantization_probe.json",
                {
                    "config": {"rows": 4, "cols": 16, "group_size": 8},
                    "quantization": {
                        "int8": {"storage_ratio_vs_fp32": 0.375, "mean_abs_error": 0.001},
                        "int4": {"storage_ratio_vs_fp32": 0.25, "mean_abs_error": 0.03},
                    },
                    "weight_only_matvec": {
                        "int4_fused_vs_materialized_max_abs_error": 0.0
                    },
                },
            )

            summary = build_summary(root)
            section = next(item for item in summary["sections"] if item["title"] == "Weight-Only Quantization")
            metrics = {item["name"]: item["value"] for item in section["metrics"]}
            self.assertEqual(metrics["int4_storage_ratio_percent"], 25.0)

    def test_scheduler_request_lifecycle_metrics_are_aggregated(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "scheduler_trace.json",
                {
                    "steps": [
                        {"admitted": 1, "decoded": 1, "finished": 0, "rejected": 0, "cache": {"used_blocks": 1, "reserved_bytes": 128}},
                        {"admitted": 1, "decoded": 2, "finished": 2, "rejected": 0, "cache": {"used_blocks": 0, "reserved_bytes": 0}},
                    ],
                    "requests": [
                        {
                            "request_id": 1,
                            "state": "finished",
                            "queue_wait_steps": 1,
                            "time_to_first_token_steps": 1,
                            "decode_steps_per_token": 1.0,
                        },
                        {
                            "request_id": 2,
                            "state": "finished",
                            "queue_wait_steps": 2,
                            "time_to_first_token_steps": 2,
                            "decode_steps_per_token": 1.0,
                        },
                    ],
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}
            metrics = {metric["name"]: metric["value"] for metric in sections["Continuous Batching"]["metrics"]}

            self.assertEqual(metrics["request_count"], 2)
            self.assertEqual(metrics["avg_ttft_steps"], 1.5)
            self.assertEqual(metrics["max_queue_wait_steps"], 2.0)

    def test_native_probe_runner_timing_is_summarized(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "native_probe_run.json",
                {
                    "counts": {"passed": 2, "skipped": 0, "failed": 0},
                    "timing": {
                        "total_elapsed_ms": 12.5,
                        "slowest_probe": "inferlab_decoder_probe",
                        "slowest_probe_elapsed_ms": 7.25,
                    },
                    "build_dir": "build",
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}
            metrics = {metric["name"]: metric["value"] for metric in sections["Native Probe Runner"]["metrics"]}

            self.assertEqual(metrics["total_elapsed_ms"], 12.5)
            self.assertEqual(metrics["slowest_probe"], "inferlab_decoder_probe")

    def test_kv_fragmentation_metrics_are_summarized(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "kv_fragmentation_probe.json",
                {
                    "config": {"num_blocks": 12, "block_size_tokens": 4},
                    "fragmentation": {
                        "after_holes_free_blocks": 7,
                        "after_reuse_used_blocks": 9,
                        "reused_released_block_count": 4,
                        "failed_allocations": 1,
                        "final_free_blocks": 12,
                        "final_used_blocks": 0,
                    },
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}
            metrics = {metric["name"]: metric["value"] for metric in sections["Paged KV Fragmentation"]["metrics"]}

            self.assertEqual(metrics["reused_released_block_count"], 4)
            self.assertEqual(metrics["failed_allocations"], 1)
            self.assertEqual(metrics["final_used_blocks"], 0)

    def test_paged_kv_locality_metrics_are_summarized(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "paged_kv_locality_probe.json",
                {
                    "config": {"token_count": 9},
                    "locality": {
                        "contiguous": {"span_element_amplification": 1.333333},
                        "fragmented": {
                            "logical_blocks": 3,
                            "non_contiguous_transitions": 2,
                            "max_physical_jump": 4,
                            "span_element_amplification": 3.111111,
                            "block_element_amplification": 1.333333,
                        },
                        "fragmented_vs_contiguous_span_amplification_delta": 1.777778,
                    },
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}
            metrics = {metric["name"]: metric["value"] for metric in sections["Paged KV Locality"]["metrics"]}

            self.assertEqual(metrics["fragmented_non_contiguous_transitions"], 2)
            self.assertEqual(metrics["span_amplification_delta"], 1.777778)

    def test_gguf_q4_k_scale_min_metrics_are_summarized(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write_json(
                root,
                "gguf_probe.json",
                {
                    "gguf": {
                        "version": 3,
                        "source": "file",
                        "input_path": "results/raw/inferlab_fixture.gguf",
                        "layout_segments": [],
                        "metadata": [],
                        "payload_previews": [
                            {
                                "type_name": "f32",
                                "raw_values_hex_prefix": "00 00 80 3f 00 00 80 bf",
                                "raw_values_prefix": [1065353216, 3212836864],
                                "decode_rule": "value = little_endian_float32(raw_4_bytes)",
                                "values_prefix": [1, -1],
                                "stats": {
                                    "value_count": 2,
                                    "checksum": 0,
                                    "min": -1,
                                    "max": 1,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "f64",
                                "raw_values_hex_prefix": "00 00 00 00 00 00 f0 3f 00 00 00 00 00 00 f0 bf",
                                "raw_values_prefix": [
                                    4607182418800017408,
                                    13830554455654793216,
                                ],
                                "decode_rule": "value = little_endian_float64(raw_8_bytes)",
                                "values_prefix": [1, -1],
                                "stats": {
                                    "value_count": 2,
                                    "checksum": 0,
                                    "min": -1,
                                    "max": 1,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "i64",
                                "raw_values_hex_prefix": "01 00 00 00 00 00 00 00 ff ff ff ff ff ff ff ff 15 cd 5b 07 00 00 00 00 eb 32 a4 f8 ff ff ff ff",
                                "raw_values_prefix": [
                                    1,
                                    18446744073709551615,
                                    123456789,
                                    18446744073586094827,
                                ],
                                "decode_rule": "value = int64_t(little_endian_raw_8_bytes)",
                                "values_prefix": [1, -1, 123456789, -123456789],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0,
                                    "min": -123456789,
                                    "max": 123456789,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "i32",
                                "raw_values_hex_prefix": "01 00 00 00 ff ff ff ff 40 e2 01 00 c0 1d fe ff",
                                "raw_values_prefix": [
                                    1,
                                    4294967295,
                                    123456,
                                    4294843840,
                                ],
                                "decode_rule": "value = int32_t(little_endian_raw_4_bytes)",
                                "values_prefix": [1, -1, 123456, -123456],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0,
                                    "min": -123456,
                                    "max": 123456,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "i16",
                                "raw_values_hex_prefix": "01 00 ff ff d2 04 2e fb",
                                "raw_values_prefix": [1, 65535, 1234, 64302],
                                "decode_rule": "value = int16_t(little_endian_raw_2_bytes)",
                                "values_prefix": [1, -1, 1234, -1234],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0,
                                    "min": -1234,
                                    "max": 1234,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "i8",
                                "raw_values_hex_prefix": "01 ff 64 9c",
                                "raw_values_prefix": [1, 255, 100, 156],
                                "decode_rule": "value = int8_t(raw_byte)",
                                "values_prefix": [1, -1, 100, -100],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0,
                                    "min": -100,
                                    "max": 100,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "f16",
                                "raw_values_hex_prefix": "00 3c 00 40 00 c1 00 00",
                                "raw_values_prefix": [15360, 16384, 49408, 0],
                                "decode_rule": "value = gguf_fp16_to_float32(raw_half)",
                                "values_prefix": [1, 2, -2.5, 0],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0.5,
                                    "min": -2.5,
                                    "max": 2,
                                    "mean": 0.125,
                                },
                            },
                            {
                                "type_name": "iq2_xxs",
                                "raw_bytes_prefix": (
                                    "00 3c 01 02 03 04 01 c1 80 10 "
                                    "05 06 07 08 05 c3 01 21"
                                ),
                                "d_fp32": 1,
                                "qs_words_prefix": [
                                    513,
                                    1027,
                                    49409,
                                    4224,
                                    1541,
                                    2055,
                                    49925,
                                    8449,
                                ],
                                "grid_indices_prefix": [1, 2, 3, 4, 5, 6, 7, 8],
                                "sign_codes_prefix": [1, 2, 3, 4, 5, 6, 7, 8],
                                "scale_nibbles": [1, 2, 3, 4, 5, 6, 7, 8],
                                "effective_scales": [
                                    0.375,
                                    0.625,
                                    0.875,
                                    1.125,
                                    1.375,
                                    1.625,
                                    1.875,
                                    2.125,
                                ],
                                "decode_rule": (
                                    "structural preview: d=f16, per-32 scale="
                                    "d*(0.5+scale_nibble)*0.25, each 8-value "
                                    "chunk selects iq2xxs_grid[grid_index] and "
                                    "ksigns_iq2xs[sign_code]"
                                ),
                            },
                            {
                                "type_name": "q2_K",
                                "d_fp32": 1,
                                "dmin_fp32": 0.5,
                                "scale_min_bytes_prefix": "00 11 22 33",
                                "packed_quant_bytes_prefix": "00 01 02 03",
                                "scale_min_pairs": [
                                    {
                                        "sub_block": 0,
                                        "scale": 0,
                                        "min": 0,
                                        "effective_scale": 0,
                                        "effective_min": 0,
                                    },
                                    {
                                        "sub_block": 15,
                                        "scale": 15,
                                        "min": 15,
                                        "effective_scale": 15,
                                        "effective_min": 7.5,
                                    },
                                ],
                                "dequant_rule": (
                                    "value = fp16(d) * scale * q2 - "
                                    "fp16(dmin) * min"
                                ),
                                "dequant_group0_values_prefix": [0, 0],
                                "dequant_group1_values_prefix": [-0.5, 0.5],
                                "dequant_values_prefix": [0, 0, -0.5],
                                "dequant_group_sums": [
                                    0,
                                    16,
                                    32,
                                    48,
                                    -32,
                                    40,
                                    -48,
                                    -56,
                                    128,
                                    144,
                                    160,
                                    176,
                                    288,
                                    520,
                                    -112,
                                    -120,
                                ],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 1184,
                                    "min": -7.5,
                                    "max": 32.5,
                                    "mean": 4.625,
                                },
                            },
                            {
                                "type_name": "q3_K",
                                "d_fp32": 1,
                                "high_mask_bytes_prefix": "00 ff ff ff",
                                "packed_quant_bytes_prefix": "00 01 02 03",
                                "scale_bytes_prefix": "80 91 a2 b3",
                                "scales": [
                                    {
                                        "group": 0,
                                        "scale": 0,
                                        "effective_scale": 0,
                                    },
                                    {
                                        "group": 15,
                                        "scale": 15,
                                        "effective_scale": 15,
                                    },
                                ],
                                "dequant_rule": (
                                    "value = fp16(d) * int8(scale) * "
                                    "(low2(qs) - (hmask_bit ? 0 : 4))"
                                ),
                                "dequant_group0_values_prefix": [0, 0],
                                "dequant_group1_values_prefix": [0, 3],
                                "dequant_values_prefix": [0, 0, -8],
                                "dequant_group_sums": [
                                    0,
                                    24,
                                    40,
                                    72,
                                    -16,
                                    80,
                                    -24,
                                    0,
                                    160,
                                    216,
                                    200,
                                    264,
                                    336,
                                    624,
                                    -56,
                                    0,
                                ],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 1920,
                                    "min": -56,
                                    "max": 39,
                                    "mean": 7.5,
                                },
                            },
                            {
                                "type_name": "q4_0",
                                "d_fp32": 1,
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2, 3, 4, 5],
                                "dequant_rule": "value = fp16(d) * (q4 - 8)",
                                "dequant_values_prefix": [-8, -7, -6, -5, -4, -3],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": -16,
                                    "min": -8,
                                    "max": 7,
                                    "mean": -0.5,
                                },
                            },
                            {
                                "type_name": "q4_1",
                                "d_fp32": 1,
                                "m_fp32": 0.5,
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2, 3, 4, 5],
                                "dequant_rule": "value = fp16(d) * q4 + fp16(m)",
                                "dequant_values_prefix": [0.5, 1.5, 2.5, 3.5, 4.5, 5.5],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": 256,
                                    "min": 0.5,
                                    "max": 15.5,
                                    "mean": 8,
                                },
                            },
                            {
                                "type_name": "q5_0",
                                "d_fp32": 1,
                                "high_bits_bytes_prefix": "00 ff 55 aa",
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2, 3, 4, 5],
                                "dequant_rule": "value = fp16(d) * (q5 - 16)",
                                "dequant_values_prefix": [-16, -15, -14, -13, -12, -11],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": -16,
                                    "min": -16,
                                    "max": 15,
                                    "mean": -0.5,
                                },
                            },
                            {
                                "type_name": "q5_1",
                                "d_fp32": 1,
                                "m_fp32": 0.5,
                                "high_bits_bytes_prefix": "00 ff 55 aa",
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2, 3, 4, 5],
                                "dequant_rule": "value = fp16(d) * q5 + fp16(m)",
                                "dequant_values_prefix": [0.5, 1.5, 2.5, 3.5, 4.5, 5.5],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": 512,
                                    "min": 0.5,
                                    "max": 31.5,
                                    "mean": 16,
                                },
                            },
                            {
                                "type_name": "q4_K",
                                "d_fp32": 1,
                                "dmin_fp32": 0.5,
                                "scale_min_pairs": [
                                    {
                                        "sub_block": 0,
                                        "scale": 0,
                                        "min": 4,
                                        "effective_scale": 0,
                                        "effective_min": 2,
                                    }
                                ],
                                "dequant_rule": (
                                    "value = fp16(d) * scale * q - fp16(dmin) * min"
                                ),
                                "dequant_subblock0_values_prefix": [-2, -2],
                                "dequant_subblock1_values_prefix": [-2.5, -2.5],
                                "dequant_values_prefix": [-2, -2, -2.5],
                                "dequant_subblock_sums": [
                                    -64,
                                    -64,
                                    384,
                                    128,
                                    1920,
                                    1296,
                                    2400,
                                    2288,
                                ],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 8288,
                                    "min": -3,
                                    "max": 150,
                                    "mean": 32.375,
                                },
                            },
                            {
                                "type_name": "q5_K",
                                "d_fp32": 1,
                                "dmin_fp32": 0.5,
                                "scale_bytes_prefix": "00 01 02 03",
                                "high_bits_bytes_prefix": "ff ff ff ff",
                                "packed_quant_bytes_prefix": "00 01 02 03",
                                "scale_min_pairs": [
                                    {
                                        "sub_block": 0,
                                        "scale": 0,
                                        "min": 4,
                                        "effective_scale": 0,
                                        "effective_min": 2,
                                    }
                                ],
                                "dequant_rule": (
                                    "value = fp16(d) * scale * "
                                    "(low4(qs) + high1(qh) * 16) - "
                                    "fp16(dmin) * min"
                                ),
                                "dequant_subblock0_values_prefix": [-2, -2],
                                "dequant_subblock1_values_prefix": [13.5, 13.5],
                                "dequant_values_prefix": [-2, -2, 13.5],
                                "dequant_subblock_sums": [
                                    -64,
                                    448,
                                    1408,
                                    1664,
                                    6016,
                                    5904,
                                    7520,
                                    7920,
                                ],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 30816,
                                    "min": -2,
                                    "max": 310,
                                    "mean": 120.375,
                                },
                            },
                            {
                                "type_name": "q6_K",
                                "d_fp32": 1,
                                "low_bits_bytes_prefix": "00 01 02 03",
                                "high_bits_bytes_prefix": "ff ff ff ff",
                                "scale_bytes_prefix": "00 01 02 03",
                                "scales": [
                                    {
                                        "group": 0,
                                        "scale": 0,
                                        "effective_scale": 0,
                                    },
                                    {
                                        "group": 15,
                                        "scale": 15,
                                        "effective_scale": 15,
                                    },
                                ],
                                "dequant_rule": (
                                    "value = fp16(d) * int8(scale) * "
                                    "(low4(ql) + high2(qh) * 16 - 32)"
                                ),
                                "dequant_group0_values_prefix": [0, 0],
                                "dequant_group1_values_prefix": [16, 17],
                                "dequant_values_prefix": [0, 0, 16],
                                "dequant_group_sums": [
                                    0,
                                    376,
                                    752,
                                    1128,
                                    1024,
                                    1360,
                                    1728,
                                    2128,
                                    3008,
                                    3384,
                                    3760,
                                    4136,
                                    3840,
                                    4368,
                                    4928,
                                    5520,
                                ],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 41440,
                                    "min": 0,
                                    "max": 345,
                                    "mean": 161.875,
                                },
                            },
                            {
                                "type_name": "q8_0",
                                "d_fp32": 1,
                                "quant_values_prefix": [-16, -15, -14],
                                "dequant_values_prefix": [-16, -15, -14],
                                "quant_sum_first_block": -16,
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": -16,
                                    "min": -16,
                                    "max": 15,
                                    "mean": -0.5,
                                },
                            },
                            {
                                "type_name": "q8_1",
                                "d_fp32": 0.5,
                                "s_fp32": -8,
                                "expected_s_from_quant_sum": -8,
                                "quant_values_prefix": [-16, -15, -14],
                                "dequant_values_prefix": [-8, -7.5, -7],
                                "quant_sum_first_block": -16,
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": -8,
                                    "min": -8,
                                    "max": 7.5,
                                    "mean": -0.25,
                                },
                            },
                            {
                                "type_name": "q8_K",
                                "d_fp32": 0.25,
                                "quant_values_prefix": [-128, -127, -126],
                                "block_sums_prefix": [-1928, -1672, -1416],
                                "block_sum_bytes_prefix": "78 f8 78 f9",
                                "dequant_rule": "value = int8_quant * float32(d); bsums store sum(qs) for each 16-value group",
                                "dequant_values_prefix": [-32, -31.75, -31.5],
                                "quant_sum_first_block": -128,
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": -32,
                                    "min": -32,
                                    "max": 31.75,
                                    "mean": -0.125,
                                    "stored_block_sum_total": -128,
                                },
                            },
                            {
                                "type_name": "q1_0",
                                "d_fp32": 1,
                                "packed_bit_bytes_prefix": "aa aa aa aa",
                                "bit_values_prefix": [0, 1, 0, 1],
                                "dequant_rule": "value = bit ? fp16(d) : -fp16(d)",
                                "dequant_values_prefix": [-1, 1, -1, 1],
                                "dequant_stats": {
                                    "value_count": 128,
                                    "checksum": 0,
                                    "min": -1,
                                    "max": 1,
                                    "mean": 0,
                                    "positive_bit_count": 64,
                                    "negative_bit_count": 64,
                                },
                            },
                            {
                                "type_name": "iq4_nl",
                                "d_fp32": 0.5,
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2],
                                "codebook_table": [
                                    -127,
                                    -104,
                                    -83,
                                    -65,
                                    -49,
                                    -35,
                                    -22,
                                    -10,
                                    1,
                                    13,
                                    25,
                                    38,
                                    53,
                                    69,
                                    89,
                                    113,
                                ],
                                "codebook_values_prefix": [-127, -104, -83],
                                "dequant_rule": "value = fp16(d) * iq4nl_codebook[q4_index]",
                                "dequant_values_prefix": [-63.5, -52, -41.5],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": -94,
                                    "min": -63.5,
                                    "max": 56.5,
                                    "mean": -2.9375,
                                },
                            },
                            {
                                "type_name": "mxfp4",
                                "e": 128,
                                "e_hex": "80",
                                "e_scale_fp32": 1,
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2],
                                "codebook_table": [
                                    0,
                                    1,
                                    2,
                                    3,
                                    4,
                                    6,
                                    8,
                                    12,
                                    0,
                                    -1,
                                    -2,
                                    -3,
                                    -4,
                                    -6,
                                    -8,
                                    -12,
                                ],
                                "codebook_values_prefix": [0, 1, 2],
                                "dequant_rule": (
                                    "value = ggml_e8m0_to_fp32_half(e) * "
                                    "kvalues_mxfp4[q4_index]"
                                ),
                                "dequant_values_prefix": [0, 1, 2],
                                "dequant_stats": {
                                    "value_count": 32,
                                    "checksum": 0,
                                    "min": -12,
                                    "max": 12,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "nvfp4",
                                "scale_bytes_prefix": "38 40 30 48",
                                "scales": [
                                    {"block": 0, "raw_scale": 56, "scale_fp32": 0.5},
                                    {"block": 1, "raw_scale": 64, "scale_fp32": 1},
                                ],
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "quant_values_prefix": [0, 1, 2],
                                "codebook_table": [
                                    0,
                                    1,
                                    2,
                                    3,
                                    4,
                                    6,
                                    8,
                                    12,
                                    0,
                                    -1,
                                    -2,
                                    -3,
                                    -4,
                                    -6,
                                    -8,
                                    -12,
                                ],
                                "codebook_values_prefix": [0, 1, 2],
                                "dequant_rule": (
                                    "value = ggml_ue4m3_to_fp32(scale_byte) * "
                                    "kvalues_mxfp4[q4_index]"
                                ),
                                "dequant_subblock0_values_prefix": [0, 0.5, 1],
                                "dequant_subblock1_values_prefix": [0, 1, 2],
                                "dequant_values_prefix": [0, 0.5, 1],
                                "dequant_subblock_sums": [0, 0, 0, 0],
                                "dequant_stats": {
                                    "value_count": 64,
                                    "checksum": 0,
                                    "min": -24,
                                    "max": 24,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "bf16",
                                "raw_values_hex_prefix": "80 3f 00 40 20 c0 00 00",
                                "raw_values_prefix": [16256, 16384, 49184, 0],
                                "decode_rule": "value = reinterpret_float32(uint32_t(raw_bf16) << 16)",
                                "values_prefix": [1, 2, -2.5, 0],
                                "stats": {
                                    "value_count": 4,
                                    "checksum": 0.5,
                                    "min": -2.5,
                                    "max": 2,
                                    "mean": 0.125,
                                },
                            },
                            {
                                "type_name": "iq4_xs",
                                "d_fp32": 1,
                                "scales_h": 43690,
                                "scales_h_hex": "aa aa",
                                "scales_l_bytes_prefix": "21 43 65 87",
                                "packed_quant_bytes_prefix": "f0 e1 d2 c3",
                                "scales": [
                                    {
                                        "block": 0,
                                        "raw_scale": 33,
                                        "signed_scale": 1,
                                        "effective_scale": 1,
                                    },
                                    {
                                        "block": 1,
                                        "raw_scale": 34,
                                        "signed_scale": 2,
                                        "effective_scale": 2,
                                    },
                                ],
                                "quant_values_prefix": [0, 1, 2],
                                "codebook_table": [
                                    -127,
                                    -104,
                                    -83,
                                    -65,
                                    -49,
                                    -35,
                                    -22,
                                    -10,
                                    1,
                                    13,
                                    25,
                                    38,
                                    53,
                                    69,
                                    89,
                                    113,
                                ],
                                "codebook_values_prefix": [-127, -104, -83],
                                "dequant_rule": "value = fp16(d) * (scale6 - 32) * iq4nl_codebook[q4_index]",
                                "dequant_subblock0_values_prefix": [-127, -104, -83],
                                "dequant_subblock1_values_prefix": [-254, -208, -166],
                                "dequant_values_prefix": [-127, -104, -83],
                                "dequant_subblock_sums": [-188, -376, -564],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": -6768,
                                    "min": -1016,
                                    "max": 904,
                                    "mean": -26.4375,
                                },
                            },
                            {
                                "type_name": "tq1_0",
                                "d_fp32": 1,
                                "packed_qs_bytes_prefix": "00 00 00 00",
                                "packed_qh_bytes_prefix": "00 80 ff 80",
                                "quant_values_prefix": [0, 0, 0, 0, 1, 1, 1, 1],
                                "quant_counts": [84, 88, 84],
                                "dequant_rule": "value = fp16(d) * (ternary_quant - 1)",
                                "dequant_values_prefix": [-1, -1, -1, -1, 0, 0, 0, 0],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": 0,
                                    "min": -1,
                                    "max": 1,
                                    "mean": 0,
                                },
                            },
                            {
                                "type_name": "tq2_0",
                                "d_fp32": 1,
                                "packed_quant_bytes_prefix": "24 24 24 24",
                                "quant_values_prefix": [0, 0, 0, 0, 1, 1, 1, 1],
                                "quant_counts": [128, 64, 64, 0],
                                "dequant_rule": "value = fp16(d) * (two_bit_quant - 1)",
                                "dequant_values_prefix": [-1, -1, -1, -1, 0, 0, 0, 0],
                                "dequant_stats": {
                                    "value_count": 256,
                                    "checksum": -64,
                                    "min": -1,
                                    "max": 1,
                                    "mean": -0.25,
                                },
                            },
                        ],
                        "tensors": [],
                    }
                },
            )

            summary = build_summary(root)
            sections = {section["title"]: section for section in summary["sections"]}
            metrics = {
                metric["name"]: metric["value"]
                for metric in sections["GGUF Parser"]["metrics"]
            }

            self.assertEqual(metrics["source"], "file")
            self.assertEqual(metrics["f32_raw_values_hex_prefix"], "00 00 80 3f 00 00 80 bf")
            self.assertEqual(metrics["f32_raw_values_prefix"], [1065353216, 3212836864])
            self.assertEqual(
                metrics["f32_decode_rule"],
                "value = little_endian_float32(raw_4_bytes)",
            )
            self.assertEqual(metrics["f32_values_prefix"], [1, -1])
            self.assertEqual(metrics["f32_stats"]["checksum"], 0)
            self.assertEqual(metrics["f32_stats"]["mean"], 0)
            self.assertEqual(
                metrics["f64_raw_values_hex_prefix"],
                "00 00 00 00 00 00 f0 3f 00 00 00 00 00 00 f0 bf",
            )
            self.assertEqual(
                metrics["f64_raw_values_prefix"],
                [4607182418800017408, 13830554455654793216],
            )
            self.assertEqual(
                metrics["f64_decode_rule"],
                "value = little_endian_float64(raw_8_bytes)",
            )
            self.assertEqual(metrics["f64_values_prefix"], [1, -1])
            self.assertEqual(metrics["f64_stats"]["checksum"], 0)
            self.assertEqual(metrics["f64_stats"]["mean"], 0)
            self.assertEqual(
                metrics["i64_raw_values_hex_prefix"],
                "01 00 00 00 00 00 00 00 ff ff ff ff ff ff ff ff 15 cd 5b 07 00 00 00 00 eb 32 a4 f8 ff ff ff ff",
            )
            self.assertEqual(
                metrics["i64_raw_values_prefix"],
                [1, 18446744073709551615, 123456789, 18446744073586094827],
            )
            self.assertEqual(
                metrics["i64_decode_rule"],
                "value = int64_t(little_endian_raw_8_bytes)",
            )
            self.assertEqual(
                metrics["i64_values_prefix"], [1, -1, 123456789, -123456789]
            )
            self.assertEqual(metrics["i64_stats"]["checksum"], 0)
            self.assertEqual(metrics["i64_stats"]["min"], -123456789)
            self.assertEqual(metrics["i64_stats"]["max"], 123456789)
            self.assertEqual(metrics["i64_stats"]["mean"], 0)
            self.assertEqual(
                metrics["i32_raw_values_hex_prefix"],
                "01 00 00 00 ff ff ff ff 40 e2 01 00 c0 1d fe ff",
            )
            self.assertEqual(
                metrics["i32_raw_values_prefix"],
                [1, 4294967295, 123456, 4294843840],
            )
            self.assertEqual(
                metrics["i32_decode_rule"],
                "value = int32_t(little_endian_raw_4_bytes)",
            )
            self.assertEqual(metrics["i32_values_prefix"], [1, -1, 123456, -123456])
            self.assertEqual(metrics["i32_stats"]["checksum"], 0)
            self.assertEqual(metrics["i32_stats"]["min"], -123456)
            self.assertEqual(metrics["i32_stats"]["max"], 123456)
            self.assertEqual(metrics["i32_stats"]["mean"], 0)
            self.assertEqual(
                metrics["i16_raw_values_hex_prefix"], "01 00 ff ff d2 04 2e fb"
            )
            self.assertEqual(metrics["i16_raw_values_prefix"], [1, 65535, 1234, 64302])
            self.assertEqual(
                metrics["i16_decode_rule"],
                "value = int16_t(little_endian_raw_2_bytes)",
            )
            self.assertEqual(metrics["i16_values_prefix"], [1, -1, 1234, -1234])
            self.assertEqual(metrics["i16_stats"]["checksum"], 0)
            self.assertEqual(metrics["i16_stats"]["min"], -1234)
            self.assertEqual(metrics["i16_stats"]["max"], 1234)
            self.assertEqual(metrics["i16_stats"]["mean"], 0)
            self.assertEqual(metrics["i8_raw_values_hex_prefix"], "01 ff 64 9c")
            self.assertEqual(metrics["i8_raw_values_prefix"], [1, 255, 100, 156])
            self.assertEqual(metrics["i8_decode_rule"], "value = int8_t(raw_byte)")
            self.assertEqual(metrics["i8_values_prefix"], [1, -1, 100, -100])
            self.assertEqual(metrics["i8_stats"]["checksum"], 0)
            self.assertEqual(metrics["i8_stats"]["min"], -100)
            self.assertEqual(metrics["i8_stats"]["max"], 100)
            self.assertEqual(metrics["i8_stats"]["mean"], 0)
            self.assertEqual(metrics["f16_raw_values_hex_prefix"], "00 3c 00 40 00 c1 00 00")
            self.assertEqual(metrics["f16_raw_values_prefix"], [15360, 16384, 49408, 0])
            self.assertEqual(
                metrics["f16_decode_rule"],
                "value = gguf_fp16_to_float32(raw_half)",
            )
            self.assertEqual(metrics["f16_values_prefix"], [1, 2, -2.5, 0])
            self.assertEqual(metrics["f16_stats"]["checksum"], 0.5)
            self.assertEqual(metrics["f16_stats"]["mean"], 0.125)
            self.assertEqual(metrics["q2_k_scale_min_bytes_prefix"], "00 11 22 33")
            self.assertEqual(metrics["q2_k_scale_min_pairs"][1]["effective_min"], 7.5)
            self.assertEqual(metrics["q2_k_dequant_group1_values_prefix"], [-0.5, 0.5])
            self.assertEqual(metrics["q2_k_dequant_group_sums"][0:3], [0, 16, 32])
            self.assertEqual(metrics["q2_k_dequant_stats"]["checksum"], 1184)
            self.assertEqual(metrics["q3_k_high_mask_bytes_prefix"], "00 ff ff ff")
            self.assertEqual(metrics["q3_k_scales"][1]["effective_scale"], 15)
            self.assertEqual(metrics["q3_k_dequant_group1_values_prefix"], [0, 3])
            self.assertEqual(metrics["q3_k_dequant_group_sums"][0:3], [0, 24, 40])
            self.assertEqual(metrics["q3_k_dequant_stats"]["checksum"], 1920)
            self.assertEqual(metrics["q4_0_packed_quant_bytes_prefix"], "f0 e1 d2 c3")
            self.assertEqual(metrics["q4_0_quant_values_prefix"], [0, 1, 2, 3, 4, 5])
            self.assertEqual(metrics["q4_0_dequant_values_prefix"], [-8, -7, -6, -5, -4, -3])
            self.assertEqual(metrics["q4_0_dequant_stats"]["checksum"], -16)
            self.assertEqual(metrics["q4_1_m_fp32"], 0.5)
            self.assertEqual(metrics["q4_1_quant_values_prefix"], [0, 1, 2, 3, 4, 5])
            self.assertEqual(metrics["q4_1_dequant_values_prefix"], [0.5, 1.5, 2.5, 3.5, 4.5, 5.5])
            self.assertEqual(metrics["q4_1_dequant_stats"]["checksum"], 256)
            self.assertEqual(metrics["q5_0_high_bits_bytes_prefix"], "00 ff 55 aa")
            self.assertEqual(metrics["q5_0_quant_values_prefix"], [0, 1, 2, 3, 4, 5])
            self.assertEqual(metrics["q5_0_dequant_values_prefix"], [-16, -15, -14, -13, -12, -11])
            self.assertEqual(metrics["q5_0_dequant_stats"]["checksum"], -16)
            self.assertEqual(metrics["q5_1_m_fp32"], 0.5)
            self.assertEqual(metrics["q5_1_high_bits_bytes_prefix"], "00 ff 55 aa")
            self.assertEqual(metrics["q5_1_quant_values_prefix"], [0, 1, 2, 3, 4, 5])
            self.assertEqual(metrics["q5_1_dequant_values_prefix"], [0.5, 1.5, 2.5, 3.5, 4.5, 5.5])
            self.assertEqual(metrics["q5_1_dequant_stats"]["checksum"], 512)
            self.assertEqual(metrics["q4_k_scale_min_pairs"][0]["effective_min"], 2)
            self.assertEqual(metrics["q4_k_dequant_subblock0_values_prefix"], [-2, -2])
            self.assertEqual(metrics["q4_k_dequant_subblock1_values_prefix"], [-2.5, -2.5])
            self.assertEqual(metrics["q4_k_dequant_values_prefix"], [-2, -2, -2.5])
            self.assertEqual(metrics["q4_k_dequant_subblock_sums"][0:2], [-64, -64])
            self.assertEqual(metrics["q4_k_dequant_stats"]["checksum"], 8288)
            self.assertEqual(metrics["q5_k_high_bits_bytes_prefix"], "ff ff ff ff")
            self.assertEqual(metrics["q5_k_scale_min_pairs"][0]["effective_min"], 2)
            self.assertEqual(metrics["q5_k_dequant_subblock1_values_prefix"], [13.5, 13.5])
            self.assertEqual(metrics["q5_k_dequant_subblock_sums"][0:2], [-64, 448])
            self.assertEqual(metrics["q5_k_dequant_stats"]["checksum"], 30816)
            self.assertEqual(metrics["q6_k_high_bits_bytes_prefix"], "ff ff ff ff")
            self.assertEqual(metrics["q6_k_scales"][1]["effective_scale"], 15)
            self.assertEqual(metrics["q6_k_dequant_group1_values_prefix"], [16, 17])
            self.assertEqual(metrics["q6_k_dequant_group_sums"][0:2], [0, 376])
            self.assertEqual(metrics["q6_k_dequant_stats"]["checksum"], 41440)
            self.assertEqual(metrics["q8_0_quant_values_prefix"], [-16, -15, -14])
            self.assertEqual(metrics["q8_0_dequant_values_prefix"], [-16, -15, -14])
            self.assertEqual(metrics["q8_0_dequant_stats"]["checksum"], -16)
            self.assertEqual(metrics["q8_0_dequant_stats"]["mean"], -0.5)
            self.assertEqual(metrics["q8_1_d_fp32"], 0.5)
            self.assertEqual(metrics["q8_1_s_fp32"], -8)
            self.assertEqual(metrics["q8_1_expected_s_from_quant_sum"], -8)
            self.assertEqual(metrics["q8_1_quant_values_prefix"], [-16, -15, -14])
            self.assertEqual(metrics["q8_1_dequant_values_prefix"], [-8, -7.5, -7])
            self.assertEqual(metrics["q8_1_dequant_stats"]["checksum"], -8)
            self.assertEqual(metrics["q8_1_dequant_stats"]["mean"], -0.25)
            self.assertEqual(metrics["q8_k_d_fp32"], 0.25)
            self.assertEqual(metrics["q8_k_quant_values_prefix"], [-128, -127, -126])
            self.assertEqual(metrics["q8_k_block_sums_prefix"], [-1928, -1672, -1416])
            self.assertEqual(metrics["q8_k_block_sum_bytes_prefix"], "78 f8 78 f9")
            self.assertEqual(metrics["q8_k_dequant_values_prefix"], [-32, -31.75, -31.5])
            self.assertEqual(metrics["q8_k_quant_sum_first_block"], -128)
            self.assertEqual(metrics["q8_k_dequant_stats"]["checksum"], -32)
            self.assertEqual(metrics["q8_k_dequant_stats"]["stored_block_sum_total"], -128)
            self.assertEqual(metrics["q1_0_d_fp32"], 1)
            self.assertEqual(metrics["q1_0_packed_bit_bytes_prefix"], "aa aa aa aa")
            self.assertEqual(metrics["q1_0_bit_values_prefix"], [0, 1, 0, 1])
            self.assertEqual(metrics["q1_0_dequant_values_prefix"], [-1, 1, -1, 1])
            self.assertEqual(metrics["q1_0_dequant_stats"]["positive_bit_count"], 64)
            self.assertEqual(metrics["q1_0_dequant_stats"]["negative_bit_count"], 64)
            self.assertEqual(
                metrics["iq2_xxs_raw_bytes_prefix"],
                "00 3c 01 02 03 04 01 c1 80 10 05 06 07 08 05 c3 01 21",
            )
            self.assertEqual(metrics["iq2_xxs_d_fp32"], 1)
            self.assertEqual(
                metrics["iq2_xxs_qs_words_prefix"],
                [513, 1027, 49409, 4224, 1541, 2055, 49925, 8449],
            )
            self.assertEqual(
                metrics["iq2_xxs_grid_indices_prefix"],
                [1, 2, 3, 4, 5, 6, 7, 8],
            )
            self.assertEqual(
                metrics["iq2_xxs_sign_codes_prefix"],
                [1, 2, 3, 4, 5, 6, 7, 8],
            )
            self.assertEqual(
                metrics["iq2_xxs_scale_nibbles"], [1, 2, 3, 4, 5, 6, 7, 8]
            )
            self.assertEqual(
                metrics["iq2_xxs_effective_scales"],
                [0.375, 0.625, 0.875, 1.125, 1.375, 1.625, 1.875, 2.125],
            )
            self.assertIn("iq2xxs_grid", metrics["iq2_xxs_decode_rule"])
            self.assertEqual(metrics["iq4_nl_d_fp32"], 0.5)
            self.assertEqual(metrics["iq4_nl_packed_quant_bytes_prefix"], "f0 e1 d2 c3")
            self.assertEqual(metrics["iq4_nl_quant_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["iq4_nl_codebook_table"][0:3], [-127, -104, -83])
            self.assertEqual(metrics["iq4_nl_codebook_values_prefix"], [-127, -104, -83])
            self.assertEqual(
                metrics["iq4_nl_dequant_rule"],
                "value = fp16(d) * iq4nl_codebook[q4_index]",
            )
            self.assertEqual(metrics["iq4_nl_dequant_values_prefix"], [-63.5, -52, -41.5])
            self.assertEqual(metrics["iq4_nl_dequant_stats"]["checksum"], -94)
            self.assertEqual(metrics["iq4_nl_dequant_stats"]["mean"], -2.9375)
            self.assertEqual(metrics["mxfp4_e"], 128)
            self.assertEqual(metrics["mxfp4_e_hex"], "80")
            self.assertEqual(metrics["mxfp4_e_scale_fp32"], 1)
            self.assertEqual(metrics["mxfp4_packed_quant_bytes_prefix"], "f0 e1 d2 c3")
            self.assertEqual(metrics["mxfp4_quant_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["mxfp4_codebook_table"][0:8], [0, 1, 2, 3, 4, 6, 8, 12])
            self.assertEqual(metrics["mxfp4_codebook_values_prefix"], [0, 1, 2])
            self.assertEqual(
                metrics["mxfp4_dequant_rule"],
                "value = ggml_e8m0_to_fp32_half(e) * kvalues_mxfp4[q4_index]",
            )
            self.assertEqual(metrics["mxfp4_dequant_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["mxfp4_dequant_stats"]["checksum"], 0)
            self.assertEqual(metrics["mxfp4_dequant_stats"]["max"], 12)
            self.assertEqual(metrics["nvfp4_scale_bytes_prefix"], "38 40 30 48")
            self.assertEqual(metrics["nvfp4_scales"][0]["scale_fp32"], 0.5)
            self.assertEqual(metrics["nvfp4_scales"][1]["raw_scale"], 64)
            self.assertEqual(metrics["nvfp4_packed_quant_bytes_prefix"], "f0 e1 d2 c3")
            self.assertEqual(metrics["nvfp4_quant_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["nvfp4_codebook_table"][0:8], [0, 1, 2, 3, 4, 6, 8, 12])
            self.assertEqual(metrics["nvfp4_codebook_values_prefix"], [0, 1, 2])
            self.assertEqual(
                metrics["nvfp4_dequant_rule"],
                "value = ggml_ue4m3_to_fp32(scale_byte) * kvalues_mxfp4[q4_index]",
            )
            self.assertEqual(metrics["nvfp4_dequant_subblock0_values_prefix"], [0, 0.5, 1])
            self.assertEqual(metrics["nvfp4_dequant_subblock1_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["nvfp4_dequant_values_prefix"], [0, 0.5, 1])
            self.assertEqual(metrics["nvfp4_dequant_subblock_sums"], [0, 0, 0, 0])
            self.assertEqual(metrics["nvfp4_dequant_stats"]["checksum"], 0)
            self.assertEqual(metrics["nvfp4_dequant_stats"]["max"], 24)
            self.assertEqual(metrics["bf16_raw_values_hex_prefix"], "80 3f 00 40 20 c0 00 00")
            self.assertEqual(metrics["bf16_raw_values_prefix"], [16256, 16384, 49184, 0])
            self.assertEqual(
                metrics["bf16_decode_rule"],
                "value = reinterpret_float32(uint32_t(raw_bf16) << 16)",
            )
            self.assertEqual(metrics["bf16_values_prefix"], [1, 2, -2.5, 0])
            self.assertEqual(metrics["bf16_stats"]["checksum"], 0.5)
            self.assertEqual(metrics["bf16_stats"]["mean"], 0.125)
            self.assertEqual(metrics["iq4_xs_d_fp32"], 1)
            self.assertEqual(metrics["iq4_xs_scales_h"], 43690)
            self.assertEqual(metrics["iq4_xs_scales_h_hex"], "aa aa")
            self.assertEqual(metrics["iq4_xs_scales_l_bytes_prefix"], "21 43 65 87")
            self.assertEqual(metrics["iq4_xs_packed_quant_bytes_prefix"], "f0 e1 d2 c3")
            self.assertEqual(metrics["iq4_xs_scales"][0]["raw_scale"], 33)
            self.assertEqual(metrics["iq4_xs_scales"][1]["signed_scale"], 2)
            self.assertEqual(metrics["iq4_xs_quant_values_prefix"], [0, 1, 2])
            self.assertEqual(metrics["iq4_xs_codebook_table"][0:3], [-127, -104, -83])
            self.assertEqual(metrics["iq4_xs_codebook_values_prefix"], [-127, -104, -83])
            self.assertEqual(
                metrics["iq4_xs_dequant_rule"],
                "value = fp16(d) * (scale6 - 32) * iq4nl_codebook[q4_index]",
            )
            self.assertEqual(metrics["iq4_xs_dequant_subblock0_values_prefix"], [-127, -104, -83])
            self.assertEqual(metrics["iq4_xs_dequant_subblock1_values_prefix"], [-254, -208, -166])
            self.assertEqual(metrics["iq4_xs_dequant_subblock_sums"][0:3], [-188, -376, -564])
            self.assertEqual(metrics["iq4_xs_dequant_stats"]["checksum"], -6768)
            self.assertEqual(metrics["iq4_xs_dequant_stats"]["mean"], -26.4375)
            self.assertEqual(metrics["tq1_0_d_fp32"], 1)
            self.assertEqual(metrics["tq1_0_packed_qs_bytes_prefix"], "00 00 00 00")
            self.assertEqual(metrics["tq1_0_packed_qh_bytes_prefix"], "00 80 ff 80")
            self.assertEqual(metrics["tq1_0_quant_values_prefix"], [0, 0, 0, 0, 1, 1, 1, 1])
            self.assertEqual(metrics["tq1_0_quant_counts"], [84, 88, 84])
            self.assertEqual(
                metrics["tq1_0_dequant_rule"],
                "value = fp16(d) * (ternary_quant - 1)",
            )
            self.assertEqual(metrics["tq1_0_dequant_values_prefix"], [-1, -1, -1, -1, 0, 0, 0, 0])
            self.assertEqual(metrics["tq1_0_dequant_stats"]["checksum"], 0)
            self.assertEqual(metrics["tq1_0_dequant_stats"]["mean"], 0)
            self.assertEqual(metrics["tq2_0_d_fp32"], 1)
            self.assertEqual(metrics["tq2_0_packed_quant_bytes_prefix"], "24 24 24 24")
            self.assertEqual(metrics["tq2_0_quant_values_prefix"], [0, 0, 0, 0, 1, 1, 1, 1])
            self.assertEqual(metrics["tq2_0_quant_counts"], [128, 64, 64, 0])
            self.assertEqual(
                metrics["tq2_0_dequant_rule"],
                "value = fp16(d) * (two_bit_quant - 1)",
            )
            self.assertEqual(metrics["tq2_0_dequant_values_prefix"], [-1, -1, -1, -1, 0, 0, 0, 0])
            self.assertEqual(metrics["tq2_0_dequant_stats"]["checksum"], -64)
            self.assertEqual(metrics["tq2_0_dequant_stats"]["mean"], -0.25)


if __name__ == "__main__":
    unittest.main()
