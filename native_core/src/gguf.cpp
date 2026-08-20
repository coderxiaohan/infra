#include "inferlab/gguf.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

namespace inferlab {
namespace {

constexpr std::uint8_t kMagic[4] = {'G', 'G', 'U', 'F'};
constexpr std::uint32_t kSupportedVersion = 3;
constexpr std::uint32_t kDefaultAlignment = 32;
constexpr std::uint64_t kGGMLKQuantBlockSize = 256;
constexpr std::uint64_t kLegacyQ4ValueCount = 32;
constexpr std::uint64_t kQ1_0TypeSizeBytes = 18;
constexpr std::uint64_t kQ1_0ValueCount = 128;
constexpr std::uint64_t kQ1_0QuantOffset = 2;
constexpr std::uint64_t kIQ2XXSTypeSizeBytes = 66;
constexpr std::uint64_t kIQ2XXSQSOffset = 2;
constexpr std::uint64_t kIQ2XXSWordCount = 32;
constexpr std::uint64_t kIQ2XXSGroups = 8;
constexpr std::uint64_t kIQ2XXSChunksPerGroup = 4;
constexpr std::uint64_t kIQ2XXSChunkCount =
    kIQ2XXSGroups * kIQ2XXSChunksPerGroup;
constexpr std::uint64_t kIQ2XXSValuesPerChunk = 8;
constexpr std::uint64_t kIQ2XXSValueCount =
    kIQ2XXSChunkCount * kIQ2XXSValuesPerChunk;
// IQ2_XXS uses fixed GGML lookup tables for sign masks and 8-lane grids.
constexpr std::uint8_t kIQ2XXSMasks[8] = {1,  2,  4,   8,
                                          16, 32, 64, 128};
constexpr std::uint8_t kIQ2XXSSigns[128] = {
    0,   129, 130, 3,   132, 5,   6,   135, 136, 9,   10,  139, 12,
    141, 142, 15,  144, 17,  18,  147, 20,  149, 150, 23,  24,  153,
    154, 27,  156, 29,  30,  159, 160, 33,  34,  163, 36,  165, 166,
    39,  40,  169, 170, 43,  172, 45,  46,  175, 48,  177, 178, 51,
    180, 53,  54,  183, 184, 57,  58,  187, 60,  189, 190, 63,  192,
    65,  66,  195, 68,  197, 198, 71,  72,  201, 202, 75,  204, 77,
    78,  207, 80,  209, 210, 83,  212, 85,  86,  215, 216, 89,  90,
    219, 92,  221, 222, 95,  96,  225, 226, 99,  228, 101, 102, 231,
    232, 105, 106, 235, 108, 237, 238, 111, 240, 113, 114, 243, 116,
    245, 246, 119, 120, 249, 250, 123, 252, 125, 126, 255};
constexpr std::uint64_t kIQ2XXSGrid[256] = {
    0x0808080808080808, 0x080808080808082b, 0x0808080808081919,
    0x0808080808082b08, 0x0808080808082b2b, 0x0808080808190819,
    0x0808080808191908, 0x08080808082b0808, 0x08080808082b082b,
    0x08080808082b2b08, 0x08080808082b2b2b, 0x0808080819080819,
    0x0808080819081908, 0x0808080819190808, 0x0808080819192b08,
    0x08080808192b0819, 0x08080808192b1908, 0x080808082b080808,
    0x080808082b08082b, 0x080808082b082b2b, 0x080808082b2b082b,
    0x0808081908080819, 0x0808081908081908, 0x0808081908190808,
    0x0808081908191919, 0x0808081919080808, 0x080808192b081908,
    0x080808192b192b08, 0x0808082b08080808, 0x0808082b0808082b,
    0x0808082b082b082b, 0x0808082b2b08082b, 0x0808190808080819,
    0x0808190808081908, 0x0808190808190808, 0x08081908082b0819,
    0x08081908082b1908, 0x0808190819080808, 0x080819081908082b,
    0x0808190819082b08, 0x08081908192b0808, 0x080819082b080819,
    0x080819082b081908, 0x080819082b190808, 0x080819082b2b1908,
    0x0808191908080808, 0x080819190808082b, 0x0808191908082b08,
    0x08081919082b0808, 0x080819191908192b, 0x08081919192b2b19,
    0x080819192b080808, 0x080819192b190819, 0x0808192b08082b19,
    0x0808192b08190808, 0x0808192b19080808, 0x0808192b2b081908,
    0x0808192b2b2b1908, 0x08082b0808080808, 0x08082b0808081919,
    0x08082b0808082b08, 0x08082b0808191908, 0x08082b08082b2b08,
    0x08082b0819080819, 0x08082b0819081908, 0x08082b0819190808,
    0x08082b081919082b, 0x08082b082b082b08, 0x08082b1908081908,
    0x08082b1919080808, 0x08082b2b0808082b, 0x08082b2b08191908,
    0x0819080808080819, 0x0819080808081908, 0x0819080808190808,
    0x08190808082b0819, 0x0819080819080808, 0x08190808192b0808,
    0x081908082b081908, 0x081908082b190808, 0x081908082b191919,
    0x0819081908080808, 0x0819081908082b08, 0x08190819082b0808,
    0x0819081919190808, 0x0819081919192b2b, 0x081908192b080808,
    0x0819082b082b1908, 0x0819082b19081919, 0x0819190808080808,
    0x0819190808082b08, 0x08191908082b0808, 0x08191908082b1919,
    0x0819190819082b19, 0x081919082b080808, 0x0819191908192b08,
    0x08191919192b082b, 0x0819192b08080808, 0x0819192b0819192b,
    0x08192b0808080819, 0x08192b0808081908, 0x08192b0808190808,
    0x08192b0819080808, 0x08192b082b080819, 0x08192b1908080808,
    0x08192b1908081919, 0x08192b192b2b0808, 0x08192b2b19190819,
    0x082b080808080808, 0x082b08080808082b, 0x082b080808082b2b,
    0x082b080819081908, 0x082b0808192b0819, 0x082b08082b080808,
    0x082b08082b08082b, 0x082b0819082b2b19, 0x082b081919082b08,
    0x082b082b08080808, 0x082b082b0808082b, 0x082b190808080819,
    0x082b190808081908, 0x082b190808190808, 0x082b190819080808,
    0x082b19081919192b, 0x082b191908080808, 0x082b191919080819,
    0x082b1919192b1908, 0x082b192b2b190808, 0x082b2b0808082b08,
    0x082b2b08082b0808, 0x082b2b082b191908, 0x082b2b2b19081908,
    0x1908080808080819, 0x1908080808081908, 0x1908080808190808,
    0x1908080808192b08, 0x19080808082b0819, 0x19080808082b1908,
    0x1908080819080808, 0x1908080819082b08, 0x190808081919192b,
    0x19080808192b0808, 0x190808082b080819, 0x190808082b081908,
    0x190808082b190808, 0x1908081908080808, 0x19080819082b0808,
    0x19080819192b0819, 0x190808192b080808, 0x190808192b081919,
    0x1908082b08080819, 0x1908082b08190808, 0x1908082b19082b08,
    0x1908082b1919192b, 0x1908082b192b2b08, 0x1908190808080808,
    0x1908190808082b08, 0x19081908082b0808, 0x190819082b080808,
    0x190819082b192b19, 0x190819190819082b, 0x19081919082b1908,
    0x1908192b08080808, 0x19082b0808080819, 0x19082b0808081908,
    0x19082b0808190808, 0x19082b0819080808, 0x19082b0819081919,
    0x19082b1908080808, 0x19082b1919192b08, 0x19082b19192b0819,
    0x19082b192b08082b, 0x19082b2b19081919, 0x19082b2b2b190808,
    0x1919080808080808, 0x1919080808082b08, 0x1919080808190819,
    0x1919080808192b19, 0x19190808082b0808, 0x191908082b080808,
    0x191908082b082b08, 0x1919081908081908, 0x191908191908082b,
    0x191908192b2b1908, 0x1919082b2b190819, 0x191919082b190808,
    0x191919082b19082b, 0x1919191908082b2b, 0x1919192b08080819,
    0x1919192b19191908, 0x19192b0808080808, 0x19192b0808190819,
    0x19192b0808192b19, 0x19192b08192b1908, 0x19192b1919080808,
    0x19192b2b08082b08, 0x192b080808081908, 0x192b080808190808,
    0x192b080819080808, 0x192b0808192b2b08, 0x192b081908080808,
    0x192b081919191919, 0x192b082b08192b08, 0x192b082b192b0808,
    0x192b190808080808, 0x192b190808081919, 0x192b191908190808,
    0x192b19190819082b, 0x192b19192b081908, 0x192b2b081908082b,
    0x2b08080808080808, 0x2b0808080808082b, 0x2b08080808082b2b,
    0x2b08080819080819, 0x2b0808082b08082b, 0x2b08081908081908,
    0x2b08081908192b08, 0x2b08081919080808, 0x2b08082b08190819,
    0x2b08190808080819, 0x2b08190808081908, 0x2b08190808190808,
    0x2b08190808191919, 0x2b08190819080808, 0x2b081908192b0808,
    0x2b08191908080808, 0x2b0819191908192b, 0x2b0819192b191908,
    0x2b08192b08082b19, 0x2b08192b19080808, 0x2b08192b192b0808,
    0x2b082b080808082b, 0x2b082b1908081908, 0x2b082b2b08190819,
    0x2b19080808081908, 0x2b19080808190808, 0x2b190808082b1908,
    0x2b19080819080808, 0x2b1908082b2b0819, 0x2b1908190819192b,
    0x2b1908192b080808, 0x2b19082b19081919, 0x2b19190808080808,
    0x2b191908082b082b, 0x2b19190819081908, 0x2b19191919190819,
    0x2b192b082b080819, 0x2b192b19082b0808, 0x2b2b08080808082b,
    0x2b2b080819190808, 0x2b2b08082b081919, 0x2b2b081908082b19,
    0x2b2b082b08080808, 0x2b2b190808192b08, 0x2b2b2b0819190808,
    0x2b2b2b1908081908};
constexpr std::uint64_t kQ4_0TypeSizeBytes = 18;
constexpr std::uint64_t kQ4_0QuantOffset = 2;
constexpr std::uint64_t kQ4_1TypeSizeBytes = 20;
constexpr std::uint64_t kQ4_1QuantOffset = 4;
constexpr std::uint64_t kLegacyQ5ValueCount = 32;
constexpr std::uint64_t kQ5_0TypeSizeBytes = 22;
constexpr std::uint64_t kQ5_0HighBitsOffset = 2;
constexpr std::uint64_t kQ5_0QuantOffset = 6;
constexpr std::uint64_t kQ5_1TypeSizeBytes = 24;
constexpr std::uint64_t kQ5_1HighBitsOffset = 4;
constexpr std::uint64_t kQ5_1QuantOffset = 8;
constexpr std::uint64_t kIQ4NLTypeSizeBytes = 18;
constexpr std::uint64_t kIQ4NLValueCount = 32;
constexpr std::uint64_t kIQ4NLQuantOffset = 2;
constexpr std::uint64_t kMXFP4TypeSizeBytes = 17;
constexpr std::uint64_t kMXFP4ValueCount = 32;
constexpr std::uint64_t kMXFP4QuantOffset = 1;
constexpr std::uint64_t kNVFP4TypeSizeBytes = 36;
constexpr std::uint64_t kNVFP4ValueCount = 64;
constexpr std::uint64_t kNVFP4SubBlocks = 4;
constexpr std::uint64_t kNVFP4ValuesPerSubBlock = 16;
constexpr std::uint64_t kNVFP4ScaleOffset = 0;
constexpr std::uint64_t kNVFP4QuantOffset = 4;
constexpr std::uint64_t kIQ4XSTypeSizeBytes = 136;
constexpr std::uint64_t kIQ4XSValueCount = 256;
constexpr std::uint64_t kIQ4XSSubBlocks = 8;
constexpr std::uint64_t kIQ4XSValuesPerSubBlock = 32;
constexpr std::uint64_t kIQ4XSScalesHOffset = 2;
constexpr std::uint64_t kIQ4XSScalesLOffset = 4;
constexpr std::uint64_t kIQ4XSQuantOffset = 8;
constexpr std::uint64_t kTQ1_0TypeSizeBytes = 56;
constexpr std::uint64_t kTQ1_0ValueCount = 256;
constexpr std::uint64_t kTQ1_0QSPackedBytes = 48;
constexpr std::uint64_t kTQ1_0QHPackedBytes = 4;
constexpr std::uint64_t kTQ1_0QuantOffset = 0;
constexpr std::uint64_t kTQ1_0HighQuantOffset = 48;
constexpr std::uint64_t kTQ1_0DOffset = 52;
constexpr std::uint64_t kTQ2_0TypeSizeBytes = 66;
constexpr std::uint64_t kTQ2_0ValueCount = 256;
constexpr std::uint64_t kTQ2_0QuantOffset = 0;
constexpr std::uint64_t kTQ2_0DOffset = 64;
constexpr std::uint64_t kQ2KTypeSizeBytes = 84;
constexpr std::uint64_t kQ2KScaleOffset = 0;
constexpr std::uint64_t kQ2KQuantOffset = 16;
constexpr std::uint64_t kQ2KDOffset = 80;
constexpr std::uint64_t kQ2KDMinOffset = 82;
constexpr std::uint64_t kQ3KTypeSizeBytes = 110;
constexpr std::uint64_t kQ3KHighMaskOffset = 0;
constexpr std::uint64_t kQ3KQuantOffset = 32;
constexpr std::uint64_t kQ3KScalesOffset = 96;
constexpr std::uint64_t kQ3KDOffset = 108;
constexpr std::uint64_t kQ4KTypeSizeBytes = 144;
constexpr std::uint64_t kQ4KScaleOffset = 4;
constexpr std::uint64_t kQ4KQuantOffset = 16;
constexpr std::uint64_t kQ5KTypeSizeBytes = 176;
constexpr std::uint64_t kQ5KHighBitsOffset = 16;
constexpr std::uint64_t kQ5KQuantOffset = 48;
constexpr std::uint64_t kQ6KTypeSizeBytes = 210;
constexpr std::uint64_t kQ6KLowBitsOffset = 0;
constexpr std::uint64_t kQ6KHighBitsOffset = 128;
constexpr std::uint64_t kQ6KScalesOffset = 192;
constexpr std::uint64_t kQ6KDOffset = 208;
constexpr std::uint64_t kQ8_0TypeSizeBytes = 34;
constexpr std::uint64_t kQ8_0ValueCount = 32;
constexpr std::uint64_t kQ8_0QuantOffset = 2;
constexpr std::uint64_t kQ8_1TypeSizeBytes = 40;
constexpr std::uint64_t kQ8_1ValueCount = 32;
constexpr std::uint64_t kQ8_1QuantOffset = 8;
constexpr std::uint64_t kQ8KTypeSizeBytes = 292;
constexpr std::uint64_t kQ8KValueCount = 256;
constexpr std::uint64_t kQ8KQuantOffset = 4;
constexpr std::uint64_t kQ8KBlockSumOffset = 260;
constexpr std::uint64_t kQ8KBlockSumCount = 16;
constexpr std::uint64_t kQ2KGroups = 16;
constexpr std::uint64_t kQ2KValuesPerGroup = 16;
constexpr std::uint64_t kQ3KGroups = 16;
constexpr std::uint64_t kQ3KValuesPerGroup = 16;
constexpr std::uint64_t kQ4KSubBlocks = 8;
constexpr std::uint64_t kQ4KValuesPerSubBlock = 32;
constexpr std::uint64_t kQ6KGroups = 16;
constexpr std::uint64_t kQ6KValuesPerGroup = 16;
constexpr std::uint64_t kMaxGGUFMetadataKVCount = 1000000ULL;
constexpr std::uint64_t kMaxGGUFTensorCount = 1000000ULL;
constexpr std::uint64_t kMaxGGUFMetadataArrayLength = 1000000ULL;
constexpr std::uint64_t kMaxGGUFMetadataValueCount = 1000000ULL;
constexpr std::size_t kMaxGGUFMetadataNestingDepth = 32;
constexpr std::uint64_t kMaxGGUFStringBytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaxGGUFAlignment = 1024U * 1024U;
constexpr std::uint64_t kMinMetadataKVEncodedBytes = 12ULL;
constexpr std::uint64_t kMinTensorInfoEncodedBytes = 32ULL;
constexpr std::int8_t kIQ4NLCodebook[16] = {
    -127, -104, -83, -65, -49, -35, -22, -10,
    1,    13,   25,  38,  53,  69,  89,  113};
constexpr std::int8_t kMXFP4Codebook[16] = {
    0, 1, 2, 3, 4, 6, 8, 12, 0, -1, -2, -3, -4, -6, -8, -12};

std::uint64_t checked_add(std::uint64_t lhs, std::uint64_t rhs,
                          const std::string& context);

std::uint64_t align_offset(const std::uint64_t offset,
                           const std::uint32_t alignment) {
  if (alignment == 0) {
    throw std::runtime_error("GGUF alignment must be non-zero");
  }
  const auto remainder = offset % alignment;
  return remainder == 0 ? offset
                        : checked_add(offset, alignment - remainder,
                                      "aligned GGUF offset");
}

std::uint64_t checked_multiply(const std::uint64_t lhs,
                               const std::uint64_t rhs,
                               const std::string& context) {
  try {
    return internal::checked_product_u64(lhs, rhs, context.c_str());
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error(error.what());
  }
}

std::uint64_t checked_add(const std::uint64_t lhs, const std::uint64_t rhs,
                          const std::string& context) {
  try {
    return internal::checked_add_u64(lhs, rhs, context.c_str());
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error(error.what());
  }
}

std::int64_t checked_add_i64(const std::int64_t lhs,
                             const std::int64_t rhs,
                             const std::string& context) {
  try {
    return internal::checked_add_i64(lhs, rhs, context.c_str());
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error(error.what());
  }
}

template <typename Signed, typename Unsigned>
Signed signed_from_twos_complement(const Unsigned value) {
  static_assert(std::is_signed<Signed>::value,
                "destination must be signed");
  static_assert(std::is_unsigned<Unsigned>::value,
                "source must be unsigned");
  static_assert(sizeof(Signed) == sizeof(Unsigned),
                "signed and unsigned widths must match");
  const auto signed_max =
      static_cast<Unsigned>(std::numeric_limits<Signed>::max());
  if (value <= signed_max) return static_cast<Signed>(value);
  const Unsigned magnitude = static_cast<Unsigned>(Unsigned{0} - value);
  const Unsigned minimum_magnitude =
      static_cast<Unsigned>(signed_max + Unsigned{1});
  if (magnitude == minimum_magnitude) {
    return std::numeric_limits<Signed>::min();
  }
  return static_cast<Signed>(-static_cast<Signed>(magnitude));
}

float require_finite_float(const float value, const std::string& context) {
  if (!std::isfinite(value)) {
    throw std::runtime_error(context + " must be finite");
  }
  return value;
}

double require_finite_double(const double value, const std::string& context) {
  if (!std::isfinite(value)) {
    throw std::runtime_error(context + " must be finite");
  }
  return value;
}

void validate_alignment(const std::uint32_t alignment) {
  if (alignment == 0) {
    throw std::runtime_error("GGUF alignment must be non-zero");
  }
  if ((alignment & (alignment - 1U)) != 0U) {
    throw std::runtime_error("GGUF alignment must be a power of two");
  }
  if (alignment > kMaxGGUFAlignment) {
    throw std::runtime_error("GGUF alignment exceeds InferLab safety cap");
  }
}

void require_byte_range(const std::vector<std::uint8_t>& bytes,
                        const std::uint64_t offset,
                        const std::uint64_t count,
                        const std::string& context) {
  const auto size = static_cast<std::uint64_t>(bytes.size());
  if (count > size || offset > size - count) {
    throw std::runtime_error(context + " byte range outside buffer");
  }
}

std::size_t checked_size_count(const std::uint64_t count,
                               const std::uint64_t max_count,
                               const std::string& context) {
  if (count > max_count) {
    throw std::runtime_error(context + " exceeds InferLab safety cap");
  }
  if (count > static_cast<std::uint64_t>(
                  std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error(context + " exceeds host size_t capacity");
  }
  return static_cast<std::size_t>(count);
}

void validate_declared_count(const std::uint64_t count,
                             const std::uint64_t max_count,
                             const std::uint64_t min_encoded_bytes,
                             const std::uint64_t remaining_bytes,
                             const std::string& context) {
  checked_size_count(count, max_count, context);
  if (min_encoded_bytes != 0 &&
      count > remaining_bytes / min_encoded_bytes) {
    throw std::runtime_error(context + " exceeds remaining GGUF input");
  }
}

std::uint64_t metadata_value_min_encoded_bytes(const GGUFMetadataType type) {
  switch (type) {
    case GGUFMetadataType::kUInt8:
    case GGUFMetadataType::kInt8:
    case GGUFMetadataType::kBool:
      return 1;
    case GGUFMetadataType::kUInt16:
    case GGUFMetadataType::kInt16:
      return 2;
    case GGUFMetadataType::kUInt32:
    case GGUFMetadataType::kInt32:
    case GGUFMetadataType::kFloat32:
      return 4;
    case GGUFMetadataType::kUInt64:
    case GGUFMetadataType::kInt64:
    case GGUFMetadataType::kFloat64:
    case GGUFMetadataType::kString:
      return 8;
    case GGUFMetadataType::kArray:
      return 12;
  }
  return 1;
}

std::uint64_t file_size_or_throw(const std::string& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    throw std::runtime_error(
        "failed to read GGUF file size: " +
        inferlab_plan::redacted_path_label(path) + ": " +
        inferlab_plan::redact_error_message(error.message(), {path}));
  }
  if (size > static_cast<std::uintmax_t>(
                 std::numeric_limits<std::uint64_t>::max())) {
    throw std::runtime_error("GGUF file size exceeds uint64_t capacity: " +
                             inferlab_plan::redacted_path_label(path));
  }
  return static_cast<std::uint64_t>(size);
}

std::vector<std::uint8_t> read_file_bytes_with_cap(
    const std::string& path,
    const std::uint64_t max_file_bytes) {
  const auto file_size = file_size_or_throw(path);
  if (file_size > max_file_bytes) {
    throw std::runtime_error(
        "GGUF file exceeds in-memory parser safety cap: " +
        inferlab_plan::redacted_path_label(path));
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open GGUF file: " +
                             inferlab_plan::redacted_path_label(path));
  }
  const auto byte_count = checked_size_count(
      file_size, max_file_bytes, "GGUF file byte count");
  if (file_size > static_cast<std::uint64_t>(
                      std::numeric_limits<std::streamsize>::max())) {
    throw std::runtime_error("GGUF file exceeds stream read capacity: " +
                             inferlab_plan::redacted_path_label(path));
  }
  std::vector<std::uint8_t> bytes(byte_count);
  if (!bytes.empty()) {
    file.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (file.gcount() != static_cast<std::streamsize>(bytes.size())) {
      throw std::runtime_error("GGUF file size changed while reading: " +
                               inferlab_plan::redacted_path_label(path));
    }
  }
  if (file.bad()) {
    throw std::runtime_error("failed while reading GGUF file: " +
                             inferlab_plan::redacted_path_label(path));
  }
  char extra = 0;
  if (file.get(extra)) {
    throw std::runtime_error("GGUF file grew while reading: " +
                             inferlab_plan::redacted_path_label(path));
  }
  if (file.bad()) {
    throw std::runtime_error("failed while checking GGUF file end: " +
                             inferlab_plan::redacted_path_label(path));
  }
  return bytes;
}

std::uint8_t byte_at(const std::vector<std::uint8_t>& bytes,
                     const std::uint64_t offset,
                     const std::string& context) {
  require_byte_range(bytes, offset, 1, context);
  return bytes[static_cast<std::size_t>(offset)];
}

std::uint16_t read_u16_le_at(const std::vector<std::uint8_t>& bytes,
                             const std::uint64_t offset,
                             const std::string& context) {
  require_byte_range(bytes, offset, 2, context + " u16");
  return static_cast<std::uint16_t>(bytes[static_cast<std::size_t>(offset)]) |
         (static_cast<std::uint16_t>(bytes[static_cast<std::size_t>(offset + 1)])
          << 8U);
}

std::int16_t read_i16_le_at(const std::vector<std::uint8_t>& bytes,
                            const std::uint64_t offset,
                            const std::string& context) {
  return signed_from_twos_complement<std::int16_t>(
      read_u16_le_at(bytes, offset, context));
}

std::uint32_t read_u32_le_at(const std::vector<std::uint8_t>& bytes,
                             const std::uint64_t offset,
                             const std::string& context) {
  require_byte_range(bytes, offset, 4, context + " u32");
  return static_cast<std::uint32_t>(bytes[static_cast<std::size_t>(offset)]) |
         (static_cast<std::uint32_t>(
              bytes[static_cast<std::size_t>(offset + 1)])
          << 8U) |
         (static_cast<std::uint32_t>(
              bytes[static_cast<std::size_t>(offset + 2)])
          << 16U) |
         (static_cast<std::uint32_t>(
              bytes[static_cast<std::size_t>(offset + 3)])
          << 24U);
}

std::uint64_t read_u64_le_at(const std::vector<std::uint8_t>& bytes,
                             const std::uint64_t offset,
                             const std::string& context) {
  require_byte_range(bytes, offset, 8, context + " u64");
  std::uint64_t value = 0;
  for (std::uint64_t i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(
                 bytes[static_cast<std::size_t>(offset + i)])
             << (8U * i);
  }
  return value;
}

float finite_float32_from_bits(const std::uint32_t raw,
                               const std::string& context) {
  float value = 0.0F;
  std::memcpy(&value, &raw, sizeof(value));
  return require_finite_float(value, context);
}

double finite_float64_from_bits(const std::uint64_t raw,
                                const std::string& context) {
  double value = 0.0;
  std::memcpy(&value, &raw, sizeof(value));
  return require_finite_double(value, context);
}

float read_f32_le_at(const std::vector<std::uint8_t>& bytes,
                     const std::uint64_t offset,
                     const std::string& context) {
  return finite_float32_from_bits(read_u32_le_at(bytes, offset, context),
                                  context);
}

float e8m0_to_float32_half(const std::uint8_t exponent) {
  const auto bits = exponent < 2
                        ? static_cast<std::uint32_t>(0x00200000U << exponent)
                        : static_cast<std::uint32_t>(exponent - 1U) << 23U;
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

float ue4m3_to_float32_half(const std::uint8_t encoded) {
  if (encoded == 0 || encoded == 0x7F) {
    return 0.0F;
  }
  const auto exponent = static_cast<int>((encoded >> 3U) & 0x0FU);
  const auto mantissa = static_cast<int>(encoded & 0x07U);
  const auto raw = exponent == 0
                       ? std::ldexp(static_cast<float>(mantissa), -9)
                       : std::ldexp(1.0F + static_cast<float>(mantissa) / 8.0F,
                                    exponent - 7);
  return raw * 0.5F;
}

class Reader {
 public:
  explicit Reader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

  std::uint64_t offset() const { return offset_; }
  std::uint64_t remaining() const {
    const auto size = static_cast<std::uint64_t>(bytes_.size());
    return offset_ <= size ? size - offset_ : 0;
  }

  void require_available(const std::uint64_t count) const {
    const auto size = static_cast<std::uint64_t>(bytes_.size());
    if (count > size || offset_ > size - count) {
      throw std::runtime_error("truncated GGUF input");
    }
  }

  std::uint8_t read_u8() {
    require_available(1);
    return bytes_[static_cast<std::size_t>(offset_++)];
  }

  std::uint16_t read_u16() {
    const auto value = read_little_endian<std::uint16_t>();
    return value;
  }

  std::uint32_t read_u32() {
    const auto value = read_little_endian<std::uint32_t>();
    return value;
  }

  std::uint64_t read_u64() {
    const auto value = read_little_endian<std::uint64_t>();
    return value;
  }

  std::int8_t read_i8() {
    return signed_from_twos_complement<std::int8_t>(read_u8());
  }

  std::int16_t read_i16() {
    return signed_from_twos_complement<std::int16_t>(read_u16());
  }

  std::int32_t read_i32() {
    return signed_from_twos_complement<std::int32_t>(read_u32());
  }

  std::int64_t read_i64() {
    return signed_from_twos_complement<std::int64_t>(read_u64());
  }

  float read_f32() {
    return finite_float32_from_bits(read_u32(),
                                    "GGUF metadata float32 value");
  }

  double read_f64() {
    return finite_float64_from_bits(read_u64(),
                                    "GGUF metadata float64 value");
  }

  std::string read_string() {
    const auto length = read_u64();
    const auto size = checked_size_count(length, kMaxGGUFStringBytes,
                                         "GGUF string length");
    require_available(length);
    std::string value;
    value.resize(size);
    const auto begin = checked_size_count(
        offset_, static_cast<std::uint64_t>(bytes_.size()),
        "GGUF string offset");
    if (size != 0U) {
      std::memcpy(value.data(), bytes_.data() + begin, size);
    }
    offset_ += length;
    return value;
  }

 private:
  template <typename T>
  T read_little_endian() {
    require_available(sizeof(T));
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      value |= static_cast<T>(bytes_[static_cast<std::size_t>(offset_ + i)])
               << (8U * i);
    }
    offset_ += sizeof(T);
    return value;
  }

  const std::vector<std::uint8_t>& bytes_;
  std::uint64_t offset_ = 0;
};

GGUFMetadataType checked_type(const std::uint32_t raw) {
  if (raw > static_cast<std::uint32_t>(GGUFMetadataType::kFloat64)) {
    throw std::runtime_error("unknown GGUF metadata value type");
  }
  return static_cast<GGUFMetadataType>(raw);
}

struct MetadataReadBudget {
  std::uint64_t remaining_values = kMaxGGUFMetadataValueCount;

