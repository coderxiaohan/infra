#include "inferlab/kv_cache.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <stdexcept>

namespace inferlab {

double KVCacheEstimate::mib_for_bytes(const std::int64_t bytes) {
  if (bytes < 0) {
    throw std::invalid_argument("KV byte count must be non-negative");
  }
  return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

double KVCacheEstimate::total_mib() const {
  return mib_for_bytes(total_bytes);
}

int dtype_bytes(const DType dtype) {
  return (dtype_bits(dtype) + 7) / 8;
}

int dtype_bits(const DType dtype) {
  switch (dtype) {
    case DType::kFloat32:
      return 32;
    case DType::kBFloat16:
    case DType::kFloat16:
      return 16;
    case DType::kInt8:
      return 8;
    case DType::kInt4:
      return 4;
  }
  throw std::runtime_error("unhandled dtype");
}

DType parse_dtype(const std::string& value) {
  if (value == "fp32" || value == "float32") {
    return DType::kFloat32;
  }
  if (value == "bf16") {
    return DType::kBFloat16;
  }
  if (value == "fp16" || value == "float16") {
    return DType::kFloat16;
  }
  if (value == "int8") {
    return DType::kInt8;
  }
  if (value == "int4") {
    return DType::kInt4;
  }
  throw std::invalid_argument("unknown dtype: " + value);
}

std::string dtype_name(const DType dtype) {
  switch (dtype) {
    case DType::kFloat32:
      return "fp32";
    case DType::kBFloat16:
      return "bf16";
    case DType::kFloat16:
      return "fp16";
    case DType::kInt8:
      return "int8";
    case DType::kInt4:
      return "int4";
  }
  throw std::runtime_error("unhandled dtype");
}

void validate(const KVCacheConfig& config) {
  const std::vector<std::pair<std::string, int>> fields = {
      {"batch_size", config.batch_size},
      {"sequence_length", config.sequence_length},
      {"num_layers", config.num_layers},
      {"num_heads", config.num_heads},
      {"head_dim", config.head_dim},
  };
  const auto invalid = std::find_if(fields.begin(), fields.end(), [](const auto& field) {
    return field.second <= 0;
  });
  if (invalid != fields.end()) {
    throw std::invalid_argument(invalid->first + " must be positive");
  }
}

KVCacheEstimate estimate_kv_cache(const KVCacheConfig& config) {
  validate(config);
  const auto bits = dtype_bits(config.dtype);
  KVCacheEstimate estimate;
  estimate.key_shape = {
      config.batch_size,
      config.num_heads,
      config.sequence_length,
      config.head_dim,
  };
  estimate.value_shape = estimate.key_shape;
  const auto kv_elements_per_token_per_layer = internal::checked_product_i64(
      2, config.batch_size, config.num_heads, config.head_dim,
      "KV elements per token per layer");
  estimate.bytes_per_token_per_layer =
      internal::checked_packed_bytes_for_bits(kv_elements_per_token_per_layer,
                                              bits,
                                              "KV bytes per token per layer");
  estimate.total_bytes = internal::checked_product_i64(
      estimate.bytes_per_token_per_layer, config.sequence_length,
      config.num_layers, "KV total bytes");
  return estimate;
}

}  // namespace inferlab
