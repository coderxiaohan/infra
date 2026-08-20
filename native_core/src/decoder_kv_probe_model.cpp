#include "decoder_kv_probe_model.hpp"

#include "checked_index.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace inferlab::decoder_kv_probe {
namespace {

constexpr int kTotalCacheBlocks = 64;
constexpr RequestId kRequestId = 1;

struct DerivedRequest {
  int final_tokens = 0;
  int cache_token_capacity = 0;
  PagedKVCacheConfig cache_config;
  probe::DecoderProbeFixturePlan fixture;
};

std::vector<float> build_inputs(const int rows, const int hidden_size,
                                const int seed) {
  std::vector<float> input(internal::checked_product_size(
      rows, hidden_size, "decoder KV probe input"));
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < hidden_size; ++col) {
      const int centered = ((row * 17 + col * 7 + seed) % 19) - 9;
      input[internal::checked_flat_index(
          row, hidden_size, col, "decoder KV input")] =
          static_cast<float>(centered) / 9.0F;
    }
  }
  return input;
}

std::vector<float> build_weight(const int rows, const int cols,
                                const int seed) {
  std::vector<float> weights(internal::checked_product_size(
      rows, cols, "decoder KV probe weight"));
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float diagonal = row == col ? 0.5F : 0.0F;
      const int centered = ((row * 13 + col * 5 + seed) % 17) - 8;
      weights[internal::checked_flat_index(
          row, cols, col, "decoder KV weight")] =
          diagonal + static_cast<float>(centered) / 96.0F;
    }
  }
  return weights;
}

bool same_cache_config(const PagedKVCacheConfig& lhs,
                       const PagedKVCacheConfig& rhs) {
  return lhs.num_blocks == rhs.num_blocks &&
         lhs.block_size_tokens == rhs.block_size_tokens &&
         lhs.bytes_per_token == rhs.bytes_per_token;
}

bool same_trace(const DecoderKVTrace& lhs, const DecoderKVTrace& rhs) {
  return lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.decode_steps == rhs.decode_steps &&
         lhs.final_tokens == rhs.final_tokens &&
         lhs.used_blocks == rhs.used_blocks &&
         lhs.free_blocks == rhs.free_blocks &&
         lhs.key_value_elements == rhs.key_value_elements &&
         lhs.last_token_output_elements == rhs.last_token_output_elements &&
         lhs.last_token_block_id == rhs.last_token_block_id &&
         lhs.last_token_block_offset == rhs.last_token_block_offset &&
         lhs.used_block_table_gather == rhs.used_block_table_gather &&
         lhs.prefill_key_checksum == rhs.prefill_key_checksum &&
         lhs.decode_key_checksum == rhs.decode_key_checksum &&
         lhs.last_token_output_checksum == rhs.last_token_output_checksum;
}

bool same_cache_profile(const DecoderKVCacheProfile& lhs,
                        const DecoderKVCacheProfile& rhs) {
  return lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.decode_steps == rhs.decode_steps &&
         lhs.final_tokens == rhs.final_tokens &&
         lhs.hidden_size == rhs.hidden_size &&
         lhs.num_heads == rhs.num_heads &&
         lhs.head_dim == rhs.head_dim &&
         lhs.block_size_tokens == rhs.block_size_tokens &&
         lhs.final_logical_blocks == rhs.final_logical_blocks &&
         lhs.kv_element_size_bytes == rhs.kv_element_size_bytes &&
         lhs.kv_elements_per_token == rhs.kv_elements_per_token &&
         lhs.prefill_kv_write_elements == rhs.prefill_kv_write_elements &&
         lhs.decode_kv_append_write_elements ==
             rhs.decode_kv_append_write_elements &&
         lhs.final_kv_elements == rhs.final_kv_elements &&
         lhs.final_logical_kv_bytes == rhs.final_logical_kv_bytes &&
         lhs.final_reserved_kv_bytes == rhs.final_reserved_kv_bytes &&
         lhs.final_kv_utilization == rhs.final_kv_utilization &&
         lhs.decode_attention_score_elements ==
             rhs.decode_attention_score_elements &&
         lhs.decode_attention_kv_read_elements ==
             rhs.decode_attention_kv_read_elements &&
         lhs.paged_gather_read_elements == rhs.paged_gather_read_elements &&
         lhs.paged_gather_write_elements == rhs.paged_gather_write_elements &&
         lhs.block_table_lookup_count == rhs.block_table_lookup_count &&
         lhs.block_table_lookup_bytes == rhs.block_table_lookup_bytes &&
         lhs.current_impl_physical_materialize_read_elements ==
             rhs.current_impl_physical_materialize_read_elements &&
         lhs.current_impl_physical_materialize_write_elements ==
             rhs.current_impl_physical_materialize_write_elements &&
         lhs.average_attention_kv_read_elements_per_decode_step ==
             rhs.average_attention_kv_read_elements_per_decode_step;
}