  void consume() {
    if (remaining_values == 0) {
      throw std::runtime_error(
          "GGUF metadata values exceed InferLab safety cap");
    }
    --remaining_values;
  }
};

GGUFMetadataValue read_metadata_value(Reader& reader, GGUFMetadataType type,
                                      MetadataReadBudget& budget,
                                      std::size_t depth);

GGUFMetadataValue read_array_value(Reader& reader,
                                   MetadataReadBudget& budget,
                                   const std::size_t depth) {
  GGUFMetadataValue value;
  value.type = GGUFMetadataType::kArray;
  value.array_type = checked_type(reader.read_u32());
  const auto length = reader.read_u64();
  if (length != 0U && depth >= kMaxGGUFMetadataNestingDepth) {
    throw std::runtime_error(
        "GGUF metadata nesting exceeds InferLab safety cap");
  }
  validate_declared_count(length, kMaxGGUFMetadataArrayLength,
                          metadata_value_min_encoded_bytes(value.array_type),
                          reader.remaining(), "GGUF metadata array length");
  if (length > budget.remaining_values) {
    throw std::runtime_error(
        "GGUF metadata array exceeds remaining value budget");
  }
  value.array_values.reserve(checked_size_count(
      length, kMaxGGUFMetadataArrayLength, "GGUF metadata array length"));
  for (std::uint64_t i = 0; i < length; ++i) {
    value.array_values.push_back(
        read_metadata_value(reader, value.array_type, budget, depth + 1U));
  }
  return value;
}

GGUFMetadataValue read_metadata_value(Reader& reader,
                                      const GGUFMetadataType type,
                                      MetadataReadBudget& budget,
                                      const std::size_t depth) {
  if (depth > kMaxGGUFMetadataNestingDepth) {
    throw std::runtime_error(
        "GGUF metadata nesting exceeds InferLab safety cap");
  }
  budget.consume();
  GGUFMetadataValue value;
  value.type = type;
  switch (type) {
    case GGUFMetadataType::kUInt8:
      value.uint_value = reader.read_u8();
      break;
    case GGUFMetadataType::kInt8:
      value.int_value = reader.read_i8();
      break;
    case GGUFMetadataType::kUInt16:
      value.uint_value = reader.read_u16();
      break;
    case GGUFMetadataType::kInt16:
      value.int_value = reader.read_i16();
      break;
    case GGUFMetadataType::kUInt32:
      value.uint_value = reader.read_u32();
      break;
    case GGUFMetadataType::kInt32:
      value.int_value = reader.read_i32();
      break;
    case GGUFMetadataType::kFloat32:
      value.float_value = reader.read_f32();
      break;
    case GGUFMetadataType::kBool: {
      const auto raw = reader.read_u8();
      if (raw > 1) {
        throw std::runtime_error("invalid GGUF boolean value");
      }
      value.bool_value = raw == 1;
      break;
    }
    case GGUFMetadataType::kString:
      value.string_value = reader.read_string();
      break;
    case GGUFMetadataType::kArray:
      return read_array_value(reader, budget, depth);
    case GGUFMetadataType::kUInt64:
      value.uint_value = reader.read_u64();
      break;
    case GGUFMetadataType::kInt64:
      value.int_value = reader.read_i64();
      break;
    case GGUFMetadataType::kFloat64:
      value.float_value = reader.read_f64();
      break;
  }
  return value;
}

std::uint32_t metadata_uint32(const std::vector<GGUFMetadataKV>& metadata,
                              const std::string& key,
                              const std::uint32_t fallback) {
  for (const auto& item : metadata) {
    if (item.key != key) continue;
    if (item.value.type != GGUFMetadataType::kUInt32) {
      throw std::runtime_error("GGUF metadata key has unexpected type: " +
                               key);
    }
    if (item.value.uint_value >
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      throw std::runtime_error("GGUF uint32 metadata value exceeds range: " +
                               key);
    }
    return static_cast<std::uint32_t>(item.value.uint_value);
  }
  return fallback;
}

std::uint64_t element_count(const std::vector<std::uint64_t>& dimensions) {
  std::uint64_t count = 1;
  for (const auto dimension : dimensions) {
    count = checked_multiply(count, dimension, "tensor element count");
  }
  return count;
}

std::uint64_t row_count(const std::vector<std::uint64_t>& dimensions) {
  std::uint64_t count = 1;
  for (std::size_t i = 1; i < dimensions.size(); ++i) {
    count = checked_multiply(count, dimensions[i], "tensor row count");
  }
  return count;
}

void annotate_tensor_layout(GGUFTensorInfo& tensor,
                            const std::uint64_t file_size) {
  tensor.type_info = gguf_tensor_type_info(tensor.type);
  tensor.element_count = element_count(tensor.dimensions);
  tensor.data_end_absolute_offset = tensor.absolute_offset;

  if (!tensor.type_info.known || tensor.type_info.deprecated ||
      tensor.type_info.block_size == 0 || tensor.type_info.type_size_bytes == 0) {
    return;
  }

  if (tensor.dimensions.empty()) {
    throw std::runtime_error("GGUF tensor has no dimensions");
  }
  const auto leading_dimension = tensor.dimensions.front();
  if (leading_dimension % tensor.type_info.block_size != 0) {
    throw std::runtime_error(
        "GGUF tensor leading dimension is not divisible by its block size");
  }

  tensor.byte_size_known = true;
  tensor.row_size_bytes =
      checked_multiply(leading_dimension / tensor.type_info.block_size,
                       tensor.type_info.type_size_bytes, "tensor row bytes");
  tensor.data_size_bytes =
      checked_multiply(tensor.row_size_bytes, row_count(tensor.dimensions),
                       "tensor data bytes");
  tensor.data_end_absolute_offset =
      checked_add(tensor.absolute_offset, tensor.data_size_bytes,
                  "tensor data end offset");
  if (tensor.data_end_absolute_offset > file_size) {
    throw std::runtime_error("GGUF tensor data span points beyond file end");
  }
}

GGUFKQuantScaleMin decode_k_quant_scale_min(
    const std::vector<std::uint8_t>& block_bytes,
    const int sub_block,
    const float d,
    const float dmin) {
  if (sub_block < 0 ||
      sub_block >= static_cast<int>(kQ4KSubBlocks)) {
    throw std::runtime_error("Q4_K sub-block index outside supported range");
  }
  std::uint8_t scale = 0;
  std::uint8_t min = 0;
  if (sub_block < 4) {
    scale = static_cast<std::uint8_t>(
        byte_at(block_bytes, kQ4KScaleOffset + sub_block, "Q4_K scale") & 63U);
    min = static_cast<std::uint8_t>(
        byte_at(block_bytes, kQ4KScaleOffset + sub_block + 4, "Q4_K min") &
        63U);
  } else {
    scale = static_cast<std::uint8_t>(
        (byte_at(block_bytes, kQ4KScaleOffset + sub_block + 4, "Q4_K scale") &
         0x0FU) |
        ((byte_at(block_bytes, kQ4KScaleOffset + sub_block - 4, "Q4_K scale") >>
          6U)
         << 4U));
    min = static_cast<std::uint8_t>(
        (byte_at(block_bytes, kQ4KScaleOffset + sub_block + 4, "Q4_K min") >>
         4U) |
        ((byte_at(block_bytes, kQ4KScaleOffset + sub_block, "Q4_K min") >> 6U)
         << 4U));
  }
  return {scale, min, d * static_cast<float>(scale),
          dmin * static_cast<float>(min)};
}

GGUFQ2KScaleMin decode_q2_k_scale_min(
    const std::vector<std::uint8_t>& block_bytes,
    const int group,
    const float d,
    const float dmin) {
  if (group < 0 ||
      group >= static_cast<int>(kQ2KGroups)) {
    throw std::runtime_error("Q2_K group index outside supported range");
  }
  const auto packed = byte_at(block_bytes,
                              kQ2KScaleOffset + static_cast<std::uint64_t>(group),
                              "Q2_K scale/min");
  const auto scale = static_cast<std::uint8_t>(packed & 0x0FU);
  const auto min = static_cast<std::uint8_t>((packed >> 4U) & 0x0FU);
  return {scale, min, d * static_cast<float>(scale),
          dmin * static_cast<float>(min)};
}

std::vector<GGUFQ3KScale> decode_q3_k_scales(
    const std::vector<std::uint8_t>& block_bytes,
    const float d) {
  std::vector<GGUFQ3KScale> scales;
  scales.reserve(static_cast<std::size_t>(kQ3KGroups));
  for (std::uint64_t lane = 0; lane < 4; ++lane) {
    const auto lower_a = byte_at(block_bytes, kQ3KScalesOffset + lane,
                                 "Q3_K scale");
    const auto lower_b = byte_at(block_bytes, kQ3KScalesOffset + 4 + lane,
                                 "Q3_K scale");
    const auto high = byte_at(block_bytes, kQ3KScalesOffset + 8 + lane,
                              "Q3_K scale high bits");

    const int decoded[4] = {
        static_cast<int>((lower_a & 0x0FU) | ((high & 0x03U) << 4U)),
        static_cast<int>((lower_b & 0x0FU) | (((high >> 2U) & 0x03U) << 4U)),
        static_cast<int>(((lower_a >> 4U) & 0x0FU) |
                         (((high >> 4U) & 0x03U) << 4U)),
        static_cast<int>(((lower_b >> 4U) & 0x0FU) |
                         (((high >> 6U) & 0x03U) << 4U)),
    };
    const int order[4] = {0, 4, 8, 12};
    for (std::size_t item = 0; item < 4; ++item) {
      const auto signed_scale =
          static_cast<std::int8_t>(decoded[item] - 32);
      const auto index = static_cast<std::size_t>(order[item] + lane);
      if (scales.size() <= index) {
        scales.resize(index + 1);
      }
      scales[index] = {signed_scale, d * static_cast<float>(signed_scale)};
    }
  }
  if (scales.size() != kQ3KGroups) {
    throw std::runtime_error("Q3_K scale decode produced unexpected group count");
  }
  return scales;
}

float checked_float_output(const double value, const std::string& context) {
  try {
    return internal::checked_float_from_double(value, context.c_str());
  } catch (const std::invalid_argument& error) {
    throw std::runtime_error(error.what());
  }
}

void add_checked_float(float& total, const float value,
                       const std::string& context) {
  total = checked_float_output(static_cast<double>(total) +
                                   static_cast<double>(value),
                               context);
}

void finalize_float_stats(const std::vector<float>& values,
                          float& checksum,
                          float& min_value,
                          float& max_value,
                          float& mean,
                          const std::string& context) {
  if (values.empty()) {
    throw std::runtime_error(context + " dequant produced no values");
  }
  min_value = values.front();
  max_value = values.front();
  double checksum_accumulator = 0.0;
  for (const auto value : values) {
    require_finite_float(value, context + " value");
    checksum_accumulator += static_cast<double>(value);
    require_finite_double(checksum_accumulator, context + " checksum");
    min_value = std::min(min_value, value);
    max_value = std::max(max_value, value);
  }
  checksum = checked_float_output(checksum_accumulator, context + " checksum");
  mean = checked_float_output(
      checksum_accumulator / static_cast<double>(values.size()),
      context + " mean");
}

void finalize_k_quant_stats(std::vector<float>& values,
                            std::vector<float>& subblock_sums,
                            float& checksum,
                            float& min_value,
                            float& max_value,
                            float& mean,
                            const std::uint64_t expected_group_count,
                            const std::string& context) {
  finalize_float_stats(values, checksum, min_value, max_value, mean, context);
  if (subblock_sums.size() != expected_group_count) {
    throw std::runtime_error(context +
                             " dequant produced unexpected group count");
  }
  for (const auto sum : subblock_sums) {
    require_finite_float(sum, context + " group sum");
  }
}

void finalize_double_stats(const std::vector<double>& values,
                           double& checksum,
                           double& min_value,
                           double& max_value,
                           double& mean,
                           const std::string& context) {
  if (values.empty()) {
    throw std::runtime_error(context + " dequant produced no values");
  }
  min_value = values.front();
  max_value = values.front();
  for (const auto value : values) {
    require_finite_double(value, context + " value");
    checksum += value;
    require_finite_double(checksum, context + " checksum");
    min_value = std::min(min_value, value);
    max_value = std::max(max_value, value);
  }
  mean = checksum / static_cast<double>(values.size());
  require_finite_double(mean, context + " mean");
}

std::size_t require_preview_value_count(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count,
    const std::uint64_t value_width,
    const std::string& context) {
  const auto required_bytes =
      checked_multiply(value_count, value_width, context + " byte size");
  if (static_cast<std::uint64_t>(tensor_bytes.size()) < required_bytes) {
    throw std::runtime_error(context + " preview requires at least " +
                             std::to_string(required_bytes) + " bytes");
  }
  return checked_size_count(
      value_count, static_cast<std::uint64_t>(tensor_bytes.size()),
      context + " value count");
}

template <typename Result, typename Raw, typename Signed, typename ReaderFn>
Result decode_signed_integer_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count,
    const std::uint64_t value_width,
    const std::string& context,
    ReaderFn read_raw) {
  const auto reserve_count = require_preview_value_count(
      tensor_bytes, value_count, value_width, context);

  Result result;
  result.raw_values.reserve(reserve_count);
  result.values.reserve(reserve_count);
  for (std::uint64_t i = 0; i < value_count; ++i) {
    const auto offset =
        checked_multiply(i, value_width, context + " value offset");
    const Raw raw = read_raw(tensor_bytes, offset, context + " value");
    const Signed value = signed_from_twos_complement<Signed>(raw);
    result.raw_values.push_back(raw);
    result.values.push_back(value);
    result.checksum = checked_add_i64(
        result.checksum, static_cast<std::int64_t>(value),
        context + " checksum");
    if (i == 0) {
      result.min = value;
      result.max = value;
    } else {
      result.min = std::min(result.min, value);
      result.max = std::max(result.max, value);
    }
  }
  if (!result.values.empty()) {
    result.mean = static_cast<double>(result.checksum) /
                  static_cast<double>(result.values.size());
  }
  return result;
}

}  // namespace

