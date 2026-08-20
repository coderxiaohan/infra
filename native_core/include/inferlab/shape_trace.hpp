#pragma once

#include "inferlab/kv_cache.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace inferlab {

struct TransformerShapeConfig {
  int batch_size = 1;
  int seq_len = 128;
  int decode_steps = 16;
  int hidden_size = 768;
  int num_layers = 12;
  int num_heads = 12;
  int head_dim = 64;
  DType dtype = DType::kFloat16;
};

struct DecodeTraceRow {
  int step = 0;
  std::vector<int> query_shape;
  int kv_sequence_length = 0;
  std::int64_t kv_total_bytes = 0;
};

struct ShapeTrace {
  TransformerShapeConfig config;
  std::vector<int> input_shape;
  std::vector<int> qkv_projection_shape;
  std::vector<int> attention_score_shape;
  KVCacheEstimate prefill_kv_cache;
  std::vector<DecodeTraceRow> decode;
};

void validate(const TransformerShapeConfig& config);
ShapeTrace build_trace(const TransformerShapeConfig& config);
std::string to_json(const ShapeTrace& trace);

}  // namespace inferlab
