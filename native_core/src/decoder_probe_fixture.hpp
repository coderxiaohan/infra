#pragma once

#include "inferlab/decoder_block.hpp"

#include "checked_index.hpp"

#include <cstddef>
#include <stdexcept>

namespace inferlab::probe {

// Keep deterministic CPU-reference fixtures bounded before any large vector is
// allocated; runtime intermediates are validated separately by decoder core.
inline constexpr std::size_t kMaxDecoderProbeFixtureElements = 33554432U;

struct DecoderProbeFixturePlan {
  std::size_t input_elements = 0;
  std::size_t weight_elements = 0;
  std::size_t total_elements = 0;
};

inline DecoderProbeFixturePlan plan_decoder_probe_fixture(
    const int hidden_size, const int ffn_size, const int input_rows) {
  if (hidden_size <= 0 || ffn_size <= 0 || input_rows <= 0) {
    throw std::invalid_argument(
        "decoder probe fixture dimensions must be positive");
  }
  const auto hidden_square = internal::checked_product_size(
      hidden_size, hidden_size, "decoder probe hidden-square weights");
  const auto hidden_ffn = internal::checked_product_size(
      hidden_size, ffn_size, "decoder probe FFN weights");
  const auto attention_weights = internal::checked_product_size(
      hidden_square, 4, "decoder probe attention weights");
  const auto ffn_weights = internal::checked_product_size(
      hidden_ffn, 3, "decoder probe FFN weights");
  const auto gamma_elements = internal::checked_product_size(
      static_cast<std::size_t>(hidden_size), 2,
      "decoder probe normalization weights");
  const auto weight_elements = internal::checked_add_size(
      internal::checked_add_size(attention_weights, ffn_weights,
                                 "decoder probe weights"),
      gamma_elements, "decoder probe weights");
  const auto input_elements = internal::checked_product_size(
      input_rows, hidden_size, "decoder probe inputs");
  const auto total_elements = internal::checked_add_size(
      weight_elements, input_elements, "decoder probe fixture elements");
  if (total_elements > kMaxDecoderProbeFixtureElements) {
    throw std::invalid_argument(
        "decoder probe fixture exceeds reference-probe element cap");
  }
  return DecoderProbeFixturePlan{
      input_elements,
      weight_elements,
      total_elements,
  };
}

template <typename WeightBuilder>
DecoderBlockWeights build_decoder_probe_weights(
    const int hidden_size, const int ffn_size,
    const WeightBuilder& build_weight) {
  (void)plan_decoder_probe_fixture(hidden_size, ffn_size, 1);
  const auto checked_build = [&](const int rows, const int cols,
                                 const int seed) {
    auto weight = build_weight(rows, cols, seed);
    const auto expected = internal::checked_product_size(
        rows, cols, "decoder probe generated weight");
    if (weight.size() != expected) {
      throw std::invalid_argument(
          "decoder probe weight builder returned an invalid shape");
    }
    return weight;
  };
  DecoderBlockWeights weights;
  weights.rms_gamma.assign(static_cast<std::size_t>(hidden_size), 1.0F);
  weights.q_weight = checked_build(hidden_size, hidden_size, 1);
  weights.k_weight = checked_build(hidden_size, hidden_size, 3);
  weights.v_weight = checked_build(hidden_size, hidden_size, 5);
  weights.o_weight = checked_build(hidden_size, hidden_size, 7);
  weights.post_attention_rms_gamma.assign(
      static_cast<std::size_t>(hidden_size), 1.0F);
  weights.gate_weight = checked_build(hidden_size, ffn_size, 9);
  weights.up_weight = checked_build(hidden_size, ffn_size, 11);
  weights.down_weight = checked_build(ffn_size, hidden_size, 13);
  return weights;
}

}  // namespace inferlab::probe