GGUFFileInfo parse_gguf_bytes(const std::vector<std::uint8_t>& bytes) {
  Reader reader(bytes);
  for (const auto expected : kMagic) {
    if (reader.read_u8() != expected) {
      throw std::runtime_error("invalid GGUF magic");
    }
  }

  GGUFFileInfo info;
  info.file_size_bytes = bytes.size();
  info.version = reader.read_u32();
  if (info.version != kSupportedVersion) {
    throw std::runtime_error("unsupported GGUF version");
  }
  info.tensor_count = reader.read_u64();
  info.metadata_kv_count = reader.read_u64();
  info.metadata_start_offset = reader.offset();
  info.header_size_bytes = info.metadata_start_offset;

  validate_declared_count(info.metadata_kv_count, kMaxGGUFMetadataKVCount,
                          kMinMetadataKVEncodedBytes, reader.remaining(),
                          "GGUF metadata key-value count");
  info.metadata.reserve(checked_size_count(
      info.metadata_kv_count, kMaxGGUFMetadataKVCount,
      "GGUF metadata key-value count"));
  std::set<std::string> metadata_keys;
  MetadataReadBudget metadata_budget;
  for (std::uint64_t i = 0; i < info.metadata_kv_count; ++i) {
    GGUFMetadataKV kv;
    kv.key = reader.read_string();
    if (!metadata_keys.insert(kv.key).second) {
      throw std::runtime_error("duplicate GGUF metadata key");
    }
    kv.value = read_metadata_value(reader, checked_type(reader.read_u32()),
                                   metadata_budget, 0);
    info.metadata.push_back(std::move(kv));
  }

  info.alignment =
      metadata_uint32(info.metadata, "general.alignment", kDefaultAlignment);
  validate_alignment(info.alignment);
  info.tensor_info_start_offset = reader.offset();
  validate_declared_count(info.tensor_count, kMaxGGUFTensorCount,
                          kMinTensorInfoEncodedBytes, reader.remaining(),
                          "GGUF tensor count");
  info.tensors.reserve(checked_size_count(
      info.tensor_count, kMaxGGUFTensorCount, "GGUF tensor count"));
  std::set<std::string> tensor_names;
  for (std::uint64_t i = 0; i < info.tensor_count; ++i) {
    GGUFTensorInfo tensor;
    tensor.name = reader.read_string();
    if (!tensor_names.insert(tensor.name).second) {
      throw std::runtime_error("duplicate GGUF tensor name");
    }
    const auto dimensions = reader.read_u32();
    if (dimensions == 0 || dimensions > 4) {
      throw std::runtime_error("invalid GGUF tensor dimension count");
    }
    tensor.dimensions.reserve(dimensions);
    for (std::uint32_t dim = 0; dim < dimensions; ++dim) {
      const auto dimension = reader.read_u64();
      if (dimension == 0) {
        throw std::runtime_error("GGUF tensor dimensions must be positive");
      }
      tensor.dimensions.push_back(dimension);
    }
    tensor.type = reader.read_u32();
    tensor.relative_offset = reader.read_u64();
    if (align_offset(tensor.relative_offset, info.alignment) !=
        tensor.relative_offset) {
      throw std::runtime_error("unaligned GGUF tensor relative offset");
    }
    info.tensors.push_back(std::move(tensor));
  }

  info.tensor_info_end_offset = reader.offset();
  info.tensor_data_start_offset =
      align_offset(info.tensor_info_end_offset, info.alignment);
  info.tensor_data_padding_bytes =
      info.tensor_data_start_offset - info.tensor_info_end_offset;
  if (info.tensor_data_start_offset > bytes.size()) {
    throw std::runtime_error("GGUF tensor data offset beyond file end");
  }
  for (auto& tensor : info.tensors) {
    tensor.absolute_offset =
        checked_add(info.tensor_data_start_offset, tensor.relative_offset,
                    "GGUF tensor absolute offset");
    if (tensor.absolute_offset > bytes.size()) {
      throw std::runtime_error("GGUF tensor points beyond file end");
    }
    annotate_tensor_layout(tensor, bytes.size());
  }
  std::vector<std::pair<std::uint64_t, std::uint64_t>> known_tensor_spans;
  known_tensor_spans.reserve(info.tensors.size());
  for (const auto& tensor : info.tensors) {
    if (tensor.byte_size_known) {
      known_tensor_spans.emplace_back(tensor.absolute_offset,
                                      tensor.data_end_absolute_offset);
    }
  }
  std::sort(known_tensor_spans.begin(), known_tensor_spans.end());
  for (std::size_t i = 1; i < known_tensor_spans.size(); ++i) {
    if (known_tensor_spans[i].first < known_tensor_spans[i - 1].second) {
      throw std::runtime_error("GGUF tensor data spans overlap");
    }
  }
  return info;
}

