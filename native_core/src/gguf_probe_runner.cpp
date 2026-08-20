#include "inferlab/gguf.hpp"
#include "gguf_probe_runner.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace {

namespace fs = std::filesystem;

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

void write_u16_le(std::vector<std::uint8_t>& bytes, const std::uint64_t offset,
                  const std::uint16_t value) {
  bytes[static_cast<std::size_t>(offset)] = static_cast<std::uint8_t>(value);
  bytes[static_cast<std::size_t>(offset + 1)] =
      static_cast<std::uint8_t>(value >> 8U);
}

void write_f32_le(std::vector<std::uint8_t>& bytes, const std::uint64_t offset,
                  const float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  for (std::uint64_t i = 0; i < sizeof(raw); ++i) {
    bytes[static_cast<std::size_t>(offset + i)] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_f64_le(std::vector<std::uint8_t>& bytes, const std::uint64_t offset,
                  const double value) {
  std::uint64_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  for (std::uint64_t i = 0; i < sizeof(raw); ++i) {
    bytes[static_cast<std::size_t>(offset + i)] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_i32_le(std::vector<std::uint8_t>& bytes, const std::uint64_t offset,
                  const std::int32_t value) {
  const auto raw = static_cast<std::uint32_t>(value);
  for (std::uint64_t i = 0; i < sizeof(raw); ++i) {
    bytes[static_cast<std::size_t>(offset + i)] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_i64_le(std::vector<std::uint8_t>& bytes, const std::uint64_t offset,
                  const std::int64_t value) {
  const auto raw = static_cast<std::uint64_t>(value);
  for (std::uint64_t i = 0; i < sizeof(raw); ++i) {
    bytes[static_cast<std::size_t>(offset + i)] =
        static_cast<std::uint8_t>((raw >> (8U * i)) & 0xFFU);
  }
}

void write_iq2_xxs_group(std::vector<std::uint8_t>& bytes,
                         const std::uint64_t offset,
                         const std::uint8_t grid0,
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
  for (std::uint64_t i = 0; i < sizeof(aux0); ++i) {
    bytes[static_cast<std::size_t>(offset + i)] =
        static_cast<std::uint8_t>((aux0 >> (8U * i)) & 0xFFU);
    bytes[static_cast<std::size_t>(offset + 4U + i)] =
        static_cast<std::uint8_t>((aux1 >> (8U * i)) & 0xFFU);
  }
}

void populate_synthetic_tensor_data(std::vector<std::uint8_t>& bytes,
                                    const std::uint64_t tensor_data_start) {
  const auto f32_start = tensor_data_start;
  std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(f32_start),
            bytes.begin() + static_cast<std::ptrdiff_t>(f32_start + 512),
            0x11);
  const float f32_values[8] = {1.0F, -1.0F, 0.5F, -0.5F,
                               2.0F, -2.0F, 3.5F, -4.0F};
  for (std::uint64_t i = 0; i < 8; ++i) {
    write_f32_le(bytes, f32_start + i * 4U, f32_values[i]);
  }

  const auto q4_k_start = tensor_data_start + 512;
  for (std::uint64_t row = 0; row < 4; ++row) {
    const auto block_start = q4_k_start + row * 144;
    write_u16_le(bytes, block_start + 0, static_cast<std::uint16_t>(0x3C00 + row));
    write_u16_le(bytes, block_start + 2, static_cast<std::uint16_t>(0x3800 + row));
    for (std::uint64_t i = 0; i < 12; ++i) {
      bytes[static_cast<std::size_t>(block_start + 4 + i)] =
          static_cast<std::uint8_t>(row * 16 + i);
    }
    for (std::uint64_t i = 0; i < 128; ++i) {
      bytes[static_cast<std::size_t>(block_start + 16 + i)] =
          static_cast<std::uint8_t>((i + row) & 0xFFU);
    }
  }

  const auto q5_k_start = tensor_data_start + 1088;
  for (std::uint64_t row = 0; row < 2; ++row) {
    const auto block_start = q5_k_start + row * 176;
    write_u16_le(bytes, block_start + 0, static_cast<std::uint16_t>(0x3C00 + row));
    write_u16_le(bytes, block_start + 2, static_cast<std::uint16_t>(0x3800 + row));
    for (std::uint64_t i = 0; i < 12; ++i) {
      bytes[static_cast<std::size_t>(block_start + 4 + i)] =
          static_cast<std::uint8_t>(row * 16 + i);
    }
    for (std::uint64_t i = 0; i < 32; ++i) {
      bytes[static_cast<std::size_t>(block_start + 16 + i)] =
          row == 0 ? 0xFF : static_cast<std::uint8_t>((0xAAU ^ i) & 0xFFU);
    }
    for (std::uint64_t i = 0; i < 128; ++i) {
      bytes[static_cast<std::size_t>(block_start + 48 + i)] =
          static_cast<std::uint8_t>((i + row) & 0xFFU);
    }
  }

  const auto q6_k_start = tensor_data_start + 1440;
  for (std::uint64_t i = 0; i < 128; ++i) {
    bytes[static_cast<std::size_t>(q6_k_start + i)] =
        static_cast<std::uint8_t>(i & 0xFFU);
  }
  for (std::uint64_t i = 0; i < 64; ++i) {
    bytes[static_cast<std::size_t>(q6_k_start + 128 + i)] = 0xFF;
  }
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q6_k_start + 192 + i)] =
        static_cast<std::uint8_t>(i & 0xFFU);
  }
  write_u16_le(bytes, q6_k_start + 208, 0x3C00);

  const auto q3_k_start = tensor_data_start + 1664;
  for (std::uint64_t i = 0; i < 32; ++i) {
    bytes[static_cast<std::size_t>(q3_k_start + i)] = 0xFF;
  }
  bytes[static_cast<std::size_t>(q3_k_start)] = 0x00;
  for (std::uint64_t i = 0; i < 64; ++i) {
    bytes[static_cast<std::size_t>(q3_k_start + 32 + i)] =
        static_cast<std::uint8_t>(i & 0xFFU);
  }
  const std::uint8_t q3_k_scales[12] = {
      0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5,
      0xE6, 0xF7, 0xAA, 0xAA, 0xAA, 0xAA};
  for (std::uint64_t i = 0; i < 12; ++i) {
    bytes[static_cast<std::size_t>(q3_k_start + 96 + i)] = q3_k_scales[i];
  }
  write_u16_le(bytes, q3_k_start + 108, 0x3C00);

  const auto q2_k_start = tensor_data_start + 1792;
  for (std::uint64_t group = 0; group < 16; ++group) {
    bytes[static_cast<std::size_t>(q2_k_start + group)] =
        static_cast<std::uint8_t>(group | (group << 4U));
  }
  for (std::uint64_t i = 0; i < 64; ++i) {
    bytes[static_cast<std::size_t>(q2_k_start + 16 + i)] =
        static_cast<std::uint8_t>(i & 0xFFU);
  }
  write_u16_le(bytes, q2_k_start + 80, 0x3C00);
  write_u16_le(bytes, q2_k_start + 82, 0x3800);

  const auto q8_0_start = tensor_data_start + 1920;
  for (std::uint64_t block = 0; block < 8; ++block) {
    const auto block_start = q8_0_start + block * 34;
    write_u16_le(bytes, block_start, static_cast<std::uint16_t>(0x3C00 + block));
    for (std::uint64_t i = 0; i < 32; ++i) {
      bytes[static_cast<std::size_t>(block_start + 2 + i)] =
          static_cast<std::uint8_t>(static_cast<int>(i) - 16 + static_cast<int>(block));
    }
  }

  const auto q4_0_start = tensor_data_start + 2208;
  write_u16_le(bytes, q4_0_start, 0x3C00);
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q4_0_start + 2 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto q4_1_start = tensor_data_start + 2240;
  write_u16_le(bytes, q4_1_start, 0x3C00);
  write_u16_le(bytes, q4_1_start + 2, 0x3800);
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q4_1_start + 4 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto q5_0_start = tensor_data_start + 2272;
  write_u16_le(bytes, q5_0_start, 0x3C00);
  const std::uint8_t q5_0_high_bits[4] = {0x00, 0xFF, 0x55, 0xAA};
  for (std::uint64_t i = 0; i < 4; ++i) {
    bytes[static_cast<std::size_t>(q5_0_start + 2 + i)] =
        q5_0_high_bits[i];
  }
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q5_0_start + 6 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto q5_1_start = tensor_data_start + 2304;
  write_u16_le(bytes, q5_1_start, 0x3C00);
  write_u16_le(bytes, q5_1_start + 2, 0x3800);
  const std::uint8_t q5_1_high_bits[4] = {0x00, 0xFF, 0x55, 0xAA};
  for (std::uint64_t i = 0; i < 4; ++i) {
    bytes[static_cast<std::size_t>(q5_1_start + 4 + i)] =
        q5_1_high_bits[i];
  }
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q5_1_start + 8 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto q8_1_start = tensor_data_start + 2336;
  write_f32_le(bytes, q8_1_start, 0.5F);
  write_f32_le(bytes, q8_1_start + 4, -8.0F);
  for (std::uint64_t i = 0; i < 32; ++i) {
    bytes[static_cast<std::size_t>(q8_1_start + 8 + i)] =
        static_cast<std::uint8_t>(static_cast<int>(i) - 16);
  }

  const auto iq4_nl_start = tensor_data_start + 2384;
  write_u16_le(bytes, iq4_nl_start, 0x3800);
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(iq4_nl_start + 2 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto q8_k_start = tensor_data_start + 2432;
  write_f32_le(bytes, q8_k_start, 0.25F);
  for (std::uint64_t i = 0; i < 256; ++i) {
    bytes[static_cast<std::size_t>(q8_k_start + 4 + i)] =
        static_cast<std::uint8_t>(static_cast<int>(i) - 128);
  }
  for (std::uint64_t group = 0; group < 16; ++group) {
    int sum = 0;
    for (std::uint64_t lane = 0; lane < 16; ++lane) {
      sum += static_cast<int>(group * 16 + lane) - 128;
    }
    write_u16_le(bytes, q8_k_start + 260 + group * 2,
                 static_cast<std::uint16_t>(
                     static_cast<std::int16_t>(sum)));
  }

  const auto q1_0_start = tensor_data_start + 2752;
  write_u16_le(bytes, q1_0_start, 0x3C00);
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(q1_0_start + 2 + i)] = 0xAA;
  }

  const auto iq4_xs_start = tensor_data_start + 2784;
  write_u16_le(bytes, iq4_xs_start, 0x3C00);
  write_u16_le(bytes, iq4_xs_start + 2, 0xAAAA);
  const std::uint8_t iq4_xs_scales_l[4] = {0x21, 0x43, 0x65, 0x87};
  for (std::uint64_t i = 0; i < 4; ++i) {
    bytes[static_cast<std::size_t>(iq4_xs_start + 4 + i)] =
        iq4_xs_scales_l[i];
  }
  for (std::uint64_t sub_block = 0; sub_block < 8; ++sub_block) {
    for (std::uint64_t i = 0; i < 16; ++i) {
      const auto offset = iq4_xs_start + 8 + sub_block * 16 + i;
      bytes[static_cast<std::size_t>(offset)] =
          static_cast<std::uint8_t>(i | ((15U - i) << 4U));
    }
  }

  const auto tq2_0_start = tensor_data_start + 2944;
  for (std::uint64_t i = 0; i < 64; ++i) {
    bytes[static_cast<std::size_t>(tq2_0_start + i)] = 0x24;
  }
  write_u16_le(bytes, tq2_0_start + 64, 0x3C00);

  const auto tq1_0_start = tensor_data_start + 3040;
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(tq1_0_start + i)] = 0x00;
    bytes[static_cast<std::size_t>(tq1_0_start + 16 + i)] = 0x80;
    bytes[static_cast<std::size_t>(tq1_0_start + 32 + i)] = 0xFF;
  }
  bytes[static_cast<std::size_t>(tq1_0_start + 48)] = 0x00;
  bytes[static_cast<std::size_t>(tq1_0_start + 49)] = 0x80;
  bytes[static_cast<std::size_t>(tq1_0_start + 50)] = 0xFF;
  bytes[static_cast<std::size_t>(tq1_0_start + 51)] = 0x80;
  write_u16_le(bytes, tq1_0_start + 52, 0x3C00);

  const auto mxfp4_start = tensor_data_start + 3104;
  bytes[static_cast<std::size_t>(mxfp4_start)] = 0x80;
  for (std::uint64_t i = 0; i < 16; ++i) {
    bytes[static_cast<std::size_t>(mxfp4_start + 1 + i)] =
        static_cast<std::uint8_t>(i | ((15U - i) << 4U));
  }

  const auto nvfp4_start = tensor_data_start + 3136;
  const std::uint8_t nvfp4_scales[4] = {0x38, 0x40, 0x30, 0x48};
  for (std::uint64_t i = 0; i < 4; ++i) {
    bytes[static_cast<std::size_t>(nvfp4_start + i)] = nvfp4_scales[i];
  }
  for (std::uint64_t block = 0; block < 4; ++block) {
    for (std::uint64_t i = 0; i < 8; ++i) {
      const auto offset = nvfp4_start + 4 + block * 8 + i;
      bytes[static_cast<std::size_t>(offset)] =
          static_cast<std::uint8_t>(i | ((15U - i) << 4U));
    }
  }

  const auto bf16_start = tensor_data_start + 3200;
  const std::uint16_t bf16_values[8] = {0x3F80, 0x4000, 0xC020, 0x0000,
                                        0x3F00, 0xBF00, 0x3FC0, 0xC000};
  for (std::uint64_t i = 0; i < 8; ++i) {
    write_u16_le(bytes, bf16_start + i * 2U, bf16_values[i]);
  }

  const auto f16_start = tensor_data_start + 3232;
  const std::uint16_t f16_values[8] = {0x3C00, 0x4000, 0xC100, 0x0000,
                                       0x3800, 0xB800, 0x4300, 0xC400};
  for (std::uint64_t i = 0; i < 8; ++i) {
    write_u16_le(bytes, f16_start + i * 2U, f16_values[i]);
  }

  const auto f64_start = tensor_data_start + 3264;
  const double f64_values[4] = {1.0, -1.0, 0.25, -0.25};
  for (std::uint64_t i = 0; i < 4; ++i) {
    write_f64_le(bytes, f64_start + i * 8U, f64_values[i]);
  }

  const auto i32_start = tensor_data_start + 3296;
  const std::int32_t i32_values[4] = {1, -1, 123456, -123456};
  for (std::uint64_t i = 0; i < 4; ++i) {
    write_i32_le(bytes, i32_start + i * 4U, i32_values[i]);
  }

  const auto i16_start = tensor_data_start + 3312;
  const std::int16_t i16_values[4] = {1, -1, 1234, -1234};
  for (std::uint64_t i = 0; i < 4; ++i) {
    write_u16_le(bytes, i16_start + i * 2U,
                 static_cast<std::uint16_t>(i16_values[i]));
  }

  const auto i8_start = tensor_data_start + 3320;
  const std::int8_t i8_values[4] = {1, -1, 100, -100};
  for (std::uint64_t i = 0; i < 4; ++i) {
    bytes[static_cast<std::size_t>(i8_start + i)] =
        static_cast<std::uint8_t>(i8_values[i]);
  }

  const auto i64_start = tensor_data_start + 3328;
  const std::int64_t i64_values[4] = {1, -1, 123456789, -123456789};
  for (std::uint64_t i = 0; i < 4; ++i) {
    write_i64_le(bytes, i64_start + i * 8U, i64_values[i]);
  }

  const auto iq2_xxs_start = tensor_data_start + 3360;
  write_u16_le(bytes, iq2_xxs_start, 0x3C00);
  for (std::uint64_t group = 0; group < 8; ++group) {
    write_iq2_xxs_group(
        bytes, iq2_xxs_start + 2U + group * 8U,
        static_cast<std::uint8_t>(1U + group * 4U),
        static_cast<std::uint8_t>(group + 1U),
        static_cast<std::uint8_t>(1U + group * 4U));
  }
}

