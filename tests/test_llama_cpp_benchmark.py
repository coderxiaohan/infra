import unittest

from scripts.run_llama_cpp_benchmark import (
    parse_llama_bench_rates,
    parse_llama_summary_rates,
    parse_llama_timings,
)


class LlamaCppBenchmarkParsingTest(unittest.TestCase):
    def test_parse_legacy_timing_output(self):
        output = (
            "llama_print_timings: prompt eval time = 12.34 ms / 8 tokens "
            "(1.54 ms per token, 648.30 tokens per second)\n"
            "llama_print_timings: eval time = 45.67 ms / 16 tokens "
            "(2.85 ms per token, 350.34 tokens per second)\n"
        )

        timings = parse_llama_timings(output)

        self.assertEqual(timings["prompt_eval_time"]["tokens"], 8)
        self.assertEqual(timings["prompt_eval_time"]["tokens_per_second"], 648.30)
        self.assertEqual(timings["eval_time"]["tokens"], 16)
        self.assertEqual(timings["eval_time"]["tokens_per_second"], 350.34)

    def test_parse_current_summary_rate_output(self):
        rates = parse_llama_summary_rates("[ Prompt: 2307.9 t/s | Generation: 220.1 t/s ]")

        self.assertEqual(rates["prompt_eval_tokens_per_s"], 2307.9)
        self.assertEqual(rates["eval_tokens_per_s"], 220.1)

    def test_parse_llama_bench_markdown_output(self):
        output = """
| model       | size | params | backend | threads | test | t/s |
| ----------- | ---: | -----: | ------- | ------: | ---: | --: |
| gpt2 ?B F16 | 158M |    82M | CPU     |       4 | pp16 | 1967.99 +/- 107.21 |
| gpt2 ?B F16 | 158M |    82M | CPU     |       4 | tg8  | 259.02 +/- 13.65 |
"""

        rates = parse_llama_bench_rates(output)

        self.assertEqual(rates["prompt_eval_tokens"], 16)
        self.assertEqual(rates["prompt_eval_tokens_per_s"], 1967.99)
        self.assertEqual(rates["eval_tokens"], 8)
        self.assertEqual(rates["eval_tokens_per_s"], 259.02)


if __name__ == "__main__":
    unittest.main()