GGUFFileInfo parse_gguf_file(const std::string& path,
                             const std::uint64_t max_file_bytes) {
  const auto bytes = read_file_bytes_with_cap(path, max_file_bytes);
  return parse_gguf_bytes(bytes);
}

std::string gguf_metadata_type_name(const GGUFMetadataType type) {
  switch (type) {
    case GGUFMetadataType::kUInt8:
      return "uint8";
    case GGUFMetadataType::kInt8:
      return "int8";
    case GGUFMetadataType::kUInt16:
      return "uint16";
    case GGUFMetadataType::kInt16:
      return "int16";
    case GGUFMetadataType::kUInt32:
      return "uint32";
    case GGUFMetadataType::kInt32:
      return "int32";
    case GGUFMetadataType::kFloat32:
      return "float32";
    case GGUFMetadataType::kBool:
      return "bool";
    case GGUFMetadataType::kString:
      return "string";
    case GGUFMetadataType::kArray:
      return "array";
    case GGUFMetadataType::kUInt64:
      return "uint64";
    case GGUFMetadataType::kInt64:
      return "int64";
    case GGUFMetadataType::kFloat64:
      return "float64";
  }
  return "unknown";
}

std::string gguf_metadata_value_to_string(const GGUFMetadataValue& value) {
  std::ostringstream out;
  switch (value.type) {
    case GGUFMetadataType::kUInt8:
    case GGUFMetadataType::kUInt16:
    case GGUFMetadataType::kUInt32:
    case GGUFMetadataType::kUInt64:
      out << value.uint_value;
      break;
    case GGUFMetadataType::kInt8:
    case GGUFMetadataType::kInt16:
    case GGUFMetadataType::kInt32:
    case GGUFMetadataType::kInt64:
      out << value.int_value;
      break;
    case GGUFMetadataType::kFloat32:
    case GGUFMetadataType::kFloat64:
      out << std::setprecision(8) << value.float_value;
      break;
    case GGUFMetadataType::kBool:
      out << (value.bool_value ? "true" : "false");
      break;
    case GGUFMetadataType::kString:
      out << value.string_value;
      break;
    case GGUFMetadataType::kArray:
      out << gguf_metadata_type_name(value.array_type) << "["
          << value.array_values.size() << "]";
      break;
    default:
      throw std::invalid_argument("unknown GGUF metadata value type");
  }
  return out.str();
}