std::vector<std::uint8_t> synthetic_gguf() {
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'G', 'G', 'U', 'F'});
  append_le<std::uint32_t>(bytes, 3);
  append_le<std::uint64_t>(bytes, 28);
  append_le<std::uint64_t>(bytes, 7);
  append_metadata_string(bytes, "general.architecture", "llama");
  append_metadata_string(bytes, "general.name", "inferlab-gguf-fixture");
  append_metadata_u32(bytes, "general.alignment", 32);
  append_metadata_u32(bytes, "general.quantization_version", 2);
  append_metadata_u32(bytes, "general.file_type", 15);
  append_metadata_u32(bytes, "llama.context_length", 128);
  append_metadata_string_array(bytes, "tokenizer.ggml.tokens",
                               {"<s>", "hello", "world", "</s>"});
  append_tensor(bytes, "token_embd.weight", {16, 8}, 0, 0);
  append_tensor(bytes, "blk.0.attn_q.weight", {256, 4}, 12, 512);
  append_tensor(bytes, "blk.0.attn_k.weight", {256, 2}, 13, 1088);
  append_tensor(bytes, "blk.0.attn_v.weight", {256, 1}, 14, 1440);
  append_tensor(bytes, "blk.0.attn_o.weight", {256, 1}, 11, 1664);
  append_tensor(bytes, "blk.0.ffn_up.weight", {256, 1}, 10, 1792);
  append_tensor(bytes, "output.weight", {32, 8}, 8, 1920);
  append_tensor(bytes, "blk.0.legacy_q4_0.weight", {32, 1}, 2, 2208);
  append_tensor(bytes, "blk.0.legacy_q4_1.weight", {32, 1}, 3, 2240);
  append_tensor(bytes, "blk.0.legacy_q5_0.weight", {32, 1}, 6, 2272);
  append_tensor(bytes, "blk.0.legacy_q5_1.weight", {32, 1}, 7, 2304);
  append_tensor(bytes, "blk.0.dot_q8_1.weight", {32, 1}, 9, 2336);
  append_tensor(bytes, "blk.0.iq4_nl.weight", {32, 1}, 20, 2384);
  append_tensor(bytes, "blk.0.q8_k.weight", {256, 1}, 15, 2432);
  append_tensor(bytes, "blk.0.q1_0.weight", {128, 1}, 41, 2752);
  append_tensor(bytes, "blk.0.iq4_xs.weight", {256, 1}, 23, 2784);
  append_tensor(bytes, "blk.0.tq2_0.weight", {256, 1}, 35, 2944);
  append_tensor(bytes, "blk.0.tq1_0.weight", {256, 1}, 34, 3040);
  append_tensor(bytes, "blk.0.mxfp4.weight", {32, 1}, 39, 3104);
  append_tensor(bytes, "blk.0.nvfp4.weight", {64, 1}, 40, 3136);
  append_tensor(bytes, "blk.0.bf16.weight", {8, 1}, 30, 3200);
  append_tensor(bytes, "blk.0.f16.weight", {8, 1}, 1, 3232);
  append_tensor(bytes, "blk.0.f64.weight", {4, 1}, 28, 3264);
  append_tensor(bytes, "blk.0.i32.weight", {4, 1}, 26, 3296);
  append_tensor(bytes, "blk.0.i16.weight", {4, 1}, 25, 3312);
  append_tensor(bytes, "blk.0.i8.weight", {4, 1}, 24, 3320);
  append_tensor(bytes, "blk.0.i64.weight", {4, 1}, 27, 3328);
  append_tensor(bytes, "blk.0.iq2_xxs.weight", {256, 1}, 16, 3360);
  while (bytes.size() % 32 != 0) {
    bytes.push_back(0);
  }
  const auto tensor_data_start = bytes.size();
  bytes.resize(bytes.size() + 3456, 0);
  populate_synthetic_tensor_data(bytes, tensor_data_start);
  return bytes;
}

