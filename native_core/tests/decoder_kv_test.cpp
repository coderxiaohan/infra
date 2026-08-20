#include "inferlab/decoder_kv.hpp"
#include "test_error_redaction.hpp"

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

inferlab::DecoderBlockWeights test_weights(const int hidden_size,
                                           const int ffn_size) {
  inferlab::DecoderBlockWeights weights;
  weights.rms_gamma.assign(static_cast<std::size_t>(hidden_size), 1.0F);
  weights.q_weight = identity_matrix(hidden_size);
  weights.k_weight = identity_matrix(hidden_size);
  weights.v_weight = identity_matrix(hidden_size);
  weights.o_weight = identity_matrix(hidden_size);
  weights.post_attention_rms_gamma.assign(static_cast<std::size_t>(hidden_size),
                                          1.0F);
  weights.gate_weight.assign(static_cast<std::size_t>(hidden_size * ffn_size),
                             0.02F);
  weights.up_weight.assign(static_cast<std::size_t>(hidden_size * ffn_size),
                           0.03F);
  weights.down_weight.assign(static_cast<std::size_t>(ffn_size * hidden_size),
                             0.04F);
  return weights;
}

void test_prefill_records_kv_cache_and_blocks() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F, 8};
  const inferlab::PagedKVCacheConfig cache_config{8, 2, 64};
  const std::vector<float> prompt = {
      1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
  };

  const auto state = inferlab::decoder_prefill_cpu(
      prompt, test_weights(config.hidden_size, config.ffn_intermediate_size),
      config, cache_config, 1);

  require(state.blocks.token_count == 3, "prefill token count mismatch");
  require(state.blocks.block_table.size() == 2,
          "prefill should allocate two blocks");
  require(state.key_cache.size() == 24, "prefill key cache size mismatch");
  require(state.value_cache.size() == 24, "prefill value cache size mismatch");
  require(state.physical_key_cache.size() == 128,
          "prefill physical key cache size mismatch");
  require(state.physical_value_cache.size() == 128,
          "prefill physical value cache size mismatch");
}

void test_decode_appends_single_token_and_crosses_block() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F, 8};
  const inferlab::PagedKVCacheConfig cache_config{8, 2, 64};
  const std::vector<float> prompt(24, 0.25F);
  const std::vector<float> decode_token(8, 0.5F);
  const auto weights =
      test_weights(config.hidden_size, config.ffn_intermediate_size);
  auto state =
      inferlab::decoder_prefill_cpu(prompt, weights, config, cache_config, 1);

  const auto output = inferlab::decoder_decode_cpu(state, decode_token, weights);

  require(output.size() == 8, "decode output size mismatch");
  require(state.blocks.token_count == 4, "decode token count mismatch");
  require(state.blocks.block_table.size() == 2,
          "four tokens with block size two should use two blocks");
  require(state.key_cache.size() == 32, "decode key cache size mismatch");
  require(state.physical_key_cache.size() == 128,
          "decode physical key cache size mismatch");
}

void test_prefill_decode_trace_records_last_location() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F, 8};
  const inferlab::PagedKVCacheConfig cache_config{8, 2, 64};
  const std::vector<float> prompt(24, 0.25F);
  const std::vector<float> decode_inputs(16, 0.5F);
  const auto output = inferlab::decoder_prefill_decode_cpu(
      prompt, decode_inputs, 2,
      test_weights(config.hidden_size, config.ffn_intermediate_size), config,
      cache_config, 1);

  require(output.trace.prompt_tokens == 3, "trace prompt token mismatch");
  require(output.trace.decode_steps == 2, "trace decode step mismatch");
  require(output.trace.final_tokens == 5, "trace final token mismatch");
  require(output.trace.used_blocks == 3, "trace used block mismatch");
  require(output.trace.last_token_block_offset == 0,
          "trace last token offset mismatch");
  require(output.trace.last_token_output_elements == 8,
          "trace output element count mismatch");
  require(output.trace.used_block_table_gather,
          "trace should record block-table gather path");
}

void test_decode_rejects_bad_token_shape() {
  bool threw = false;
  try {
    const inferlab::DecoderBlockConfig config{2, 8, 2, 10000.0F, 1e-6F, 8};
    const inferlab::PagedKVCacheConfig cache_config{8, 2, 64};
    const auto weights =
        test_weights(config.hidden_size, config.ffn_intermediate_size);
    auto state = inferlab::decoder_prefill_cpu(std::vector<float>(16, 0.25F),
                                               weights, config, cache_config, 1);
    (void)inferlab::decoder_decode_cpu(state, std::vector<float>(7, 0.5F),
                                       weights);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "bad decode token shape did not throw");
}

void test_decoder_kv_cache_profile_counts_decode_traffic() {
  const inferlab::DecoderBlockConfig config{3, 8, 2, 10000.0F, 1e-6F, 8};
  const inferlab::PagedKVCacheConfig cache_config{8, 2, 64};
  const auto profile =
      inferlab::profile_decoder_kv_cache(3, 2, config, cache_config);

  require(profile.final_tokens == 5, "profile final tokens mismatch");
  require(profile.head_dim == 4, "profile head dim mismatch");
  require(profile.final_logical_blocks == 3,
          "profile logical block count mismatch");
  require(profile.kv_elements_per_token == 16,
          "profile KV elements per token mismatch");
  require(profile.prefill_kv_write_elements == 48,
          "profile prefill KV writes mismatch");
  require(profile.decode_kv_append_write_elements == 32,
          "profile decode append writes mismatch");
  require(profile.final_kv_elements == 80,
          "profile final KV elements mismatch");
  require(profile.final_logical_kv_bytes == 320,
          "profile logical KV bytes mismatch");
  require(profile.final_reserved_kv_bytes == 384,
          "profile reserved KV bytes mismatch");
  require(profile.decode_attention_score_elements == 18,
          "profile attention score elements mismatch");
  require(profile.decode_attention_kv_read_elements == 144,
          "profile attention KV reads mismatch");
  require(profile.paged_gather_read_elements == 144,
          "profile gather reads mismatch");
  require(profile.paged_gather_write_elements == 144,
          "profile gather writes mismatch");
  require(profile.block_table_lookup_count == 18,
          "profile block lookup count mismatch");
  require(profile.block_table_lookup_bytes == 72,
          "profile block lookup bytes mismatch");
  require(profile.current_impl_physical_materialize_write_elements == 144,
          "profile materialize writes mismatch");
}

}  // namespace

int main() {
  try {
    test_prefill_records_kv_cache_and_blocks();
    test_decode_appends_single_token_and_crosses_block();
    test_prefill_decode_trace_records_last_location();
    test_decode_rejects_bad_token_shape();
    test_decoder_kv_cache_profile_counts_decode_traffic();
  } catch (const std::exception& error) {
    std::cerr << "decoder_kv_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
