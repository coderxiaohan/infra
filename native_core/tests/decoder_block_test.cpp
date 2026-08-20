#include "inferlab/decoder_block.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<float> identity_matrix(const int size) {
  std::vector<float> matrix(static_cast<std::size_t>(size * size), 0.0F);
  for (int i = 0; i < size; ++i) {
    matrix[static_cast<std::size_t>(i * size + i)] = 1.0F;
  }
  return matrix;
}

inferlab::DecoderBlockWeights identity_weights(const int hidden_size) {
  inferlab::DecoderBlockWeights weights;
  weights.rms_gamma.assign(static_cast<std::size_t>(hidden_size), 1.0F);
  weights.q_weight = identity_matrix(hidden_size);
  weights.k_weight = identity_matrix(hidden_size);
  weights.v_weight = identity_matrix(hidden_size);
  weights.o_weight = identity_matrix(hidden_size);
  weights.post_attention_rms_gamma.assign(static_cast<std::size_t>(hidden_size),
                                          1.0F);
  weights.gate_weight = identity_matrix(hidden_size);
  weights.up_weight = identity_matrix(hidden_size);
  weights.down_weight = identity_matrix(hidden_size);
  return weights;
}

inferlab::DecoderBlockWeights zero_ffn_weights(const int hidden_size) {
  auto weights = identity_weights(hidden_size);
  weights.gate_weight.assign(static_cast<std::size_t>(hidden_size * hidden_size),
                             0.0F);
  weights.up_weight.assign(static_cast<std::size_t>(hidden_size * hidden_size),
                           0.0F);
  weights.down_weight.assign(static_cast<std::size_t>(hidden_size * hidden_size),
                             0.0F);
  return weights;
}

void test_decoder_block_outputs_expected_shape() {
  const inferlab::DecoderBlockConfig config{2, 4, 1, 10000.0F, 1e-6F};
  const std::vector<float> input = {
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
  };
  const auto result = inferlab::decoder_block_attention_cpu(
      input, identity_weights(config.hidden_size), config);

  require(result.hidden_states.size() == input.size(),
          "decoder output size mismatch");
  require(result.trace.seq_len == 2, "trace seq_len mismatch");
  require(result.trace.hidden_size == 4, "trace hidden_size mismatch");
  require(result.trace.head_dim == 4, "trace head_dim mismatch");
  require(result.trace.attention_score_elements == 4,
          "trace attention score element count mismatch");
}

void test_causal_path_keeps_first_token_self_only() {
  const inferlab::DecoderBlockConfig config{2, 4, 1, 10000.0F, 1e-6F};
  const std::vector<float> input = {
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
  };
  const auto result = inferlab::decoder_block_attention_cpu(
      input, identity_weights(config.hidden_size), config);
  const float expected_first_value = 1.0F / std::sqrt(0.25F + 1e-6F);

  require(std::fabs(result.hidden_states[0] - expected_first_value) < 1e-5F,
          "first token value should come from its own normalized value");
  require(std::fabs(result.hidden_states[1]) < 1e-5F,
          "first token should not attend to future token");
}

void test_decoder_block_is_deterministic() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F};
  const std::vector<float> input = {
      0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F,
      0.8F, 0.7F, 0.6F, 0.5F, 0.4F, 0.3F, 0.2F, 0.1F,
      1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
  };
  const auto weights = identity_weights(config.hidden_size);
  const auto first = inferlab::decoder_block_attention_cpu(input, weights, config);
  const auto second = inferlab::decoder_block_attention_cpu(input, weights, config);

  require(first.hidden_states == second.hidden_states,
          "decoder output should be deterministic");
  require(std::fabs(first.trace.output_checksum -
                    second.trace.output_checksum) < 1e-6F,
          "decoder checksum should be deterministic");
}

void test_decoder_block_rejects_bad_head_dim() {
  bool threw = false;
  try {
    const inferlab::DecoderBlockConfig config{2, 6, 2, 10000.0F, 1e-6F};
    (void)inferlab::decoder_block_attention_cpu(
        std::vector<float>(12, 1.0F), identity_weights(6), config);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "odd RoPE head_dim did not throw");
}

void test_full_decoder_adds_attention_residual() {
  const inferlab::DecoderBlockConfig config{2, 4, 1, 10000.0F, 1e-6F, 4};
  const std::vector<float> input = {
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
  };
  const auto weights = zero_ffn_weights(config.hidden_size);
  const auto attention =
      inferlab::decoder_block_attention_cpu(input, weights, config);
  const auto full = inferlab::decoder_block_cpu(input, weights, config);

  for (std::size_t i = 0; i < input.size(); ++i) {
    require(std::fabs(full.hidden_states[i] -
                      (input[i] + attention.hidden_states[i])) < 1e-5F,
            "full decoder should include attention residual");
  }
  require(std::fabs(full.trace.ffn_checksum) < 1e-6F,
          "zero FFN should have zero checksum");
}

void test_full_decoder_records_ffn_trace() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F, 12};
  const std::vector<float> input = {
      0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F,
      0.8F, 0.7F, 0.6F, 0.5F, 0.4F, 0.3F, 0.2F, 0.1F,
      1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
  };
  auto weights = identity_weights(config.hidden_size);
  weights.gate_weight.assign(
      static_cast<std::size_t>(config.hidden_size * config.ffn_intermediate_size),
      0.05F);
  weights.up_weight.assign(
      static_cast<std::size_t>(config.hidden_size * config.ffn_intermediate_size),
      0.10F);
  weights.down_weight.assign(
      static_cast<std::size_t>(config.ffn_intermediate_size * config.hidden_size),
      0.02F);

  const auto result = inferlab::decoder_block_cpu(input, weights, config);

  require(result.hidden_states.size() == input.size(),
          "full decoder output size mismatch");
  require(result.trace.ffn_intermediate_size == 12,
          "full decoder ffn size trace mismatch");
  require(result.trace.ffn_elements == 36,
          "full decoder ffn element count mismatch");
  require(std::fabs(result.trace.ffn_checksum) > 1e-6F,
          "full decoder FFN checksum should be nonzero");
}

void test_full_decoder_rejects_bad_ffn_shape() {
  bool threw = false;
  try {
    const inferlab::DecoderBlockConfig config{2, 4, 1, 10000.0F, 1e-6F, 8};
    auto weights = identity_weights(config.hidden_size);
    (void)inferlab::decoder_block_cpu(std::vector<float>(8, 1.0F), weights,
                                      config);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "bad FFN shape did not throw");
}

}  // namespace

int main() {
  try {
    test_decoder_block_outputs_expected_shape();
    test_causal_path_keeps_first_token_self_only();
    test_decoder_block_is_deterministic();
    test_decoder_block_rejects_bad_head_dim();
    test_full_decoder_adds_attention_residual();
    test_full_decoder_records_ffn_trace();
    test_full_decoder_rejects_bad_ffn_shape();
  } catch (const std::exception& error) {
    std::cerr << "decoder_block_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