std::uint64_t file_size_or_throw(const std::string& path) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error) {
    throw std::runtime_error("failed to read input file size: " +
                             inferlab_plan::redacted_path_label(path) + ": " +
                             inferlab_plan::redact_error_message(error.message(),
                                                                 {path}));
  }
  if (size > static_cast<std::uintmax_t>(
                 std::numeric_limits<std::uint64_t>::max())) {
    throw std::runtime_error("input file size exceeds uint64_t capacity: " +
                             inferlab_plan::redacted_path_label(path));
  }
  return static_cast<std::uint64_t>(size);
}

std::vector<std::uint8_t> read_binary_file(const std::string& path,
                                           const std::uint64_t max_input_bytes) {
  const auto file_size = file_size_or_throw(path);
  if (file_size > max_input_bytes) {
    throw std::runtime_error(
        "input GGUF exceeds full-buffer probe safety cap; increase "
        "--max-input-bytes only on a machine with enough memory");
  }
  if (file_size > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error("input GGUF exceeds host size_t capacity");
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open input path: " +
                             inferlab_plan::redacted_path_label(path));
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
  constexpr std::size_t kReadChunkBytes = 64U * 1024U * 1024U;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto chunk_size =
        std::min(kReadChunkBytes, bytes.size() - offset);
    file.read(reinterpret_cast<char*>(bytes.data() + offset),
              static_cast<std::streamsize>(chunk_size));
    if (file.gcount() != static_cast<std::streamsize>(chunk_size)) {
      throw std::runtime_error(
          "input GGUF changed size or became unreadable during bounded read");
    }
    offset += chunk_size;
  }
  char trailing_byte = 0;
  file.read(&trailing_byte, 1);
  if (file.gcount() != 0) {
    throw std::runtime_error(
        "input GGUF grew beyond the preflighted bounded read size");
  }
  if (file.bad()) {
    throw std::runtime_error("input GGUF read failed after bounded read");
  }
  return bytes;
}

std::string spaces(const int count) {
  if (count < 0) {
    throw std::invalid_argument("JSON indentation must be non-negative");
  }
  return std::string(static_cast<std::size_t>(count), ' ');
}

std::string hex_prefix(const std::vector<std::uint8_t>& bytes,
                       const std::uint64_t start,
                       const std::uint64_t end_exclusive,
                       const std::size_t limit = 32) {
  std::ostringstream out;
  const auto safe_start =
      std::min<std::uint64_t>(start, static_cast<std::uint64_t>(bytes.size()));
  const auto safe_end = std::min<std::uint64_t>(
      end_exclusive, static_cast<std::uint64_t>(bytes.size()));
  if (safe_end <= safe_start) {
    return "";
  }
  const auto count =
      std::min<std::uint64_t>(safe_end - safe_start, static_cast<std::uint64_t>(limit));
  for (std::uint64_t i = 0; i < count; ++i) {
    if (i != 0) {
      out << " ";
    }
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(bytes[static_cast<std::size_t>(safe_start + i)])
        << std::dec << std::setfill(' ');
  }
  return out.str();
}

std::string ascii_prefix(const std::vector<std::uint8_t>& bytes,
                         const std::uint64_t start,
                         const std::uint64_t end_exclusive,
                         const std::size_t limit = 32) {
  std::string out;
  const auto safe_start =
      std::min<std::uint64_t>(start, static_cast<std::uint64_t>(bytes.size()));
  const auto safe_end = std::min<std::uint64_t>(
      end_exclusive, static_cast<std::uint64_t>(bytes.size()));
  if (safe_end <= safe_start) {
    return "";
  }
  const auto count =
      std::min<std::uint64_t>(safe_end - safe_start, static_cast<std::uint64_t>(limit));
  out.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t i = 0; i < count; ++i) {
    const auto ch = bytes[static_cast<std::size_t>(safe_start + i)];
    out.push_back(std::isprint(ch) ? static_cast<char>(ch) : '.');
  }
  return out;
}

std::vector<std::uint8_t> byte_slice(const std::vector<std::uint8_t>& bytes,
                                     const std::uint64_t start,
                                     const std::uint64_t count) {
  const auto buffer_size = static_cast<std::uint64_t>(bytes.size());
  if (start > buffer_size || count > buffer_size - start) {
    throw std::runtime_error("byte_slice range outside byte buffer");
  }
  const auto start_index = static_cast<std::size_t>(start);
  const auto slice_size = static_cast<std::size_t>(count);
  std::vector<std::uint8_t> slice(slice_size);
  if (slice_size != 0U) {
    std::memcpy(slice.data(), bytes.data() + start_index, slice_size);
  }
  return slice;
}

std::string float_json_value(const float value) {
  if (std::isnan(value)) {
    return "\"nan\"";
  }
  if (std::isinf(value)) {
    return value > 0 ? "\"inf\"" : "\"-inf\"";
  }
  std::ostringstream out;
  out << std::setprecision(8) << value;
  return out.str();
}

std::string double_json_value(const double value) {
  if (std::isnan(value)) {
    return "\"nan\"";
  }
  if (std::isinf(value)) {
    return value > 0 ? "\"inf\"" : "\"-inf\"";
  }
  std::ostringstream out;
  out << std::setprecision(17) << value;
  return out.str();
}