GGUFTensorTypeInfo gguf_tensor_type_info(const std::uint32_t type) {
  const auto known = true;
  const auto quantized = true;
  const auto scalar = false;
  const auto live = false;
  switch (type) {
    case 0:
      return {type, "f32", 1, 4, scalar, live, known};
    case 1:
      return {type, "f16", 1, 2, scalar, live, known};
    case 2:
      return {type, "q4_0", kLegacyQ4ValueCount, kQ4_0TypeSizeBytes,
              quantized, live, known};
    case 3:
      return {type, "q4_1", kLegacyQ4ValueCount, kQ4_1TypeSizeBytes,
              quantized, live, known};
    case 4:
      return {type, "deprecated_q4_2", 0, 0, scalar, true, known};
    case 5:
      return {type, "deprecated_q4_3", 0, 0, scalar, true, known};
    case 6:
      return {type, "q5_0", kLegacyQ5ValueCount, kQ5_0TypeSizeBytes,
              quantized, live, known};
    case 7:
      return {type, "q5_1", kLegacyQ5ValueCount, kQ5_1TypeSizeBytes,
              quantized, live, known};
    case 8:
      return {type, "q8_0", kQ8_0ValueCount, kQ8_0TypeSizeBytes,
              quantized, live, known};
    case 9:
      return {type, "q8_1", kQ8_1ValueCount, kQ8_1TypeSizeBytes,
              quantized, live, known};
    case 10:
      return {type, "q2_K", kGGMLKQuantBlockSize, kQ2KTypeSizeBytes,
              quantized, live, known};
    case 11:
      return {type, "q3_K", kGGMLKQuantBlockSize, kQ3KTypeSizeBytes,
              quantized, live, known};
    case 12:
      return {type, "q4_K", kGGMLKQuantBlockSize, kQ4KTypeSizeBytes,
              quantized, live, known};
    case 13:
      return {type, "q5_K", kGGMLKQuantBlockSize, kQ5KTypeSizeBytes,
              quantized, live, known};
    case 14:
      return {type, "q6_K", kGGMLKQuantBlockSize, kQ6KTypeSizeBytes,
              quantized, live, known};
    case 15:
      return {type, "q8_K", kGGMLKQuantBlockSize, kQ8KTypeSizeBytes,
              quantized, live, known};
    case 16:
      return {type, "iq2_xxs", kIQ2XXSValueCount, kIQ2XXSTypeSizeBytes,
              quantized, live, known};
    case 17:
      return {type, "iq2_xs", kGGMLKQuantBlockSize,
              2 + kGGMLKQuantBlockSize / 8 * 2 +
                  kGGMLKQuantBlockSize / 32,
              quantized, live, known};
    case 18:
      return {type, "iq3_xxs", kGGMLKQuantBlockSize,
              2 + 3 * (kGGMLKQuantBlockSize / 8), quantized, live, known};
    case 19:
      return {type, "iq1_s", kGGMLKQuantBlockSize,
              2 + kGGMLKQuantBlockSize / 8 + kGGMLKQuantBlockSize / 16,
              quantized, live, known};
    case 20:
      return {type, "iq4_nl", kIQ4NLValueCount, kIQ4NLTypeSizeBytes,
              quantized, live, known};
    case 21:
      return {type, "iq3_s", kGGMLKQuantBlockSize,
              2 + 13 * (kGGMLKQuantBlockSize / 32) +
                  kGGMLKQuantBlockSize / 64,
              quantized, live, known};
    case 22:
      return {type, "iq2_s", kGGMLKQuantBlockSize,
              2 + kGGMLKQuantBlockSize / 4 + kGGMLKQuantBlockSize / 16,
              quantized, live, known};
    case 23:
      return {type, "iq4_xs", kIQ4XSValueCount, kIQ4XSTypeSizeBytes,
              quantized, live, known};
    case 24:
      return {type, "i8", 1, 1, scalar, live, known};
    case 25:
      return {type, "i16", 1, 2, scalar, live, known};
    case 26:
      return {type, "i32", 1, 4, scalar, live, known};
    case 27:
      return {type, "i64", 1, 8, scalar, live, known};
    case 28:
      return {type, "f64", 1, 8, scalar, live, known};
    case 29:
      return {type, "iq1_m", kGGMLKQuantBlockSize,
              kGGMLKQuantBlockSize / 8 + kGGMLKQuantBlockSize / 16 +
                  kGGMLKQuantBlockSize / 32,
              quantized, live, known};
    case 30:
      return {type, "bf16", 1, 2, scalar, live, known};
    case 31:
      return {type, "removed_q4_0_4_4", 0, 0, scalar, true, known};
    case 32:
      return {type, "removed_q4_0_4_8", 0, 0, scalar, true, known};
    case 33:
      return {type, "removed_q4_0_8_8", 0, 0, scalar, true, known};
    case 34:
      return {type, "tq1_0", kTQ1_0ValueCount,
              kTQ1_0TypeSizeBytes, quantized, live, known};
    case 35:
      return {type, "tq2_0", kTQ2_0ValueCount, kTQ2_0TypeSizeBytes,
              quantized, live, known};
    case 36:
      return {type, "removed_iq4_nl_4_4", 0, 0, scalar, true, known};
    case 37:
      return {type, "removed_iq4_nl_4_8", 0, 0, scalar, true, known};
    case 38:
      return {type, "removed_iq4_nl_8_8", 0, 0, scalar, true, known};
    case 39:
      return {type, "mxfp4", kMXFP4ValueCount, kMXFP4TypeSizeBytes,
              quantized, live, known};
    case 40:
      return {type, "nvfp4", kNVFP4ValueCount, kNVFP4TypeSizeBytes,
              quantized, live, known};
    case 41:
      return {type, "q1_0", kQ1_0ValueCount, kQ1_0TypeSizeBytes,
              quantized, live, known};
    default:
      return {type, "unknown", 0, 0, scalar, live, false};
  }
}

std::string gguf_tensor_type_name(const std::uint32_t type) {
  return gguf_tensor_type_info(type).name;
}

float gguf_fp16_to_float32(const std::uint16_t raw) {
  const int sign = (raw & 0x8000U) ? -1 : 1;
  const int exponent = static_cast<int>((raw >> 10U) & 0x1FU);
  const int fraction = static_cast<int>(raw & 0x03FFU);
  if (exponent == 0) {
    if (fraction == 0) {
      return sign < 0 ? -0.0F : 0.0F;
    }
    return static_cast<float>(
        sign * std::ldexp(static_cast<double>(fraction) / 1024.0, -14));
  }
  if (exponent == 31) {
    if (fraction == 0) {
      return sign * std::numeric_limits<float>::infinity();
    }
    return std::numeric_limits<float>::quiet_NaN();
  }
  return static_cast<float>(
      sign * std::ldexp(1.0 + static_cast<double>(fraction) / 1024.0,
                        exponent - 15));
}

GGUFF32ValuesPreview decode_gguf_f32_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  GGUFF32ValuesPreview result;
  const auto reserve_count =
      require_preview_value_count(tensor_bytes, value_count, 4U, "F32");
  result.raw_values.reserve(reserve_count);
  result.values.reserve(reserve_count);
  for (std::uint64_t i = 0; i < value_count; ++i) {
    const auto offset = checked_multiply(i, 4U, "F32 value offset");
    const auto raw = read_u32_le_at(tensor_bytes, offset, "F32 value");
    result.raw_values.push_back(raw);
    result.values.push_back(finite_float32_from_bits(raw, "F32 value"));
  }
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "F32");
  return result;
}

GGUFF64ValuesPreview decode_gguf_f64_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  GGUFF64ValuesPreview result;
  const auto reserve_count =
      require_preview_value_count(tensor_bytes, value_count, 8U, "F64");
  result.raw_values.reserve(reserve_count);
  result.values.reserve(reserve_count);
  for (std::uint64_t i = 0; i < value_count; ++i) {
    const auto offset = checked_multiply(i, 8U, "F64 value offset");
    const auto raw = read_u64_le_at(tensor_bytes, offset, "F64 value");
    result.raw_values.push_back(raw);
    result.values.push_back(finite_float64_from_bits(raw, "F64 value"));
  }
  finalize_double_stats(result.values, result.checksum, result.min, result.max,
                        result.mean, "F64");
  return result;
}

GGUFInt64ValuesPreview decode_gguf_i64_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  return decode_signed_integer_values<GGUFInt64ValuesPreview, std::uint64_t,
                                      std::int64_t>(
      tensor_bytes, value_count, 8U, "I64", read_u64_le_at);
}

GGUFInt32ValuesPreview decode_gguf_i32_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  return decode_signed_integer_values<GGUFInt32ValuesPreview, std::uint32_t,
                                      std::int32_t>(
      tensor_bytes, value_count, 4U, "I32", read_u32_le_at);
}

GGUFInt16ValuesPreview decode_gguf_i16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  return decode_signed_integer_values<GGUFInt16ValuesPreview, std::uint16_t,
                                      std::int16_t>(
      tensor_bytes, value_count, 2U, "I16", read_u16_le_at);
}

GGUFInt8ValuesPreview decode_gguf_i8_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  const auto read_u8_at = [](const std::vector<std::uint8_t>& bytes,
                             const std::uint64_t offset,
                             const std::string& context) {
    return byte_at(bytes, offset, context);
  };
  return decode_signed_integer_values<GGUFInt8ValuesPreview, std::uint8_t,
                                      std::int8_t>(
      tensor_bytes, value_count, 1U, "I8", read_u8_at);
}

GGUFF16ValuesPreview decode_gguf_f16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  GGUFF16ValuesPreview result;
  const auto reserve_count =
      require_preview_value_count(tensor_bytes, value_count, 2U, "F16");
  result.raw_values.reserve(reserve_count);
  result.values.reserve(reserve_count);
  for (std::uint64_t i = 0; i < value_count; ++i) {
    const auto offset = checked_multiply(i, 2U, "F16 value offset");
    const auto raw = read_u16_le_at(tensor_bytes, offset, "F16 value");
    result.raw_values.push_back(raw);
    result.values.push_back(gguf_fp16_to_float32(raw));
  }
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "F16");
  return result;
}

float gguf_bf16_to_float32(const std::uint16_t raw) {
  const auto bits = static_cast<std::uint32_t>(raw) << 16U;
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

GGUFBF16ValuesPreview decode_gguf_bf16_values(
    const std::vector<std::uint8_t>& tensor_bytes,
    const std::uint64_t value_count) {
  GGUFBF16ValuesPreview result;
  const auto reserve_count =
      require_preview_value_count(tensor_bytes, value_count, 2U, "BF16");
  result.raw_values.reserve(reserve_count);
  result.values.reserve(reserve_count);
  for (std::uint64_t i = 0; i < value_count; ++i) {
    const auto offset = checked_multiply(i, 2U, "BF16 value offset");
    const auto raw = read_u16_le_at(tensor_bytes, offset, "BF16 value");
    result.raw_values.push_back(raw);
    result.values.push_back(gguf_bf16_to_float32(raw));
  }
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "BF16");
  return result;
}

GGUFIQ2XXSBlockPreview preview_gguf_iq2_xxs_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kIQ2XXSTypeSizeBytes) {
    throw std::runtime_error("IQ2_XXS block requires at least 66 bytes");
  }

  GGUFIQ2XXSBlockPreview result;
  result.d = require_finite_float(
      gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "IQ2_XXS d")),
      "IQ2_XXS d");
  result.qs_words.reserve(static_cast<std::size_t>(kIQ2XXSWordCount));
  result.grid_indices.reserve(static_cast<std::size_t>(kIQ2XXSChunkCount));
  result.sign_codes.reserve(static_cast<std::size_t>(kIQ2XXSChunkCount));
  result.scale_nibbles.reserve(static_cast<std::size_t>(kIQ2XXSGroups));
  result.effective_scales.reserve(static_cast<std::size_t>(kIQ2XXSGroups));

  for (std::uint64_t i = 0; i < kIQ2XXSWordCount; ++i) {
    result.qs_words.push_back(read_u16_le_at(
        block_bytes, kIQ2XXSQSOffset + i * 2U, "IQ2_XXS qs word"));
  }

  for (std::uint64_t group = 0; group < kIQ2XXSGroups; ++group) {
    const auto group_offset = kIQ2XXSQSOffset + group * 8U;
    const auto aux1 =
        read_u32_le_at(block_bytes, group_offset + 4U, "IQ2_XXS aux1");
    const auto scale_nibble =
        static_cast<std::uint8_t>((aux1 >> 28U) & 0x0FU);
    result.scale_nibbles.push_back(scale_nibble);
    result.effective_scales.push_back(checked_float_output(
        static_cast<double>(result.d) *
            (0.5 + static_cast<double>(scale_nibble)) * 0.25,
        "IQ2_XXS effective scale"));
    for (std::uint64_t chunk = 0; chunk < kIQ2XXSChunksPerGroup; ++chunk) {
      result.grid_indices.push_back(
          byte_at(block_bytes, group_offset + chunk, "IQ2_XXS grid index"));
      result.sign_codes.push_back(
          static_cast<std::uint8_t>((aux1 >> (7U * chunk)) & 0x7FU));
    }
  }

  return result;
}

