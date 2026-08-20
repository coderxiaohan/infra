#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inferlab {

enum class GGUFMetadataType : std::uint32_t {
  kUInt8 = 0,
  kInt8 = 1,
  kUInt16 = 2,
  kInt16 = 3,
  kUInt32 = 4,
  kInt32 = 5,
  kFloat32 = 6,
  kBool = 7,
  kString = 8,
  kArray = 9,
  kUInt64 = 10,
  kInt64 = 11,
  kFloat64 = 12,
};

struct GGUFMetadataValue {
  GGUFMetadataType type = GGUFMetadataType::kUInt32;
  GGUFMetadataType array_type = GGUFMetadataType::kUInt32;
  std::uint64_t uint_value = 0;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  bool bool_value = false;
  std::string string_value;
  std::vector<GGUFMetadataValue> array_values;
};

struct GGUFMetadataKV {
  std::string key;
  GGUFMetadataValue value;
};

struct GGUFTensorTypeInfo {
  std::uint32_t type = 0;
  std::string name = "unknown";
  std::uint64_t block_size = 0;
  std::uint64_t type_size_bytes = 0;
  bool quantized = false;
  bool deprecated = false;
  bool known = false;
};

struct GGUFTensorInfo {
  std::string name;
  std::vector<std::uint64_t> dimensions;
  std::uint32_t type = 0;
  GGUFTensorTypeInfo type_info;
  std::uint64_t relative_offset = 0;
  std::uint64_t absolute_offset = 0;
  std::uint64_t element_count = 0;
  std::uint64_t row_size_bytes = 0;
  std::uint64_t data_size_bytes = 0;
  std::uint64_t data_end_absolute_offset = 0;
  bool byte_size_known = false;
};

struct GGUFFileInfo {
  std::uint32_t version = 0;
  std::uint64_t tensor_count = 0;
  std::uint64_t metadata_kv_count = 0;
  std::uint32_t alignment = 32;
  std::uint64_t file_size_bytes = 0;
  std::uint64_t header_size_bytes = 0;
  std::uint64_t metadata_start_offset = 0;
  std::uint64_t tensor_info_start_offset = 0;
  std::uint64_t tensor_info_end_offset = 0;
  std::uint64_t tensor_data_padding_bytes = 0;
  std::uint64_t tensor_data_start_offset = 0;
  std::vector<GGUFMetadataKV> metadata;
  std::vector<GGUFTensorInfo> tensors;
};

// Format-specific preview records preserve GGUF wire-layout evidence. They are
// scalar inspection results, not a generic production quantized-tensor ABI.

struct GGUFKQuantScaleMin {
  std::uint8_t scale = 0;
  std::uint8_t min = 0;
  float effective_scale = 0.0F;
  float effective_min = 0.0F;
};

using GGUFQ4KScaleMin = GGUFKQuantScaleMin;
using GGUFQ5KScaleMin = GGUFKQuantScaleMin;
using GGUFQ2KScaleMin = GGUFKQuantScaleMin;

struct GGUFQ6KScale {
  std::int8_t scale = 0;
  float effective_scale = 0.0F;
};

using GGUFQ3KScale = GGUFQ6KScale;

