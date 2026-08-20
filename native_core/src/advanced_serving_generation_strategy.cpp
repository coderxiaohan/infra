#include "inferlab/advanced_serving_generation_strategy.hpp"

#include <algorithm>

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {

SpeculativeDecodeResult estimate_speculative_decode(
    const SpeculativeDecodeScenario& scenario) {
  const int output_tokens = std::max(0, scenario.output_tokens);
  const int verify_window = std::max(1, scenario.verify_window);
  const double acceptance_rate =
      advanced_serving_internal::finite_unit_interval(
          scenario.acceptance_rate, "speculative acceptance rate");
  const double draft_tps =
      advanced_serving_internal::positive_finite_or_default(
          scenario.draft_tokens_per_second, 2500.0,
          "draft-model tokens per second");
  const double target_tps =
      advanced_serving_internal::positive_finite_or_default(
          scenario.target_tokens_per_second, 700.0,
          "target-model tokens per second");
  const double verify_ms =
      advanced_serving_internal::non_negative_finite_or_zero(
          scenario.target_verify_ms_per_token,
          "target verify milliseconds per token");
  const double round_overhead_ms =
      advanced_serving_internal::non_negative_finite_or_zero(
          scenario.round_overhead_ms,
          "speculative round overhead milliseconds");

  SpeculativeDecodeResult result;
  result.scenario = scenario;
  result.scenario.output_tokens = output_tokens;
  result.scenario.verify_window = verify_window;
  result.scenario.acceptance_rate = acceptance_rate;
  result.scenario.draft_tokens_per_second = draft_tps;
  result.scenario.target_tokens_per_second = target_tps;
  result.scenario.target_verify_ms_per_token = verify_ms;
  result.scenario.round_overhead_ms = round_overhead_ms;
  const double draft_ms_per_token =
      advanced_serving_internal::require_finite(
          1000.0 / draft_tps,
          "speculative draft milliseconds per token");
  const double target_ms_per_token =
      advanced_serving_internal::require_finite(
          1000.0 / target_tps,
          "speculative target milliseconds per token");
  const double round_overhead_per_draft_token =
      advanced_serving_internal::require_finite(
          round_overhead_ms / static_cast<double>(verify_window),
          "speculative round overhead per draft token");
  const double speculative_cost_per_draft_token =
      internal::checked_add_finite_double(
          std::max(draft_ms_per_token, verify_ms),
          round_overhead_per_draft_token,
          "speculative cost per draft token");
  result.break_even_acceptance_rate =
      advanced_serving_internal::finite_unit_interval(
          advanced_serving_internal::require_finite(
              speculative_cost_per_draft_token / target_ms_per_token,
              "speculative break-even acceptance rate"),
          "speculative break-even acceptance rate");
  result.target_only_ms =
      output_tokens == 0
          ? 0.0
          : advanced_serving_internal::require_finite(
                static_cast<double>(output_tokens) * 1000.0 / target_tps,
                "target-only decode milliseconds");

  if (output_tokens == 0) {
    result.speedup = 1.0;
    return result;
  }
  if (acceptance_rate == 0.0) {
    result.speculative_ms = result.target_only_ms;
    result.speedup = 1.0;
    result.tokens_per_second = target_tps;
    return result;
  }

  const double accepted_per_round =
      advanced_serving_internal::finite_product(
          static_cast<double>(verify_window), acceptance_rate,
          "accepted speculative tokens per round");
  const int rounds = internal::checked_positive_ceil_double_to_int(
      static_cast<double>(output_tokens) / accepted_per_round,
      "speculative decode rounds");
  const int draft_tokens = internal::checked_multiply_int(
      rounds, verify_window, "speculative draft tokens");
  const double accepted_tokens_estimate =
      advanced_serving_internal::finite_product(
          static_cast<double>(draft_tokens), acceptance_rate,
          "speculative accepted-token estimate");
  const int accepted_tokens = std::min(
      output_tokens, internal::checked_non_negative_round_double_to_int(
                         accepted_tokens_estimate,
                         "speculative accepted tokens"));
  const double draft_ms = advanced_serving_internal::require_finite(
      static_cast<double>(draft_tokens) * 1000.0 / draft_tps,
      "speculative draft milliseconds");
  const double target_verify_ms =
      advanced_serving_internal::finite_product(
          static_cast<double>(draft_tokens), verify_ms,
          "speculative target verify milliseconds");
  const double total_round_overhead_ms =
      advanced_serving_internal::finite_product(
          static_cast<double>(rounds), result.scenario.round_overhead_ms,
          "speculative round overhead milliseconds");
  const double speculative_ms =
      advanced_serving_internal::require_finite(
          std::max(draft_ms, target_verify_ms) + total_round_overhead_ms,
          "speculative decode milliseconds");

  result.target_verify_rounds = rounds;
  result.draft_tokens = draft_tokens;
  result.accepted_tokens = accepted_tokens;
  result.rejected_tokens = draft_tokens - accepted_tokens;
  result.speculative_ms = speculative_ms;
  result.speedup = advanced_serving_internal::require_finite(
      result.target_only_ms / speculative_ms, "speculative decode speedup");
  result.tokens_per_second = advanced_serving_internal::require_finite(
      static_cast<double>(output_tokens) * 1000.0 / speculative_ms,
      "speculative tokens per second");
  return result;
}

std::vector<SpeculativeDecodeScenario> default_speculative_decode_scenarios() {
  return {
      {"chat_acceptance_high", 256, 4, 0.82, 2600.0, 700.0, 0.32, 0.15},
      {"coding_acceptance_medium", 512, 6, 0.64, 2200.0, 620.0, 0.40, 0.15},
      {"reasoning_acceptance_low", 384, 4, 0.42, 1800.0, 540.0, 0.46, 0.15},
      {"wide_window_experiment", 768, 8, 0.58, 2400.0, 650.0, 0.38, 0.15},
  };
}

}  // namespace inferlab
