#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inferlab {

enum class DType {
  kFloat32,
  kBFloat16,
  kFloat16,
  kInt8,
  kInt4,
};

struct KVCacheConfig {
  int batch_size = 1;
  int sequence_length = 128;
  int num_layers = 12;
  int num_heads = 12;
  int head_dim = 64;
  DType dtype = DType::kFloat16;
};

struct KVCacheEstimate {
  std::vector<int> key_shape;
  std::vector<int> value_shape;
  std::int64_t bytes_per_token_per_layer = 0;
  std::int64_t total_bytes = 0;

  static double mib_for_bytes(std::int64_t bytes);
  double total_mib() const;
};

int dtype_bytes(DType dtype);
int dtype_bits(DType dtype);
DType parse_dtype(const std::string& value);
std::string dtype_name(DType dtype);
void validate(const KVCacheConfig& config);
KVCacheEstimate estimate_kv_cache(const KVCacheConfig& config);

}  // namespace inferlab