GGUFIQ2XXSBlockDequant dequantize_gguf_iq2_xxs_block(
    const std::vector<std::uint8_t>& block_bytes) {
  const auto preview = preview_gguf_iq2_xxs_block(block_bytes);

  GGUFIQ2XXSBlockDequant result;
  result.d = preview.d;
  result.qs_words = preview.qs_words;
  result.grid_indices = preview.grid_indices;
  result.sign_codes = preview.sign_codes;
  result.scale_nibbles = preview.scale_nibbles;
  result.effective_scales = preview.effective_scales;
  result.values.reserve(static_cast<std::size_t>(kIQ2XXSValueCount));
  result.group_sums.reserve(static_cast<std::size_t>(kIQ2XXSGroups));

  for (std::uint64_t group = 0; group < kIQ2XXSGroups; ++group) {
    float group_sum = 0.0F;
    const auto scale = result.effective_scales[static_cast<std::size_t>(group)];
    for (std::uint64_t chunk = 0; chunk < kIQ2XXSChunksPerGroup; ++chunk) {
      const auto chunk_index =
          static_cast<std::size_t>(group) *
              static_cast<std::size_t>(kIQ2XXSChunksPerGroup) +
          static_cast<std::size_t>(chunk);
      const auto grid =
          kIQ2XXSGrid[result.grid_indices[chunk_index]];
      const auto signs = kIQ2XXSSigns[result.sign_codes[chunk_index]];
      for (std::uint64_t lane = 0; lane < kIQ2XXSValuesPerChunk; ++lane) {
        const auto grid_value = static_cast<std::uint8_t>(
            (grid >> (lane * 8U)) & 0xFFU);
        const auto sign =
            (signs & kIQ2XXSMasks[static_cast<std::size_t>(lane)]) != 0
                ? -1.0F
                : 1.0F;
        const auto value = scale * static_cast<float>(grid_value) * sign;
        result.values.push_back(value);
        add_checked_float(group_sum, value, "IQ2_XXS group sum");
      }
    }
    result.group_sums.push_back(group_sum);
  }

  finalize_k_quant_stats(result.values, result.group_sums, result.checksum,
                         result.min, result.max, result.mean,
                         kIQ2XXSGroups, "IQ2_XXS");
  return result;
}

GGUFQ1_0BlockDequant dequantize_gguf_q1_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ1_0TypeSizeBytes) {
    throw std::runtime_error("Q1_0 block requires at least 18 bytes");
  }

  GGUFQ1_0BlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q1_0 d"));
  result.bit_values.reserve(static_cast<std::size_t>(kQ1_0ValueCount));
  result.values.reserve(static_cast<std::size_t>(kQ1_0ValueCount));

  for (std::uint64_t i = 0; i < kQ1_0ValueCount; ++i) {
    const auto packed =
        byte_at(block_bytes, kQ1_0QuantOffset + i / 8, "Q1_0 bit quant");
    const auto bit = static_cast<std::uint8_t>((packed >> (i % 8)) & 0x01U);
    const auto value = bit == 0 ? -result.d : result.d;
    result.bit_values.push_back(bit);
    result.values.push_back(value);
    if (bit == 0) {
      ++result.negative_bit_count;
    } else {
      ++result.positive_bit_count;
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q1_0");
  return result;
}

GGUFQ2KBlockDequant dequantize_gguf_q2_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ2KTypeSizeBytes) {
    throw std::runtime_error("Q2_K block requires at least 84 bytes");
  }

  GGUFQ2KBlockDequant result;
  result.d = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, kQ2KDOffset, "Q2_K d"));
  result.dmin = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, kQ2KDMinOffset, "Q2_K dmin"));
  result.scale_min_pairs.reserve(static_cast<std::size_t>(kQ2KGroups));
  result.values.reserve(static_cast<std::size_t>(kGGMLKQuantBlockSize));
  result.group_sums.reserve(static_cast<std::size_t>(kQ2KGroups));

  std::size_t group = 0;
  for (std::uint64_t half = 0; half < 2; ++half) {
    const auto quant_base = half * 32;
    int shift = 0;
    for (std::uint64_t plane = 0; plane < 4; ++plane) {
      for (std::uint64_t side = 0; side < 2; ++side) {
        const auto pair = decode_q2_k_scale_min(
            block_bytes, static_cast<int>(group), result.d, result.dmin);
        result.scale_min_pairs.push_back(pair);
        float group_sum = 0.0F;
        const auto lane_base = side * 16;
        for (std::uint64_t lane = 0; lane < kQ2KValuesPerGroup; ++lane) {
          const auto packed = byte_at(block_bytes,
                                      kQ2KQuantOffset + quant_base +
                                          lane_base + lane,
                                      "Q2_K quant");
          const auto q = static_cast<int>((packed >> shift) & 0x03U);
          const auto value =
              pair.effective_scale * static_cast<float>(q) -
              pair.effective_min;
          result.values.push_back(value);
          add_checked_float(group_sum, value, "Q2_K group sum");
        }
        result.group_sums.push_back(group_sum);
        ++group;
      }
      shift += 2;
    }
  }

  finalize_k_quant_stats(result.values, result.group_sums, result.checksum,
                         result.min, result.max, result.mean, kQ2KGroups,
                         "Q2_K");
  return result;
}

GGUFQ4_0BlockDequant dequantize_gguf_q4_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ4_0TypeSizeBytes) {
    throw std::runtime_error("Q4_0 block requires at least 18 bytes");
  }

  GGUFQ4_0BlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q4_0 d"));
  result.quant_values.assign(static_cast<std::size_t>(kLegacyQ4ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kLegacyQ4ValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kLegacyQ4ValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kQ4_0QuantOffset + i, "Q4_0 quant");
    const auto low = static_cast<std::uint8_t>(packed & 0x0FU);
    const auto high = static_cast<std::uint8_t>((packed >> 4U) & 0x0FU);
    result.quant_values[static_cast<std::size_t>(i)] = low;
    result.quant_values[static_cast<std::size_t>(i + kLegacyQ4ValueCount / 2)] =
        high;
    result.values[static_cast<std::size_t>(i)] =
        result.d * static_cast<float>(static_cast<int>(low) - 8);
    result.values[static_cast<std::size_t>(i + kLegacyQ4ValueCount / 2)] =
        result.d * static_cast<float>(static_cast<int>(high) - 8);
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q4_0");
  return result;
}

GGUFQ4_1BlockDequant dequantize_gguf_q4_1_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ4_1TypeSizeBytes) {
    throw std::runtime_error("Q4_1 block requires at least 20 bytes");
  }

  GGUFQ4_1BlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q4_1 d"));
  result.m = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 2, "Q4_1 m"));
  result.quant_values.assign(static_cast<std::size_t>(kLegacyQ4ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kLegacyQ4ValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kLegacyQ4ValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kQ4_1QuantOffset + i, "Q4_1 quant");
    const auto low = static_cast<std::uint8_t>(packed & 0x0FU);
    const auto high = static_cast<std::uint8_t>((packed >> 4U) & 0x0FU);
    result.quant_values[static_cast<std::size_t>(i)] = low;
    result.quant_values[static_cast<std::size_t>(i + kLegacyQ4ValueCount / 2)] =
        high;
    result.values[static_cast<std::size_t>(i)] =
        result.d * static_cast<float>(low) + result.m;
    result.values[static_cast<std::size_t>(i + kLegacyQ4ValueCount / 2)] =
        result.d * static_cast<float>(high) + result.m;
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q4_1");
  return result;
}

GGUFQ5_0BlockDequant dequantize_gguf_q5_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ5_0TypeSizeBytes) {
    throw std::runtime_error("Q5_0 block requires at least 22 bytes");
  }

  GGUFQ5_0BlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q5_0 d"));
  result.quant_values.assign(static_cast<std::size_t>(kLegacyQ5ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kLegacyQ5ValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kLegacyQ5ValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kQ5_0QuantOffset + i, "Q5_0 quant");
    const std::uint8_t low_values[2] = {
        static_cast<std::uint8_t>(packed & 0x0FU),
        static_cast<std::uint8_t>((packed >> 4U) & 0x0FU),
    };
    const std::uint64_t value_indexes[2] = {i, i + kLegacyQ5ValueCount / 2};
    for (std::size_t half = 0; half < 2; ++half) {
      const auto value_index = value_indexes[half];
      const auto high_byte = byte_at(block_bytes,
                                     kQ5_0HighBitsOffset + value_index / 8,
                                     "Q5_0 high bits");
      const auto high = ((high_byte >> (value_index % 8)) & 0x01U) != 0U;
      const auto quant = static_cast<std::uint8_t>(low_values[half] +
                                                   (high ? 16U : 0U));
      result.quant_values[static_cast<std::size_t>(value_index)] = quant;
      result.values[static_cast<std::size_t>(value_index)] =
          result.d * static_cast<float>(static_cast<int>(quant) - 16);
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q5_0");
  return result;
}

GGUFQ5_1BlockDequant dequantize_gguf_q5_1_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ5_1TypeSizeBytes) {
    throw std::runtime_error("Q5_1 block requires at least 24 bytes");
  }

  GGUFQ5_1BlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q5_1 d"));
  result.m = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 2, "Q5_1 m"));
  result.quant_values.assign(static_cast<std::size_t>(kLegacyQ5ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kLegacyQ5ValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kLegacyQ5ValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kQ5_1QuantOffset + i, "Q5_1 quant");
    const std::uint8_t low_values[2] = {
        static_cast<std::uint8_t>(packed & 0x0FU),
        static_cast<std::uint8_t>((packed >> 4U) & 0x0FU),
    };
    const std::uint64_t value_indexes[2] = {i, i + kLegacyQ5ValueCount / 2};
    for (std::size_t half = 0; half < 2; ++half) {
      const auto value_index = value_indexes[half];
      const auto high_byte = byte_at(block_bytes,
                                     kQ5_1HighBitsOffset + value_index / 8,
                                     "Q5_1 high bits");
      const auto high = ((high_byte >> (value_index % 8)) & 0x01U) != 0U;
      const auto quant = static_cast<std::uint8_t>(low_values[half] +
                                                   (high ? 16U : 0U));
      result.quant_values[static_cast<std::size_t>(value_index)] = quant;
      result.values[static_cast<std::size_t>(value_index)] =
          result.d * static_cast<float>(quant) + result.m;
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q5_1");
  return result;
}

GGUFIQ4NLBlockDequant dequantize_gguf_iq4_nl_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kIQ4NLTypeSizeBytes) {
    throw std::runtime_error("IQ4_NL block requires at least 18 bytes");
  }

  GGUFIQ4NLBlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "IQ4_NL d"));
  result.quant_values.assign(static_cast<std::size_t>(kIQ4NLValueCount), 0);
  result.codebook_values.assign(static_cast<std::size_t>(kIQ4NLValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kIQ4NLValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kIQ4NLValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kIQ4NLQuantOffset + i, "IQ4_NL quant");
    const std::uint8_t low_high[2] = {
        static_cast<std::uint8_t>(packed & 0x0FU),
        static_cast<std::uint8_t>(packed >> 4U),
    };
    const std::uint64_t positions[2] = {i, i + kIQ4NLValueCount / 2};
    for (std::size_t lane = 0; lane < 2; ++lane) {
      const auto position = static_cast<std::size_t>(positions[lane]);
      const auto quant = low_high[lane];
      const auto codebook_value = kIQ4NLCodebook[quant];
      result.quant_values[position] = quant;
      result.codebook_values[position] = codebook_value;
      result.values[position] =
          result.d * static_cast<float>(codebook_value);
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "IQ4_NL");
  return result;
}