std::string q4_k_scale_min_pairs_json(
    const std::vector<inferlab::GGUFQ4KScaleMin>& pairs,
    const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t sub_block = 0; sub_block < pairs.size(); ++sub_block) {
    const auto& pair = pairs[sub_block];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"effective_min\": "
        << float_json_value(pair.effective_min) << ",\n";
    out << spaces(indent + 4) << "\"effective_scale\": "
        << float_json_value(pair.effective_scale) << ",\n";
    out << spaces(indent + 4) << "\"min\": "
        << static_cast<int>(pair.min) << ",\n";
    out << spaces(indent + 4) << "\"scale\": "
        << static_cast<int>(pair.scale) << ",\n";
    out << spaces(indent + 4) << "\"sub_block\": " << sub_block << "\n";
    out << nested << "}" << (sub_block + 1 == pairs.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string float_group_values_json(const std::vector<float>& values,
                                    const int group,
                                    const std::size_t group_width,
                                    const std::uint64_t limit,
                                    const char* label) {
  if (group < 0 || group_width == 0U) {
    throw std::runtime_error(std::string(label) +
                             " index outside supported range");
  }
  const auto start = inferlab::internal::checked_product_size(
      static_cast<std::size_t>(group), group_width,
      "GGUF preview group offset");
  if (start >= values.size()) {
    throw std::runtime_error(std::string(label) +
                             " index outside value buffer");
  }
  const auto available = values.size() - start;
  auto emitted = std::min(group_width, available);
  if (limit < emitted) {
    emitted = static_cast<std::size_t>(limit);
  }

  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < emitted; ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << float_json_value(values[start + i]);
  }
  out << "]";
  return out.str();
}

std::string q4_k_dequant_subblock_values_json(
    const std::vector<float>& values,
    const int sub_block,
    const std::uint64_t limit) {
  return float_group_values_json(values, sub_block, 32U, limit,
                                 "Q4_K sub-block");
}

template <typename T>
std::string numeric_values_json(const std::vector<T>& values,
                                const std::size_t limit) {
  std::ostringstream out;
  out << "[";
  const auto emitted = std::min(values.size(), limit);
  for (std::size_t i = 0; i < emitted; ++i) {
    if (i != 0) {
      out << ", ";
    }
    if constexpr (std::is_same_v<T, float>) {
      out << float_json_value(values[i]);
    } else if constexpr (std::is_same_v<T, double>) {
      out << double_json_value(values[i]);
    } else {
      static_assert(std::is_integral_v<T>,
                    "numeric_values_json requires a numeric scalar");
      out << +values[i];
    }
  }
  out << "]";
  return out.str();
}

std::string float_values_json(const std::vector<float>& values,
                              const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string double_values_json(const std::vector<double>& values,
                               const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string int8_vector_json(const std::vector<std::int8_t>& values,
                             const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string int16_vector_json(const std::vector<std::int16_t>& values,
                              const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string uint8_vector_json(const std::vector<std::uint8_t>& values,
                              const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string uint16_vector_json(const std::vector<std::uint16_t>& values,
                               const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string uint32_vector_json(const std::vector<std::uint32_t>& values,
                               const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string int32_vector_json(const std::vector<std::int32_t>& values,
                              const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string int64_vector_json(const std::vector<std::int64_t>& values,
                              const std::size_t limit) {
  return numeric_values_json(values, limit);
}

std::string uint64_vector_json(const std::vector<std::uint64_t>& values,
                               const std::size_t limit) {
  return numeric_values_json(values, limit);
}

template <typename Dequant>
std::string float_dequant_stats_json(const Dequant& dequant,
                                     const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << float_json_value(dequant.checksum)
      << ",\n";
  out << nested << "\"max\": " << float_json_value(dequant.max) << ",\n";
  out << nested << "\"mean\": " << float_json_value(dequant.mean) << ",\n";
  out << nested << "\"min\": " << float_json_value(dequant.min) << ",\n";
  out << nested << "\"value_count\": " << dequant.values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

std::string q2_k_dequant_stats_json(
    const inferlab::GGUFQ2KBlockDequant& dequant, const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string legacy_q4_dequant_stats_json(const std::vector<float>& values,
                                         const float checksum,
                                         const float min_value,
                                         const float max_value,
                                         const float mean,
                                         const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << float_json_value(checksum) << ",\n";
  out << nested << "\"max\": " << float_json_value(max_value) << ",\n";
  out << nested << "\"mean\": " << float_json_value(mean) << ",\n";
  out << nested << "\"min\": " << float_json_value(min_value) << ",\n";
  out << nested << "\"value_count\": " << values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

std::string double_values_stats_json(const std::vector<double>& values,
                                     const double checksum,
                                     const double min_value,
                                     const double max_value,
                                     const double mean,
                                     const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << double_json_value(checksum) << ",\n";
  out << nested << "\"max\": " << double_json_value(max_value) << ",\n";
  out << nested << "\"mean\": " << double_json_value(mean) << ",\n";
  out << nested << "\"min\": " << double_json_value(min_value) << ",\n";
  out << nested << "\"value_count\": " << values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

template <typename Preview>
std::string integer_values_stats_json(const Preview& preview,
                                      const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << +preview.checksum << ",\n";
  out << nested << "\"max\": " << +preview.max << ",\n";
  out << nested << "\"mean\": " << double_json_value(preview.mean) << ",\n";
  out << nested << "\"min\": " << +preview.min << ",\n";
  out << nested << "\"value_count\": " << preview.values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

std::string int64_values_stats_json(
    const inferlab::GGUFInt64ValuesPreview& preview, const int indent) {
  return integer_values_stats_json(preview, indent);
}

std::string int32_values_stats_json(
    const inferlab::GGUFInt32ValuesPreview& preview, const int indent) {
  return integer_values_stats_json(preview, indent);
}

std::string int16_values_stats_json(
    const inferlab::GGUFInt16ValuesPreview& preview, const int indent) {
  return integer_values_stats_json(preview, indent);
}

std::string int8_values_stats_json(
    const inferlab::GGUFInt8ValuesPreview& preview, const int indent) {
  return integer_values_stats_json(preview, indent);
}

std::string q1_0_dequant_stats_json(
    const inferlab::GGUFQ1_0BlockDequant& dequant,
    const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << float_json_value(dequant.checksum)
      << ",\n";
  out << nested << "\"max\": " << float_json_value(dequant.max) << ",\n";
  out << nested << "\"mean\": " << float_json_value(dequant.mean) << ",\n";
  out << nested << "\"min\": " << float_json_value(dequant.min) << ",\n";
  out << nested << "\"negative_bit_count\": "
      << dequant.negative_bit_count << ",\n";
  out << nested << "\"positive_bit_count\": "
      << dequant.positive_bit_count << ",\n";
  out << nested << "\"value_count\": " << dequant.values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

std::string q8_0_dequant_stats_json(
    const inferlab::GGUFQ8_0BlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q8_1_dequant_stats_json(
    const inferlab::GGUFQ8_1BlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q8_k_dequant_stats_json(
    const inferlab::GGUFQ8KBlockDequant& dequant,
    const int indent) {
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"checksum\": " << float_json_value(dequant.checksum)
      << ",\n";
  out << nested << "\"max\": " << float_json_value(dequant.max) << ",\n";
  out << nested << "\"mean\": " << float_json_value(dequant.mean) << ",\n";
  out << nested << "\"min\": " << float_json_value(dequant.min) << ",\n";
  out << nested << "\"stored_block_sum_total\": "
      << dequant.stored_block_sum_total << ",\n";
  out << nested << "\"value_count\": " << dequant.values.size() << "\n";
  out << spaces(indent) << "}";
  return out.str();
}

std::string q3_k_dequant_stats_json(
    const inferlab::GGUFQ3KBlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q4_k_dequant_stats_json(
    const inferlab::GGUFQ4KBlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q5_k_dequant_stats_json(
    const inferlab::GGUFQ5KBlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q6_k_scales_json(const std::vector<inferlab::GGUFQ6KScale>& scales,
                             const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t group = 0; group < scales.size(); ++group) {
    const auto& scale = scales[group];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"effective_scale\": "
        << float_json_value(scale.effective_scale) << ",\n";
    out << spaces(indent + 4) << "\"group\": " << group << ",\n";
    out << spaces(indent + 4) << "\"scale\": "
        << static_cast<int>(scale.scale) << "\n";
    out << nested << "}" << (group + 1 == scales.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string iq4_xs_scales_json(
    const std::vector<inferlab::GGUFIQ4XSScale>& scales,
    const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t block = 0; block < scales.size(); ++block) {
    const auto& scale = scales[block];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"block\": " << block << ",\n";
    out << spaces(indent + 4) << "\"effective_scale\": "
        << float_json_value(scale.effective_scale) << ",\n";
    out << spaces(indent + 4) << "\"raw_scale\": "
        << static_cast<int>(scale.raw_scale) << ",\n";
    out << spaces(indent + 4) << "\"signed_scale\": "
        << static_cast<int>(scale.signed_scale) << "\n";
    out << nested << "}" << (block + 1 == scales.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string nvfp4_scales_json(
    const std::vector<inferlab::GGUFNVFP4SubBlockScale>& scales,
    const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t block = 0; block < scales.size(); ++block) {
    const auto& scale = scales[block];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"block\": " << block << ",\n";
    out << spaces(indent + 4) << "\"raw_scale\": "
        << static_cast<int>(scale.raw_scale) << ",\n";
    out << spaces(indent + 4) << "\"scale_fp32\": "
        << float_json_value(scale.scale) << "\n";
    out << nested << "}" << (block + 1 == scales.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string q6_k_dequant_stats_json(
    const inferlab::GGUFQ6KBlockDequant& dequant,
    const int indent) {
  return float_dequant_stats_json(dequant, indent);
}

std::string q2_k_dequant_group_values_json(
    const std::vector<float>& values,
    const int group,
    const std::uint64_t limit) {
  return float_group_values_json(values, group, 16U, limit, "Q2_K group");
}

std::string q3_k_dequant_group_values_json(
    const std::vector<float>& values,
    const int group,
    const std::uint64_t limit) {
  return float_group_values_json(values, group, 16U, limit, "Q3_K group");
}

std::string q6_k_dequant_group_values_json(
    const std::vector<float>& values,
    const int group,
    const std::uint64_t limit) {
  return float_group_values_json(values, group, 16U, limit, "Q6_K group");
}

std::string unpacked_nibbles_json(const std::vector<std::uint8_t>& bytes,
                                  const std::uint64_t start,
                                  const std::uint64_t byte_count,
                                  const std::uint64_t nibble_limit) {
  std::ostringstream out;
  out << "[";
  const auto safe_start =
      std::min<std::uint64_t>(start, static_cast<std::uint64_t>(bytes.size()));
  const auto available = static_cast<std::uint64_t>(bytes.size()) - safe_start;
  const auto emitted_bytes = std::min(byte_count, available);
  std::uint64_t emitted_nibbles = 0;
  for (std::uint64_t i = 0; i < emitted_bytes && emitted_nibbles < nibble_limit;
       ++i) {
    const auto packed = bytes[static_cast<std::size_t>(safe_start + i)];
    const int values[2] = {static_cast<int>(packed & 0x0FU),
                           static_cast<int>((packed >> 4U) & 0x0FU)};
    for (const auto value : values) {
      if (emitted_nibbles >= nibble_limit) {
        break;
      }
      if (emitted_nibbles != 0) {
        out << ", ";
      }
      out << value;
      ++emitted_nibbles;
    }
  }
  out << "]";
  return out.str();
}

void append_layout_segment(std::ostringstream& out,
                           const std::vector<std::uint8_t>& bytes,
                           const std::string& name,
                           const std::uint64_t start,
                           const std::uint64_t end_exclusive,
                           const int indent,
                           const bool trailing_comma) {
  out << spaces(indent) << "{\n";
  out << spaces(indent + 2) << "\"ascii_prefix\": "
      << inferlab_plan::json_string(ascii_prefix(bytes, start, end_exclusive)) << ",\n";
  out << spaces(indent + 2) << "\"end_exclusive\": " << end_exclusive
      << ",\n";
  out << spaces(indent + 2) << "\"hex_prefix\": "
      << inferlab_plan::json_string(hex_prefix(bytes, start, end_exclusive)) << ",\n";
  out << spaces(indent + 2) << "\"name\": " << inferlab_plan::json_string(name) << ",\n";
  out << spaces(indent + 2) << "\"size_bytes\": "
      << (end_exclusive >= start ? end_exclusive - start : 0) << ",\n";
  out << spaces(indent + 2) << "\"start\": " << start << "\n";
  out << spaces(indent) << "}" << (trailing_comma ? "," : "") << "\n";
}

std::string layout_segments_json(const inferlab::GGUFFileInfo& info,
                                 const std::vector<std::uint8_t>& bytes,
                                 const int indent) {
  const auto pad = spaces(indent);
  std::ostringstream out;
  out << "[\n";
  append_layout_segment(out, bytes, "header", 0, info.header_size_bytes,
                        indent + 2, true);
  append_layout_segment(out, bytes, "metadata", info.metadata_start_offset,
                        info.tensor_info_start_offset, indent + 2, true);
  append_layout_segment(out, bytes, "tensor_infos", info.tensor_info_start_offset,
                        info.tensor_info_end_offset, indent + 2, true);
  append_layout_segment(out, bytes, "tensor_data_padding",
                        info.tensor_info_end_offset,
                        info.tensor_data_start_offset, indent + 2, true);
  append_layout_segment(out, bytes, "tensor_data", info.tensor_data_start_offset,
                        static_cast<std::uint64_t>(bytes.size()), indent + 2,
                        false);
  out << pad << "]";
  return out.str();
}

std::string dimensions_json(const std::vector<std::uint64_t>& dimensions) {
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < dimensions.size(); ++i) {
    out << dimensions[i] << (i + 1 == dimensions.size() ? "" : ", ");
  }
  out << "]";
  return out.str();
}

std::string metadata_json(const std::vector<inferlab::GGUFMetadataKV>& metadata,
                          const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < metadata.size(); ++i) {
    const auto& item = metadata[i];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"key\": "
        << inferlab_plan::json_string(item.key) << ",\n";
    out << spaces(indent + 4) << "\"type\": "
        << inferlab_plan::json_string(
               inferlab::gguf_metadata_type_name(item.value.type))
        << ",\n";
    if (item.value.type == inferlab::GGUFMetadataType::kArray) {
      out << spaces(indent + 4) << "\"array_type\": "
          << inferlab_plan::json_string(
                 inferlab::gguf_metadata_type_name(item.value.array_type))
          << ",\n";
      out << spaces(indent + 4) << "\"array_length\": "
          << item.value.array_values.size() << ",\n";
    }
    out << spaces(indent + 4) << "\"value\": "
        << inferlab_plan::json_string(
               inferlab::gguf_metadata_value_to_string(item.value))
        << "\n";
    out << nested << "}" << (i + 1 == metadata.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string tensors_json(const std::vector<inferlab::GGUFTensorInfo>& tensors,
                         const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < tensors.size(); ++i) {
    const auto& tensor = tensors[i];
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"absolute_offset\": "
        << tensor.absolute_offset << ",\n";
    out << spaces(indent + 4) << "\"block_size\": "
        << tensor.type_info.block_size << ",\n";
    out << spaces(indent + 4) << "\"byte_size_known\": "
        << (tensor.byte_size_known ? "true" : "false") << ",\n";
    out << spaces(indent + 4) << "\"data_end_absolute_offset\": "
        << tensor.data_end_absolute_offset << ",\n";
    out << spaces(indent + 4) << "\"data_size_bytes\": "
        << tensor.data_size_bytes << ",\n";
    out << spaces(indent + 4) << "\"dimensions\": "
        << dimensions_json(tensor.dimensions) << ",\n";
    out << spaces(indent + 4) << "\"element_count\": "
        << tensor.element_count << ",\n";
    out << spaces(indent + 4) << "\"name\": "
        << inferlab_plan::json_string(tensor.name) << ",\n";
    out << spaces(indent + 4) << "\"quantized\": "
        << (tensor.type_info.quantized ? "true" : "false") << ",\n";
    out << spaces(indent + 4) << "\"relative_offset\": "
        << tensor.relative_offset << ",\n";
    out << spaces(indent + 4) << "\"row_size_bytes\": "
        << tensor.row_size_bytes << ",\n";
    out << spaces(indent + 4) << "\"type\": " << tensor.type << ",\n";
    out << spaces(indent + 4) << "\"type_name\": "
        << inferlab_plan::json_string(tensor.type_info.name) << ",\n";
    out << spaces(indent + 4) << "\"type_size_bytes\": "
        << tensor.type_info.type_size_bytes << "\n";
    out << nested << "}" << (i + 1 == tensors.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

bool supports_payload_preview(const std::string& type_name) {
  constexpr std::string_view kPreviewableTypes[] = {
      "f32",     "f64",     "i64",   "i32",   "i16",   "i8",
      "f16",     "q4_0",    "q1_0",  "iq2_xxs", "q4_1", "q5_0",
      "q5_1",    "iq4_nl",  "mxfp4", "nvfp4", "bf16",  "iq4_xs",
      "tq1_0",   "tq2_0",   "q2_K",  "q3_K",  "q4_K",  "q5_K",
      "q6_K",    "q8_K",    "q8_0",  "q8_1",
  };
  for (const auto candidate : kPreviewableTypes) {
    if (type_name == candidate) {
      return true;
    }
  }
  return false;
}

std::string payload_previews_json(
    const inferlab::GGUFFileInfo& info,
    const std::vector<std::uint8_t>& bytes,
    const int indent) {
  const auto pad = spaces(indent);
  const auto nested = spaces(indent + 2);
  bool first = true;
  std::ostringstream out;
  out << "[\n";
  for (const auto& tensor : info.tensors) {
    if (!supports_payload_preview(tensor.type_info.name)) {
      continue;
    }
    if (!first) {
      out << ",\n";
    }
    first = false;
    out << nested << "{\n";
    out << spaces(indent + 4) << "\"block_index\": 0,\n";
    out << spaces(indent + 4) << "\"block_size\": "
        << tensor.type_info.block_size << ",\n";
    out << spaces(indent + 4) << "\"name\": " << inferlab_plan::json_string(tensor.name)
        << ",\n";
    out << spaces(indent + 4) << "\"start\": " << tensor.absolute_offset
        << ",\n";
    out << spaces(indent + 4) << "\"type_name\": "
        << inferlab_plan::json_string(tensor.type_info.name) << ",\n";
    out << spaces(indent + 4) << "\"type_size_bytes\": "
        << tensor.type_info.type_size_bytes;
    if (tensor.type_info.name == "f32") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 8);
      const auto preview_bytes = preview_count * 4U;
      const auto f32_preview = inferlab::decode_gguf_f32_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint32_vector_json(f32_preview.raw_values,
                                f32_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = little_endian_float32(raw_4_bytes)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << float_values_json(f32_preview.values, f32_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << legacy_q4_dequant_stats_json(
                 f32_preview.values, f32_preview.checksum, f32_preview.min,
                 f32_preview.max, f32_preview.mean, indent + 4)
          << "\n";
    } else if (tensor.type_info.name == "f64") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 4);
      const auto preview_bytes = preview_count * 8U;
      const auto f64_preview = inferlab::decode_gguf_f64_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint64_vector_json(f64_preview.raw_values,
                                f64_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = little_endian_float64(raw_8_bytes)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << double_values_json(f64_preview.values, f64_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << double_values_stats_json(
                 f64_preview.values, f64_preview.checksum, f64_preview.min,
                 f64_preview.max, f64_preview.mean, indent + 4)
          << "\n";
    } else if (tensor.type_info.name == "i64") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 4);
      const auto preview_bytes = preview_count * 8U;
      const auto i64_preview = inferlab::decode_gguf_i64_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint64_vector_json(i64_preview.raw_values,
                                i64_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = int64_t(little_endian_raw_8_bytes)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << int64_vector_json(i64_preview.values, i64_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << int64_values_stats_json(i64_preview, indent + 4) << "\n";
    } else if (tensor.type_info.name == "i32") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 4);
      const auto preview_bytes = preview_count * 4U;
      const auto i32_preview = inferlab::decode_gguf_i32_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint32_vector_json(i32_preview.raw_values,
                                i32_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = int32_t(little_endian_raw_4_bytes)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << int32_vector_json(i32_preview.values, i32_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << int32_values_stats_json(i32_preview, indent + 4) << "\n";
    } else if (tensor.type_info.name == "i16") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 4);
      const auto preview_bytes = preview_count * 2U;
      const auto i16_preview = inferlab::decode_gguf_i16_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint16_vector_json(i16_preview.raw_values,
                                i16_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = int16_t(little_endian_raw_2_bytes)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << int16_vector_json(i16_preview.values, i16_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << int16_values_stats_json(i16_preview, indent + 4) << "\n";
    } else if (tensor.type_info.name == "i8") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 4);
      const auto preview_bytes = preview_count;
      const auto i8_preview = inferlab::decode_gguf_i8_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint8_vector_json(i8_preview.raw_values,
                               i8_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = int8_t(raw_byte)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << int8_vector_json(i8_preview.values, i8_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << int8_values_stats_json(i8_preview, indent + 4) << "\n";
    } else if (tensor.type_info.name == "f16") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 8);
      const auto preview_bytes = preview_count * 2U;
      const auto f16_preview = inferlab::decode_gguf_f16_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint16_vector_json(f16_preview.raw_values,
                                f16_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = gguf_fp16_to_float32(raw_half)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << float_values_json(f16_preview.values, f16_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << legacy_q4_dequant_stats_json(
                 f16_preview.values, f16_preview.checksum, f16_preview.min,
                 f16_preview.max, f16_preview.mean, indent + 4)
          << "\n";
    } else if (tensor.type_info.name == "iq2_xxs") {
      const auto iq2_xxs_dequant = inferlab::dequantize_gguf_iq2_xxs_block(
          byte_slice(bytes, tensor.absolute_offset, tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset +
                                        tensor.type_info.type_size_bytes,
                                    tensor.type_info.type_size_bytes))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(iq2_xxs_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"qs_words_prefix\": "
          << uint16_vector_json(iq2_xxs_dequant.qs_words, 8) << ",\n";
      out << spaces(indent + 4) << "\"grid_indices_prefix\": "
          << uint8_vector_json(iq2_xxs_dequant.grid_indices, 8) << ",\n";
      out << spaces(indent + 4) << "\"sign_codes_prefix\": "
          << uint8_vector_json(iq2_xxs_dequant.sign_codes, 8) << ",\n";
      out << spaces(indent + 4) << "\"scale_nibbles\": "
          << uint8_vector_json(iq2_xxs_dequant.scale_nibbles,
                               iq2_xxs_dequant.scale_nibbles.size())
          << ",\n";
      out << spaces(indent + 4) << "\"effective_scales\": "
          << float_values_json(iq2_xxs_dequant.effective_scales,
                               iq2_xxs_dequant.effective_scales.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string(
                 "value = d*(0.5+scale_nibble)*0.25 * iq2xxs_grid[grid_index][lane] * (ksigns_iq2xs[sign_code] bit ? -1 : 1)")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(iq2_xxs_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_group_sums\": "
          << float_values_json(iq2_xxs_dequant.group_sums,
                               iq2_xxs_dequant.group_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 iq2_xxs_dequant.values, iq2_xxs_dequant.checksum,
                 iq2_xxs_dequant.min, iq2_xxs_dequant.max,
                 iq2_xxs_dequant.mean, indent + 4)
          << "\n";
    } else if (tensor.type_info.name == "q1_0") {
      const auto q1_0_dequant = inferlab::dequantize_gguf_q1_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q1_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"packed_bit_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 18, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"bit_values_prefix\": "
          << uint8_vector_json(q1_0_dequant.bit_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = bit ? fp16(d) : -fp16(d)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q1_0_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q1_0_dequant_stats_json(q1_0_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"bit_order\": "
          << inferlab_plan::json_string("bits are read least-significant-bit first within each byte")
          << "\n";
    } else if (tensor.type_info.name == "q4_0") {
      const auto q4_0_dequant = inferlab::dequantize_gguf_q4_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q4_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 18, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(q4_0_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * (q4 - 8)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q4_0_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 q4_0_dequant.values, q4_0_dequant.checksum,
                 q4_0_dequant.min, q4_0_dequant.max, q4_0_dequant.mean,
                 indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "q4_1") {
      const auto q4_1_dequant = inferlab::dequantize_gguf_q4_1_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q4_1_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"m_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 4, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"m_fp32\": "
          << float_json_value(q4_1_dequant.m) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 20, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(q4_1_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * q4 + fp16(m)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q4_1_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 q4_1_dequant.values, q4_1_dequant.checksum,
                 q4_1_dequant.min, q4_1_dequant.max, q4_1_dequant.mean,
                 indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "q5_0") {
      const auto q5_0_dequant = inferlab::dequantize_gguf_q5_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q5_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"high_bits_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 6, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 6,
                                    tensor.absolute_offset + 22, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(q5_0_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * (q5 - 16)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q5_0_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 q5_0_dequant.values, q5_0_dequant.checksum,
                 q5_0_dequant.min, q5_0_dequant.max, q5_0_dequant.mean,
                 indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"bit_order\": "
          << inferlab_plan::json_string("high bit uses qh[value_index / 8] bit value_index % 8")
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "q5_1") {
      const auto q5_1_dequant = inferlab::dequantize_gguf_q5_1_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q5_1_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"m_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 4, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"m_fp32\": "
          << float_json_value(q5_1_dequant.m) << ",\n";
      out << spaces(indent + 4) << "\"high_bits_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 8, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 8,
                                    tensor.absolute_offset + 24, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(q5_1_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * q5 + fp16(m)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q5_1_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 q5_1_dequant.values, q5_1_dequant.checksum,
                 q5_1_dequant.min, q5_1_dequant.max, q5_1_dequant.mean,
                 indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"bit_order\": "
          << inferlab_plan::json_string("high bit uses qh[value_index / 8] bit value_index % 8")
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "iq4_nl") {
      const auto iq4_nl_dequant = inferlab::dequantize_gguf_iq4_nl_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(iq4_nl_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 18, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(iq4_nl_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"codebook_table\": "
          << "[-127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69, 89, 113]"
          << ",\n";
      out << spaces(indent + 4) << "\"codebook_values_prefix\": "
          << int8_vector_json(iq4_nl_dequant.codebook_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * iq4nl_codebook[q4_index]") << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(iq4_nl_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 iq4_nl_dequant.values, iq4_nl_dequant.checksum,
                 iq4_nl_dequant.min, iq4_nl_dequant.max,
                 iq4_nl_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "mxfp4") {
      const auto mxfp4_dequant = inferlab::dequantize_gguf_mxfp4_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"e\": "
          << static_cast<unsigned int>(mxfp4_dequant.e) << ",\n";
      out << spaces(indent + 4) << "\"e_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 1, 1))
          << ",\n";
      out << spaces(indent + 4) << "\"e_scale_fp32\": "
          << float_json_value(mxfp4_dequant.e_scale) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 1,
                                    tensor.absolute_offset + 17, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(mxfp4_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"codebook_table\": "
          << "[0, 1, 2, 3, 4, 6, 8, 12, 0, -1, -2, -3, -4, -6, -8, -12]"
          << ",\n";
      out << spaces(indent + 4) << "\"codebook_values_prefix\": "
          << int8_vector_json(mxfp4_dequant.codebook_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = ggml_e8m0_to_fp32_half(e) * kvalues_mxfp4[q4_index]")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(mxfp4_dequant.values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 mxfp4_dequant.values, mxfp4_dequant.checksum,
                 mxfp4_dequant.min, mxfp4_dequant.max,
                 mxfp4_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("first 16 values use low nibbles; second 16 use high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "nvfp4") {
      const auto nvfp4_dequant = inferlab::dequantize_gguf_nvfp4_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"scale_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 4, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"scales\": "
          << nvfp4_scales_json(nvfp4_dequant.scales, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 36, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(nvfp4_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"codebook_table\": "
          << "[0, 1, 2, 3, 4, 6, 8, 12, 0, -1, -2, -3, -4, -6, -8, -12]"
          << ",\n";
      out << spaces(indent + 4) << "\"codebook_values_prefix\": "
          << int8_vector_json(nvfp4_dequant.codebook_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = ggml_ue4m3_to_fp32(scale_byte) * kvalues_mxfp4[q4_index]")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock0_values_prefix\": "
          << q4_k_dequant_subblock_values_json(nvfp4_dequant.values, 0, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock1_values_prefix\": "
          << q4_k_dequant_subblock_values_json(nvfp4_dequant.values, 1, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(nvfp4_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock_sums\": "
          << float_values_json(nvfp4_dequant.subblock_sums,
                               nvfp4_dequant.subblock_sums.size())
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 nvfp4_dequant.values, nvfp4_dequant.checksum,
                 nvfp4_dequant.min, nvfp4_dequant.max,
                 nvfp4_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("each 16-value sub-block uses 8 low nibbles followed by 8 high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "bf16") {
      const auto preview_count = std::min<std::uint64_t>(tensor.element_count, 8);
      const auto preview_bytes = preview_count * 2U;
      const auto bf16_preview = inferlab::decode_gguf_bf16_values(
          byte_slice(bytes, tensor.absolute_offset, preview_bytes),
          preview_count);
      out << ",\n";
      out << spaces(indent + 4) << "\"raw_values_hex_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + preview_bytes,
                                    static_cast<std::size_t>(preview_bytes)))
          << ",\n";
      out << spaces(indent + 4) << "\"raw_values_prefix\": "
          << uint16_vector_json(bf16_preview.raw_values,
                                bf16_preview.raw_values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"decode_rule\": "
          << inferlab_plan::json_string("value = reinterpret_float32(uint32_t(raw_bf16) << 16)")
          << ",\n";
      out << spaces(indent + 4) << "\"values_prefix\": "
          << float_values_json(bf16_preview.values, bf16_preview.values.size())
          << ",\n";
      out << spaces(indent + 4) << "\"stats\": "
          << legacy_q4_dequant_stats_json(
                 bf16_preview.values, bf16_preview.checksum,
                 bf16_preview.min, bf16_preview.max, bf16_preview.mean,
                 indent + 4)
          << "\n";
    } else if (tensor.type_info.name == "iq4_xs") {
      const auto iq4_xs_dequant = inferlab::dequantize_gguf_iq4_xs_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(iq4_xs_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"scales_h_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 4, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"scales_h\": "
          << iq4_xs_dequant.scales_h << ",\n";
      out << spaces(indent + 4) << "\"scales_l_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 8, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 8,
                                    tensor.absolute_offset + 136, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scales\": "
          << iq4_xs_scales_json(iq4_xs_dequant.scales, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(iq4_xs_dequant.quant_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"codebook_table\": "
          << "[-127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69, 89, 113]"
          << ",\n";
      out << spaces(indent + 4) << "\"codebook_values_prefix\": "
          << int8_vector_json(iq4_xs_dequant.codebook_values, 32) << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * (scale6 - 32) * iq4nl_codebook[q4_index]")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock0_values_prefix\": "
          << q4_k_dequant_subblock_values_json(iq4_xs_dequant.values, 0, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock1_values_prefix\": "
          << q4_k_dequant_subblock_values_json(iq4_xs_dequant.values, 1, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(iq4_xs_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock_sums\": "
          << float_values_json(iq4_xs_dequant.subblock_sums,
                               iq4_xs_dequant.subblock_sums.size())
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 iq4_xs_dequant.values, iq4_xs_dequant.checksum,
                 iq4_xs_dequant.min, iq4_xs_dequant.max,
                 iq4_xs_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"nibble_order\": "
          << inferlab_plan::json_string("each 32-value sub-block uses 16 low nibbles followed by 16 high nibbles")
          << "\n";
    } else if (tensor.type_info.name == "tq1_0") {
      const auto tq1_0_dequant = inferlab::dequantize_gguf_tq1_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 52,
                                    tensor.absolute_offset + 54, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(tq1_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"packed_qs_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 48, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_qh_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 48,
                                    tensor.absolute_offset + 52, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(tq1_0_dequant.quant_values, 64) << ",\n";
      out << spaces(indent + 4) << "\"quant_counts\": "
          << uint64_vector_json(tq1_0_dequant.quant_counts,
                                tq1_0_dequant.quant_counts.size())
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * (ternary_quant - 1)")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(tq1_0_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 tq1_0_dequant.values, tq1_0_dequant.checksum,
                 tq1_0_dequant.min, tq1_0_dequant.max,
                 tq1_0_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"trit_order\": "
          << inferlab_plan::json_string("48 qs bytes expand as 32-byte then 16-byte groups with five ternary digits per byte; 4 qh bytes then add four tail ternary digits per byte")
          << "\n";
    } else if (tensor.type_info.name == "tq2_0") {
      const auto tq2_0_dequant = inferlab::dequantize_gguf_tq2_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 64,
                                    tensor.absolute_offset + 66, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(tq2_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 64, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << uint8_vector_json(tq2_0_dequant.quant_values, 64) << ",\n";
      out << spaces(indent + 4) << "\"quant_counts\": "
          << uint64_vector_json(tq2_0_dequant.quant_counts,
                                tq2_0_dequant.quant_counts.size())
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * (two_bit_quant - 1)")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(tq2_0_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << legacy_q4_dequant_stats_json(
                 tq2_0_dequant.values, tq2_0_dequant.checksum,
                 tq2_0_dequant.min, tq2_0_dequant.max,
                 tq2_0_dequant.mean, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"bit_order\": "
          << inferlab_plan::json_string("64 packed bytes are expanded in 32-byte groups, reading 2-bit planes l=0..3")
          << "\n";
    } else if (tensor.type_info.name == "q2_K") {
      const auto q2_k_dequant = inferlab::dequantize_gguf_q2_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 80,
                                    tensor.absolute_offset + 82, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q2_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"dmin_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 82,
                                    tensor.absolute_offset + 84, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"dmin_fp32\": "
          << float_json_value(q2_k_dequant.dmin) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 16,
                                    tensor.absolute_offset + 80, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_min_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 16, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_min_pairs\": "
          << q4_k_scale_min_pairs_json(q2_k_dequant.scale_min_pairs,
                                       indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * scale * q2 - fp16(dmin) * min")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group0_values_prefix\": "
          << q2_k_dequant_group_values_json(q2_k_dequant.values, 0, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group1_values_prefix\": "
          << q2_k_dequant_group_values_json(q2_k_dequant.values, 1, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q2_k_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_group_sums\": "
          << float_values_json(q2_k_dequant.group_sums,
                               q2_k_dequant.group_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q2_k_dequant_stats_json(q2_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"scale_groups\": "
          << q2_k_dequant.scale_min_pairs.size() << "\n";
    } else if (tensor.type_info.name == "q3_K") {
      const auto q3_k_dequant = inferlab::dequantize_gguf_q3_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 108,
                                    tensor.absolute_offset + 110, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q3_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"high_mask_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 32, 32))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 32,
                                    tensor.absolute_offset + 96, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 96,
                                    tensor.absolute_offset + 108, 12))
          << ",\n";
      out << spaces(indent + 4) << "\"scales\": "
          << q6_k_scales_json(q3_k_dequant.scales, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * int8(scale) * (low2(qs) - (hmask_bit ? 0 : 4))")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group0_values_prefix\": "
          << q3_k_dequant_group_values_json(q3_k_dequant.values, 0, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group1_values_prefix\": "
          << q3_k_dequant_group_values_json(q3_k_dequant.values, 1, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q3_k_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_group_sums\": "
          << float_values_json(q3_k_dequant.group_sums,
                               q3_k_dequant.group_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q3_k_dequant_stats_json(q3_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"scale_groups\": "
          << q3_k_dequant.scales.size() << "\n";
    } else if (tensor.type_info.name == "q4_K") {
      const auto q4_k_dequant = inferlab::dequantize_gguf_q4_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q4_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"dmin_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 4, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"dmin_fp32\": "
          << float_json_value(q4_k_dequant.dmin) << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 16,
                                    tensor.absolute_offset + 144, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 16, 12))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_min_pairs\": "
          << q4_k_scale_min_pairs_json(q4_k_dequant.scale_min_pairs,
                                       indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * scale * q - fp16(dmin) * min")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock0_values_prefix\": "
          << q4_k_dequant_subblock_values_json(
                 q4_k_dequant.values, 0, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock1_values_prefix\": "
          << q4_k_dequant_subblock_values_json(
                 q4_k_dequant.values, 1, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q4_k_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock_sums\": "
          << float_values_json(q4_k_dequant.subblock_sums,
                               q4_k_dequant.subblock_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q4_k_dequant_stats_json(q4_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"unpacked_nibbles_prefix\": "
          << unpacked_nibbles_json(bytes, tensor.absolute_offset + 16, 16, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"sub_blocks\": 8\n";
    } else if (tensor.type_info.name == "q5_K") {
      const auto q5_k_dequant = inferlab::dequantize_gguf_q5_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q5_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"dmin_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 2,
                                    tensor.absolute_offset + 4, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"dmin_fp32\": "
          << float_json_value(q5_k_dequant.dmin) << ",\n";
      out << spaces(indent + 4) << "\"high_bits_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 16,
                                    tensor.absolute_offset + 48, 32))
          << ",\n";
      out << spaces(indent + 4) << "\"packed_quant_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 48,
                                    tensor.absolute_offset + 176, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 16, 12))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_min_pairs\": "
          << q4_k_scale_min_pairs_json(q5_k_dequant.scale_min_pairs,
                                       indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * scale * (low4(qs) + high1(qh) * 16) - fp16(dmin) * min")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock0_values_prefix\": "
          << q4_k_dequant_subblock_values_json(
                 q5_k_dequant.values, 0, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock1_values_prefix\": "
          << q4_k_dequant_subblock_values_json(
                 q5_k_dequant.values, 1, 32)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q5_k_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_subblock_sums\": "
          << float_values_json(q5_k_dequant.subblock_sums,
                               q5_k_dequant.subblock_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q5_k_dequant_stats_json(q5_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"sub_blocks\": 8\n";
    } else if (tensor.type_info.name == "q6_K") {
      const auto q6_k_dequant = inferlab::dequantize_gguf_q6_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 208,
                                    tensor.absolute_offset + 210, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q6_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"high_bits_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 128,
                                    tensor.absolute_offset + 192, 32))
          << ",\n";
      out << spaces(indent + 4) << "\"low_bits_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 128, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scale_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 192,
                                    tensor.absolute_offset + 208, 16))
          << ",\n";
      out << spaces(indent + 4) << "\"scales\": "
          << q6_k_scales_json(q6_k_dequant.scales, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = fp16(d) * int8(scale) * (low4(ql) + high2(qh) * 16 - 32)")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group0_values_prefix\": "
          << q6_k_dequant_group_values_json(q6_k_dequant.values, 0, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_group1_values_prefix\": "
          << q6_k_dequant_group_values_json(q6_k_dequant.values, 1, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q6_k_dequant.values, 64) << ",\n";
      out << spaces(indent + 4) << "\"dequant_group_sums\": "
          << float_values_json(q6_k_dequant.group_sums,
                               q6_k_dequant.group_sums.size()) << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q6_k_dequant_stats_json(q6_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"scale_groups\": "
          << q6_k_dequant.scales.size() << "\n";
    } else if (tensor.type_info.name == "q8_K") {
      const auto q8_k_dequant = inferlab::dequantize_gguf_q8_k_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_float_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 4, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q8_k_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"block_sums_prefix\": "
          << int16_vector_json(q8_k_dequant.block_sums, 16) << ",\n";
      out << spaces(indent + 4) << "\"block_sum_bytes_prefix\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 260,
                                    tensor.absolute_offset + 292, 32))
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q8_k_dequant.values, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = int8_quant * float32(d); bsums store sum(qs) for each 16-value group")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q8_k_dequant_stats_json(q8_k_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"quant_sum_first_block\": "
          << q8_k_dequant.quant_sum << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << int8_vector_json(q8_k_dequant.quant_values, 16)
          << "\n";
    } else if (tensor.type_info.name == "q8_1") {
      const auto q8_1_dequant = inferlab::dequantize_gguf_q8_1_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_float_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 4, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q8_1_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"s_float_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset + 4,
                                    tensor.absolute_offset + 8, 4))
          << ",\n";
      out << spaces(indent + 4) << "\"s_fp32\": "
          << float_json_value(q8_1_dequant.s) << ",\n";
      out << spaces(indent + 4) << "\"expected_s_from_quant_sum\": "
          << float_json_value(q8_1_dequant.expected_s_from_quant_sum)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q8_1_dequant.values, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = int8_quant * float32(d); s = d * sum(qs)")
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q8_1_dequant_stats_json(q8_1_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"quant_sum_first_block\": "
          << q8_1_dequant.quant_sum << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << int8_vector_json(q8_1_dequant.quant_values, 16)
          << "\n";
    } else if (tensor.type_info.name == "q8_0") {
      const auto q8_0_dequant = inferlab::dequantize_gguf_q8_0_block(
          byte_slice(bytes, tensor.absolute_offset,
                     tensor.type_info.type_size_bytes));
      out << ",\n";
      out << spaces(indent + 4) << "\"d_half_hex\": "
          << inferlab_plan::json_string(hex_prefix(bytes, tensor.absolute_offset,
                                    tensor.absolute_offset + 2, 2))
          << ",\n";
      out << spaces(indent + 4) << "\"d_fp32\": "
          << float_json_value(q8_0_dequant.d) << ",\n";
      out << spaces(indent + 4) << "\"dequant_values_prefix\": "
          << float_values_json(q8_0_dequant.values, 16)
          << ",\n";
      out << spaces(indent + 4) << "\"dequant_rule\": "
          << inferlab_plan::json_string("value = int8_quant * fp16(d)") << ",\n";
      out << spaces(indent + 4) << "\"dequant_stats\": "
          << q8_0_dequant_stats_json(q8_0_dequant, indent + 4)
          << ",\n";
      out << spaces(indent + 4) << "\"quant_sum_first_block\": "
          << q8_0_dequant.quant_sum << ",\n";
      out << spaces(indent + 4) << "\"quant_values_prefix\": "
          << int8_vector_json(q8_0_dequant.quant_values, 16)
          << "\n";
    }
    out << nested << "}";
  }
  if (!first) {
    out << "\n";
  }
  out << pad << "]";
  return out.str();
}

std::uint64_t known_tensor_data_bytes(
    const std::vector<inferlab::GGUFTensorInfo>& tensors) {
  std::uint64_t total = 0;
  for (const auto& tensor : tensors) {
    if (tensor.byte_size_known) {
      total = inferlab::internal::checked_add_u64(
          total, tensor.data_size_bytes, "known GGUF tensor data bytes");
    }
  }
  return total;
}

std::uint64_t quantized_tensor_count(
    const std::vector<inferlab::GGUFTensorInfo>& tensors) {
  std::uint64_t total = 0;
  for (const auto& tensor : tensors) {
    if (tensor.type_info.quantized) {
      ++total;
    }
  }
  return total;
}

std::string to_json(const inferlab::GGUFFileInfo& info,
                    const std::vector<std::uint8_t>& bytes,
                    const std::string& source,
                    const std::string& input_path,
                    const std::uint64_t max_input_bytes) {
  const auto fixture_size = bytes.size();
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_gguf_probe",
      "Executed bounded scalar GGUF inspection and preview decoding only; not "
      "a production mmap/streaming model loader, product runtime validation, "
      "or GPU dequantization benchmark.");
  out << "  \"gguf\": {\n";
  out << "    \"alignment\": " << info.alignment << ",\n";
  out << "    \"file_size_bytes\": " << info.file_size_bytes << ",\n";
  out << "    \"fixture_size_bytes\": " << fixture_size << ",\n";
  out << "    \"header_size_bytes\": " << info.header_size_bytes << ",\n";
  out << "    \"input_path\": "
      << inferlab_plan::json_string(
             inferlab_plan::redacted_path_label(input_path))
      << ",\n";
  out << "    \"input_path_redacted\": "
      << (inferlab_plan::path_was_redacted(
              input_path, inferlab_plan::redacted_path_label(input_path))
              ? "true"
              : "false")
      << ",\n";
  out << "    \"input_read_mode\": "
      << inferlab_plan::json_string(
             source == "file" ? "full_buffer_with_size_guard"
                              : "embedded_synthetic")
      << ",\n";
  out << "    \"max_input_bytes\": " << max_input_bytes << ",\n";
  out << "    \"layout_segments\": "
      << layout_segments_json(info, bytes, 4) << ",\n";
  out << "    \"metadata\": " << metadata_json(info.metadata, 4) << ",\n";
  out << "    \"metadata_kv_count\": " << info.metadata_kv_count << ",\n";
  out << "    \"metadata_start_offset\": " << info.metadata_start_offset
      << ",\n";
  out << "    \"payload_previews\": "
      << payload_previews_json(info, bytes, 4) << ",\n";
  out << "    \"known_tensor_data_bytes\": "
      << known_tensor_data_bytes(info.tensors) << ",\n";
  out << "    \"quantized_tensor_count\": "
      << quantized_tensor_count(info.tensors) << ",\n";
  out << "    \"source\": " << inferlab_plan::json_string(source) << ",\n";
  out << "    \"tensor_count\": " << info.tensor_count << ",\n";
  out << "    \"tensor_data_start_offset\": " << info.tensor_data_start_offset
      << ",\n";
  out << "    \"tensor_data_padding_bytes\": "
      << info.tensor_data_padding_bytes << ",\n";
  out << "    \"tensor_info_end_offset\": " << info.tensor_info_end_offset
      << ",\n";
  out << "    \"tensor_info_start_offset\": " << info.tensor_info_start_offset
      << ",\n";
  out << "    \"tensors\": " << tensors_json(info.tensors, 4) << ",\n";
  out << "    \"version\": " << info.version << "\n";
  out << "  },\n";
  out << "  \"schema_version\": 1\n";
  out << "}\n";
  return out.str();
}

const inferlab_plan::CliOptionSpec kCliOptions{
    {"--repo-root", "--input", "--output", "--max-input-bytes"},
    {},
};

}  // namespace

int inferlab::gguf_probe::run(const int argc, char** argv) {
  std::string input_path;
  std::string output_path;
  std::string repo_root = ".";
  try {
    if (inferlab_plan::handle_cli_help(argc, argv, argv[0], kCliOptions)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, kCliOptions);
    repo_root = inferlab_plan::arg_value_with_spec(
        argc, argv, "--repo-root", ".", kCliOptions);
    const std::string input_arg =
        inferlab_plan::arg_value_with_spec(argc, argv, "--input", "",
                                           kCliOptions);
    const std::string output_arg =
        inferlab_plan::arg_value_with_spec(argc, argv, "--output", "",
                                           kCliOptions);
    const auto max_input_bytes = inferlab_plan::parse_positive_u64_arg(
        "--max-input-bytes",
        inferlab_plan::arg_value_with_spec(
            argc, argv, "--max-input-bytes",
            std::to_string(inferlab::kDefaultMaxInMemoryGGUFFileBytes),
            kCliOptions));
    input_path =
        input_arg.empty()
            ? ""
            : inferlab_plan::resolve_repo_relative_path(repo_root, input_arg)
                  .generic_string();
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      repo_root, output_arg)
                      .generic_string();

    const auto bytes =
        input_path.empty() ? synthetic_gguf()
                           : read_binary_file(input_path, max_input_bytes);
    const auto info = inferlab::parse_gguf_bytes(bytes);
    const auto payload =
        to_json(info, bytes, input_path.empty() ? "embedded_synthetic" : "file",
                input_path, max_input_bytes);
    inferlab_plan::write_or_print(output_path, payload);
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "error: ", {repo_root, input_path, output_path});
    inferlab_plan::print_cli_usage(argv[0], kCliOptions);
    return 1;
  }
  return 0;
}
