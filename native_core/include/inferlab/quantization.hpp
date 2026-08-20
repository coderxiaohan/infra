#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace inferlab {

enum class QuantizationBits {
  kInt8 = 8,
  kInt4 = 4,
};

struct QuantizedMatrix {
  int rows = 0;
  int cols = 0;
  int group_size = 0;
  QuantizationBits bits = QuantizationBits::kInt8;
  std::vector<float> scales;
  // A valid populated matrix has exactly one payload according to bits.
  std::vector<std::int8_t> int8_data;
  std::vector<std::uint8_t> int4_packed_data;
};

struct QuantizationErrorStats {
  float max_abs_error = 0.0F;
  float mean_abs_error = 0.0F;
};

QuantizedMatrix quantize_symmetric_per_group(const std::vector<float>& weights,
                                             int rows, int cols,
                                             int group_size,
                                             QuantizationBits bits);

QuantizedMatrix quantize_symmetric_per_channel(
    const std::vector<float>& weights, int rows, int cols,
    QuantizationBits bits);

std::vector<float> dequantize(const QuantizedMatrix& matrix);

// Returns payload plus scale storage after validating the matrix contract.
std::size_t quantized_storage_bytes(const QuantizedMatrix& matrix);

std::vector<float> weight_only_matvec(const QuantizedMatrix& matrix,
                                      const std::vector<float>& activation);

std::vector<float> fused_dequant_matvec(const QuantizedMatrix& matrix,
                                        const std::vector<float>& activation);

QuantizationErrorStats error_stats(const std::vector<float>& reference,
                                   const std::vector<float>& actual);

// Inputs are saturated to the signed INT4 storage range [-8, 7].
std::uint8_t pack_int4_pair(std::int8_t low, std::int8_t high);
std::int8_t unpack_int4_low(std::uint8_t packed);
std::int8_t unpack_int4_high(std::uint8_t packed);

}  // namespace inferlab
