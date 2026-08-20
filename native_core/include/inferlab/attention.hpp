#pragma once

#include <cstdint>
#include <vector>

namespace inferlab {

struct AttentionShape {
  int num_heads = 0;
  int query_length = 0;
  int key_length = 0;
  int head_dim = 0;
};

struct AttentionReadProfile {
  std::int64_t score_elements = 0;
  std::int64_t unmasked_score_elements = 0;
  std::int64_t masked_score_elements = 0;
  std::int64_t query_element_reads_for_scores = 0;
  std::int64_t key_element_reads_for_scores = 0;
  std::int64_t value_element_reads_dense = 0;
  std::int64_t value_element_reads_mask_aware = 0;
  std::int64_t kv_element_reads_dense = 0;
  std::int64_t kv_element_reads_mask_aware = 0;
  std::int64_t score_intermediate_bytes = 0;
  std::int64_t probability_intermediate_bytes = 0;
  std::int64_t total_intermediate_bytes = 0;
  std::int64_t output_write_elements = 0;
  std::int64_t qk_multiply_add_ops = 0;
  std::int64_t value_multiply_add_ops_dense = 0;
  std::int64_t value_multiply_add_ops_mask_aware = 0;
  double causal_mask_sparsity = 0.0;
};

AttentionReadProfile profile_attention_reads(const AttentionShape& shape,
                                             bool causal);

std::vector<float> scaled_dot_product_attention_cpu(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape);

std::vector<float> scaled_dot_product_attention_cpu(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape, bool causal);

}  // namespace inferlab
