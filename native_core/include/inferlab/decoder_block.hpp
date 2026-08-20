#pragma once

#include <cstddef>
#include <vector>

namespace inferlab {

struct DecoderBlockConfig {
  int seq_len = 0;
  int hidden_size = 0;
  int num_heads = 0;
  float rope_base = 10000.0F;
  float rms_epsilon = 1e-6F;
  int ffn_intermediate_size = 0;
};

struct DecoderBlockWeights {
  std::vector<float> rms_gamma;
  std::vector<float> q_weight;
  std::vector<float> k_weight;
  std::vector<float> v_weight;
  std::vector<float> o_weight;
  std::vector<float> post_attention_rms_gamma;
  std::vector<float> gate_weight;
  std::vector<float> up_weight;
  std::vector<float> down_weight;
};

struct DecoderBlockTrace {
  int seq_len = 0;
  int hidden_size = 0;
  int num_heads = 0;
  int head_dim = 0;
  int ffn_intermediate_size = 0;
  std::size_t input_elements = 0;
  std::size_t projected_elements = 0;
  std::size_t attention_score_elements = 0;
  std::size_t ffn_elements = 0;
  std::size_t output_elements = 0;
  float normalized_checksum = 0.0F;
  float query_checksum = 0.0F;
  float attention_checksum = 0.0F;
  float attention_residual_checksum = 0.0F;
  float post_attention_norm_checksum = 0.0F;
  float ffn_checksum = 0.0F;
  float output_checksum = 0.0F;
};

struct DecoderBlockOutput {
  std::vector<float> hidden_states;
  DecoderBlockTrace trace;
};

DecoderBlockOutput decoder_block_attention_cpu(
    const std::vector<float>& input, const DecoderBlockWeights& weights,
    const DecoderBlockConfig& config);

DecoderBlockOutput decoder_block_cpu(const std::vector<float>& input,
                                     const DecoderBlockWeights& weights,
                                     const DecoderBlockConfig& config);

}  // namespace inferlab
