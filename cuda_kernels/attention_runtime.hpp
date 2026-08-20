#pragma once

#include <vector>

#include "inferlab/attention.hpp"

namespace inferlab {

struct CudaAttentionResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> scaled_dot_product_attention_cuda_runtime(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape, bool causal);

CudaAttentionResult scaled_dot_product_attention_cuda_runtime_profiled(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape, bool causal);

std::vector<float> scaled_dot_product_attention_shared_cuda_runtime(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape, bool causal);

CudaAttentionResult scaled_dot_product_attention_shared_cuda_runtime_profiled(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape, bool causal);

}  // namespace inferlab
