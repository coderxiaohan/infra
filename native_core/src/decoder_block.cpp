#include "inferlab/decoder_block.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "checked_index.hpp"
#include "inferlab/attention.hpp"
#include "inferlab/matrix_ops.hpp"
#include "inferlab/normalization.hpp"
#include "inferlab/rope.hpp"

namespace inferlab {
namespace {

bool finite_positive(const float value) {
  return std::isfinite(value) && value > 0.0F;
}

void validate_config(const DecoderBlockConfig& config) {
  if (config.seq_len <= 0 || config.hidden_size <= 0 ||
      config.num_heads <= 0) {
    throw std::invalid_argument("decoder config dimensions must be positive");
  }
  if (config.hidden_size % config.num_heads != 0) {
    throw std::invalid_argument("hidden_size must be divisible by num_heads");
  }
  if ((config.hidden_size / config.num_heads) % 2 != 0) {
    throw std::invalid_argument("decoder head_dim must be even for RoPE");
  }
  if (!finite_positive(config.rope_base)) {
    throw std::invalid_argument("decoder RoPE base must be finite and positive");
  }
  if (!finite_positive(config.rms_epsilon)) {
    throw std::invalid_argument("decoder RMS epsilon must be finite and positive");
  }
}

void validate_full_config(const DecoderBlockConfig& config) {
  validate_config(config);
  if (config.ffn_intermediate_size <= 0) {
    throw std::invalid_argument(
        "decoder ffn_intermediate_size must be positive");
  }
}

void validate_matrix_shape(const std::vector<float>& weights, const int rows,
                           const int cols, const std::string& name) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(name + " dimensions must be positive");
  }
  if (weights.size() != internal::checked_product_size(rows, cols,
                                                       name.c_str())) {
    throw std::invalid_argument(name + " size mismatch");
  }
}

void validate_attention_inputs(const std::vector<float>& input,
                               const DecoderBlockWeights& weights,
                               const DecoderBlockConfig& config) {
  validate_config(config);
  if (input.size() !=
      internal::checked_product_size(config.seq_len, config.hidden_size,
                                     "decoder input")) {
    throw std::invalid_argument("decoder input size mismatch");
  }
  if (weights.rms_gamma.size() !=
      static_cast<std::size_t>(config.hidden_size)) {
    throw std::invalid_argument("decoder rms_gamma size must equal hidden_size");
  }
  validate_matrix_shape(weights.q_weight, config.hidden_size,
                        config.hidden_size, "q_weight");
  validate_matrix_shape(weights.k_weight, config.hidden_size,
                        config.hidden_size, "k_weight");
  validate_matrix_shape(weights.v_weight, config.hidden_size,
                        config.hidden_size, "v_weight");
  validate_matrix_shape(weights.o_weight, config.hidden_size,
                        config.hidden_size, "o_weight");
}

void validate_full_inputs(const std::vector<float>& input,
                          const DecoderBlockWeights& weights,
                          const DecoderBlockConfig& config) {
  validate_full_config(config);
  validate_attention_inputs(input, weights, config);
  if (weights.post_attention_rms_gamma.size() !=
      static_cast<std::size_t>(config.hidden_size)) {
    throw std::invalid_argument(
        "decoder post_attention_rms_gamma size must equal hidden_size");
  }
  validate_matrix_shape(weights.gate_weight, config.hidden_size,
                        config.ffn_intermediate_size, "gate_weight");
  validate_matrix_shape(weights.up_weight, config.hidden_size,
                        config.ffn_intermediate_size, "up_weight");
  validate_matrix_shape(weights.down_weight, config.ffn_intermediate_size,
                        config.hidden_size, "down_weight");
}

float checksum(const std::vector<float>& values) {
  internal::validate_all_finite(values, "decoder checksum values");
  float total = 0.0F;
  for (const auto value : values) {
    total += value;
    if (!std::isfinite(total)) {
      throw std::invalid_argument("decoder checksum must be finite");
    }
  }
  return total;
}

std::size_t hidden_index(const int token, const int hidden,
                         const int hidden_size) {
  return internal::checked_flat_index(token, hidden_size, hidden,
                                      "decoder hidden");
}

std::size_t head_major_index(const int head, const int token, const int dim,
                             const int seq_len, const int head_dim) {
  return internal::checked_flat_index3(head, seq_len, token, head_dim, dim,
                                       "decoder head-major");
}