DerivedRequest derive_request(const Request& request) {
  const auto& config = request.decoder_config;
  if (config.seq_len <= 0 || config.hidden_size <= 0 ||
      config.num_heads <= 0 || config.ffn_intermediate_size <= 0 ||
      request.decode_steps <= 0 || request.block_size_tokens <= 0 ||
      !std::isfinite(config.rms_epsilon) || config.rms_epsilon <= 0.0F ||
      !std::isfinite(config.rope_base) || config.rope_base <= 0.0F) {
    throw std::invalid_argument(
        "decoder KV probe dimensions and constants must be positive");
  }
  if (config.hidden_size % config.num_heads != 0) {
    throw std::invalid_argument("hidden-size must be divisible by heads");
  }
  DerivedRequest derived;
  derived.final_tokens = internal::checked_add_int(
      config.seq_len, request.decode_steps,
      "decoder KV probe final tokens");
  derived.cache_token_capacity = internal::checked_multiply_int(
      kTotalCacheBlocks, request.block_size_tokens,
      "decoder KV probe token capacity");
  if (derived.final_tokens > derived.cache_token_capacity) {
    throw std::invalid_argument(
        "prompt plus decode tokens exceed decoder KV probe cache capacity");
  }
  derived.fixture = probe::plan_decoder_probe_fixture(
      config.hidden_size, config.ffn_intermediate_size,
      derived.final_tokens);
  const int head_dim = config.hidden_size / config.num_heads;
  const int kv_bytes_per_token = internal::checked_int_from_i64(
      internal::checked_product_i64(
          config.num_heads, head_dim, 2,
          static_cast<std::int64_t>(sizeof(float)),
          "decoder KV probe bytes per token"),
      "decoder KV probe bytes per token");
  derived.cache_config = {
      kTotalCacheBlocks, request.block_size_tokens, kv_bytes_per_token};
  return derived;
}

DecoderKVRunOutput execute_request(const Request& request,
                                   const DerivedRequest& derived) {
  const auto& config = request.decoder_config;
  const auto prompt =
      build_inputs(config.seq_len, config.hidden_size, 1);
  const auto decode_inputs =
      build_inputs(request.decode_steps, config.hidden_size, 23);
  const auto weights = probe::build_decoder_probe_weights(
      config.hidden_size, config.ffn_intermediate_size, build_weight);
  return decoder_prefill_decode_cpu(
      prompt, decode_inputs, request.decode_steps, weights, config,
      derived.cache_config, kRequestId);
}

bool same_fixture(const probe::DecoderProbeFixturePlan& lhs,
                  const probe::DecoderProbeFixturePlan& rhs) {
  return lhs.input_elements == rhs.input_elements &&
         lhs.weight_elements == rhs.weight_elements &&
         lhs.total_elements == rhs.total_elements;
}

}  // namespace

Plan build_plan(const Request& request) {
  const auto derived = derive_request(request);
  Plan plan;
  plan.request = request;
  plan.cache_config = derived.cache_config;
  plan.fixture = derived.fixture;
  {
    auto output = execute_request(request, derived);
    plan.trace = output.trace;
    plan.cache_profile = output.cache_profile;
    plan.last_token_hidden = std::move(output.last_token_hidden);
  }
  plan.cache_token_capacity = derived.cache_token_capacity;
  plan.last_token_output_abs_sum =
      internal::checked_float_vector_abs_sum_as_double(
          plan.last_token_hidden,
          "decoder KV probe last-token output");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  const auto derived = derive_request(plan.request);
  if (!same_cache_config(plan.cache_config, derived.cache_config) ||
      !same_fixture(plan.fixture, derived.fixture) ||
      plan.cache_token_capacity != derived.cache_token_capacity) {
    throw std::logic_error(
        "decoder KV probe derived plan is inconsistent");
  }
  const auto expected = execute_request(plan.request, derived);
  if (!same_trace(plan.trace, expected.trace) ||
      !same_cache_profile(plan.cache_profile, expected.cache_profile) ||
      plan.last_token_hidden != expected.last_token_hidden) {
    throw std::logic_error(
        "decoder KV probe result does not match canonical execution");
  }
  if (plan.trace.prompt_tokens != plan.request.decoder_config.seq_len ||
      plan.trace.decode_steps != plan.request.decode_steps ||
      plan.trace.final_tokens != derived.final_tokens ||
      plan.last_token_hidden.size() !=
          static_cast<std::size_t>(
              plan.request.decoder_config.hidden_size)) {
    throw std::logic_error(
        "decoder KV probe trace does not match request");
  }
  const double expected_abs_sum =
      internal::checked_float_vector_abs_sum_as_double(
          plan.last_token_hidden,
          "decoder KV probe last-token output");
  if (!std::isfinite(plan.last_token_output_abs_sum) ||
      plan.last_token_output_abs_sum != expected_abs_sum) {
    throw std::logic_error(
        "decoder KV probe output summary is inconsistent");
  }
}

}  // namespace inferlab::decoder_kv_probe
