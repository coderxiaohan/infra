#include "inferlab/gguf.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstring>
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

template <typename T>
void append_le(std::vector<std::uint8_t>& bytes, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    bytes.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
  }
}

void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
  append_le<std::uint64_t>(bytes, value.size());
  bytes.insert(bytes.end(), value.begin(), value.end());
}

void append_metadata_string(std::vector<std::uint8_t>& bytes,
                            const std::string& key,
                            const std::string& value) {
  append_string(bytes, key);
  append_le<std::uint32_t>(
      bytes, static_cast<std::uint32_t>(inferlab::GGUFMetadataType::kString));
  append_string(bytes, value);
}

void append_metadata_u32(std::vector<std::uint8_t>& bytes,
                         const std::string& key, const std::uint32_t value) {
  append_string(bytes, key);
  append_le<std::uint32_t>(
      bytes, static_cast<std::uint32_t>(inferlab::GGUFMetadataType::kUInt32));
  append_le<std::uint32_t>(bytes, value);
}

void append_metadata_string_array(std::vector<std::uint8_t>& bytes,
                                  const std::string& key,
                                  const std::vector<std::string>& values) {
  append_string(bytes, key);
  append_le<std::uint32_t>(
      bytes, static_cast<std::uint32_t>(inferlab::GGUFMetadataType::kArray));
  append_le<std::uint32_t>(
      bytes, static_cast<std::uint32_t>(inferlab::GGUFMetadataType::kString));
  append_le<std::uint64_t>(bytes, values.size());
  for (const auto& value : values) {
    append_string(bytes, value);
  }
}

void append_tensor(std::vector<std::uint8_t>& bytes, const std::string& name,
                   const std::vector<std::uint64_t>& dims,
                   const std::uint32_t type,
                   const std::uint64_t relative_offset) {
  append_string(bytes, name);
  append_le<std::uint32_t>(bytes, dims.size());
  for (const auto dim : dims) {
    append_le<std::uint64_t>(bytes, dim);
  }
  append_le<std::uint32_t>(bytes, type);
  append_le<std::uint64_t>(bytes, relative_offset);
}