int hidden_from_head_dim(const int head, const int head_dim, const int dim) {
  const auto hidden =
      internal::checked_flat_index(head, head_dim, dim, "decoder hidden dim");
  if (hidden > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("decoder hidden index exceeds int range");
  }
  return static_cast<int>(hidden);
}

std::vector<float> add_vectors(const std::vector<float>& lhs,
                               const std::vector<float>& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::invalid_argument("residual add size mismatch");
  }
  internal::validate_all_finite(lhs, "residual add lhs");
  internal::validate_all_finite(rhs, "residual add rhs");
  std::vector<float> output(lhs.size(), 0.0F);
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const float value = lhs[i] + rhs[i];
    if (!std::isfinite(value)) {
      throw std::invalid_argument("residual add output must be finite");
    }
    output[i] = value;
  }
  return output;
}

float silu(const float value) {
  return value / (1.0F + std::exp(-value));
}

std::vector<float> swiglu_cpu(const std::vector<float>& gate,
                              const std::vector<float>& up) {
  if (gate.size() != up.size()) {
    throw std::invalid_argument("SwiGLU gate/up size mismatch");
  }
  internal::validate_all_finite(gate, "SwiGLU gate");
  internal::validate_all_finite(up, "SwiGLU up");
  std::vector<float> output(gate.size(), 0.0F);
  for (std::size_t i = 0; i < gate.size(); ++i) {
    const float value = silu(gate[i]) * up[i];
    if (!std::isfinite(value)) {
      throw std::invalid_argument("SwiGLU output must be finite");
    }
    output[i] = value;
  }
  return output;
}

std::vector<float> hidden_to_head_major(const std::vector<float>& matrix,
                                        const DecoderBlockConfig& config) {
  const int head_dim = config.hidden_size / config.num_heads;
  std::vector<float> output(matrix.size(), 0.0F);
  for (int token = 0; token < config.seq_len; ++token) {
    for (int head = 0; head < config.num_heads; ++head) {
      for (int dim = 0; dim < head_dim; ++dim) {
        const int hidden = hidden_from_head_dim(head, head_dim, dim);
        output[head_major_index(head, token, dim, config.seq_len, head_dim)] =
            matrix[hidden_index(token, hidden, config.hidden_size)];
      }
    }
  }
  return output;
}

std::vector<float> head_major_to_hidden(const std::vector<float>& tensor,
                                        const DecoderBlockConfig& config) {
  const int head_dim = config.hidden_size / config.num_heads;
  std::vector<float> output(tensor.size(), 0.0F);
  for (int token = 0; token < config.seq_len; ++token) {
    for (int head = 0; head < config.num_heads; ++head) {
      for (int dim = 0; dim < head_dim; ++dim) {
        const int hidden = hidden_from_head_dim(head, head_dim, dim);
        output[hidden_index(token, hidden, config.hidden_size)] =
            tensor[head_major_index(head, token, dim, config.seq_len, head_dim)];
      }
    }
  }
  return output;
}

std::vector<float> apply_rope_per_head(const std::vector<float>& tensor,
                                       const DecoderBlockConfig& config) {
  const int head_dim = config.hidden_size / config.num_heads;
  std::vector<float> output(tensor.size(), 0.0F);
  const auto head_elements =
      internal::checked_product_size(config.seq_len, head_dim,
                                     "decoder RoPE head elements");

  for (int head = 0; head < config.num_heads; ++head) {
    const auto head_offset = internal::checked_flat_index3(
        head, config.seq_len, 0, head_dim, 0, "decoder RoPE head offset");
    const auto begin =
        tensor.begin() +
        internal::checked_ptrdiff_offset(head_offset, "decoder RoPE head offset");
    const auto end =
        begin + internal::checked_ptrdiff_offset(head_elements,
                                                "decoder RoPE head elements");
    const std::vector<float> head_slice(begin, end);
    const auto rotated =
        apply_rope_cpu(head_slice, config.seq_len, head_dim, config.rope_base);
    const auto output_begin =
        output.begin() +
        internal::checked_ptrdiff_offset(head_offset, "decoder RoPE output");
    std::copy(rotated.begin(), rotated.end(), output_begin);
  }
  return output;
}

