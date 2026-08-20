#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "inferlab/decoder_block.hpp"
#include "inferlab/paged_kv_cache.hpp"

namespace inferlab {

struct DecoderKVState {
  RequestId request_id = 0;
  DecoderBlockConfig config;
  PagedKVCacheConfig cache_config;
  RequestBlocks blocks;
  std::vector<float> key_cache;
  std::vector<float> value_cache;
  std::vector<float> physical_key_cache;
  std::vector<float> physical_value_cache;
};

struct DecoderKVTrace {
  int prompt_tokens = 0;
  int decode_steps = 0;
  int final_tokens = 0;
  int used_blocks = 0;
  int free_blocks = 0;
  std::size_t key_value_elements = 0;
  std::size_t last_token_output_elements = 0;
  BlockId last_token_block_id = -1;
  int last_token_block_offset = 0;
  bool used_block_table_gather = false;
  float prefill_key_checksum = 0.0F;
  float decode_key_checksum = 0.0F;
  float last_token_output_checksum = 0.0F;
};

struct DecoderKVCacheProfile {
  int prompt_tokens = 0;
  int decode_steps = 0;
  int final_tokens = 0;
  int hidden_size = 0;
  int num_heads = 0;
  int head_dim = 0;
  int block_size_tokens = 0;
  int final_logical_blocks = 0;
  int kv_element_size_bytes = 0;
  std::int64_t kv_elements_per_token = 0;
  std::int64_t prefill_kv_write_elements = 0;
  std::int64_t decode_kv_append_write_elements = 0;
  std::int64_t final_kv_elements = 0;
  std::int64_t final_logical_kv_bytes = 0;
  std::int64_t final_reserved_kv_bytes = 0;
  double final_kv_utilization = 0.0;
  std::int64_t decode_attention_score_elements = 0;
  std::int64_t decode_attention_kv_read_elements = 0;
  std::int64_t paged_gather_read_elements = 0;
  std::int64_t paged_gather_write_elements = 0;
  std::int64_t block_table_lookup_count = 0;
  std::int64_t block_table_lookup_bytes = 0;
  std::int64_t current_impl_physical_materialize_read_elements = 0;
  std::int64_t current_impl_physical_materialize_write_elements = 0;
  double average_attention_kv_read_elements_per_decode_step = 0.0;
};

struct DecoderKVRunOutput {
  DecoderKVState state;
  std::vector<float> last_token_hidden;
  DecoderKVTrace trace;
  DecoderKVCacheProfile cache_profile;
};

DecoderKVState decoder_prefill_cpu(const std::vector<float>& prompt_input,
                                   const DecoderBlockWeights& weights,
                                   const DecoderBlockConfig& config,
                                   const PagedKVCacheConfig& cache_config,
                                   RequestId request_id);

std::vector<float> decoder_decode_cpu(DecoderKVState& state,
                                      const std::vector<float>& token_input,
                                      const DecoderBlockWeights& weights);

DecoderKVRunOutput decoder_prefill_decode_cpu(
    const std::vector<float>& prompt_input,
    const std::vector<float>& decode_inputs, int decode_steps,
    const DecoderBlockWeights& weights, const DecoderBlockConfig& config,
    const PagedKVCacheConfig& cache_config, RequestId request_id);

DecoderKVCacheProfile profile_decoder_kv_cache(
    int prompt_tokens, int decode_steps, const DecoderBlockConfig& config,
    const PagedKVCacheConfig& cache_config);

}  // namespace inferlab
