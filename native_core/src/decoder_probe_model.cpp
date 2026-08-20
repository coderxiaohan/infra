#include "decoder_probe_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace inferlab::decoder_probe {
namespace {

struct Execution {
  probe::DecoderProbeFixturePlan fixture;
  DecoderBlockOutput attention_only;
  DecoderBlockOutput output;
};

bool same_fixture(const probe::DecoderProbeFixturePlan& lhs,
                  const probe::DecoderProbeFixturePlan& rhs) {
  return lhs.input_elements == rhs.input_elements &&
         lhs.weight_elements == rhs.weight_elements &&
         lhs.total_elements == rhs.total_elements;
}

bool same_trace(const DecoderBlockTrace& lhs,
                const DecoderBlockTrace& rhs) {
  return lhs.seq_len == rhs.seq_len &&
         lhs.hidden_size == rhs.hidden_size &&
         lhs.num_heads == rhs.num_heads &&
         lhs.head_dim == rhs.head_dim &&
         lhs.ffn_intermediate_size == rhs.ffn_intermediate_size &&
         lhs.input_elements == rhs.input_elements &&
         lhs.projected_elements == rhs.projected_elements &&
         lhs.attention_score_elements == rhs.attention_score_elements &&
         lhs.ffn_elements == rhs.ffn_elements &&
         lhs.output_elements == rhs.output_elements &&
         lhs.normalized_checksum == rhs.normalized_checksum &&
         lhs.query_checksum == rhs.query_checksum &&
         lhs.attention_checksum == rhs.attention_checksum &&
         lhs.attention_residual_checksum ==
             rhs.attention_residual_checksum &&
         lhs.post_attention_norm_checksum ==
             rhs.post_attention_norm_checksum &&
         lhs.ffn_checksum == rhs.ffn_checksum &&
         lhs.output_checksum == rhs.output_checksum;
}

bool same_output(const DecoderBlockOutput& lhs,
                 const DecoderBlockOutput& rhs) {
  return lhs.hidden_states == rhs.hidden_states &&
         same_trace(lhs.trace, rhs.trace);
}

void validate_config(const DecoderBlockConfig& config) {
  if (config.seq_len <= 0 || config.hidden_size <= 0 ||
      config.num_heads <= 0 || config.ffn_intermediate_size <= 0 ||
      !std::isfinite(config.rope_base) || config.rope_base <= 0.0F ||
      !std::isfinite(config.rms_epsilon) ||
      config.rms_epsilon <= 0.0F) {
    throw std::invalid_argument(
        "decoder probe dimensions and constants must be positive");
  }
  if (config.hidden_size % config.num_heads != 0) {
    throw std::invalid_argument(
        "hidden-size must be divisible by heads");
  }
}

std::vector<float> build_input(const int seq_len,
                               const int hidden_size) {
  std::vector<float> input(
      inferlab_plan::checked_element_count(
          "decoder input", seq_len, hidden_size));
  for (int token = 0; token < seq_len; ++token) {
    for (int hidden = 0; hidden < hidden_size; ++hidden) {
      const int centered = ((token * 19 + hidden * 7) % 17) - 8;
      input[internal::checked_flat_index(
          token, hidden_size, hidden, "decoder input")] =
          static_cast<float>(centered) / 8.0F;
    }
  }
  return input;
}

std::vector<float> build_weight(const int rows, const int cols,
                                const int seed) {
  std::vector<float> weights(
      inferlab_plan::checked_element_count(
          "decoder weight", rows, cols));
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float diagonal = row == col ? 0.75F : 0.0F;
      const int centered = ((row * 11 + col * 5 + seed) % 13) - 6;
      weights[internal::checked_flat_index(
          row, cols, col, "decoder weight")] =
          diagonal + static_cast<float>(centered) / 80.0F;
    }
  }
  return weights;
}

Execution execute(const DecoderBlockConfig& config) {
  validate_config(config);
  Execution execution;
  execution.fixture = probe::plan_decoder_probe_fixture(
      config.hidden_size, config.ffn_intermediate_size,
      config.seq_len);
  const auto input = build_input(config.seq_len, config.hidden_size);
  const auto weights = probe::build_decoder_probe_weights(
      config.hidden_size, config.ffn_intermediate_size, build_weight);
  execution.attention_only =
      decoder_block_attention_cpu(input, weights, config);
  execution.output = decoder_block_cpu(input, weights, config);
  return execution;
}

}  // namespace

DecoderBlockConfig default_config() {
  DecoderBlockConfig config;
  config.seq_len = 4;
  config.hidden_size = 16;
  config.num_heads = 4;
  config.ffn_intermediate_size = 16;
  return config;
}

Plan build_plan(const DecoderBlockConfig& config) {
  auto execution = execute(config);
  Plan plan;
  plan.config = config;
  plan.fixture = execution.fixture;
  plan.attention_only = std::move(execution.attention_only);
  plan.output = std::move(execution.output);
  plan.output_abs_sum =
      internal::checked_float_vector_abs_sum_as_double(
          plan.output.hidden_states, "decoder probe output");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_config(plan.config);
  const auto expected = execute(plan.config);
  if (!same_fixture(plan.fixture, expected.fixture) ||
      !same_output(plan.attention_only, expected.attention_only) ||
      !same_output(plan.output, expected.output)) {
    throw std::logic_error(
        "decoder probe result does not match canonical execution");
  }
  const double expected_abs_sum =
      internal::checked_float_vector_abs_sum_as_double(
          plan.output.hidden_states, "decoder probe output");
  if (!std::isfinite(plan.output_abs_sum) ||
      plan.output_abs_sum != expected_abs_sum) {
    throw std::logic_error(
        "decoder probe output summary is inconsistent");
  }
}

}  // namespace inferlab::decoder_probe
