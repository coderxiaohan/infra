#include "inferlab/attention.hpp"

#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "checked_index.hpp"
#include "inferlab/matrix_ops.hpp"

namespace inferlab {
namespace {

void validate_attention_shape(const AttentionShape& shape) {
  if (shape.num_heads <= 0 || shape.query_length <= 0 ||
      shape.key_length <= 0 || shape.head_dim <= 0) {
    throw std::invalid_argument("attention dimensions must be positive");
  }
}

void validate_attention_inputs(const std::vector<float>& query,
                               const std::vector<float>& key,
                               const std::vector<float>& value,
                               const AttentionShape& shape) {
  validate_attention_shape(shape);
  const auto query_size = internal::checked_product_size(
      shape.num_heads, shape.query_length, shape.head_dim, "query");
  const auto kv_size = internal::checked_product_size(
      shape.num_heads, shape.key_length, shape.head_dim, "key/value");
  if (query.size() != query_size) {
    throw std::invalid_argument("query size mismatch");
  }
  if (key.size() != kv_size) {
    throw std::invalid_argument("key size mismatch");
  }
  if (value.size() != kv_size) {
    throw std::invalid_argument("value size mismatch");
  }
  internal::validate_all_finite(query, "attention query");
  internal::validate_all_finite(key, "attention key");
  internal::validate_all_finite(value, "attention value");
}

std::size_t q_index(const AttentionShape& shape, const int head,
                    const int token, const int dim) {
  return internal::checked_flat_index3(head, shape.query_length, token,
                                       shape.head_dim, dim, "query");
}

std::size_t kv_index(const AttentionShape& shape, const int head,
                     const int token, const int dim) {
  return internal::checked_flat_index3(head, shape.key_length, token,
                                       shape.head_dim, dim, "key/value");
}

std::int64_t checked_triangular_count(const std::int64_t value,
                                      const char* name) {
  const auto next = internal::checked_add_i64(value, 1, name);
  const auto even_first = (value % 2) == 0;
  const auto lhs = even_first ? value / 2 : value;
  const auto rhs = even_first ? next : next / 2;
  return internal::checked_product_i64(lhs, rhs, name);
}

}  // namespace

AttentionReadProfile profile_attention_reads(const AttentionShape& shape,
                                             const bool causal) {
  validate_attention_shape(shape);
  if (causal && shape.query_length != shape.key_length) {
    throw std::invalid_argument(
        "causal attention profile requires query_length == key_length");
  }

  const auto heads = static_cast<std::int64_t>(shape.num_heads);
  const auto queries = static_cast<std::int64_t>(shape.query_length);
  const auto keys = static_cast<std::int64_t>(shape.key_length);
  const auto head_dim = static_cast<std::int64_t>(shape.head_dim);
  constexpr std::int64_t element_bytes =
      static_cast<std::int64_t>(sizeof(float));
  const auto dense_score_pairs = internal::checked_product_i64(
      heads, queries, keys, "attention dense score pairs");
  const auto unmasked_pairs =
      causal
          ? internal::checked_product_i64(
                heads, checked_triangular_count(
                           queries, "attention causal score pairs per head"),
                "attention causal score pairs")
          : dense_score_pairs;

  AttentionReadProfile profile;
  profile.score_elements = dense_score_pairs;
  profile.unmasked_score_elements = unmasked_pairs;
  profile.masked_score_elements = internal::checked_add_i64(
      dense_score_pairs, -unmasked_pairs, "attention masked score pairs");
  profile.query_element_reads_for_scores = internal::checked_product_i64(
      unmasked_pairs, head_dim, "attention query reads for scores");
  profile.key_element_reads_for_scores = internal::checked_product_i64(
      unmasked_pairs, head_dim, "attention key reads for scores");
  profile.value_element_reads_dense = internal::checked_product_i64(
      dense_score_pairs, head_dim, "attention dense value reads");
  profile.value_element_reads_mask_aware = internal::checked_product_i64(
      unmasked_pairs, head_dim, "attention mask-aware value reads");
  profile.kv_element_reads_dense = internal::checked_add_i64(
      profile.key_element_reads_for_scores, profile.value_element_reads_dense,
      "attention dense KV reads");
  profile.kv_element_reads_mask_aware =
      internal::checked_add_i64(profile.key_element_reads_for_scores,
                                profile.value_element_reads_mask_aware,
                                "attention mask-aware KV reads");
  profile.score_intermediate_bytes = internal::checked_product_i64(
      profile.score_elements, element_bytes, "attention score bytes");
  profile.probability_intermediate_bytes = internal::checked_product_i64(
      profile.score_elements, element_bytes, "attention probability bytes");
  profile.total_intermediate_bytes = internal::checked_add_i64(
      profile.score_intermediate_bytes, profile.probability_intermediate_bytes,
      "attention intermediate bytes");
  profile.output_write_elements = internal::checked_product_i64(
      heads, queries, head_dim, "attention output writes");
  profile.qk_multiply_add_ops = internal::checked_product_i64(
      unmasked_pairs, head_dim, 2, "attention QK multiply-add ops");
  profile.value_multiply_add_ops_dense = internal::checked_product_i64(
      dense_score_pairs, head_dim, 2, "attention dense value multiply-add ops");
  profile.value_multiply_add_ops_mask_aware =
      internal::checked_product_i64(unmasked_pairs, head_dim, 2,
                                    "attention mask-aware value multiply-add ops");
  if (profile.score_elements > 0) {
    profile.causal_mask_sparsity =
        static_cast<double>(profile.masked_score_elements) /
        static_cast<double>(profile.score_elements);
  }
  return profile;
}

std::vector<float> scaled_dot_product_attention_cpu(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape) {
  return scaled_dot_product_attention_cpu(query, key, value, shape, false);
}

std::vector<float> scaled_dot_product_attention_cpu(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape,
    const bool causal) {
  validate_attention_inputs(query, key, value, shape);
  if (causal && shape.query_length != shape.key_length) {
    throw std::invalid_argument(
        "causal attention currently requires query_length == key_length");
  }

  std::vector<float> output(
      internal::checked_product_size(shape.num_heads, shape.query_length,
                                     shape.head_dim, "attention output"),
      0.0F);
  const float scale = 1.0F / std::sqrt(static_cast<float>(shape.head_dim));

  for (int head = 0; head < shape.num_heads; ++head) {
    std::vector<float> scores(
        internal::checked_product_size(shape.query_length, shape.key_length,
                                       "attention scores"),
        0.0F);
    for (int q = 0; q < shape.query_length; ++q) {
      for (int k = 0; k < shape.key_length; ++k) {
        if (causal && k > q) {
          scores[internal::checked_flat_index(q, shape.key_length, k,
                                              "attention scores")] =
              -std::numeric_limits<float>::infinity();
          continue;
        }
        float dot = 0.0F;
        for (int dim = 0; dim < shape.head_dim; ++dim) {
          dot += query[q_index(shape, head, q, dim)] *
                 key[kv_index(shape, head, k, dim)];
        }
        if (!std::isfinite(dot)) {
          throw std::invalid_argument("attention score dot product must be finite");
        }
        const float score = dot * scale;
        if (!std::isfinite(score)) {
          throw std::invalid_argument("attention score must be finite");
        }
        scores[internal::checked_flat_index(q, shape.key_length, k,
                                            "attention scores")] = score;
      }
    }

    const auto probabilities =
        row_softmax_cpu(scores, shape.query_length, shape.key_length);

    for (int q = 0; q < shape.query_length; ++q) {
      for (int dim = 0; dim < shape.head_dim; ++dim) {
        float sum = 0.0F;
        for (int k = 0; k < shape.key_length; ++k) {
          sum += probabilities[internal::checked_flat_index(q, shape.key_length,
                                                            k,
                                                            "attention probs")] *
                 value[kv_index(shape, head, k, dim)];
        }
        if (!std::isfinite(sum)) {
          throw std::invalid_argument("attention output must be finite");
        }
        output[q_index(shape, head, q, dim)] = sum;
      }
    }
  }
  return output;
}

}  // namespace inferlab