GGUFMXFP4BlockDequant dequantize_gguf_mxfp4_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kMXFP4TypeSizeBytes) {
    throw std::runtime_error("MXFP4 block requires at least 17 bytes");
  }

  GGUFMXFP4BlockDequant result;
  result.e = byte_at(block_bytes, 0, "MXFP4 e");
  result.e_scale = e8m0_to_float32_half(result.e);
  result.quant_values.assign(static_cast<std::size_t>(kMXFP4ValueCount), 0);
  result.codebook_values.assign(static_cast<std::size_t>(kMXFP4ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kMXFP4ValueCount), 0.0F);

  for (std::uint64_t i = 0; i < kMXFP4ValueCount / 2; ++i) {
    const auto packed =
        byte_at(block_bytes, kMXFP4QuantOffset + i, "MXFP4 quant");
    const std::uint8_t low_high[2] = {
        static_cast<std::uint8_t>(packed & 0x0FU),
        static_cast<std::uint8_t>(packed >> 4U),
    };
    const std::uint64_t positions[2] = {i, i + kMXFP4ValueCount / 2};
    for (std::size_t lane = 0; lane < 2; ++lane) {
      const auto position = static_cast<std::size_t>(positions[lane]);
      const auto quant = low_high[lane];
      const auto codebook_value = kMXFP4Codebook[quant];
      result.quant_values[position] = quant;
      result.codebook_values[position] = codebook_value;
      result.values[position] =
          result.e_scale * static_cast<float>(codebook_value);
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "MXFP4");
  return result;
}

GGUFNVFP4BlockDequant dequantize_gguf_nvfp4_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kNVFP4TypeSizeBytes) {
    throw std::runtime_error("NVFP4 block requires at least 36 bytes");
  }

  GGUFNVFP4BlockDequant result;
  result.scales.reserve(static_cast<std::size_t>(kNVFP4SubBlocks));
  result.quant_values.assign(static_cast<std::size_t>(kNVFP4ValueCount), 0);
  result.codebook_values.assign(static_cast<std::size_t>(kNVFP4ValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kNVFP4ValueCount), 0.0F);
  result.subblock_sums.assign(static_cast<std::size_t>(kNVFP4SubBlocks), 0.0F);

  for (std::uint64_t block = 0; block < kNVFP4SubBlocks; ++block) {
    const auto raw_scale =
        byte_at(block_bytes, kNVFP4ScaleOffset + block, "NVFP4 scale");
    const auto scale = ue4m3_to_float32_half(raw_scale);
    result.scales.push_back({raw_scale, scale});

    for (std::uint64_t lane = 0; lane < kNVFP4ValuesPerSubBlock / 2; ++lane) {
      const auto packed = byte_at(block_bytes,
                                  kNVFP4QuantOffset +
                                      block * (kNVFP4ValuesPerSubBlock / 2) +
                                      lane,
                                  "NVFP4 quant");
      const std::uint8_t quant_values[2] = {
          static_cast<std::uint8_t>(packed & 0x0FU),
          static_cast<std::uint8_t>(packed >> 4U),
      };
      const std::uint64_t positions[2] = {
          block * kNVFP4ValuesPerSubBlock + lane,
          block * kNVFP4ValuesPerSubBlock + lane +
              kNVFP4ValuesPerSubBlock / 2,
      };
      for (std::size_t half = 0; half < 2; ++half) {
        const auto position = static_cast<std::size_t>(positions[half]);
        const auto quant = quant_values[half];
        const auto codebook_value = kMXFP4Codebook[quant];
        const auto value = scale * static_cast<float>(codebook_value);
        result.quant_values[position] = quant;
        result.codebook_values[position] = codebook_value;
        result.values[position] = value;
        add_checked_float(
            result.subblock_sums[static_cast<std::size_t>(block)], value,
            "NVFP4 sub-block sum");
      }
    }
  }

  finalize_k_quant_stats(result.values, result.subblock_sums, result.checksum,
                         result.min, result.max, result.mean, kNVFP4SubBlocks,
                         "NVFP4");
  return result;
}

GGUFIQ4XSBlockDequant dequantize_gguf_iq4_xs_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kIQ4XSTypeSizeBytes) {
    throw std::runtime_error("IQ4_XS block requires at least 136 bytes");
  }

  GGUFIQ4XSBlockDequant result;
  result.d = gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "IQ4_XS d"));
  result.scales_h =
      read_u16_le_at(block_bytes, kIQ4XSScalesHOffset, "IQ4_XS scales_h");
  result.scales.reserve(static_cast<std::size_t>(kIQ4XSSubBlocks));
  result.quant_values.assign(static_cast<std::size_t>(kIQ4XSValueCount), 0);
  result.codebook_values.assign(static_cast<std::size_t>(kIQ4XSValueCount), 0);
  result.values.assign(static_cast<std::size_t>(kIQ4XSValueCount), 0.0F);
  result.subblock_sums.assign(static_cast<std::size_t>(kIQ4XSSubBlocks), 0.0F);

  for (std::uint64_t block = 0; block < kIQ4XSSubBlocks; ++block) {
    const auto packed_low =
        byte_at(block_bytes, kIQ4XSScalesLOffset + block / 2, "IQ4_XS scales_l");
    const auto low = static_cast<std::uint8_t>(
        (packed_low >> (4U * (block % 2))) & 0x0FU);
    const auto high = static_cast<std::uint8_t>(
        (result.scales_h >> (2U * block)) & 0x03U);
    const auto raw_scale = static_cast<std::uint8_t>(low | (high << 4U));
    const auto signed_scale =
        static_cast<std::int8_t>(static_cast<int>(raw_scale) - 32);
    const auto effective_scale =
        result.d * static_cast<float>(signed_scale);
    result.scales.push_back({raw_scale, signed_scale, effective_scale});

    for (std::uint64_t lane = 0; lane < kIQ4XSValuesPerSubBlock / 2; ++lane) {
      const auto packed = byte_at(block_bytes,
                                  kIQ4XSQuantOffset + block * 16 + lane,
                                  "IQ4_XS quant");
      const std::uint8_t quant_values[2] = {
          static_cast<std::uint8_t>(packed & 0x0FU),
          static_cast<std::uint8_t>(packed >> 4U),
      };
      const std::uint64_t positions[2] = {
          block * kIQ4XSValuesPerSubBlock + lane,
          block * kIQ4XSValuesPerSubBlock + lane +
              kIQ4XSValuesPerSubBlock / 2,
      };
      for (std::size_t half = 0; half < 2; ++half) {
        const auto position = static_cast<std::size_t>(positions[half]);
        const auto quant = quant_values[half];
        const auto codebook_value = kIQ4NLCodebook[quant];
        const auto value =
            effective_scale * static_cast<float>(codebook_value);
        result.quant_values[position] = quant;
        result.codebook_values[position] = codebook_value;
        result.values[position] = value;
        add_checked_float(
            result.subblock_sums[static_cast<std::size_t>(block)], value,
            "IQ4_XS sub-block sum");
      }
    }
  }

  finalize_k_quant_stats(result.values, result.subblock_sums, result.checksum,
                         result.min, result.max, result.mean, kIQ4XSSubBlocks,
                         "IQ4_XS");
  return result;
}

GGUFTQ1_0BlockDequant dequantize_gguf_tq1_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kTQ1_0TypeSizeBytes) {
    throw std::runtime_error("TQ1_0 block requires at least 56 bytes");
  }

  GGUFTQ1_0BlockDequant result;
  result.d =
      gguf_fp16_to_float32(read_u16_le_at(block_bytes, kTQ1_0DOffset, "TQ1_0 d"));
  result.quant_values.reserve(static_cast<std::size_t>(kTQ1_0ValueCount));
  result.quant_counts.assign(3, 0);
  result.values.reserve(static_cast<std::size_t>(kTQ1_0ValueCount));

  const std::uint8_t pow3[5] = {1, 3, 9, 27, 81};
  auto append_packed_trits = [&](const std::uint64_t offset,
                                 const std::uint64_t byte_count,
                                 const std::uint64_t trits_per_byte) {
    for (std::uint64_t n = 0; n < trits_per_byte; ++n) {
      for (std::uint64_t m = 0; m < byte_count; ++m) {
        const auto packed =
            byte_at(block_bytes, offset + m, "TQ1_0 quant");
        const auto wrapped = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(packed) * pow3[n]);
        const auto quant = static_cast<std::uint8_t>(
            (static_cast<std::uint16_t>(wrapped) * 3U) >> 8U);
        if (quant >= result.quant_counts.size()) {
          throw std::runtime_error("TQ1_0 quant outside supported range");
        }
        const auto value =
            static_cast<float>(static_cast<int>(quant) - 1) * result.d;
        result.quant_values.push_back(quant);
        ++result.quant_counts[static_cast<std::size_t>(quant)];
        result.values.push_back(value);
      }
    }
  };

  append_packed_trits(kTQ1_0QuantOffset, 32, 5);
  append_packed_trits(kTQ1_0QuantOffset + 32, kTQ1_0QSPackedBytes - 32, 5);
  append_packed_trits(kTQ1_0HighQuantOffset, kTQ1_0QHPackedBytes, 4);

  if (result.values.size() != kTQ1_0ValueCount) {
    throw std::runtime_error("TQ1_0 dequant produced unexpected value count");
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "TQ1_0");
  return result;
}

GGUFTQ2_0BlockDequant dequantize_gguf_tq2_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kTQ2_0TypeSizeBytes) {
    throw std::runtime_error("TQ2_0 block requires at least 66 bytes");
  }

  GGUFTQ2_0BlockDequant result;
  result.d =
      gguf_fp16_to_float32(read_u16_le_at(block_bytes, kTQ2_0DOffset, "TQ2_0 d"));
  result.quant_values.reserve(static_cast<std::size_t>(kTQ2_0ValueCount));
  result.quant_counts.assign(4, 0);
  result.values.reserve(static_cast<std::size_t>(kTQ2_0ValueCount));

  for (std::uint64_t byte_group = 0; byte_group < 64; byte_group += 32) {
    for (std::uint64_t plane = 0; plane < 4; ++plane) {
      for (std::uint64_t lane = 0; lane < 32; ++lane) {
        const auto packed =
            byte_at(block_bytes, kTQ2_0QuantOffset + byte_group + lane,
                    "TQ2_0 quant");
        const auto quant =
            static_cast<std::uint8_t>((packed >> (plane * 2U)) & 0x03U);
        const auto value =
            static_cast<float>(static_cast<int>(quant) - 1) * result.d;
        result.quant_values.push_back(quant);
        ++result.quant_counts[static_cast<std::size_t>(quant)];
        result.values.push_back(value);
      }
    }
  }

  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "TQ2_0");
  return result;
}

GGUFQ3KBlockDequant dequantize_gguf_q3_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ3KTypeSizeBytes) {
    throw std::runtime_error("Q3_K block requires at least 110 bytes");
  }

  GGUFQ3KBlockDequant result;
  result.d = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, kQ3KDOffset, "Q3_K d"));
  result.scales = decode_q3_k_scales(block_bytes, result.d);
  result.values.reserve(static_cast<std::size_t>(kGGMLKQuantBlockSize));
  result.group_sums.assign(static_cast<std::size_t>(kQ3KGroups), 0.0F);

  std::uint8_t high_mask = 1;
  std::size_t scale_index = 0;
  for (std::uint64_t half = 0; half < 2; ++half) {
    int shift = 0;
    const auto quant_base = half * 32;
    for (std::uint64_t plane = 0; plane < 4; ++plane) {
      const auto scale_a = result.scales[scale_index++].effective_scale;
      for (std::uint64_t lane = 0; lane < 16; ++lane) {
        const auto packed = byte_at(block_bytes,
                                    kQ3KQuantOffset + quant_base + lane,
                                    "Q3_K quant");
        const auto high_present =
            (byte_at(block_bytes, kQ3KHighMaskOffset + lane,
                     "Q3_K high mask") &
             high_mask) != 0;
        const auto q = static_cast<int>((packed >> shift) & 0x03U) -
                       (high_present ? 0 : 4);
        const auto value = scale_a * static_cast<float>(q);
        result.values.push_back(value);
        add_checked_float(
            result.group_sums[(result.values.size() - 1) /
                              static_cast<std::size_t>(kQ3KValuesPerGroup)],
            value, "Q3_K group sum");
      }

      const auto scale_b = result.scales[scale_index++].effective_scale;
      for (std::uint64_t lane = 0; lane < 16; ++lane) {
        const auto packed = byte_at(block_bytes,
                                    kQ3KQuantOffset + quant_base + 16 + lane,
                                    "Q3_K quant");
        const auto high_present =
            (byte_at(block_bytes, kQ3KHighMaskOffset + 16 + lane,
                     "Q3_K high mask") &
             high_mask) != 0;
        const auto q = static_cast<int>((packed >> shift) & 0x03U) -
                       (high_present ? 0 : 4);
        const auto value = scale_b * static_cast<float>(q);
        result.values.push_back(value);
        add_checked_float(
            result.group_sums[(result.values.size() - 1) /
                              static_cast<std::size_t>(kQ3KValuesPerGroup)],
            value, "Q3_K group sum");
      }

      shift += 2;
      high_mask = static_cast<std::uint8_t>(high_mask << 1U);
    }
  }

  finalize_k_quant_stats(result.values, result.group_sums, result.checksum,
                         result.min, result.max, result.mean, kQ3KGroups,
                         "Q3_K");
  return result;
}