void write_u16_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_f32_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  for (std::size_t i = 0; i < sizeof(raw); ++i) {
    bytes[offset + i] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_f64_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const double value) {
  std::uint64_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  for (std::size_t i = 0; i < sizeof(raw); ++i) {
    bytes[offset + i] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_i16_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const std::int16_t value) {
  write_u16_le(bytes, offset, static_cast<std::uint16_t>(value));
}

void write_i32_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const std::int32_t value) {
  const auto raw = static_cast<std::uint32_t>(value);
  for (std::size_t i = 0; i < sizeof(raw); ++i) {
    bytes[offset + i] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_i64_le(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                  const std::int64_t value) {
  const auto raw = static_cast<std::uint64_t>(value);
  for (std::size_t i = 0; i < sizeof(raw); ++i) {
    bytes[offset + i] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_iq2_xxs_group(std::vector<std::uint8_t>& bytes,
                         const std::size_t offset, const std::uint8_t grid0,
                         const std::uint8_t scale_nibble,
                         const std::uint8_t sign0) {
  const auto aux0 = static_cast<std::uint32_t>(grid0) |
                    (static_cast<std::uint32_t>(grid0 + 1U) << 8U) |
                    (static_cast<std::uint32_t>(grid0 + 2U) << 16U) |
                    (static_cast<std::uint32_t>(grid0 + 3U) << 24U);
  const auto aux1 =
      (static_cast<std::uint32_t>(scale_nibble & 0x0FU) << 28U) |
      static_cast<std::uint32_t>(sign0 & 0x7FU) |
      (static_cast<std::uint32_t>((sign0 + 1U) & 0x7FU) << 7U) |
      (static_cast<std::uint32_t>((sign0 + 2U) & 0x7FU) << 14U) |
      (static_cast<std::uint32_t>((sign0 + 3U) & 0x7FU) << 21U);
  for (std::size_t i = 0; i < sizeof(aux0); ++i) {
    bytes[offset + i] =
        static_cast<std::uint8_t>((aux0 >> (8U * i)) & 0xFFU);
    bytes[offset + 4U + i] =
        static_cast<std::uint8_t>((aux1 >> (8U * i)) & 0xFFU);
  }
}

std::vector<std::uint8_t> synthetic_q4_k_block() {
  std::vector<std::uint8_t> block(144, 0);
  write_u16_le(block, 0, 0x3C00);
  write_u16_le(block, 2, 0x3800);
  for (std::size_t i = 0; i < 12; ++i) {
    block[4 + i] = static_cast<std::uint8_t>(i);
  }
  for (std::size_t i = 0; i < 128; ++i) {
    block[16 + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q4_0_block() {
  std::vector<std::uint8_t> block(18, 0);
  write_u16_le(block, 0, 0x3C00);
  for (std::size_t i = 0; i < 16; ++i) {
    block[2 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q1_0_block() {
  std::vector<std::uint8_t> block(18, 0);
  write_u16_le(block, 0, 0x3C00);
  for (std::size_t i = 0; i < 16; ++i) {
    block[2 + i] = 0xAA;
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q4_1_block() {
  std::vector<std::uint8_t> block(20, 0);
  write_u16_le(block, 0, 0x3C00);
  write_u16_le(block, 2, 0x3800);
  for (std::size_t i = 0; i < 16; ++i) {
    block[4 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q5_0_block() {
  std::vector<std::uint8_t> block(22, 0);
  write_u16_le(block, 0, 0x3C00);
  const std::uint8_t high_bits[4] = {0x00, 0xFF, 0x55, 0xAA};
  for (std::size_t i = 0; i < 4; ++i) {
    block[2 + i] = high_bits[i];
  }
  for (std::size_t i = 0; i < 16; ++i) {
    block[6 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q5_1_block() {
  std::vector<std::uint8_t> block(24, 0);
  write_u16_le(block, 0, 0x3C00);
  write_u16_le(block, 2, 0x3800);
  const std::uint8_t high_bits[4] = {0x00, 0xFF, 0x55, 0xAA};
  for (std::size_t i = 0; i < 4; ++i) {
    block[4 + i] = high_bits[i];
  }
  for (std::size_t i = 0; i < 16; ++i) {
    block[8 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_iq4_nl_block() {
  std::vector<std::uint8_t> block(18, 0);
  write_u16_le(block, 0, 0x3800);
  for (std::size_t i = 0; i < 16; ++i) {
    block[2 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_mxfp4_block() {
  std::vector<std::uint8_t> block(17, 0);
  block[0] = 0x80;
  for (std::size_t i = 0; i < 16; ++i) {
    block[1 + i] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }
  return block;
}

std::vector<std::uint8_t> synthetic_nvfp4_block() {
  std::vector<std::uint8_t> block(36, 0);
  const std::uint8_t scales[4] = {0x38, 0x40, 0x30, 0x48};
  for (std::size_t i = 0; i < 4; ++i) {
    block[i] = scales[i];
  }
  for (std::size_t sub_block = 0; sub_block < 4; ++sub_block) {
    for (std::size_t i = 0; i < 8; ++i) {
      block[4 + sub_block * 8 + i] =
          static_cast<std::uint8_t>(i | ((15U - i) << 4U));
    }
  }
  return block;
}

std::vector<std::uint8_t> synthetic_f32_values() {
  std::vector<std::uint8_t> bytes(16, 0);
  write_f32_le(bytes, 0, 1.0F);
  write_f32_le(bytes, 4, -1.0F);
  write_f32_le(bytes, 8, 0.5F);
  write_f32_le(bytes, 12, -0.5F);
  return bytes;
}

std::vector<std::uint8_t> synthetic_f64_values() {
  std::vector<std::uint8_t> bytes(32, 0);
  write_f64_le(bytes, 0, 1.0);
  write_f64_le(bytes, 8, -1.0);
  write_f64_le(bytes, 16, 0.25);
  write_f64_le(bytes, 24, -0.25);
  return bytes;
}

std::vector<std::uint8_t> synthetic_iq2_xxs_block() {
  std::vector<std::uint8_t> bytes(66, 0);
  write_u16_le(bytes, 0, 0x3C00);
  for (std::size_t group = 0; group < 8; ++group) {
    write_iq2_xxs_group(bytes, 2 + group * 8,
                        static_cast<std::uint8_t>(1 + group * 4),
                        static_cast<std::uint8_t>(group + 1),
                        static_cast<std::uint8_t>(1 + group * 4));
  }
  return bytes;
}

std::vector<std::uint8_t> synthetic_i64_values() {
  std::vector<std::uint8_t> bytes(32, 0);
  write_i64_le(bytes, 0, 1);
  write_i64_le(bytes, 8, -1);
  write_i64_le(bytes, 16, 123456789);
  write_i64_le(bytes, 24, -123456789);
  return bytes;
}

std::vector<std::uint8_t> synthetic_i32_values() {
  std::vector<std::uint8_t> bytes(16, 0);
  write_i32_le(bytes, 0, 1);
  write_i32_le(bytes, 4, -1);
  write_i32_le(bytes, 8, 123456);
  write_i32_le(bytes, 12, -123456);
  return bytes;
}

std::vector<std::uint8_t> synthetic_i16_values() {
  std::vector<std::uint8_t> bytes(8, 0);
  write_i16_le(bytes, 0, 1);
  write_i16_le(bytes, 2, -1);
  write_i16_le(bytes, 4, 1234);
  write_i16_le(bytes, 6, -1234);
  return bytes;
}

std::vector<std::uint8_t> synthetic_i8_values() {
  return {0x01, 0xFF, 0x64, 0x9C};
}

std::vector<std::uint8_t> synthetic_f16_values() {
  std::vector<std::uint8_t> bytes(8, 0);
  write_u16_le(bytes, 0, 0x3C00);
  write_u16_le(bytes, 2, 0x4000);
  write_u16_le(bytes, 4, 0xC100);
  write_u16_le(bytes, 6, 0x0000);
  return bytes;
}

std::vector<std::uint8_t> synthetic_bf16_values() {
  std::vector<std::uint8_t> bytes(8, 0);
  write_u16_le(bytes, 0, 0x3F80);
  write_u16_le(bytes, 2, 0x4000);
  write_u16_le(bytes, 4, 0xC020);
  write_u16_le(bytes, 6, 0x0000);
  return bytes;
}

std::vector<std::uint8_t> synthetic_iq4_xs_block() {
  std::vector<std::uint8_t> block(136, 0);
  write_u16_le(block, 0, 0x3C00);
  write_u16_le(block, 2, 0xAAAA);
  const std::uint8_t scales_l[4] = {0x21, 0x43, 0x65, 0x87};
  for (std::size_t i = 0; i < 4; ++i) {
    block[4 + i] = scales_l[i];
  }
  for (std::size_t sub_block = 0; sub_block < 8; ++sub_block) {
    for (std::size_t i = 0; i < 16; ++i) {
      block[8 + sub_block * 16 + i] =
          static_cast<std::uint8_t>(i | ((15U - i) << 4U));
    }
  }
  return block;
}

std::vector<std::uint8_t> synthetic_tq1_0_block() {
  std::vector<std::uint8_t> block(56, 0);
  for (std::size_t i = 0; i < 16; ++i) {
    block[i] = 0x00;
    block[16 + i] = 0x80;
    block[32 + i] = 0xFF;
  }
  block[48] = 0x00;
  block[49] = 0x80;
  block[50] = 0xFF;
  block[51] = 0x80;
  write_u16_le(block, 52, 0x3C00);
  return block;
}

std::vector<std::uint8_t> synthetic_tq2_0_block() {
  std::vector<std::uint8_t> block(66, 0);
  for (std::size_t i = 0; i < 64; ++i) {
    block[i] = 0x24;
  }
  write_u16_le(block, 64, 0x3C00);
  return block;
}

std::vector<std::uint8_t> synthetic_q2_k_block() {
  std::vector<std::uint8_t> block(84, 0);
  for (std::size_t group = 0; group < 16; ++group) {
    block[group] = static_cast<std::uint8_t>(group | (group << 4U));
  }
  for (std::size_t i = 0; i < 64; ++i) {
    block[16 + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }
  write_u16_le(block, 80, 0x3C00);
  write_u16_le(block, 82, 0x3800);
  return block;
}

std::vector<std::uint8_t> synthetic_q3_k_block() {
  std::vector<std::uint8_t> block(110, 0);
  for (std::size_t i = 0; i < 32; ++i) {
    block[i] = 0xFF;
  }
  block[0] = 0x00;
  for (std::size_t i = 0; i < 64; ++i) {
    block[32 + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }
  const std::uint8_t packed_scales[12] = {
      0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5,
      0xE6, 0xF7, 0xAA, 0xAA, 0xAA, 0xAA};
  for (std::size_t i = 0; i < 12; ++i) {
    block[96 + i] = packed_scales[i];
  }
  write_u16_le(block, 108, 0x3C00);
  return block;
}

std::vector<std::uint8_t> synthetic_q5_k_block() {
  std::vector<std::uint8_t> block(176, 0);
  write_u16_le(block, 0, 0x3C00);
  write_u16_le(block, 2, 0x3800);
  for (std::size_t i = 0; i < 12; ++i) {
    block[4 + i] = static_cast<std::uint8_t>(i);
  }
  for (std::size_t i = 0; i < 32; ++i) {
    block[16 + i] = 0xFF;
  }
  for (std::size_t i = 0; i < 128; ++i) {
    block[48 + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q6_k_block() {
  std::vector<std::uint8_t> block(210, 0);
  for (std::size_t i = 0; i < 128; ++i) {
    block[i] = static_cast<std::uint8_t>(i & 0xFFU);
  }
  for (std::size_t i = 0; i < 64; ++i) {
    block[128 + i] = 0xFF;
  }
  for (std::size_t i = 0; i < 16; ++i) {
    block[192 + i] = static_cast<std::uint8_t>(i);
  }
  write_u16_le(block, 208, 0x3C00);
  return block;
}

std::vector<std::uint8_t> synthetic_q8_0_block() {
  std::vector<std::uint8_t> block(34, 0);
  write_u16_le(block, 0, 0x3C00);
  for (std::size_t i = 0; i < 32; ++i) {
    block[2 + i] = static_cast<std::uint8_t>(static_cast<int>(i) - 16);
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q8_1_block() {
  std::vector<std::uint8_t> block(40, 0);
  write_f32_le(block, 0, 0.5F);
  write_f32_le(block, 4, -8.0F);
  for (std::size_t i = 0; i < 32; ++i) {
    block[8 + i] = static_cast<std::uint8_t>(static_cast<int>(i) - 16);
  }
  return block;
}

std::vector<std::uint8_t> synthetic_q8_k_block() {
  std::vector<std::uint8_t> block(292, 0);
  write_f32_le(block, 0, 0.25F);
  for (std::size_t i = 0; i < 256; ++i) {
    block[4 + i] = static_cast<std::uint8_t>(static_cast<int>(i) - 128);
  }
  for (std::size_t group = 0; group < 16; ++group) {
    int sum = 0;
    for (std::size_t lane = 0; lane < 16; ++lane) {
      sum += static_cast<int>(group * 16 + lane) - 128;
    }
    write_i16_le(block, 260 + group * 2, static_cast<std::int16_t>(sum));
  }
  return block;
}

void require_near(const float actual, const float expected,
                  const std::string& message) {
  if (std::fabs(actual - expected) > 1.0e-5F) {
    throw std::runtime_error(message);
  }
}

std::vector<std::uint8_t> synthetic_gguf() {
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'G', 'G', 'U', 'F'});
  append_le<std::uint32_t>(bytes, 3);
  append_le<std::uint64_t>(bytes, 4);
  append_le<std::uint64_t>(bytes, 5);
  append_metadata_string(bytes, "general.architecture", "llama");
  append_metadata_string(bytes, "general.name", "tiny-fixture");
  append_metadata_u32(bytes, "general.alignment", 32);
  append_metadata_u32(bytes, "general.quantization_version", 2);
  append_metadata_string_array(bytes, "tokenizer.ggml.tokens",
                               {"<s>", "hello", "world"});
  append_tensor(bytes, "token_embd.weight", {8, 4}, 0, 0);
  append_tensor(bytes, "blk.0.attn_q.weight", {256, 4}, 12, 128);
  append_tensor(bytes, "blk.0.attn_k.weight", {256, 2}, 13, 704);
  append_tensor(bytes, "output.weight", {32, 8}, 8, 1056);
  while (bytes.size() % 32 != 0) {
    bytes.push_back(0);
  }
  bytes.resize(bytes.size() + 1328, 0xAB);
  return bytes;
}

void test_parses_synthetic_header_metadata_and_tensors() {
  const auto bytes = synthetic_gguf();
  const auto info = inferlab::parse_gguf_bytes(bytes);
  require(info.version == 3, "version mismatch");
  require(info.tensor_count == 4, "tensor count mismatch");
  require(info.metadata_kv_count == 5, "metadata count mismatch");
  require(info.alignment == 32, "alignment mismatch");
  require(info.file_size_bytes == bytes.size(), "file size mismatch");
  require(info.header_size_bytes == 24, "header size mismatch");
  require(info.metadata_start_offset == info.header_size_bytes,
          "metadata start mismatch");
  require(info.tensor_info_start_offset > info.metadata_start_offset,
          "tensor info start mismatch");
  require(info.tensor_info_end_offset > info.tensor_info_start_offset,
          "tensor info end mismatch");
  require(info.tensor_data_start_offset >= info.tensor_info_end_offset,
          "tensor data start before tensor info end");
  require(info.tensor_data_padding_bytes ==
              info.tensor_data_start_offset - info.tensor_info_end_offset,
          "tensor data padding mismatch");
  require(info.metadata.size() == 5, "metadata vector size mismatch");
  require(info.metadata[0].key == "general.architecture",
          "metadata key mismatch");
  require(info.metadata[0].value.string_value == "llama",
          "metadata string mismatch");
  require(info.metadata[4].value.type == inferlab::GGUFMetadataType::kArray,
          "metadata array type mismatch");
  require(info.metadata[4].value.array_values.size() == 3,
          "metadata array length mismatch");
  require(info.tensors.size() == 4, "tensor vector size mismatch");
  require(info.tensors[0].name == "token_embd.weight",
          "tensor name mismatch");
  require(info.tensors[0].dimensions == std::vector<std::uint64_t>({8, 4}),
          "tensor dimensions mismatch");
  require(info.tensors[0].type_info.name == "f32",
          "f32 tensor type name mismatch");
  require(info.tensors[0].element_count == 32, "f32 element count mismatch");
  require(info.tensors[0].row_size_bytes == 32, "f32 row bytes mismatch");
  require(info.tensors[0].data_size_bytes == 128,
          "f32 data bytes mismatch");
  require(info.tensors[0].relative_offset == 0,
          "first tensor relative offset mismatch");
  require(info.tensors[1].type_info.name == "q4_K",
          "q4_K tensor type name mismatch");
  require(info.tensors[1].type_info.block_size == 256,
          "q4_K block size mismatch");
  require(info.tensors[1].type_info.type_size_bytes == 144,
          "q4_K type bytes mismatch");
  require(info.tensors[1].row_size_bytes == 144,
          "q4_K row bytes mismatch");
  require(info.tensors[1].data_size_bytes == 576,
          "q4_K data bytes mismatch");
  require(info.tensors[1].relative_offset == 128,
          "second tensor relative offset mismatch");
  require(info.tensors[2].type_info.name == "q5_K",
          "q5_K tensor type name mismatch");
  require(info.tensors[2].type_info.block_size == 256,
          "q5_K block size mismatch");
  require(info.tensors[2].type_info.type_size_bytes == 176,
          "q5_K type bytes mismatch");
  require(info.tensors[2].row_size_bytes == 176,
          "q5_K row bytes mismatch");
  require(info.tensors[2].data_size_bytes == 352,
          "q5_K data bytes mismatch");
  require(info.tensors[2].relative_offset == 704,
          "third tensor relative offset mismatch");
  require(info.tensors[3].type_info.name == "q8_0",
          "q8_0 tensor type name mismatch");
  require(info.tensors[3].type_info.block_size == 32,
          "q8_0 block size mismatch");
  require(info.tensors[3].data_size_bytes == 272,
          "q8_0 data bytes mismatch");
  require(info.tensors[3].relative_offset == 1056,
          "fourth tensor relative offset mismatch");
  require(info.tensor_data_start_offset % 32 == 0,
          "tensor data start is not aligned");
  require(info.tensors[3].absolute_offset ==
              info.tensor_data_start_offset + 1056,
          "tensor absolute offset mismatch");
  require(info.tensors[3].data_end_absolute_offset ==
              info.tensor_data_start_offset + 1328,
          "tensor data end offset mismatch");
  require(inferlab::gguf_tensor_type_name(12) == "q4_K",
          "tensor type name helper mismatch");
  require(inferlab::gguf_tensor_type_name(13) == "q5_K",
          "Q5_K tensor type name helper mismatch");
}

void test_rejects_bad_magic() {
  auto bytes = synthetic_gguf();
  bytes[0] = 'B';
  bool rejected = false;
  try {
    (void)inferlab::parse_gguf_bytes(bytes);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "bad magic should be rejected");
}

void test_rejects_unaligned_tensor_offset() {
  auto bytes = synthetic_gguf();
  const auto info = inferlab::parse_gguf_bytes(bytes);
  const std::size_t relative_offset_field =
      static_cast<std::size_t>(info.tensors[1].absolute_offset -
                               info.tensor_data_start_offset);
  (void)relative_offset_field;
  bool rejected = false;

  std::vector<std::uint8_t> malformed;
  malformed.insert(malformed.end(), {'G', 'G', 'U', 'F'});
  append_le<std::uint32_t>(malformed, 3);
  append_le<std::uint64_t>(malformed, 1);
  append_le<std::uint64_t>(malformed, 1);
  append_metadata_u32(malformed, "general.alignment", 32);
  append_tensor(malformed, "bad.weight", {4}, 0, 1);
  malformed.resize(malformed.size() + 64, 0);
  try {
    (void)inferlab::parse_gguf_bytes(malformed);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "unaligned tensor offset should be rejected");
}

void test_rejects_quantized_leading_dimension_not_matching_block() {
  std::vector<std::uint8_t> malformed;
  malformed.insert(malformed.end(), {'G', 'G', 'U', 'F'});
  append_le<std::uint32_t>(malformed, 3);
  append_le<std::uint64_t>(malformed, 1);
  append_le<std::uint64_t>(malformed, 1);
  append_metadata_u32(malformed, "general.alignment", 32);
  append_tensor(malformed, "bad_q4_k.weight", {128, 2}, 12, 0);
  while (malformed.size() % 32 != 0) {
    malformed.push_back(0);
  }
  malformed.resize(malformed.size() + 256, 0);

  bool rejected = false;
  try {
    (void)inferlab::parse_gguf_bytes(malformed);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "invalid quantized leading dimension should be rejected");
}

void test_dequantizes_synthetic_q2_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q2_k_block(synthetic_q2_k_block());

  require_near(dequant.d, 1.0F, "Q2_K d mismatch");
  require_near(dequant.dmin, 0.5F, "Q2_K dmin mismatch");
  require(dequant.scale_min_pairs.size() == 16,
          "Q2_K scale/min pair count mismatch");
  require(dequant.values.size() == 256, "Q2_K value count mismatch");
  require(dequant.group_sums.size() == 16,
          "Q2_K group sum count mismatch");

  require(dequant.scale_min_pairs[0].scale == 0,
          "Q2_K pair 0 scale mismatch");
  require(dequant.scale_min_pairs[0].min == 0,
          "Q2_K pair 0 min mismatch");
  require(dequant.scale_min_pairs[15].scale == 15,
          "Q2_K pair 15 scale mismatch");
  require(dequant.scale_min_pairs[15].min == 15,
          "Q2_K pair 15 min mismatch");
  require_near(dequant.scale_min_pairs[15].effective_scale, 15.0F,
               "Q2_K effective scale mismatch");
  require_near(dequant.scale_min_pairs[15].effective_min, 7.5F,
               "Q2_K effective min mismatch");

  require_near(dequant.values[0], 0.0F, "Q2_K first value mismatch");
  require_near(dequant.values[16], -0.5F,
               "Q2_K group 1 first value mismatch");
  require_near(dequant.values[17], 0.5F,
               "Q2_K group 1 second value mismatch");
  require_near(dequant.values[31], 2.5F,
               "Q2_K group 1 final value mismatch");
  require_near(dequant.values[32], -1.0F,
               "Q2_K shift-2 group value mismatch");
  require_near(dequant.values[128], -4.0F,
               "Q2_K second half first value mismatch");
  require_near(dequant.values[255], -7.5F,
               "Q2_K final value mismatch");

  require_near(dequant.group_sums[0], 0.0F,
               "Q2_K group 0 sum mismatch");
  require_near(dequant.group_sums[1], 16.0F,
               "Q2_K group 1 sum mismatch");
  require_near(dequant.group_sums[13], 520.0F,
               "Q2_K group 13 sum mismatch");
  require_near(dequant.group_sums[15], -120.0F,
               "Q2_K group 15 sum mismatch");
  require_near(dequant.checksum, 1184.0F, "Q2_K checksum mismatch");
  require_near(dequant.min, -7.5F, "Q2_K min mismatch");
  require_near(dequant.max, 32.5F, "Q2_K max mismatch");
  require_near(dequant.mean, 4.625F, "Q2_K mean mismatch");
}

void test_rejects_short_q2_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q2_k_block(std::vector<std::uint8_t>(32, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q2_K block should be rejected");
}

void test_dequantizes_synthetic_q3_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q3_k_block(synthetic_q3_k_block());

  require_near(dequant.d, 1.0F, "Q3_K d mismatch");
  require(dequant.scales.size() == 16, "Q3_K scale count mismatch");
  require(dequant.values.size() == 256, "Q3_K value count mismatch");
  require(dequant.group_sums.size() == 16,
          "Q3_K group sum count mismatch");

  require(dequant.scales[0].scale == 0, "Q3_K scale 0 mismatch");
  require(dequant.scales[7].scale == 7, "Q3_K scale 7 mismatch");
  require(dequant.scales[15].scale == 15, "Q3_K scale 15 mismatch");
  require_near(dequant.scales[15].effective_scale, 15.0F,
               "Q3_K effective scale mismatch");

  require_near(dequant.values[0], 0.0F, "Q3_K first value mismatch");
  require_near(dequant.values[16], 0.0F, "Q3_K group 1 first value mismatch");
  require_near(dequant.values[31], 3.0F, "Q3_K group 1 final value mismatch");
  require_near(dequant.values[32], -8.0F,
               "Q3_K missing hmask high bit mismatch");
  require_near(dequant.values[64], -16.0F,
               "Q3_K shift-4 missing hmask value mismatch");
  require_near(dequant.values[128], -32.0F,
               "Q3_K second-half missing hmask value mismatch");
  require_near(dequant.values[224], -56.0F,
               "Q3_K final-plane missing hmask value mismatch");

  require_near(dequant.group_sums[0], 0.0F,
               "Q3_K group 0 sum mismatch");
  require_near(dequant.group_sums[1], 24.0F,
               "Q3_K group 1 sum mismatch");
  require_near(dequant.group_sums[14], -56.0F,
               "Q3_K group 14 sum mismatch");
  require_near(dequant.checksum, 1920.0F, "Q3_K checksum mismatch");
  require_near(dequant.min, -56.0F, "Q3_K min mismatch");
  require_near(dequant.max, 39.0F, "Q3_K max mismatch");
  require_near(dequant.mean, 7.5F, "Q3_K mean mismatch");
}

void test_rejects_short_q3_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q3_k_block(std::vector<std::uint8_t>(64, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q3_K block should be rejected");
}

void test_dequantizes_synthetic_q1_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q1_0_block(synthetic_q1_0_block());

  require_near(dequant.d, 1.0F, "Q1_0 d mismatch");
  require(dequant.bit_values.size() == 128,
          "Q1_0 bit value count mismatch");
  require(dequant.values.size() == 128, "Q1_0 value count mismatch");
  require(dequant.negative_bit_count == 64,
          "Q1_0 negative bit count mismatch");
  require(dequant.positive_bit_count == 64,
          "Q1_0 positive bit count mismatch");

  require(dequant.bit_values[0] == 0, "Q1_0 first bit mismatch");
  require(dequant.bit_values[1] == 1, "Q1_0 second bit mismatch");
  require(dequant.bit_values[126] == 0, "Q1_0 penultimate bit mismatch");
  require(dequant.bit_values[127] == 1, "Q1_0 final bit mismatch");
  require_near(dequant.values[0], -1.0F, "Q1_0 first value mismatch");
  require_near(dequant.values[1], 1.0F, "Q1_0 second value mismatch");
  require_near(dequant.values[127], 1.0F, "Q1_0 final value mismatch");
  require_near(dequant.checksum, 0.0F, "Q1_0 checksum mismatch");
  require_near(dequant.min, -1.0F, "Q1_0 min mismatch");
  require_near(dequant.max, 1.0F, "Q1_0 max mismatch");
  require_near(dequant.mean, 0.0F, "Q1_0 mean mismatch");
}

void test_rejects_short_q1_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q1_0_block(std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q1_0 block should be rejected");
}

void test_dequantizes_synthetic_q4_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q4_0_block(synthetic_q4_0_block());

  require_near(dequant.d, 1.0F, "Q4_0 d mismatch");
  require(dequant.quant_values.size() == 32,
          "Q4_0 quant value count mismatch");
  require(dequant.values.size() == 32, "Q4_0 value count mismatch");

  require(dequant.quant_values[0] == 0, "Q4_0 low nibble 0 mismatch");
  require(dequant.quant_values[15] == 15, "Q4_0 low nibble 15 mismatch");
  require(dequant.quant_values[16] == 15, "Q4_0 high nibble 0 mismatch");
  require(dequant.quant_values[31] == 0, "Q4_0 high nibble 15 mismatch");

  require_near(dequant.values[0], -8.0F, "Q4_0 first value mismatch");
  require_near(dequant.values[15], 7.0F, "Q4_0 low-half final mismatch");
  require_near(dequant.values[16], 7.0F, "Q4_0 high-half first mismatch");
  require_near(dequant.values[31], -8.0F, "Q4_0 final value mismatch");
  require_near(dequant.checksum, -16.0F, "Q4_0 checksum mismatch");
  require_near(dequant.min, -8.0F, "Q4_0 min mismatch");
  require_near(dequant.max, 7.0F, "Q4_0 max mismatch");
  require_near(dequant.mean, -0.5F, "Q4_0 mean mismatch");
}

void test_rejects_short_q4_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q4_0_block(std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q4_0 block should be rejected");
}

void test_dequantizes_synthetic_q4_1_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q4_1_block(synthetic_q4_1_block());

  require_near(dequant.d, 1.0F, "Q4_1 d mismatch");
  require_near(dequant.m, 0.5F, "Q4_1 m mismatch");
  require(dequant.quant_values.size() == 32,
          "Q4_1 quant value count mismatch");
  require(dequant.values.size() == 32, "Q4_1 value count mismatch");

  require(dequant.quant_values[0] == 0, "Q4_1 low nibble 0 mismatch");
  require(dequant.quant_values[15] == 15, "Q4_1 low nibble 15 mismatch");
  require(dequant.quant_values[16] == 15, "Q4_1 high nibble 0 mismatch");
  require(dequant.quant_values[31] == 0, "Q4_1 high nibble 15 mismatch");

  require_near(dequant.values[0], 0.5F, "Q4_1 first value mismatch");
  require_near(dequant.values[15], 15.5F, "Q4_1 low-half final mismatch");
  require_near(dequant.values[16], 15.5F, "Q4_1 high-half first mismatch");
  require_near(dequant.values[31], 0.5F, "Q4_1 final value mismatch");
  require_near(dequant.checksum, 256.0F, "Q4_1 checksum mismatch");
  require_near(dequant.min, 0.5F, "Q4_1 min mismatch");
  require_near(dequant.max, 15.5F, "Q4_1 max mismatch");
  require_near(dequant.mean, 8.0F, "Q4_1 mean mismatch");
}

void test_rejects_short_q4_1_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q4_1_block(std::vector<std::uint8_t>(18, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q4_1 block should be rejected");
}

void test_dequantizes_synthetic_q5_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q5_0_block(synthetic_q5_0_block());

  require_near(dequant.d, 1.0F, "Q5_0 d mismatch");
  require(dequant.quant_values.size() == 32,
          "Q5_0 quant value count mismatch");
  require(dequant.values.size() == 32, "Q5_0 value count mismatch");

  require(dequant.quant_values[0] == 0, "Q5_0 low quant 0 mismatch");
  require(dequant.quant_values[7] == 7, "Q5_0 low quant 7 mismatch");
  require(dequant.quant_values[8] == 24, "Q5_0 high-bit quant 8 mismatch");
  require(dequant.quant_values[15] == 31, "Q5_0 high-bit quant 15 mismatch");
  require(dequant.quant_values[16] == 31,
          "Q5_0 second half high-bit mismatch");
  require(dequant.quant_values[17] == 14,
          "Q5_0 second half low-bit mismatch");
  require(dequant.quant_values[31] == 16, "Q5_0 final quant mismatch");

  require_near(dequant.values[0], -16.0F, "Q5_0 first value mismatch");
  require_near(dequant.values[8], 8.0F, "Q5_0 high-bit value mismatch");
  require_near(dequant.values[16], 15.0F,
               "Q5_0 second half high-bit value mismatch");
  require_near(dequant.values[17], -2.0F,
               "Q5_0 second half low-bit value mismatch");
  require_near(dequant.values[31], 0.0F, "Q5_0 final value mismatch");
  require_near(dequant.checksum, -16.0F, "Q5_0 checksum mismatch");
  require_near(dequant.min, -16.0F, "Q5_0 min mismatch");
  require_near(dequant.max, 15.0F, "Q5_0 max mismatch");
  require_near(dequant.mean, -0.5F, "Q5_0 mean mismatch");
}

void test_rejects_short_q5_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q5_0_block(std::vector<std::uint8_t>(20, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q5_0 block should be rejected");
}

void test_dequantizes_synthetic_q5_1_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q5_1_block(synthetic_q5_1_block());

  require_near(dequant.d, 1.0F, "Q5_1 d mismatch");
  require_near(dequant.m, 0.5F, "Q5_1 m mismatch");
  require(dequant.quant_values.size() == 32,
          "Q5_1 quant value count mismatch");
  require(dequant.values.size() == 32, "Q5_1 value count mismatch");

  require(dequant.quant_values[0] == 0, "Q5_1 low quant 0 mismatch");
  require(dequant.quant_values[8] == 24, "Q5_1 high-bit quant 8 mismatch");
  require(dequant.quant_values[16] == 31,
          "Q5_1 second half high-bit mismatch");
  require(dequant.quant_values[17] == 14,
          "Q5_1 second half low-bit mismatch");
  require(dequant.quant_values[31] == 16, "Q5_1 final quant mismatch");

  require_near(dequant.values[0], 0.5F, "Q5_1 first value mismatch");
  require_near(dequant.values[8], 24.5F, "Q5_1 high-bit value mismatch");
  require_near(dequant.values[16], 31.5F,
               "Q5_1 second half high-bit value mismatch");
  require_near(dequant.values[17], 14.5F,
               "Q5_1 second half low-bit value mismatch");
  require_near(dequant.values[31], 16.5F, "Q5_1 final value mismatch");
  require_near(dequant.checksum, 512.0F, "Q5_1 checksum mismatch");
  require_near(dequant.min, 0.5F, "Q5_1 min mismatch");
  require_near(dequant.max, 31.5F, "Q5_1 max mismatch");
  require_near(dequant.mean, 16.0F, "Q5_1 mean mismatch");
}

void test_rejects_short_q5_1_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q5_1_block(std::vector<std::uint8_t>(22, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q5_1 block should be rejected");
}

void test_dequantizes_synthetic_iq4_nl_block() {
  const auto dequant =
      inferlab::dequantize_gguf_iq4_nl_block(synthetic_iq4_nl_block());

  require_near(dequant.d, 0.5F, "IQ4_NL d mismatch");
  require(dequant.quant_values.size() == 32,
          "IQ4_NL quant value count mismatch");
  require(dequant.codebook_values.size() == 32,
          "IQ4_NL codebook value count mismatch");
  require(dequant.values.size() == 32, "IQ4_NL value count mismatch");

  require(dequant.quant_values[0] == 0, "IQ4_NL first quant mismatch");
  require(dequant.quant_values[15] == 15, "IQ4_NL low-half quant mismatch");
  require(dequant.quant_values[16] == 15, "IQ4_NL high-half quant mismatch");
  require(dequant.quant_values[31] == 0, "IQ4_NL final quant mismatch");
  require(dequant.codebook_values[0] == static_cast<std::int8_t>(-127),
          "IQ4_NL first codebook mismatch");
  require(dequant.codebook_values[15] == static_cast<std::int8_t>(113),
          "IQ4_NL low-half codebook mismatch");
  require(dequant.codebook_values[16] == static_cast<std::int8_t>(113),
          "IQ4_NL high-half codebook mismatch");
  require(dequant.codebook_values[31] == static_cast<std::int8_t>(-127),
          "IQ4_NL final codebook mismatch");
  require_near(dequant.values[0], -63.5F, "IQ4_NL first value mismatch");
  require_near(dequant.values[15], 56.5F, "IQ4_NL low-half value mismatch");
  require_near(dequant.values[16], 56.5F, "IQ4_NL high-half value mismatch");
  require_near(dequant.values[31], -63.5F, "IQ4_NL final value mismatch");
  require_near(dequant.checksum, -94.0F, "IQ4_NL checksum mismatch");
  require_near(dequant.min, -63.5F, "IQ4_NL min mismatch");
  require_near(dequant.max, 56.5F, "IQ4_NL max mismatch");
  require_near(dequant.mean, -2.9375F, "IQ4_NL mean mismatch");
}

void test_rejects_short_iq4_nl_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_iq4_nl_block(
        std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short IQ4_NL block should be rejected");
}

void test_dequantizes_synthetic_mxfp4_block() {
  const auto dequant =
      inferlab::dequantize_gguf_mxfp4_block(synthetic_mxfp4_block());

  require(dequant.e == 128, "MXFP4 exponent mismatch");
  require_near(dequant.e_scale, 1.0F, "MXFP4 e scale mismatch");
  require(dequant.quant_values.size() == 32,
          "MXFP4 quant value count mismatch");
  require(dequant.codebook_values.size() == 32,
          "MXFP4 codebook value count mismatch");
  require(dequant.values.size() == 32, "MXFP4 value count mismatch");

  require(dequant.quant_values[0] == 0, "MXFP4 first quant mismatch");
  require(dequant.quant_values[15] == 15, "MXFP4 low-half quant mismatch");
  require(dequant.quant_values[16] == 15, "MXFP4 high-half quant mismatch");
  require(dequant.quant_values[31] == 0, "MXFP4 final quant mismatch");
  require(dequant.codebook_values[0] == static_cast<std::int8_t>(0),
          "MXFP4 first codebook mismatch");
  require(dequant.codebook_values[7] == static_cast<std::int8_t>(12),
          "MXFP4 positive codebook mismatch");
  require(dequant.codebook_values[15] == static_cast<std::int8_t>(-12),
          "MXFP4 low-half codebook mismatch");
  require(dequant.codebook_values[16] == static_cast<std::int8_t>(-12),
          "MXFP4 high-half codebook mismatch");
  require(dequant.codebook_values[31] == static_cast<std::int8_t>(0),
          "MXFP4 final codebook mismatch");
  require_near(dequant.values[0], 0.0F, "MXFP4 first value mismatch");
  require_near(dequant.values[7], 12.0F, "MXFP4 positive value mismatch");
  require_near(dequant.values[15], -12.0F,
               "MXFP4 low-half value mismatch");
  require_near(dequant.values[16], -12.0F,
               "MXFP4 high-half value mismatch");
  require_near(dequant.values[31], 0.0F, "MXFP4 final value mismatch");
  require_near(dequant.checksum, 0.0F, "MXFP4 checksum mismatch");
  require_near(dequant.min, -12.0F, "MXFP4 min mismatch");
  require_near(dequant.max, 12.0F, "MXFP4 max mismatch");
  require_near(dequant.mean, 0.0F, "MXFP4 mean mismatch");
}

void test_rejects_short_mxfp4_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_mxfp4_block(
        std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short MXFP4 block should be rejected");
}

void test_dequantizes_synthetic_nvfp4_block() {
  const auto dequant =
      inferlab::dequantize_gguf_nvfp4_block(synthetic_nvfp4_block());

  require(dequant.scales.size() == 4, "NVFP4 scale count mismatch");
  require(dequant.quant_values.size() == 64,
          "NVFP4 quant value count mismatch");
  require(dequant.codebook_values.size() == 64,
          "NVFP4 codebook value count mismatch");
  require(dequant.values.size() == 64, "NVFP4 value count mismatch");
  require(dequant.subblock_sums.size() == 4,
          "NVFP4 sub-block sum count mismatch");

  require(dequant.scales[0].raw_scale == 0x38,
          "NVFP4 first raw scale mismatch");
  require(dequant.scales[3].raw_scale == 0x48,
          "NVFP4 final raw scale mismatch");
  require_near(dequant.scales[0].scale, 0.5F,
               "NVFP4 first scale mismatch");
  require_near(dequant.scales[1].scale, 1.0F,
               "NVFP4 second scale mismatch");
  require_near(dequant.scales[2].scale, 0.25F,
               "NVFP4 third scale mismatch");
  require_near(dequant.scales[3].scale, 2.0F,
               "NVFP4 final scale mismatch");

  require(dequant.quant_values[0] == 0, "NVFP4 first quant mismatch");
  require(dequant.quant_values[7] == 7, "NVFP4 low-half quant mismatch");
  require(dequant.quant_values[8] == 15, "NVFP4 high-half quant mismatch");
  require(dequant.quant_values[15] == 8, "NVFP4 sub-block final quant mismatch");
  require(dequant.codebook_values[7] == static_cast<std::int8_t>(12),
          "NVFP4 positive codebook mismatch");
  require(dequant.codebook_values[8] == static_cast<std::int8_t>(-12),
          "NVFP4 negative codebook mismatch");
  require_near(dequant.values[7], 6.0F, "NVFP4 scaled positive mismatch");
  require_near(dequant.values[8], -6.0F, "NVFP4 scaled negative mismatch");
  require_near(dequant.values[23], 12.0F,
               "NVFP4 second sub-block positive mismatch");
  require_near(dequant.values[24], -12.0F,
               "NVFP4 second sub-block negative mismatch");
  require_near(dequant.values[55], 24.0F,
               "NVFP4 final sub-block positive mismatch");
  require_near(dequant.values[56], -24.0F,
               "NVFP4 final sub-block negative mismatch");
  for (const auto sum : dequant.subblock_sums) {
    require_near(sum, 0.0F, "NVFP4 sub-block sum mismatch");
  }
  require_near(dequant.checksum, 0.0F, "NVFP4 checksum mismatch");
  require_near(dequant.min, -24.0F, "NVFP4 min mismatch");
  require_near(dequant.max, 24.0F, "NVFP4 max mismatch");
  require_near(dequant.mean, 0.0F, "NVFP4 mean mismatch");
}

void test_rejects_short_nvfp4_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_nvfp4_block(
        std::vector<std::uint8_t>(35, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short NVFP4 block should be rejected");
}

void test_decodes_f32_values() {
  const auto preview =
      inferlab::decode_gguf_f32_values(synthetic_f32_values(), 4);
  require(preview.raw_values == std::vector<std::uint32_t>(
                                    {0x3F800000, 0xBF800000, 0x3F000000,
                                     0xBF000000}),
          "F32 raw values mismatch");
  require(preview.values.size() == 4, "F32 value count mismatch");
  require_near(preview.values[0], 1.0F, "F32 first value mismatch");
  require_near(preview.values[1], -1.0F, "F32 second value mismatch");
  require_near(preview.values[2], 0.5F, "F32 third value mismatch");
  require_near(preview.values[3], -0.5F, "F32 final value mismatch");
  require_near(preview.checksum, 0.0F, "F32 checksum mismatch");
  require_near(preview.min, -1.0F, "F32 min mismatch");
  require_near(preview.max, 1.0F, "F32 max mismatch");
  require_near(preview.mean, 0.0F, "F32 mean mismatch");
}

void test_rejects_short_f32_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_f32_values(
        std::vector<std::uint8_t>(15, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short F32 tensor should be rejected");
}

void test_decodes_f64_values() {
  const auto preview =
      inferlab::decode_gguf_f64_values(synthetic_f64_values(), 4);
  require(preview.raw_values == std::vector<std::uint64_t>(
                                    {0x3FF0000000000000ULL,
                                     0xBFF0000000000000ULL,
                                     0x3FD0000000000000ULL,
                                     0xBFD0000000000000ULL}),
          "F64 raw values mismatch");
  require(preview.values.size() == 4, "F64 value count mismatch");
  require_near(preview.values[0], 1.0, "F64 first value mismatch");
  require_near(preview.values[1], -1.0, "F64 second value mismatch");
  require_near(preview.values[2], 0.25, "F64 third value mismatch");
  require_near(preview.values[3], -0.25, "F64 final value mismatch");
  require_near(preview.checksum, 0.0, "F64 checksum mismatch");
  require_near(preview.min, -1.0, "F64 min mismatch");
  require_near(preview.max, 1.0, "F64 max mismatch");
  require_near(preview.mean, 0.0, "F64 mean mismatch");
}

void test_rejects_short_f64_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_f64_values(
        std::vector<std::uint8_t>(31, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short F64 tensor should be rejected");
}

void test_previews_iq2_xxs_structure() {
  const auto preview = inferlab::preview_gguf_iq2_xxs_block(
      synthetic_iq2_xxs_block());
  require_near(preview.d, 1.0F, "IQ2_XXS d mismatch");
  require(preview.qs_words.size() == 32, "IQ2_XXS qs word count mismatch");
  require(preview.grid_indices.size() == 32,
          "IQ2_XXS grid index count mismatch");
  require(preview.sign_codes.size() == 32,
          "IQ2_XXS sign code count mismatch");
  require(preview.scale_nibbles == std::vector<std::uint8_t>(
                                       {1, 2, 3, 4, 5, 6, 7, 8}),
          "IQ2_XXS scale nibbles mismatch");
  require(preview.grid_indices[0] == 1 && preview.grid_indices[1] == 2 &&
              preview.grid_indices[4] == 5,
          "IQ2_XXS grid indices mismatch");
  require(preview.sign_codes[0] == 1 && preview.sign_codes[1] == 2 &&
              preview.sign_codes[4] == 5,
          "IQ2_XXS sign codes mismatch");
  require_near(preview.effective_scales[0], 0.375F,
               "IQ2_XXS effective scale 0 mismatch");
  require_near(preview.effective_scales[1], 0.625F,
               "IQ2_XXS effective scale 1 mismatch");
}

void test_rejects_short_iq2_xxs_block() {
  bool rejected = false;
  try {
    (void)inferlab::preview_gguf_iq2_xxs_block(
        std::vector<std::uint8_t>(65, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short IQ2_XXS block should be rejected");
}

void test_decodes_i64_values() {
  const auto preview =
      inferlab::decode_gguf_i64_values(synthetic_i64_values(), 4);
  require(preview.raw_values == std::vector<std::uint64_t>(
                                    {0x0000000000000001ULL,
                                     0xFFFFFFFFFFFFFFFFULL,
                                     0x00000000075BCD15ULL,
                                     0xFFFFFFFFF8A432EBULL}),
          "I64 raw values mismatch");
  require(preview.values ==
              std::vector<std::int64_t>({1, -1, 123456789, -123456789}),
          "I64 values mismatch");
  require(preview.checksum == 0, "I64 checksum mismatch");
  require(preview.min == -123456789, "I64 min mismatch");
  require(preview.max == 123456789, "I64 max mismatch");
  require_near(static_cast<float>(preview.mean), 0.0F, "I64 mean mismatch");
}

void test_rejects_short_i64_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_i64_values(
        std::vector<std::uint8_t>(31, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short I64 tensor should be rejected");
}

void test_decodes_i32_values() {
  const auto preview =
      inferlab::decode_gguf_i32_values(synthetic_i32_values(), 4);
  require(preview.raw_values == std::vector<std::uint32_t>(
                                    {0x00000001U, 0xFFFFFFFFU, 0x0001E240U,
                                     0xFFFE1DC0U}),
          "I32 raw values mismatch");
  require(preview.values ==
              std::vector<std::int32_t>({1, -1, 123456, -123456}),
          "I32 values mismatch");
  require(preview.checksum == 0, "I32 checksum mismatch");
  require(preview.min == -123456, "I32 min mismatch");
  require(preview.max == 123456, "I32 max mismatch");
  require_near(static_cast<float>(preview.mean), 0.0F, "I32 mean mismatch");
}

void test_rejects_short_i32_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_i32_values(
        std::vector<std::uint8_t>(15, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short I32 tensor should be rejected");
}

void test_decodes_i16_values() {
  const auto preview =
      inferlab::decode_gguf_i16_values(synthetic_i16_values(), 4);
  require(preview.raw_values == std::vector<std::uint16_t>(
                                    {0x0001U, 0xFFFFU, 0x04D2U, 0xFB2EU}),
          "I16 raw values mismatch");
  require(preview.values == std::vector<std::int16_t>({1, -1, 1234, -1234}),
          "I16 values mismatch");
  require(preview.checksum == 0, "I16 checksum mismatch");
  require(preview.min == -1234, "I16 min mismatch");
  require(preview.max == 1234, "I16 max mismatch");
  require_near(static_cast<float>(preview.mean), 0.0F, "I16 mean mismatch");
}

void test_rejects_short_i16_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_i16_values(
        std::vector<std::uint8_t>(7, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short I16 tensor should be rejected");
}

void test_decodes_i8_values() {
  const auto preview =
      inferlab::decode_gguf_i8_values(synthetic_i8_values(), 4);
  require(preview.raw_values ==
              std::vector<std::uint8_t>({0x01, 0xFF, 0x64, 0x9C}),
          "I8 raw values mismatch");
  require(preview.values == std::vector<std::int8_t>({1, -1, 100, -100}),
          "I8 values mismatch");
  require(preview.checksum == 0, "I8 checksum mismatch");
  require(preview.min == -100, "I8 min mismatch");
  require(preview.max == 100, "I8 max mismatch");
  require_near(static_cast<float>(preview.mean), 0.0F, "I8 mean mismatch");
}

void test_rejects_short_i8_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_i8_values(
        std::vector<std::uint8_t>(3, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short I8 tensor should be rejected");
}

void test_decodes_f16_values() {
  require_near(inferlab::gguf_fp16_to_float32(0x3C00), 1.0F,
               "F16 1.0 conversion mismatch");
  require_near(inferlab::gguf_fp16_to_float32(0x4000), 2.0F,
               "F16 2.0 conversion mismatch");
  require_near(inferlab::gguf_fp16_to_float32(0xC100), -2.5F,
               "F16 -2.5 conversion mismatch");

  const auto preview =
      inferlab::decode_gguf_f16_values(synthetic_f16_values(), 4);
  require(preview.raw_values == std::vector<std::uint16_t>(
                                    {0x3C00, 0x4000, 0xC100, 0x0000}),
          "F16 raw values mismatch");
  require(preview.values.size() == 4, "F16 value count mismatch");
  require_near(preview.values[0], 1.0F, "F16 first value mismatch");
  require_near(preview.values[1], 2.0F, "F16 second value mismatch");
  require_near(preview.values[2], -2.5F, "F16 third value mismatch");
  require_near(preview.values[3], 0.0F, "F16 final value mismatch");
  require_near(preview.checksum, 0.5F, "F16 checksum mismatch");
  require_near(preview.min, -2.5F, "F16 min mismatch");
  require_near(preview.max, 2.0F, "F16 max mismatch");
  require_near(preview.mean, 0.125F, "F16 mean mismatch");
}

void test_rejects_short_f16_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_f16_values(
        std::vector<std::uint8_t>(7, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short F16 tensor should be rejected");
}

void test_decodes_bf16_values() {
  require_near(inferlab::gguf_bf16_to_float32(0x3F80), 1.0F,
               "BF16 1.0 conversion mismatch");
  require_near(inferlab::gguf_bf16_to_float32(0x4000), 2.0F,
               "BF16 2.0 conversion mismatch");
  require_near(inferlab::gguf_bf16_to_float32(0xC020), -2.5F,
               "BF16 -2.5 conversion mismatch");

  const auto preview =
      inferlab::decode_gguf_bf16_values(synthetic_bf16_values(), 4);
  require(preview.raw_values == std::vector<std::uint16_t>(
                                    {0x3F80, 0x4000, 0xC020, 0x0000}),
          "BF16 raw values mismatch");
  require(preview.values.size() == 4, "BF16 value count mismatch");
  require_near(preview.values[0], 1.0F, "BF16 first value mismatch");
  require_near(preview.values[1], 2.0F, "BF16 second value mismatch");
  require_near(preview.values[2], -2.5F, "BF16 third value mismatch");
  require_near(preview.values[3], 0.0F, "BF16 final value mismatch");
  require_near(preview.checksum, 0.5F, "BF16 checksum mismatch");
  require_near(preview.min, -2.5F, "BF16 min mismatch");
  require_near(preview.max, 2.0F, "BF16 max mismatch");
  require_near(preview.mean, 0.125F, "BF16 mean mismatch");
}

void test_rejects_short_bf16_values() {
  bool rejected = false;
  try {
    (void)inferlab::decode_gguf_bf16_values(
        std::vector<std::uint8_t>(7, 0), 4);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short BF16 tensor should be rejected");
}

void test_dequantizes_synthetic_iq4_xs_block() {
  const auto dequant =
      inferlab::dequantize_gguf_iq4_xs_block(synthetic_iq4_xs_block());

  require_near(dequant.d, 1.0F, "IQ4_XS d mismatch");
  require(dequant.scales_h == 0xAAAA, "IQ4_XS scales_h mismatch");
  require(dequant.scales.size() == 8, "IQ4_XS scale count mismatch");
  require(dequant.quant_values.size() == 256,
          "IQ4_XS quant value count mismatch");
  require(dequant.codebook_values.size() == 256,
          "IQ4_XS codebook value count mismatch");
  require(dequant.values.size() == 256, "IQ4_XS value count mismatch");
  require(dequant.subblock_sums.size() == 8,
          "IQ4_XS sub-block sum count mismatch");

  require(dequant.scales[0].raw_scale == 33,
          "IQ4_XS first raw scale mismatch");
  require(dequant.scales[0].signed_scale == 1,
          "IQ4_XS first signed scale mismatch");
  require_near(dequant.scales[0].effective_scale, 1.0F,
               "IQ4_XS first effective scale mismatch");
  require(dequant.scales[7].raw_scale == 40,
          "IQ4_XS last raw scale mismatch");
  require(dequant.scales[7].signed_scale == 8,
          "IQ4_XS last signed scale mismatch");
  require_near(dequant.scales[7].effective_scale, 8.0F,
               "IQ4_XS last effective scale mismatch");

  require(dequant.quant_values[0] == 0, "IQ4_XS first quant mismatch");
  require(dequant.quant_values[15] == 15, "IQ4_XS low-half quant mismatch");
  require(dequant.quant_values[16] == 15, "IQ4_XS high-half quant mismatch");
  require(dequant.quant_values[31] == 0, "IQ4_XS final quant mismatch");
  require(dequant.codebook_values[0] == static_cast<std::int8_t>(-127),
          "IQ4_XS first codebook mismatch");
  require(dequant.codebook_values[15] == static_cast<std::int8_t>(113),
          "IQ4_XS low-half codebook mismatch");
  require(dequant.codebook_values[16] == static_cast<std::int8_t>(113),
          "IQ4_XS high-half codebook mismatch");
  require(dequant.codebook_values[31] == static_cast<std::int8_t>(-127),
          "IQ4_XS final codebook mismatch");
  require_near(dequant.values[0], -127.0F, "IQ4_XS first value mismatch");
  require_near(dequant.values[15], 113.0F,
               "IQ4_XS low-half value mismatch");
  require_near(dequant.values[32], -254.0F,
               "IQ4_XS second sub-block first value mismatch");
  require_near(dequant.values[255], -1016.0F,
               "IQ4_XS final value mismatch");
  require_near(dequant.subblock_sums[0], -188.0F,
               "IQ4_XS first sub-block sum mismatch");
  require_near(dequant.subblock_sums[7], -1504.0F,
               "IQ4_XS last sub-block sum mismatch");
  require_near(dequant.checksum, -6768.0F, "IQ4_XS checksum mismatch");
  require_near(dequant.min, -1016.0F, "IQ4_XS min mismatch");
  require_near(dequant.max, 904.0F, "IQ4_XS max mismatch");
  require_near(dequant.mean, -26.4375F, "IQ4_XS mean mismatch");
}

void test_rejects_short_iq4_xs_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_iq4_xs_block(
        std::vector<std::uint8_t>(128, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short IQ4_XS block should be rejected");
}

void test_dequantizes_synthetic_tq1_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_tq1_0_block(synthetic_tq1_0_block());

  require_near(dequant.d, 1.0F, "TQ1_0 d mismatch");
  require(dequant.quant_values.size() == 256,
          "TQ1_0 quant value count mismatch");
  require(dequant.values.size() == 256, "TQ1_0 value count mismatch");
  require(dequant.quant_counts.size() == 3,
          "TQ1_0 quant count bucket mismatch");
  require(dequant.quant_counts[0] == 84, "TQ1_0 q0 count mismatch");
  require(dequant.quant_counts[1] == 88, "TQ1_0 q1 count mismatch");
  require(dequant.quant_counts[2] == 84, "TQ1_0 q2 count mismatch");

  require(dequant.quant_values[0] == 0, "TQ1_0 first quant mismatch");
  require(dequant.quant_values[16] == 1, "TQ1_0 middle qs quant mismatch");
  require(dequant.quant_values[160] == 2, "TQ1_0 second group quant mismatch");
  require(dequant.quant_values[240] == 0, "TQ1_0 qh first quant mismatch");
  require(dequant.quant_values[242] == 2, "TQ1_0 qh high quant mismatch");
  require(dequant.quant_values[255] == 1, "TQ1_0 final qh quant mismatch");
  require_near(dequant.values[0], -1.0F, "TQ1_0 first value mismatch");
  require_near(dequant.values[16], 0.0F, "TQ1_0 zero value mismatch");
  require_near(dequant.values[160], 1.0F, "TQ1_0 positive value mismatch");
  require_near(dequant.values[240], -1.0F, "TQ1_0 qh first value mismatch");
  require_near(dequant.values[242], 1.0F, "TQ1_0 qh high value mismatch");
  require_near(dequant.values[255], 0.0F, "TQ1_0 final qh value mismatch");
  require_near(dequant.checksum, 0.0F, "TQ1_0 checksum mismatch");
  require_near(dequant.min, -1.0F, "TQ1_0 min mismatch");
  require_near(dequant.max, 1.0F, "TQ1_0 max mismatch");
  require_near(dequant.mean, 0.0F, "TQ1_0 mean mismatch");
}

void test_rejects_short_tq1_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_tq1_0_block(
        std::vector<std::uint8_t>(52, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short TQ1_0 block should be rejected");
}

void test_dequantizes_synthetic_tq2_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_tq2_0_block(synthetic_tq2_0_block());

  require_near(dequant.d, 1.0F, "TQ2_0 d mismatch");
  require(dequant.quant_values.size() == 256,
          "TQ2_0 quant value count mismatch");
  require(dequant.values.size() == 256, "TQ2_0 value count mismatch");
  require(dequant.quant_counts.size() == 4,
          "TQ2_0 quant count bucket mismatch");
  require(dequant.quant_counts[0] == 128, "TQ2_0 q0 count mismatch");
  require(dequant.quant_counts[1] == 64, "TQ2_0 q1 count mismatch");
  require(dequant.quant_counts[2] == 64, "TQ2_0 q2 count mismatch");
  require(dequant.quant_counts[3] == 0, "TQ2_0 q3 count mismatch");

  require(dequant.quant_values[0] == 0, "TQ2_0 first quant mismatch");
  require(dequant.quant_values[32] == 1, "TQ2_0 second plane mismatch");
  require(dequant.quant_values[64] == 2, "TQ2_0 third plane mismatch");
  require(dequant.quant_values[96] == 0, "TQ2_0 fourth plane mismatch");
  require_near(dequant.values[0], -1.0F, "TQ2_0 first value mismatch");
  require_near(dequant.values[32], 0.0F, "TQ2_0 second plane value mismatch");
  require_near(dequant.values[64], 1.0F, "TQ2_0 third plane value mismatch");
  require_near(dequant.values[96], -1.0F, "TQ2_0 fourth plane value mismatch");
  require_near(dequant.checksum, -64.0F, "TQ2_0 checksum mismatch");
  require_near(dequant.min, -1.0F, "TQ2_0 min mismatch");
  require_near(dequant.max, 1.0F, "TQ2_0 max mismatch");
  require_near(dequant.mean, -0.25F, "TQ2_0 mean mismatch");
}

void test_rejects_short_tq2_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_tq2_0_block(
        std::vector<std::uint8_t>(64, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short TQ2_0 block should be rejected");
}

void test_dequantizes_synthetic_q4_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q4_k_block(synthetic_q4_k_block());

  require_near(inferlab::gguf_fp16_to_float32(0x3C00), 1.0F,
               "fp16 1.0 conversion mismatch");
  require_near(inferlab::gguf_fp16_to_float32(0x3800), 0.5F,
               "fp16 0.5 conversion mismatch");
  require_near(dequant.d, 1.0F, "Q4_K d mismatch");
  require_near(dequant.dmin, 0.5F, "Q4_K dmin mismatch");
  require(dequant.scale_min_pairs.size() == 8,
          "Q4_K scale/min pair count mismatch");
  require(dequant.values.size() == 256, "Q4_K value count mismatch");
  require(dequant.subblock_sums.size() == 8,
          "Q4_K sub-block sum count mismatch");

  require(dequant.scale_min_pairs[0].scale == 0,
          "Q4_K pair 0 scale mismatch");
  require(dequant.scale_min_pairs[0].min == 4,
          "Q4_K pair 0 min mismatch");
  require_near(dequant.scale_min_pairs[0].effective_scale, 0.0F,
               "Q4_K pair 0 effective scale mismatch");
  require_near(dequant.scale_min_pairs[0].effective_min, 2.0F,
               "Q4_K pair 0 effective min mismatch");
  require(dequant.scale_min_pairs[7].scale == 11,
          "Q4_K pair 7 scale mismatch");
  require(dequant.scale_min_pairs[7].min == 0,
          "Q4_K pair 7 min mismatch");

  require_near(dequant.values[0], -2.0F, "Q4_K first value mismatch");
  require_near(dequant.values[32], -2.5F, "Q4_K sub-block 1 value mismatch");
  require_near(dequant.values[48], -1.5F,
               "Q4_K sub-block 1 high-nibble value mismatch");
  require_near(dequant.subblock_sums[0], -64.0F,
               "Q4_K sub-block 0 sum mismatch");
  require_near(dequant.subblock_sums[2], 384.0F,
               "Q4_K sub-block 2 sum mismatch");
  require_near(dequant.checksum, 8288.0F, "Q4_K checksum mismatch");
  require_near(dequant.min, -3.0F, "Q4_K min mismatch");
  require_near(dequant.max, 150.0F, "Q4_K max mismatch");
  require_near(dequant.mean, 32.375F, "Q4_K mean mismatch");
}

void test_rejects_short_q4_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q4_k_block(std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q4_K block should be rejected");
}

void test_dequantizes_synthetic_q5_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q5_k_block(synthetic_q5_k_block());

  require_near(dequant.d, 1.0F, "Q5_K d mismatch");
  require_near(dequant.dmin, 0.5F, "Q5_K dmin mismatch");
  require(dequant.scale_min_pairs.size() == 8,
          "Q5_K scale/min pair count mismatch");
  require(dequant.values.size() == 256, "Q5_K value count mismatch");
  require(dequant.subblock_sums.size() == 8,
          "Q5_K sub-block sum count mismatch");

  require(dequant.scale_min_pairs[0].scale == 0,
          "Q5_K pair 0 scale mismatch");
  require(dequant.scale_min_pairs[0].min == 4,
          "Q5_K pair 0 min mismatch");
  require(dequant.scale_min_pairs[7].scale == 11,
          "Q5_K pair 7 scale mismatch");
  require(dequant.scale_min_pairs[7].min == 0,
          "Q5_K pair 7 min mismatch");

  require_near(dequant.values[0], -2.0F, "Q5_K first value mismatch");
  require_near(dequant.values[32], 13.5F,
               "Q5_K sub-block 1 high-bit value mismatch");
  require_near(dequant.values[48], 14.5F,
               "Q5_K sub-block 1 high-nibble value mismatch");
  require_near(dequant.subblock_sums[0], -64.0F,
               "Q5_K sub-block 0 sum mismatch");
  require_near(dequant.subblock_sums[1], 448.0F,
               "Q5_K sub-block 1 sum mismatch");
  require_near(dequant.subblock_sums[7], 7920.0F,
               "Q5_K sub-block 7 sum mismatch");
  require_near(dequant.checksum, 30816.0F, "Q5_K checksum mismatch");
  require_near(dequant.min, -2.0F, "Q5_K min mismatch");
  require_near(dequant.max, 310.0F, "Q5_K max mismatch");
  require_near(dequant.mean, 120.375F, "Q5_K mean mismatch");
}

void test_rejects_short_q5_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q5_k_block(std::vector<std::uint8_t>(32, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q5_K block should be rejected");
}

void test_dequantizes_synthetic_q6_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q6_k_block(synthetic_q6_k_block());

  require_near(dequant.d, 1.0F, "Q6_K d mismatch");
  require(dequant.scales.size() == 16, "Q6_K scale count mismatch");
  require(dequant.values.size() == 256, "Q6_K value count mismatch");
  require(dequant.group_sums.size() == 16, "Q6_K group sum count mismatch");

  require(dequant.scales[0].scale == 0, "Q6_K scale 0 mismatch");
  require(dequant.scales[15].scale == 15, "Q6_K scale 15 mismatch");
  require_near(dequant.scales[15].effective_scale, 15.0F,
               "Q6_K effective scale mismatch");

  require_near(dequant.values[0], 0.0F, "Q6_K first value mismatch");
  require_near(dequant.values[16], 16.0F, "Q6_K scale lane value mismatch");
  require_near(dequant.values[32], 32.0F, "Q6_K low-nibble value mismatch");
  require_near(dequant.values[64], 64.0F, "Q6_K high-nibble value mismatch");
  require_near(dequant.values[96], 108.0F,
               "Q6_K qh high-two-bit value mismatch");
  require_near(dequant.values[255], 345.0F, "Q6_K final value mismatch");

  require_near(dequant.group_sums[0], 0.0F,
               "Q6_K group 0 sum mismatch");
  require_near(dequant.group_sums[1], 376.0F,
               "Q6_K group 1 sum mismatch");
  require_near(dequant.group_sums[15], 5520.0F,
               "Q6_K group 15 sum mismatch");
  require_near(dequant.checksum, 41440.0F, "Q6_K checksum mismatch");
  require_near(dequant.min, 0.0F, "Q6_K min mismatch");
  require_near(dequant.max, 345.0F, "Q6_K max mismatch");
  require_near(dequant.mean, 161.875F, "Q6_K mean mismatch");
}

void test_rejects_short_q6_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q6_k_block(std::vector<std::uint8_t>(64, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q6_K block should be rejected");
}

void test_dequantizes_synthetic_q8_0_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q8_0_block(synthetic_q8_0_block());

  require_near(dequant.d, 1.0F, "Q8_0 d mismatch");
  require(dequant.quant_values.size() == 32,
          "Q8_0 quant value count mismatch");
  require(dequant.values.size() == 32, "Q8_0 value count mismatch");

  require(dequant.quant_values[0] == static_cast<std::int8_t>(-16),
          "Q8_0 first quant mismatch");
  require(dequant.quant_values[15] == static_cast<std::int8_t>(-1),
          "Q8_0 mid quant mismatch");
  require(dequant.quant_values[31] == static_cast<std::int8_t>(15),
          "Q8_0 final quant mismatch");
  require_near(dequant.values[0], -16.0F, "Q8_0 first value mismatch");
  require_near(dequant.values[15], -1.0F, "Q8_0 mid value mismatch");
  require_near(dequant.values[31], 15.0F, "Q8_0 final value mismatch");
  require(dequant.quant_sum == -16, "Q8_0 quant sum mismatch");
  require_near(dequant.checksum, -16.0F, "Q8_0 checksum mismatch");
  require_near(dequant.min, -16.0F, "Q8_0 min mismatch");
  require_near(dequant.max, 15.0F, "Q8_0 max mismatch");
  require_near(dequant.mean, -0.5F, "Q8_0 mean mismatch");
}

void test_rejects_short_q8_0_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q8_0_block(std::vector<std::uint8_t>(16, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q8_0 block should be rejected");
}

void test_dequantizes_synthetic_q8_1_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q8_1_block(synthetic_q8_1_block());

  require_near(dequant.d, 0.5F, "Q8_1 d mismatch");
  require_near(dequant.s, -8.0F, "Q8_1 s mismatch");
  require(dequant.quant_values.size() == 32,
          "Q8_1 quant value count mismatch");
  require(dequant.values.size() == 32, "Q8_1 value count mismatch");

  require(dequant.quant_values[0] == static_cast<std::int8_t>(-16),
          "Q8_1 first quant mismatch");
  require(dequant.quant_values[15] == static_cast<std::int8_t>(-1),
          "Q8_1 mid quant mismatch");
  require(dequant.quant_values[31] == static_cast<std::int8_t>(15),
          "Q8_1 final quant mismatch");
  require_near(dequant.values[0], -8.0F, "Q8_1 first value mismatch");
  require_near(dequant.values[15], -0.5F, "Q8_1 mid value mismatch");
  require_near(dequant.values[31], 7.5F, "Q8_1 final value mismatch");
  require(dequant.quant_sum == -16, "Q8_1 quant sum mismatch");
  require_near(dequant.expected_s_from_quant_sum, dequant.s,
               "Q8_1 stored s mismatch");
  require_near(dequant.checksum, -8.0F, "Q8_1 checksum mismatch");
  require_near(dequant.min, -8.0F, "Q8_1 min mismatch");
  require_near(dequant.max, 7.5F, "Q8_1 max mismatch");
  require_near(dequant.mean, -0.25F, "Q8_1 mean mismatch");
}

void test_rejects_short_q8_1_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q8_1_block(std::vector<std::uint8_t>(36, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q8_1 block should be rejected");
}

void test_dequantizes_synthetic_q8_k_block() {
  const auto dequant =
      inferlab::dequantize_gguf_q8_k_block(synthetic_q8_k_block());

  require_near(dequant.d, 0.25F, "Q8_K d mismatch");
  require(dequant.quant_values.size() == 256,
          "Q8_K quant value count mismatch");
  require(dequant.block_sums.size() == 16,
          "Q8_K block sum count mismatch");
  require(dequant.values.size() == 256, "Q8_K value count mismatch");

  require(dequant.quant_values[0] == static_cast<std::int8_t>(-128),
          "Q8_K first quant mismatch");
  require(dequant.quant_values[128] == static_cast<std::int8_t>(0),
          "Q8_K zero quant mismatch");
  require(dequant.quant_values[255] == static_cast<std::int8_t>(127),
          "Q8_K final quant mismatch");
  require(dequant.block_sums[0] == static_cast<std::int16_t>(-1928),
          "Q8_K first block sum mismatch");
  require(dequant.block_sums[8] == static_cast<std::int16_t>(120),
          "Q8_K middle block sum mismatch");
  require(dequant.block_sums[15] == static_cast<std::int16_t>(1912),
          "Q8_K final block sum mismatch");
  require(dequant.quant_sum == -128, "Q8_K quant sum mismatch");
  require(dequant.stored_block_sum_total == dequant.quant_sum,
          "Q8_K stored sum total mismatch");
  require_near(dequant.values[0], -32.0F, "Q8_K first value mismatch");
  require_near(dequant.values[128], 0.0F, "Q8_K zero value mismatch");
  require_near(dequant.values[255], 31.75F, "Q8_K final value mismatch");
  require_near(dequant.checksum, -32.0F, "Q8_K checksum mismatch");
  require_near(dequant.min, -32.0F, "Q8_K min mismatch");
  require_near(dequant.max, 31.75F, "Q8_K max mismatch");
  require_near(dequant.mean, -0.125F, "Q8_K mean mismatch");
}

void test_rejects_short_q8_k_block() {
  bool rejected = false;
  try {
    (void)inferlab::dequantize_gguf_q8_k_block(std::vector<std::uint8_t>(260, 0));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "short Q8_K block should be rejected");
}

}  // namespace

int main() {
  try {
    test_parses_synthetic_header_metadata_and_tensors();
    test_rejects_bad_magic();
    test_rejects_unaligned_tensor_offset();
    test_rejects_quantized_leading_dimension_not_matching_block();
    test_dequantizes_synthetic_q2_k_block();
    test_rejects_short_q2_k_block();
    test_dequantizes_synthetic_q3_k_block();
    test_rejects_short_q3_k_block();
    test_dequantizes_synthetic_q1_0_block();
    test_rejects_short_q1_0_block();
    test_dequantizes_synthetic_q4_0_block();
    test_rejects_short_q4_0_block();
    test_dequantizes_synthetic_q4_1_block();
    test_rejects_short_q4_1_block();
    test_dequantizes_synthetic_q5_0_block();
    test_rejects_short_q5_0_block();
    test_dequantizes_synthetic_q5_1_block();
    test_rejects_short_q5_1_block();
    test_dequantizes_synthetic_iq4_nl_block();
    test_rejects_short_iq4_nl_block();
    test_dequantizes_synthetic_mxfp4_block();
    test_rejects_short_mxfp4_block();
    test_dequantizes_synthetic_nvfp4_block();
    test_rejects_short_nvfp4_block();
    test_decodes_f32_values();
    test_rejects_short_f32_values();
    test_decodes_f64_values();
    test_rejects_short_f64_values();
    test_previews_iq2_xxs_structure();
    test_rejects_short_iq2_xxs_block();
    test_decodes_i64_values();
    test_rejects_short_i64_values();
    test_decodes_i32_values();
    test_rejects_short_i32_values();
    test_decodes_i16_values();
    test_rejects_short_i16_values();
    test_decodes_i8_values();
    test_rejects_short_i8_values();
    test_decodes_f16_values();
    test_rejects_short_f16_values();
    test_decodes_bf16_values();
    test_rejects_short_bf16_values();
    test_dequantizes_synthetic_iq4_xs_block();
    test_rejects_short_iq4_xs_block();
    test_dequantizes_synthetic_tq1_0_block();
    test_rejects_short_tq1_0_block();
    test_dequantizes_synthetic_tq2_0_block();
    test_rejects_short_tq2_0_block();
    test_dequantizes_synthetic_q4_k_block();
    test_rejects_short_q4_k_block();
    test_dequantizes_synthetic_q5_k_block();
    test_rejects_short_q5_k_block();
    test_dequantizes_synthetic_q6_k_block();
    test_rejects_short_q6_k_block();
    test_dequantizes_synthetic_q8_0_block();
    test_rejects_short_q8_0_block();
    test_dequantizes_synthetic_q8_1_block();
    test_rejects_short_q8_1_block();
    test_dequantizes_synthetic_q8_k_block();
    test_rejects_short_q8_k_block();
  } catch (const std::exception& error) {
    std::cerr << "gguf_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