DecoderBlockTrace build_trace(const DecoderBlockConfig& config,
                              const std::vector<float>& normalized,
                              const std::vector<float>& query,
                              const std::vector<float>& attention,
                              const std::vector<float>& output) {
  const int head_dim = config.hidden_size / config.num_heads;
  DecoderBlockTrace trace;
  trace.seq_len = config.seq_len;
  trace.hidden_size = config.hidden_size;
  trace.num_heads = config.num_heads;
  trace.head_dim = head_dim;
  trace.ffn_intermediate_size = config.ffn_intermediate_size;
  trace.input_elements =
      internal::checked_product_size(config.seq_len, config.hidden_size,
                                     "decoder trace input");
  trace.projected_elements = trace.input_elements;
  trace.attention_score_elements = internal::checked_product_size(
      config.num_heads, config.seq_len, config.seq_len,
      "decoder trace attention scores");
  trace.output_elements = output.size();
  trace.normalized_checksum = checksum(normalized);
  trace.query_checksum = checksum(query);
  trace.attention_checksum = checksum(attention);
  trace.output_checksum = checksum(output);
  return trace;
}

DecoderBlockOutput attention_sublayer_cpu(const std::vector<float>& input,
                                          const DecoderBlockWeights& weights,
                                          const DecoderBlockConfig& config) {
  const auto normalized = rms_norm_cpu(input, config.seq_len,
                                       config.hidden_size, weights.rms_gamma,
                                       config.rms_epsilon);
  const auto query_hidden =
      matmul_cpu(normalized, config.seq_len, config.hidden_size,
                 weights.q_weight, config.hidden_size);
  const auto key_hidden =
      matmul_cpu(normalized, config.seq_len, config.hidden_size,
                 weights.k_weight, config.hidden_size);
  const auto value_hidden =
      matmul_cpu(normalized, config.seq_len, config.hidden_size,
                 weights.v_weight, config.hidden_size);

  const auto query =
      apply_rope_per_head(hidden_to_head_major(query_hidden, config), config);
  const auto key =
      apply_rope_per_head(hidden_to_head_major(key_hidden, config), config);
  const auto value = hidden_to_head_major(value_hidden, config);
  const AttentionShape attention_shape{
      config.num_heads, config.seq_len, config.seq_len,
      config.hidden_size / config.num_heads};
  const auto attended = scaled_dot_product_attention_cpu(
      query, key, value, attention_shape, true);
  const auto attended_hidden = head_major_to_hidden(attended, config);
  const auto output = matmul_cpu(attended_hidden, config.seq_len,
                                config.hidden_size, weights.o_weight,
                                config.hidden_size);

  return DecoderBlockOutput{
      output, build_trace(config, normalized, query, attended, output)};
}

}  // namespace

DecoderBlockOutput decoder_block_attention_cpu(
    const std::vector<float>& input, const DecoderBlockWeights& weights,
    const DecoderBlockConfig& config) {
  validate_attention_inputs(input, weights, config);
  return attention_sublayer_cpu(input, weights, config);
}

DecoderBlockOutput decoder_block_cpu(const std::vector<float>& input,
                                     const DecoderBlockWeights& weights,
                                     const DecoderBlockConfig& config) {
  validate_full_inputs(input, weights, config);

  auto attention = attention_sublayer_cpu(input, weights, config);
  const auto attention_residual = add_vectors(input, attention.hidden_states);
  const auto ffn_input = rms_norm_cpu(attention_residual, config.seq_len,
                                     config.hidden_size,
                                     weights.post_attention_rms_gamma,
                                     config.rms_epsilon);
  const auto gate = matmul_cpu(ffn_input, config.seq_len, config.hidden_size,
                              weights.gate_weight,
                              config.ffn_intermediate_size);
  const auto up = matmul_cpu(ffn_input, config.seq_len, config.hidden_size,
                            weights.up_weight, config.ffn_intermediate_size);
  const auto activated = swiglu_cpu(gate, up);
  const auto ffn_output =
      matmul_cpu(activated, config.seq_len, config.ffn_intermediate_size,
                 weights.down_weight, config.hidden_size);
  const auto output = add_vectors(attention_residual, ffn_output);

  attention.hidden_states = output;
  attention.trace.ffn_intermediate_size = config.ffn_intermediate_size;
  attention.trace.ffn_elements = activated.size();
  attention.trace.output_elements = output.size();
  attention.trace.attention_residual_checksum = checksum(attention_residual);
  attention.trace.post_attention_norm_checksum = checksum(ffn_input);
  attention.trace.ffn_checksum = checksum(ffn_output);
  attention.trace.output_checksum = checksum(output);
  return attention;
}

}  // namespace inferlab