GGUFQ4KBlockDequant dequantize_gguf_q4_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ4KTypeSizeBytes) {
    throw std::runtime_error("Q4_K block requires at least 144 bytes");
  }

  GGUFQ4KBlockDequant result;
  result.d = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, 0, "Q4_K d"));
  result.dmin = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, 2, "Q4_K dmin"));
  result.scale_min_pairs.reserve(static_cast<std::size_t>(kQ4KSubBlocks));
  result.values.reserve(static_cast<std::size_t>(kGGMLKQuantBlockSize));
  result.subblock_sums.reserve(static_cast<std::size_t>(kQ4KSubBlocks));

  for (int sub_block = 0; sub_block < static_cast<int>(kQ4KSubBlocks);
       ++sub_block) {
    const auto pair =
        decode_k_quant_scale_min(block_bytes, sub_block, result.d, result.dmin);
    result.scale_min_pairs.push_back(pair);
    float subblock_sum = 0.0F;
    const std::uint64_t byte_group_offset =
        static_cast<std::uint64_t>(sub_block / 2) * kQ4KValuesPerSubBlock;
    const bool high_nibble = (sub_block % 2) != 0;
    for (std::uint64_t i = 0; i < kQ4KValuesPerSubBlock; ++i) {
      const auto packed = byte_at(block_bytes,
                                  kQ4KQuantOffset + byte_group_offset + i,
                                  "Q4_K quant");
      const auto q = high_nibble ? static_cast<int>((packed >> 4U) & 0x0FU)
                                 : static_cast<int>(packed & 0x0FU);
      const auto value =
          pair.effective_scale * static_cast<float>(q) - pair.effective_min;
      result.values.push_back(value);
      add_checked_float(subblock_sum, value, "Q4_K sub-block sum");
    }
    result.subblock_sums.push_back(subblock_sum);
  }

  finalize_k_quant_stats(result.values, result.subblock_sums, result.checksum,
                         result.min, result.max, result.mean, kQ4KSubBlocks,
                         "Q4_K");
  return result;
}

GGUFQ5KBlockDequant dequantize_gguf_q5_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ5KTypeSizeBytes) {
    throw std::runtime_error("Q5_K block requires at least 176 bytes");
  }

  GGUFQ5KBlockDequant result;
  result.d = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, 0, "Q5_K d"));
  result.dmin = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, 2, "Q5_K dmin"));
  result.scale_min_pairs.reserve(static_cast<std::size_t>(kQ4KSubBlocks));
  result.values.reserve(static_cast<std::size_t>(kGGMLKQuantBlockSize));
  result.subblock_sums.reserve(static_cast<std::size_t>(kQ4KSubBlocks));

  for (int sub_block = 0; sub_block < static_cast<int>(kQ4KSubBlocks);
       ++sub_block) {
    const auto pair =
        decode_k_quant_scale_min(block_bytes, sub_block, result.d, result.dmin);
    result.scale_min_pairs.push_back(pair);
    float subblock_sum = 0.0F;
    const std::uint64_t byte_group_offset =
        static_cast<std::uint64_t>(sub_block / 2) * kQ4KValuesPerSubBlock;
    const bool high_nibble = (sub_block % 2) != 0;
    const auto high_bit_mask =
        static_cast<std::uint8_t>(1U << static_cast<unsigned>(sub_block));
    for (std::uint64_t i = 0; i < kQ4KValuesPerSubBlock; ++i) {
      const auto packed = byte_at(block_bytes,
                                  kQ5KQuantOffset + byte_group_offset + i,
                                  "Q5_K quant");
      const auto high = byte_at(block_bytes, kQ5KHighBitsOffset + i,
                                "Q5_K high bits") &
                        high_bit_mask;
      const auto low = high_nibble ? static_cast<int>((packed >> 4U) & 0x0FU)
                                   : static_cast<int>(packed & 0x0FU);
      const auto q = low + (high != 0 ? 16 : 0);
      const auto value =
          pair.effective_scale * static_cast<float>(q) - pair.effective_min;
      result.values.push_back(value);
      add_checked_float(subblock_sum, value, "Q5_K sub-block sum");
    }
    result.subblock_sums.push_back(subblock_sum);
  }

  finalize_k_quant_stats(result.values, result.subblock_sums, result.checksum,
                         result.min, result.max, result.mean, kQ4KSubBlocks,
                         "Q5_K");
  return result;
}

GGUFQ6KBlockDequant dequantize_gguf_q6_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ6KTypeSizeBytes) {
    throw std::runtime_error("Q6_K block requires at least 210 bytes");
  }

  GGUFQ6KBlockDequant result;
  result.d = gguf_fp16_to_float32(
      read_u16_le_at(block_bytes, kQ6KDOffset, "Q6_K d"));
  result.scales.reserve(static_cast<std::size_t>(kQ6KGroups));
  result.values.assign(static_cast<std::size_t>(kGGMLKQuantBlockSize), 0.0F);
  result.group_sums.assign(static_cast<std::size_t>(kQ6KGroups), 0.0F);

  for (std::uint64_t i = 0; i < kQ6KGroups; ++i) {
    const auto scale = signed_from_twos_complement<std::int8_t>(
        byte_at(block_bytes, kQ6KScalesOffset + i, "Q6_K scale"));
    result.scales.push_back(
        {scale, result.d * static_cast<float>(scale)});
  }

  for (std::uint64_t half = 0; half < 2; ++half) {
    const auto ql_base = half * 64;
    const auto qh_base = half * 32;
    const auto scale_base = half * 8;
    const auto output_base = half * 128;
    for (std::uint64_t lane = 0; lane < 32; ++lane) {
      const auto scale_lane = lane / 16;
      const auto packed_low_a = byte_at(
          block_bytes, kQ6KLowBitsOffset + ql_base + lane, "Q6_K ql");
      const auto packed_low_b = byte_at(
          block_bytes, kQ6KLowBitsOffset + ql_base + 32 + lane, "Q6_K ql");
      const auto packed_high = byte_at(
          block_bytes, kQ6KHighBitsOffset + qh_base + lane, "Q6_K qh");

      const int quants[4] = {
          static_cast<int>((packed_low_a & 0x0FU) |
                           (((packed_high >> 0U) & 0x03U) << 4U)) -
              32,
          static_cast<int>((packed_low_b & 0x0FU) |
                           (((packed_high >> 2U) & 0x03U) << 4U)) -
              32,
          static_cast<int>(((packed_low_a >> 4U) & 0x0FU) |
                           (((packed_high >> 4U) & 0x03U) << 4U)) -
              32,
          static_cast<int>(((packed_low_b >> 4U) & 0x0FU) |
                           (((packed_high >> 6U) & 0x03U) << 4U)) -
              32,
      };
      const std::uint64_t value_offsets[4] = {0, 32, 64, 96};
      const std::uint64_t scale_offsets[4] = {0, 2, 4, 6};
      for (std::size_t item = 0; item < 4; ++item) {
        const auto value_index = output_base + value_offsets[item] + lane;
        const auto scale_index = scale_base + scale_lane + scale_offsets[item];
        const auto value = result.scales[static_cast<std::size_t>(scale_index)]
                               .effective_scale *
                           static_cast<float>(quants[item]);
        result.values[static_cast<std::size_t>(value_index)] = value;
        add_checked_float(
            result.group_sums[static_cast<std::size_t>(
                value_index / kQ6KValuesPerGroup)],
            value, "Q6_K group sum");
      }
    }
  }

  finalize_k_quant_stats(result.values, result.group_sums, result.checksum,
                         result.min, result.max, result.mean, kQ6KGroups,
                         "Q6_K");
  return result;
}

GGUFQ8_0BlockDequant dequantize_gguf_q8_0_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ8_0TypeSizeBytes) {
    throw std::runtime_error("Q8_0 block requires at least 34 bytes");
  }

  GGUFQ8_0BlockDequant result;
  result.d = require_finite_float(
      gguf_fp16_to_float32(read_u16_le_at(block_bytes, 0, "Q8_0 d")),
      "Q8_0 d");
  result.quant_values.reserve(static_cast<std::size_t>(kQ8_0ValueCount));
  result.values.reserve(static_cast<std::size_t>(kQ8_0ValueCount));

  for (std::uint64_t i = 0; i < kQ8_0ValueCount; ++i) {
    const auto quant = signed_from_twos_complement<std::int8_t>(
        byte_at(block_bytes, kQ8_0QuantOffset + i, "Q8_0 quant"));
    const auto value = checked_float_output(
        static_cast<double>(result.d) * static_cast<double>(quant),
        "Q8_0 value");
    result.quant_values.push_back(quant);
    result.values.push_back(value);
    result.quant_sum = checked_add_i64(
        result.quant_sum, static_cast<std::int64_t>(quant),
        "Q8_0 quant sum");
  }
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q8_0");
  return result;
}

GGUFQ8_1BlockDequant dequantize_gguf_q8_1_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ8_1TypeSizeBytes) {
    throw std::runtime_error("Q8_1 block requires at least 40 bytes");
  }

  GGUFQ8_1BlockDequant result;
  result.d = read_f32_le_at(block_bytes, 0, "Q8_1 d");
  result.s = read_f32_le_at(block_bytes, 4, "Q8_1 s");
  result.quant_values.reserve(static_cast<std::size_t>(kQ8_1ValueCount));
  result.values.reserve(static_cast<std::size_t>(kQ8_1ValueCount));

  for (std::uint64_t i = 0; i < kQ8_1ValueCount; ++i) {
    const auto quant = signed_from_twos_complement<std::int8_t>(
        byte_at(block_bytes, kQ8_1QuantOffset + i, "Q8_1 quant"));
    const auto value = checked_float_output(
        static_cast<double>(result.d) * static_cast<double>(quant),
        "Q8_1 value");
    result.quant_values.push_back(quant);
    result.values.push_back(value);
    result.quant_sum = checked_add_i64(
        result.quant_sum, static_cast<std::int64_t>(quant),
        "Q8_1 quant sum");
  }

  result.expected_s_from_quant_sum = checked_float_output(
      static_cast<double>(result.d) * static_cast<double>(result.quant_sum),
      "Q8_1 expected sum");
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q8_1");
  return result;
}

GGUFQ8KBlockDequant dequantize_gguf_q8_k_block(
    const std::vector<std::uint8_t>& block_bytes) {
  if (block_bytes.size() < kQ8KTypeSizeBytes) {
    throw std::runtime_error("Q8_K block requires at least 292 bytes");
  }

  GGUFQ8KBlockDequant result;
  result.d = read_f32_le_at(block_bytes, 0, "Q8_K d");
  result.quant_values.reserve(static_cast<std::size_t>(kQ8KValueCount));
  result.block_sums.reserve(static_cast<std::size_t>(kQ8KBlockSumCount));
  result.values.reserve(static_cast<std::size_t>(kQ8KValueCount));
  std::vector<std::int64_t> actual_block_sums(
      static_cast<std::size_t>(kQ8KBlockSumCount), 0);

  for (std::uint64_t block = 0; block < kQ8KBlockSumCount; ++block) {
    const auto stored_sum = read_i16_le_at(
        block_bytes, kQ8KBlockSumOffset + block * 2, "Q8_K block sum");
    result.block_sums.push_back(stored_sum);
    result.stored_block_sum_total = checked_add_i64(
        result.stored_block_sum_total, static_cast<std::int64_t>(stored_sum),
        "Q8_K stored block sum total");
  }

  for (std::uint64_t i = 0; i < kQ8KValueCount; ++i) {
    const auto quant = signed_from_twos_complement<std::int8_t>(
        byte_at(block_bytes, kQ8KQuantOffset + i, "Q8_K quant"));
    const auto value = checked_float_output(
        static_cast<double>(result.d) * static_cast<double>(quant),
        "Q8_K value");
    result.quant_values.push_back(quant);
    result.values.push_back(value);
    result.quant_sum = checked_add_i64(
        result.quant_sum, static_cast<std::int64_t>(quant),
        "Q8_K quant sum");
    const auto block_index =
        static_cast<std::size_t>(i / (kQ8KValueCount / kQ8KBlockSumCount));
    actual_block_sums[block_index] = checked_add_i64(
        actual_block_sums[block_index], static_cast<std::int64_t>(quant),
        "Q8_K actual block sum");
  }

  for (std::size_t i = 0; i < actual_block_sums.size(); ++i) {
    if (actual_block_sums[i] != result.block_sums[i]) {
      throw std::runtime_error(
          "Q8_K stored block sum does not match quant values");
    }
  }
  finalize_float_stats(result.values, result.checksum, result.min, result.max,
                       result.mean, "Q8_K");
  return result;
}

}  // namespace inferlab