struct GGUFQ2KBlockDequant {
  float d = 0.0F;
  float dmin = 0.0F;
  std::vector<GGUFQ2KScaleMin> scale_min_pairs;
  std::vector<float> values;
  std::vector<float> group_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ4_0BlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ1_0BlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> bit_values;
  std::vector<float> values;
  std::uint64_t positive_bit_count = 0;
  std::uint64_t negative_bit_count = 0;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFIQ2XXSBlockPreview {
  float d = 0.0F;
  std::vector<std::uint16_t> qs_words;
  std::vector<std::uint8_t> grid_indices;
  std::vector<std::uint8_t> sign_codes;
  std::vector<std::uint8_t> scale_nibbles;
  std::vector<float> effective_scales;
};

struct GGUFIQ2XXSBlockDequant {
  float d = 0.0F;
  std::vector<std::uint16_t> qs_words;
  std::vector<std::uint8_t> grid_indices;
  std::vector<std::uint8_t> sign_codes;
  std::vector<std::uint8_t> scale_nibbles;
  std::vector<float> effective_scales;
  std::vector<float> values;
  std::vector<float> group_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ4_1BlockDequant {
  float d = 0.0F;
  float m = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ5_0BlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ5_1BlockDequant {
  float d = 0.0F;
  float m = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFIQ4NLBlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::int8_t> codebook_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFMXFP4BlockDequant {
  std::uint8_t e = 0;
  float e_scale = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::int8_t> codebook_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFNVFP4SubBlockScale {
  std::uint8_t raw_scale = 0;
  float scale = 0.0F;
};

struct GGUFNVFP4BlockDequant {
  std::vector<GGUFNVFP4SubBlockScale> scales;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::int8_t> codebook_values;
  std::vector<float> values;
  std::vector<float> subblock_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFF32ValuesPreview {
  std::vector<std::uint32_t> raw_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFF64ValuesPreview {
  std::vector<std::uint64_t> raw_values;
  std::vector<double> values;
  double checksum = 0.0;
  double min = 0.0;
  double max = 0.0;
  double mean = 0.0;
};

struct GGUFInt32ValuesPreview {
  std::vector<std::uint32_t> raw_values;
  std::vector<std::int32_t> values;
  std::int64_t checksum = 0;
  std::int32_t min = 0;
  std::int32_t max = 0;
  double mean = 0.0;
};

struct GGUFInt64ValuesPreview {
  std::vector<std::uint64_t> raw_values;
  std::vector<std::int64_t> values;
  std::int64_t checksum = 0;
  std::int64_t min = 0;
  std::int64_t max = 0;
  double mean = 0.0;
};

struct GGUFInt16ValuesPreview {
  std::vector<std::uint16_t> raw_values;
  std::vector<std::int16_t> values;
  std::int64_t checksum = 0;
  std::int16_t min = 0;
  std::int16_t max = 0;
  double mean = 0.0;
};

struct GGUFInt8ValuesPreview {
  std::vector<std::uint8_t> raw_values;
  std::vector<std::int8_t> values;
  std::int64_t checksum = 0;
  std::int8_t min = 0;
  std::int8_t max = 0;
  double mean = 0.0;
};

struct GGUFF16ValuesPreview {
  std::vector<std::uint16_t> raw_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFBF16ValuesPreview {
  std::vector<std::uint16_t> raw_values;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFIQ4XSScale {
  std::uint8_t raw_scale = 0;
  std::int8_t signed_scale = 0;
  float effective_scale = 0.0F;
};

struct GGUFIQ4XSBlockDequant {
  float d = 0.0F;
  std::uint16_t scales_h = 0;
  std::vector<GGUFIQ4XSScale> scales;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::int8_t> codebook_values;
  std::vector<float> values;
  std::vector<float> subblock_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFTQ1_0BlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::uint64_t> quant_counts;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFTQ2_0BlockDequant {
  float d = 0.0F;
  std::vector<std::uint8_t> quant_values;
  std::vector<std::uint64_t> quant_counts;
  std::vector<float> values;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ3KBlockDequant {
  float d = 0.0F;
  std::vector<GGUFQ3KScale> scales;
  std::vector<float> values;
  std::vector<float> group_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ4KBlockDequant {
  float d = 0.0F;
  float dmin = 0.0F;
  std::vector<GGUFQ4KScaleMin> scale_min_pairs;
  std::vector<float> values;
  std::vector<float> subblock_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ5KBlockDequant {
  float d = 0.0F;
  float dmin = 0.0F;
  std::vector<GGUFQ5KScaleMin> scale_min_pairs;
  std::vector<float> values;
  std::vector<float> subblock_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ6KBlockDequant {
  float d = 0.0F;
  std::vector<GGUFQ6KScale> scales;
  std::vector<float> values;
  std::vector<float> group_sums;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ8_0BlockDequant {
  float d = 0.0F;
  std::vector<std::int8_t> quant_values;
  std::vector<float> values;
  std::int64_t quant_sum = 0;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ8_1BlockDequant {
  float d = 0.0F;
  float s = 0.0F;
  float expected_s_from_quant_sum = 0.0F;
  std::vector<std::int8_t> quant_values;
  std::vector<float> values;
  std::int64_t quant_sum = 0;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

struct GGUFQ8KBlockDequant {
  float d = 0.0F;
  std::vector<std::int8_t> quant_values;
  std::vector<std::int16_t> block_sums;
  std::vector<float> values;
  std::int64_t quant_sum = 0;
  std::int64_t stored_block_sum_total = 0;
  float checksum = 0.0F;
  float min = 0.0F;
  float max = 0.0F;
  float mean = 0.0F;
};

constexpr std::uint64_t kDefaultMaxInMemoryGGUFFileBytes =
    512ULL * 1024ULL * 1024ULL;

GGUFFileInfo parse_gguf_file(
    const std::string& path,
    std::uint64_t max_file_bytes = kDefaultMaxInMemoryGGUFFileBytes);
// The caller already owns this buffer; parser-level metadata and nesting caps
// still apply, while the file-size cap belongs to parse_gguf_file().
GGUFFileInfo parse_gguf_bytes(const std::vector<std::uint8_t>& bytes);

std::string gguf_metadata_type_name(GGUFMetadataType type);
std::string gguf_metadata_value_to_string(const GGUFMetadataValue& value);
GGUFTensorTypeInfo gguf_tensor_type_info(std::uint32_t type);
std::string gguf_tensor_type_name(std::uint32_t type);
float gguf_fp16_to_float32(std::uint16_t raw);
GGUFF32ValuesPreview decode_gguf_f32_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFF64ValuesPreview decode_gguf_f64_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFInt64ValuesPreview decode_gguf_i64_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFInt32ValuesPreview decode_gguf_i32_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFInt16ValuesPreview decode_gguf_i16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFInt8ValuesPreview decode_gguf_i8_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFF16ValuesPreview decode_gguf_f16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
float gguf_bf16_to_float32(std::uint16_t raw);
GGUFBF16ValuesPreview decode_gguf_bf16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    std::uint64_t value_count);
GGUFIQ2XXSBlockPreview preview_gguf_iq2_xxs_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFIQ2XXSBlockDequant dequantize_gguf_iq2_xxs_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ1_0BlockDequant dequantize_gguf_q1_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ2KBlockDequant dequantize_gguf_q2_k_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ4_0BlockDequant dequantize_gguf_q4_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ4_1BlockDequant dequantize_gguf_q4_1_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ5_0BlockDequant dequantize_gguf_q5_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ5_1BlockDequant dequantize_gguf_q5_1_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFIQ4NLBlockDequant dequantize_gguf_iq4_nl_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFMXFP4BlockDequant dequantize_gguf_mxfp4_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFNVFP4BlockDequant dequantize_gguf_nvfp4_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFIQ4XSBlockDequant dequantize_gguf_iq4_xs_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFTQ1_0BlockDequant dequantize_gguf_tq1_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFTQ2_0BlockDequant dequantize_gguf_tq2_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ3KBlockDequant dequantize_gguf_q3_k_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ4KBlockDequant dequantize_gguf_q4_k_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ5KBlockDequant dequantize_gguf_q5_k_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ6KBlockDequant dequantize_gguf_q6_k_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ8_0BlockDequant dequantize_gguf_q8_0_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ8_1BlockDequant dequantize_gguf_q8_1_block(
    const std::vector<std::uint8_t>& block_bytes);
GGUFQ8KBlockDequant dequantize_gguf_q8_k_block(
    const std::vector<std::uint8_t>& block_bytes);

}  // namespace inferlab
