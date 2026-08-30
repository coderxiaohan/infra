import unittest

from scripts.build_trtllm_showcase_engine import (
    expected_token_profile,
    validate_config,
    validate_engine_config,
)


def valid_config():
    return {
        "schema_version": 1,
        "model": {
            "name": "distilgpt2",
            "path": "models/distilgpt2",
            "dtype": "float16",
        },
        "engine": {
            "output_dir": "results/engine",
            "workspace": "results/workspace",
            "tensor_parallel_size": 1,
            "pipeline_parallel_size": 1,
            "max_input_len": 64,
            "max_seq_len": 96,
            "opt_batch_size": 1,
            "max_batch_size": 1,
            "opt_num_tokens": 16,
            "max_num_tokens": 64,
            "multiple_profiles": False,
            "use_paged_context_fmha": True,
        },
    }


class TensorRTLLMShowcaseEngineTest(unittest.TestCase):
    def test_config_produces_dynamic_token_profile(self):
        config = validate_config(valid_config())

        self.assertEqual(expected_token_profile(config), [1, 16, 64])

    def test_multiple_profiles_must_be_disabled(self):
        config = valid_config()
        config["engine"]["multiple_profiles"] = True

        with self.assertRaisesRegex(ValueError, "must be false"):
            validate_config(config)

    def test_optimum_tokens_must_not_exceed_maximum(self):
        config = valid_config()
        config["engine"]["opt_num_tokens"] = 65

        with self.assertRaisesRegex(ValueError, "opt_num_tokens"):
            validate_config(config)

    def test_engine_contract_accepts_requested_dynamic_config(self):
        config = valid_config()
        engine_payload = {
            "build_config": {
                "max_input_len": 64,
                "max_seq_len": 96,
                "opt_batch_size": 1,
                "max_batch_size": 1,
                "opt_num_tokens": 16,
                "max_num_tokens": 64,
                "plugin_config": {
                    "multiple_profiles": False,
                    "use_paged_context_fmha": True,
                },
            }
        }

        self.assertEqual(validate_engine_config(engine_payload, config), [])

    def test_engine_contract_rejects_fixed_multiple_profiles(self):
        config = valid_config()
        engine_payload = {
            "build_config": {
                "max_input_len": 64,
                "max_seq_len": 96,
                "opt_batch_size": 1,
                "max_batch_size": 1,
                "opt_num_tokens": 16,
                "max_num_tokens": 64,
                "plugin_config": {
                    "multiple_profiles": True,
                    "use_paged_context_fmha": True,
                },
            }
        }

        errors = validate_engine_config(engine_payload, config)

        self.assertIn("plugin_config.multiple_profiles must be false", errors)


if __name__ == "__main__":
    unittest.main()
