#include "inferlab/decoder_kv.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "checked_index.hpp"
#include "inferlab/attention.hpp"
#include "inferlab/matrix_ops.hpp"
#include "inferlab/normalization.hpp"
#include "inferlab/paged_attention.hpp"
#include "inferlab/rope.hpp"

namespace inferlab {
namespace {

bool finite_positive(const float value) {
  return std::isfinite(value) && value > 0.0F;
}

void validate_model_config(const DecoderBlockConfig& config) {
  if (config.seq_len <= 0 || config.hidden_size <= 0 ||
      config.num_heads <= 0 || config.ffn_intermediate_size <= 0) {
    throw std::invalid_argument("decoder KV config dimensions must be positive");
  }
  if (config.hidden_size % config.num_heads != 0) {
    throw std::invalid_argument("hidden_size must be divisible by num_heads");
  }
  if ((config.hidden_size / config.num_heads) % 2 != 0) {
    throw std::invalid_argument("decoder KV head_dim must be even for RoPE");
  }
  if (!finite_positive(config.rope_base) ||
      !finite_positive(config.rms_epsilon)) {
    throw std::invalid_argument(
        "decoder KV scalar config must be finite and positive");
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

void validate_weights(const DecoderBlockWeights& weights,
                      const DecoderBlockConfig& config) {
  if (weights.rms_gamma.size() !=
      static_cast<std::size_t>(config.hidden_size)) {
    throw std::invalid_argument("rms_gamma size mismatch");
  }
  if (weights.post_attention_rms_gamma.size() !=
      static_cast<std::size_t>(config.hidden_size)) {
    throw std::invalid_argument("post_attention_rms_gamma size mismatch");
  }
  validate_matrix_shape(weights.q_weight, config.hidden_size,
                        config.hidden_size, "q_weight");
  validate_matrix_shape(weights.k_weight, config.hidden_size,
                        config.hidden_size, "k_weight");
  validate_matrix_shape(weights.v_weight, config.hidden_size,
                        config.hidden_size, "v_weight");
  validate_matrix_shape(weights.o_weight, config.hidden_size,
                        config.hidden_size, "o_weight");
  validate_matrix_shape(weights.gate_weight, config.hidden_size,
                        config.ffn_intermediate_size, "gate_weight");
  validate_matrix_shape(weights.up_weight, config.hidden_size,
                        config.ffn_intermediate_size, "up_weight");
  validate_matrix_shape(weights.down_weight, config.ffn_intermediate_size,
                        config.hidden_size, "down_weight");
}

float checksum(const std::vector<float>& values) {
  internal::validate_all_finite(values, "decoder KV checksum values");
  float total = 0.0F;
  for (const auto value : values) {
    total += value;
    if (!std::isfinite(total)) {
      throw std::invalid_argument("decoder KV checksum must be finite");
    }
  }
  return total;
}

float silu(const float value) {
  return value / (1.0F + std::exp(-value));
}

std::vector<float> add_vectors(const std::vector<float>& lhs,
                               const std::vector<float>& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::invalid_argument("decoder KV residual add size mismatch");
  }
  internal::validate_all_finite(lhs, "decoder KV residual lhs");
  internal::validate_all_finite(rhs, "decoder KV residual rhs");
  std::vector<float> output(lhs.size(), 0.0F);
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const float value = lhs[i] + rhs[i];
    if (!std::isfinite(value)) {
      throw std::invalid_argument("decoder KV residual output must be finite");
    }
    output[i] = value;
  }
  return output;
}

std::vector<float> swiglu_cpu(const std::vector<float>& gate,
                              const std::vector<float>& up) {
  if (gate.size() != up.size()) {
    throw std::invalid_argument("decoder KV SwiGLU size mismatch");
  }
  internal::validate_all_finite(gate, "decoder KV SwiGLU gate");
  internal::validate_all_finite(up, "decoder KV SwiGLU up");
  std::vector<float> output(gate.size(), 0.0F);
  for (std::size_t i = 0; i < gate.size(); ++i) {
    const float value = silu(gate[i]) * up[i];
    if (!std::isfinite(value)) {
      throw std::invalid_argument("decoder KV SwiGLU output must be finite");
    }
    output[i] = value;
  }
  return output;
}

std::size_t hidden_index(const int token, const int hidden,
                         const int hidden_size) {
  return internal::checked_flat_index(token, hidden_size, hidden,
                                      "decoder KV hidden");
}

std::size_t head_major_index(const int head, const int token, const int dim,
                             const int seq_len, const int head_dim) {
  return internal::checked_flat_index3(head, seq_len, token, head_dim, dim,
                                       "decoder KV head-major");
}

int hidden_from_head_dim(const int head, const int head_dim, const int dim) {
  const auto hidden =
      internal::checked_flat_index(head, head_dim, dim, "decoder KV hidden dim");
  if (hidden > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("decoder KV hidden index exceeds int range");
  }
  return static_cast<int>(hidden);
}

std::int64_t checked_triangular_count(const std::int64_t value,
                                      const char* name) {
  const auto next = internal::checked_add_i64(value, 1, name);
  const auto even_first = (value % 2) == 0;
  const auto lhs = even_first ? value / 2 : value;
  const auto rhs = even_first ? next : next / 2;
  return internal::checked_product_i64(lhs, rhs, name);
}

std::vector<float> hidden_to_head_major(const std::vector<float>& matrix,
                                        const int seq_len,
                                        const int hidden_size,
                                        const int num_heads) {
  const int head_dim = hidden_size / num_heads;
  std::vector<float> output(matrix.size(), 0.0F);
  for (int token = 0; token < seq_len; ++token) {
    for (int head = 0; head < num_heads; ++head) {
      for (int dim = 0; dim < head_dim; ++dim) {
        const int hidden = hidden_from_head_dim(head, head_dim, dim);
        output[head_major_index(head, token, dim, seq_len, head_dim)] =
            matrix[hidden_index(token, hidden, hidden_size)];
      }
    }
  }
  return output;
}

std::vector<float> head_major_to_hidden(const std::vector<float>& tensor,
                                        const int seq_len,
                                        const int hidden_size,
                                        const int num_heads) {
  const int head_dim = hidden_size / num_heads;
  std::vector<float> output(tensor.size(), 0.0F);
  for (int token = 0; token < seq_len; ++token) {
    for (int head = 0; head < num_heads; ++head) {
      for (int dim = 0; dim < head_dim; ++dim) {
        const int hidden = hidden_from_head_dim(head, head_dim, dim);
        output[hidden_index(token, hidden, hidden_size)] =
            tensor[head_major_index(head, token, dim, seq_len, head_dim)];
      }
    }
  }
  return output;
}

std::vector<float> apply_rope_per_head(const std::vector<float>& tensor,
                                       const int seq_len,
                                       const DecoderBlockConfig& config,
                                       const int position_offset) {
  const int head_dim = config.hidden_size / config.num_heads;
  std::vector<float> output(tensor.size(), 0.0F);
  const auto head_elements =
      internal::checked_product_size(seq_len, head_dim,
                                     "decoder KV RoPE head elements");
  for (int head = 0; head < config.num_heads; ++head) {
    const auto head_offset = internal::checked_flat_index3(
        head, seq_len, 0, head_dim, 0, "decoder KV RoPE head offset");
    const auto begin =
        tensor.begin() +
        internal::checked_ptrdiff_offset(head_offset,
                                         "decoder KV RoPE head offset");
    const auto end =
        begin + internal::checked_ptrdiff_offset(head_elements,
                                                "decoder KV RoPE head elements");
    const std::vector<float> head_slice(begin, end);
    const auto rotated =
        apply_rope_cpu(head_slice, seq_len, head_dim, config.rope_base,
                       position_offset);
    const auto output_begin =
        output.begin() +
        internal::checked_ptrdiff_offset(head_offset,
                                         "decoder KV RoPE output");
    std::copy(rotated.begin(), rotated.end(), output_begin);
  }
  return output;
}

std::vector<float> append_head_major_token(const std::vector<float>& cache,
                                           const std::vector<float>& token,
                                           const int old_tokens,
                                           const DecoderBlockConfig& config) {
  const int head_dim = config.hidden_size / config.num_heads;
  if (old_tokens < 0 || old_tokens == std::numeric_limits<int>::max()) {
    throw std::invalid_argument("decoder KV old_tokens is invalid");
  }
  const int new_tokens =
      internal::checked_add_int(old_tokens, 1, "decoder KV tokens");
  if (cache.size() !=
      internal::checked_non_negative_product_size(
          config.num_heads, old_tokens, head_dim, "decoder KV cache")) {
    throw std::invalid_argument("decoder KV cache size mismatch");
  }
  if (token.size() !=
      internal::checked_product_size(config.num_heads, head_dim,
                                     "decoder KV token")) {
    throw std::invalid_argument("decoder KV token size mismatch");
  }

  std::vector<float> output(
      internal::checked_product_size(config.num_heads, new_tokens, head_dim,
                                     "decoder KV appended cache"),
      0.0F);
  for (int head = 0; head < config.num_heads; ++head) {
    for (int token_index = 0; token_index < old_tokens; ++token_index) {
      for (int dim = 0; dim < head_dim; ++dim) {
        output[head_major_index(head, token_index, dim, new_tokens, head_dim)] =
            cache[head_major_index(head, token_index, dim, old_tokens,
                                   head_dim)];
      }
    }
    for (int dim = 0; dim < head_dim; ++dim) {
      output[head_major_index(head, old_tokens, dim, new_tokens, head_dim)] =
          token[head_major_index(head, 0, dim, 1, head_dim)];
    }
  }
  return output;
}

std::size_t physical_index(const BlockId block_id, const int head,
                           const int block_offset, const int dim,
                           const DecoderBlockConfig& config,
                           const PagedKVCacheConfig& cache_config) {
  const int head_dim = config.hidden_size / config.num_heads;
  const auto block_head_token = internal::checked_flat_index3(
      block_id, config.num_heads, head, cache_config.block_size_tokens,
      block_offset, "decoder KV physical token");
  return internal::checked_flat_index_size(block_head_token, head_dim, dim,
                                           "decoder KV physical cache");
}

std::vector<float> materialize_physical_cache(
    const std::vector<float>& logical_cache, const RequestBlocks& blocks,
    const DecoderBlockConfig& config, const PagedKVCacheConfig& cache_config) {
  const int head_dim = config.hidden_size / config.num_heads;
  if (logical_cache.size() !=
      internal::checked_non_negative_product_size(
          config.num_heads, blocks.token_count, head_dim, "logical KV cache")) {
    throw std::invalid_argument("logical KV cache size mismatch");
  }
  std::vector<float> physical(
      internal::checked_product_size(cache_config.num_blocks, config.num_heads,
                                     cache_config.block_size_tokens, head_dim,
                                     "physical KV cache"),
      0.0F);
  for (int token = 0; token < blocks.token_count; ++token) {
    const int logical_block = token / cache_config.block_size_tokens;
    if (logical_block >=
        internal::checked_int_from_size(blocks.block_table.size(),
                                        "decoder KV block table size")) {
      throw std::invalid_argument("block table missing logical KV block");
    }
    const int block_offset = token % cache_config.block_size_tokens;
    const BlockId block_id =
        blocks.block_table[static_cast<std::size_t>(logical_block)];
    for (int head = 0; head < config.num_heads; ++head) {
      for (int dim = 0; dim < head_dim; ++dim) {
        physical[physical_index(block_id, head, block_offset, dim, config,
                                cache_config)] =
            logical_cache[head_major_index(head, token, dim, blocks.token_count,
                                           head_dim)];
      }
    }
  }
  return physical;
}

std::vector<float> gather_state_cache(const std::vector<float>& physical_cache,
                                      const DecoderKVState& state) {
  const PagedKVShape shape{
      state.config.num_heads,
      state.config.hidden_size / state.config.num_heads,
      state.cache_config.block_size_tokens,
  };
  return gather_paged_kv_cache(physical_cache, state.blocks, shape);
}

std::vector<float> project_head_major(const std::vector<float>& input,
                                      const int seq_len,
                                      const std::vector<float>& weights,
                                      const DecoderBlockConfig& config,
                                      const bool apply_rope,
                                      const int position_offset) {
  const auto projected = matmul_cpu(input, seq_len, config.hidden_size, weights,
                                    config.hidden_size);
  auto head_major = hidden_to_head_major(projected, seq_len, config.hidden_size,
                                        config.num_heads);
  if (apply_rope) {
    head_major =
        apply_rope_per_head(head_major, seq_len, config, position_offset);
  }
  return head_major;
}

std::vector<float> ffn_sublayer_cpu(const std::vector<float>& input,
                                    const DecoderBlockWeights& weights,
                                    const DecoderBlockConfig& config,
                                    const int rows) {
  const auto normalized =
      rms_norm_cpu(input, rows, config.hidden_size,
                   weights.post_attention_rms_gamma, config.rms_epsilon);
  const auto gate = matmul_cpu(normalized, rows, config.hidden_size,
                              weights.gate_weight,
                              config.ffn_intermediate_size);
  const auto up = matmul_cpu(normalized, rows, config.hidden_size,
                            weights.up_weight, config.ffn_intermediate_size);
  const auto activated = swiglu_cpu(gate, up);
  return matmul_cpu(activated, rows, config.ffn_intermediate_size,
                    weights.down_weight, config.hidden_size);
}

RequestBlocks allocate_blocks(const PagedKVCacheConfig& cache_config,
                              const RequestId request_id,
                              const int token_count) {
  PagedKVCacheManager manager(cache_config);
  if (!manager.add_request(request_id, token_count)) {
    throw std::runtime_error("failed to allocate decoder KV blocks");
  }
  const auto blocks = manager.request_blocks(request_id);
  if (!blocks.has_value()) {
    throw std::runtime_error("decoder KV blocks missing after allocation");
  }
  return *blocks;
}

AllocationStats stats_for_tokens(const PagedKVCacheConfig& cache_config,
                                 const RequestId request_id,
                                 const int token_count) {
  PagedKVCacheManager manager(cache_config);
  if (!manager.add_request(request_id, token_count)) {
    throw std::runtime_error("failed to allocate decoder KV stats blocks");
  }
  return manager.stats();
}

std::vector<float> decode_one_token(DecoderKVState& state,
                                    const std::vector<float>& token_input,
                                    const DecoderBlockWeights& weights) {
  const int previous_tokens = state.blocks.token_count;
  const auto token_config = state.config;
  if (state.request_id <= 0 || previous_tokens <= 0 ||
      token_config.seq_len != previous_tokens) {
    throw std::invalid_argument(
        "decoder KV state token metadata is inconsistent");
  }
  if (token_input.size() != static_cast<std::size_t>(token_config.hidden_size)) {
    throw std::invalid_argument("decode token input size mismatch");
  }
  const int next_tokens =
      internal::checked_add_int(previous_tokens, 1, "decoder KV next tokens");

  const auto normalized =
      rms_norm_cpu(token_input, 1, token_config.hidden_size,
                   weights.rms_gamma, token_config.rms_epsilon);
  const auto query = project_head_major(normalized, 1, weights.q_weight,
                                        token_config, true, previous_tokens);
  const auto key_token = project_head_major(normalized, 1, weights.k_weight,
                                           token_config, true, previous_tokens);
  const auto value_token = project_head_major(normalized, 1, weights.v_weight,
                                             token_config, false,
                                             previous_tokens);

  DecoderKVState next_state;
  next_state.request_id = state.request_id;
  next_state.config = token_config;
  next_state.config.seq_len = next_tokens;
  next_state.cache_config = state.cache_config;
  next_state.blocks =
      allocate_blocks(next_state.cache_config, next_state.request_id, next_tokens);
  next_state.key_cache = append_head_major_token(
      state.key_cache, key_token, previous_tokens, token_config);
  next_state.value_cache = append_head_major_token(
      state.value_cache, value_token, previous_tokens, token_config);
  next_state.physical_key_cache = materialize_physical_cache(
      next_state.key_cache, next_state.blocks, next_state.config,
      next_state.cache_config);
  next_state.physical_value_cache = materialize_physical_cache(
      next_state.value_cache, next_state.blocks, next_state.config,
      next_state.cache_config);
  next_state.key_cache =
      gather_state_cache(next_state.physical_key_cache, next_state);
  next_state.value_cache =
      gather_state_cache(next_state.physical_value_cache, next_state);

  const AttentionShape attention_shape{
      token_config.num_heads, 1, next_tokens,
      token_config.hidden_size / token_config.num_heads};
  const auto attended = scaled_dot_product_attention_cpu(
      query, next_state.key_cache, next_state.value_cache, attention_shape,
      false);
  const auto attended_hidden =
      head_major_to_hidden(attended, 1, token_config.hidden_size,
                           token_config.num_heads);
  const auto attention_output =
      matmul_cpu(attended_hidden, 1, token_config.hidden_size,
                 weights.o_weight, token_config.hidden_size);
  const auto attention_residual = add_vectors(token_input, attention_output);
  const auto ffn_output =
      ffn_sublayer_cpu(attention_residual, weights, token_config, 1);
  auto output = add_vectors(attention_residual, ffn_output);
  state = std::move(next_state);
  return output;
}

}  // namespace

DecoderKVCacheProfile profile_decoder_kv_cache(
    const int prompt_tokens, const int decode_steps,
    const DecoderBlockConfig& config,
    const PagedKVCacheConfig& cache_config) {
  validate_model_config(config);
  if (prompt_tokens <= 0) {
    throw std::invalid_argument("profile prompt_tokens must be positive");
  }
  if (decode_steps < 0) {
    throw std::invalid_argument("profile decode_steps must be non-negative");
  }
  if (cache_config.block_size_tokens <= 0) {
    throw std::invalid_argument("profile block_size_tokens must be positive");
  }

  const int head_dim = config.hidden_size / config.num_heads;
  const int final_tokens =
      internal::checked_add_int(prompt_tokens, decode_steps,
                                "decoder KV final tokens");
  const auto kv_elements_per_token = internal::checked_product_i64(
      config.hidden_size, 2, "decoder KV elements per token");
  const auto element_bytes = static_cast<std::int64_t>(sizeof(float));

  DecoderKVCacheProfile profile;
  profile.prompt_tokens = prompt_tokens;
  profile.decode_steps = decode_steps;
  profile.final_tokens = final_tokens;
  profile.hidden_size = config.hidden_size;
  profile.num_heads = config.num_heads;
  profile.head_dim = head_dim;
  profile.block_size_tokens = cache_config.block_size_tokens;
  profile.final_logical_blocks =
      internal::checked_ceil_div_non_negative_int(
          final_tokens, cache_config.block_size_tokens,
          "decoder KV final logical blocks");
  profile.kv_element_size_bytes = static_cast<int>(element_bytes);
  profile.kv_elements_per_token = kv_elements_per_token;
  profile.prefill_kv_write_elements =
      internal::checked_product_i64(prompt_tokens, kv_elements_per_token,
                                    "decoder KV prefill write elements");
  profile.decode_kv_append_write_elements =
      internal::checked_product_i64(decode_steps, kv_elements_per_token,
                                    "decoder KV decode append write elements");
  profile.final_kv_elements =
      internal::checked_product_i64(final_tokens, kv_elements_per_token,
                                    "decoder KV final elements");
  profile.final_logical_kv_bytes =
      internal::checked_product_i64(profile.final_kv_elements, element_bytes,
                                    "decoder KV logical bytes");
  const std::int64_t reserved_tokens =
      internal::checked_product_i64(profile.final_logical_blocks,
                                    cache_config.block_size_tokens,
                                    "decoder KV reserved tokens");
  const std::int64_t reserved_elements =
      internal::checked_product_i64(reserved_tokens, kv_elements_per_token,
                                    "decoder KV reserved elements");
  profile.final_reserved_kv_bytes =
      internal::checked_product_i64(reserved_elements, element_bytes,
                                    "decoder KV reserved bytes");
  if (profile.final_reserved_kv_bytes > 0) {
    profile.final_kv_utilization =
        static_cast<double>(profile.final_logical_kv_bytes) /
        static_cast<double>(profile.final_reserved_kv_bytes);
  }

  const auto decode_history_tokens = internal::checked_add_i64(
      internal::checked_product_i64(decode_steps, prompt_tokens,
                                    "decoder KV decode history prompt tokens"),
      checked_triangular_count(decode_steps,
                               "decoder KV decode history generated tokens"),
      "decoder KV decode history tokens");
  profile.decode_attention_score_elements =
      internal::checked_product_i64(decode_history_tokens, config.num_heads,
                                    "decoder KV decode attention scores");
  profile.decode_attention_kv_read_elements =
      internal::checked_product_i64(decode_history_tokens,
                                    kv_elements_per_token,
                                    "decoder KV decode attention reads");
  profile.paged_gather_read_elements = profile.decode_attention_kv_read_elements;
  profile.paged_gather_write_elements =
      profile.decode_attention_kv_read_elements;
  profile.block_table_lookup_count = internal::checked_product_i64(
      decode_history_tokens, 2, "decoder KV block table lookups");
  profile.block_table_lookup_bytes = internal::checked_product_i64(
      profile.block_table_lookup_count, static_cast<std::int64_t>(sizeof(BlockId)),
      "decoder KV block table lookup bytes");
  profile.current_impl_physical_materialize_read_elements =
      profile.decode_attention_kv_read_elements;
  profile.current_impl_physical_materialize_write_elements =
      profile.decode_attention_kv_read_elements;
  if (decode_steps > 0) {
    profile.average_attention_kv_read_elements_per_decode_step =
        static_cast<double>(profile.decode_attention_kv_read_elements) /
        static_cast<double>(decode_steps);
  }
  return profile;
}

DecoderKVState decoder_prefill_cpu(const std::vector<float>& prompt_input,
                                   const DecoderBlockWeights& weights,
                                   const DecoderBlockConfig& config,
                                   const PagedKVCacheConfig& cache_config,
                                   const RequestId request_id) {
  validate_model_config(config);
  validate_weights(weights, config);
  if (prompt_input.size() !=
      internal::checked_product_size(config.seq_len, config.hidden_size,
                                     "prefill input")) {
    throw std::invalid_argument("prefill input size mismatch");
  }

  const auto normalized =
      rms_norm_cpu(prompt_input, config.seq_len, config.hidden_size,
                   weights.rms_gamma, config.rms_epsilon);

  DecoderKVState state;
  state.request_id = request_id;
  state.config = config;
  state.cache_config = cache_config;
  state.blocks = allocate_blocks(cache_config, request_id, config.seq_len);
  state.key_cache = project_head_major(normalized, config.seq_len,
                                       weights.k_weight, config, true, 0);
  state.value_cache = project_head_major(normalized, config.seq_len,
                                         weights.v_weight, config, false, 0);
  state.physical_key_cache = materialize_physical_cache(
      state.key_cache, state.blocks, state.config, state.cache_config);
  state.physical_value_cache = materialize_physical_cache(
      state.value_cache, state.blocks, state.config, state.cache_config);
  state.key_cache = gather_state_cache(state.physical_key_cache, state);
  state.value_cache = gather_state_cache(state.physical_value_cache, state);
  return state;
}

std::vector<float> decoder_decode_cpu(DecoderKVState& state,
                                      const std::vector<float>& token_input,
                                      const DecoderBlockWeights& weights) {
  validate_model_config(state.config);
  validate_weights(weights, state.config);
  return decode_one_token(state, token_input, weights);
}

DecoderKVRunOutput decoder_prefill_decode_cpu(
    const std::vector<float>& prompt_input,
    const std::vector<float>& decode_inputs, const int decode_steps,
    const DecoderBlockWeights& weights, const DecoderBlockConfig& config,
    const PagedKVCacheConfig& cache_config, const RequestId request_id) {
  if (decode_steps < 0) {
    throw std::invalid_argument("decode_steps must be non-negative");
  }
  if (decode_inputs.size() !=
      internal::checked_non_negative_product_size(decode_steps,
                                                  config.hidden_size,
                                                  "decode inputs")) {
    throw std::invalid_argument("decode inputs size mismatch");
  }

  auto state =
      decoder_prefill_cpu(prompt_input, weights, config, cache_config,
                          request_id);
  const float prefill_key_checksum = checksum(state.key_cache);
  std::vector<float> last_token_hidden;
  for (int step = 0; step < decode_steps; ++step) {
    const auto step_offset =
        internal::checked_flat_index(step, config.hidden_size, 0,
                                     "decode input step");
    const auto begin =
        decode_inputs.begin() +
        internal::checked_ptrdiff_offset(step_offset, "decode input step");
    const auto end =
        begin + internal::checked_ptrdiff_offset(
                    static_cast<std::size_t>(config.hidden_size),
                    "decode input hidden size");
    const std::vector<float> token_input(begin, end);
    last_token_hidden = decoder_decode_cpu(state, token_input, weights);
  }

  const auto stats = stats_for_tokens(cache_config, request_id,
                                      state.blocks.token_count);
  const int last_token_index = state.blocks.token_count - 1;
  const int logical_block = last_token_index / cache_config.block_size_tokens;
  const BlockId last_block =
      state.blocks.block_table[static_cast<std::size_t>(logical_block)];

  DecoderKVTrace trace;
  const float final_key_checksum = checksum(state.key_cache);
  const float decode_key_checksum = final_key_checksum - prefill_key_checksum;
  if (!std::isfinite(decode_key_checksum)) {
    throw std::invalid_argument("decoder KV decode checksum must be finite");
  }
  trace.prompt_tokens = config.seq_len;
  trace.decode_steps = decode_steps;
  trace.final_tokens = state.blocks.token_count;
  trace.used_blocks = stats.used_blocks;
  trace.free_blocks = stats.free_blocks;
  trace.key_value_elements = internal::checked_add_size(
      state.key_cache.size(), state.value_cache.size(),
      "decoder KV trace key/value elements");
  trace.last_token_output_elements = last_token_hidden.size();
  trace.last_token_block_id = last_block;
  trace.last_token_block_offset =
      last_token_index % cache_config.block_size_tokens;
  trace.used_block_table_gather = true;
  trace.prefill_key_checksum = prefill_key_checksum;
  trace.decode_key_checksum = decode_key_checksum;
  trace.last_token_output_checksum = checksum(last_token_hidden);

  auto profile =
      profile_decoder_kv_cache(config.seq_len, decode_steps, config,
                               cache_config);

  return DecoderKVRunOutput{std::move(state), std::move(last_token_hidden),
                            trace, profile};
}

}  // namespace inferlab
