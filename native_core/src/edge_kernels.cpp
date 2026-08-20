#include "inferlab/edge_kernels.hpp"
#include "inferlab/normalization.hpp"
#include "inferlab/rope.hpp"

#include "checked_index.hpp"
#include "normalization_internal.hpp"
#include "quantization_internal.hpp"
#include "rope_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#if !defined(INFERLAB_ENABLE_ARM_NEON_BUILD)
#define INFERLAB_ENABLE_ARM_NEON_BUILD 0
#endif

#if !defined(INFERLAB_ENABLE_ARM_SVE_REQUESTED)
#define INFERLAB_ENABLE_ARM_SVE_REQUESTED 0
#endif

#if INFERLAB_ENABLE_ARM_NEON_BUILD && defined(__aarch64__) && \
    defined(__ARM_NEON)
#define INFERLAB_EDGE_NEON_IMPLEMENTATION 1
#include <arm_neon.h>
#else
#define INFERLAB_EDGE_NEON_IMPLEMENTATION 0
#endif

#if defined(__linux__) && defined(__aarch64__)
#include <sys/auxv.h>
#if defined(__has_include)
#if __has_include(<asm/hwcap.h>)
#include <asm/hwcap.h>
#endif
#endif
#endif

namespace inferlab {
namespace {

struct BackendSelection {
  EdgeKernelBackend selected = EdgeKernelBackend::kScalar;
  bool fallback_used = false;
  std::string reason;
};

BackendSelection select_backend(const EdgeKernelBackend requested) {
  const auto capabilities = detect_edge_isa_capabilities();
  switch (requested) {
    case EdgeKernelBackend::kScalar:
      return {EdgeKernelBackend::kScalar, false,
              "scalar_explicitly_requested"};
    case EdgeKernelBackend::kArmNeon:
      if (!capabilities.neon_compiled) {
        throw std::runtime_error(
            "ARM NEON backend was requested but was not compiled");
      }
      if (!capabilities.neon_runtime_available) {
        throw std::runtime_error(
            "ARM NEON backend was requested but is unavailable at runtime");
      }
      return {EdgeKernelBackend::kArmNeon, false,
              "arm_neon_explicitly_requested"};
    case EdgeKernelBackend::kAuto:
      break;
    default:
      throw std::invalid_argument("unknown edge kernel backend");
  }
  if (capabilities.neon_compiled && capabilities.neon_runtime_available) {
    return {EdgeKernelBackend::kArmNeon, false,
            "arm_neon_selected_by_runtime_capability"};
  }
  return {EdgeKernelBackend::kScalar, true,
          capabilities.neon_compiled
              ? "arm_neon_runtime_capability_unavailable"
              : "arm_neon_implementation_not_compiled"};
}

void assign_execution(const EdgeKernelBackend requested,
                      const BackendSelection& selection,
                      EdgeKernelExecution* execution) {
  if (execution == nullptr) return;
  execution->requested = requested;
  execution->selected = selection.selected;
  execution->fallback_used = selection.fallback_used;
  execution->selection_reason = selection.reason;
}

#if INFERLAB_EDGE_NEON_IMPLEMENTATION

float32x4_t int8_low4_to_float(const int8x8_t value) {
  const int16x8_t widened16 = vmovl_s8(value);
  return vcvtq_f32_s32(vmovl_s16(vget_low_s16(widened16)));
}

float32x4_t int8_high4_to_float(const int8x8_t value) {
  const int16x8_t widened16 = vmovl_s8(value);
  return vcvtq_f32_s32(vmovl_s16(vget_high_s16(widened16)));
}

double dot_product_four_as_double(const float32x4_t lhs,
                                  const float* rhs) {
  const float32x4_t rhs_values = vld1q_f32(rhs);
  const float64x2_t low_product = vmulq_f64(
      vcvt_f64_f32(vget_low_f32(lhs)),
      vcvt_f64_f32(vget_low_f32(rhs_values)));
  const float64x2_t high_product = vmulq_f64(
      vcvt_f64_f32(vget_high_f32(lhs)),
      vcvt_f64_f32(vget_high_f32(rhs_values)));
  return vaddvq_f64(low_product) + vaddvq_f64(high_product);
}

std::vector<float> rms_norm_neon(const std::vector<float>& matrix,
                                 const int rows, const int cols,
                                 const std::vector<float>& gamma,
                                 const float epsilon) {
  internal::validate_norm_inputs(matrix, rows, cols, gamma);
  if (!internal::finite_positive_normalization_value(epsilon)) {
    throw std::invalid_argument(
        "rms_norm_with_backend epsilon must be finite and positive");
  }

  std::vector<float> output(matrix.size(), 0.0F);
  for (int row = 0; row < rows; ++row) {
    const auto row_offset = internal::checked_flat_index(
        row, cols, 0, "rms_norm_with_backend input");
    int col = 0;
    float64x2_t square_sum_low = vdupq_n_f64(0.0);
    float64x2_t square_sum_high = vdupq_n_f64(0.0);
    for (; col <= cols - 4; col += 4) {
      const float32x4_t value = vld1q_f32(matrix.data() + row_offset + col);
      const float64x2_t low = vcvt_f64_f32(vget_low_f32(value));
      const float64x2_t high = vcvt_f64_f32(vget_high_f32(value));
      square_sum_low = vfmaq_f64(square_sum_low, low, low);
      square_sum_high = vfmaq_f64(square_sum_high, high, high);
    }
    double mean_square_sum =
        vaddvq_f64(square_sum_low) + vaddvq_f64(square_sum_high);
    for (; col < cols; ++col) {
      const double value = matrix[row_offset + static_cast<std::size_t>(col)];
      mean_square_sum += value * value;
    }
    const float mean_square = internal::checked_float_from_double(
        mean_square_sum / static_cast<double>(cols),
        "rms_norm_with_backend mean square");
    const float denominator = internal::checked_norm_denominator(
        mean_square, epsilon, "rms_norm_with_backend");
    const float inv_rms = 1.0F / std::sqrt(denominator);
    if (!std::isfinite(inv_rms)) {
      throw std::invalid_argument(
          "rms_norm_with_backend inv_rms must be finite");
    }

    col = 0;
    const float32x4_t inv = vdupq_n_f32(inv_rms);
    for (; col <= cols - 4; col += 4) {
      const float32x4_t value = vld1q_f32(matrix.data() + row_offset + col);
      const float32x4_t scale = vld1q_f32(gamma.data() + col);
      const float32x4_t normalized = vmulq_f32(vmulq_f32(value, inv), scale);
      vst1q_f32(output.data() + row_offset + col, normalized);
    }
    for (; col < cols; ++col) {
      output[row_offset + static_cast<std::size_t>(col)] =
          matrix[row_offset + static_cast<std::size_t>(col)] * inv_rms *
          gamma[static_cast<std::size_t>(col)];
    }
    for (int output_col = 0; output_col < cols; ++output_col) {
      if (!std::isfinite(
              output[row_offset + static_cast<std::size_t>(output_col)])) {
        throw std::invalid_argument(
            "rms_norm_with_backend output must be finite");
      }
    }
  }
  return output;
}

std::vector<float> rope_neon(const std::vector<float>& input,
                             const int seq_len, const int dim,
                             const float base, const int position_offset) {
  internal::validate_rope_inputs(input, seq_len, dim, base, position_offset);
  std::vector<float> output(input.size(), 0.0F);
  for (int position = 0; position < seq_len; ++position) {
    const auto row_offset = internal::checked_flat_index(
        position, dim, 0, "apply_rope_with_backend input");
    int col = 0;
    for (; col <= dim - 8; col += 8) {
      float cosine[4]{};
      float sine[4]{};
      for (int pair = 0; pair < 4; ++pair) {
        internal::rope_rotation(col + pair * 2, dim, base, position,
                                position_offset, &cosine[pair], &sine[pair]);
      }
      const float32x4x2_t input_pairs =
          vld2q_f32(input.data() + row_offset + col);
      const float32x4_t cos_values = vld1q_f32(cosine);
      const float32x4_t sin_values = vld1q_f32(sine);
      float32x4x2_t output_pairs;
      output_pairs.val[0] = vsubq_f32(
          vmulq_f32(input_pairs.val[0], cos_values),
          vmulq_f32(input_pairs.val[1], sin_values));
      output_pairs.val[1] = vaddq_f32(
          vmulq_f32(input_pairs.val[0], sin_values),
          vmulq_f32(input_pairs.val[1], cos_values));
      vst2q_f32(output.data() + row_offset + col, output_pairs);
    }
    for (; col < dim; col += 2) {
      float cosine = 0.0F;
      float sine = 0.0F;
      internal::rope_rotation(col, dim, base, position, position_offset,
                              &cosine, &sine);
      const auto index = row_offset + static_cast<std::size_t>(col);
      const float x0 = input[index];
      const float x1 = input[index + 1U];
      output[index] = x0 * cosine - x1 * sine;
      output[index + 1U] = x0 * sine + x1 * cosine;
    }
  }
  internal::validate_all_finite(output,
                                "apply_rope_with_backend output");
  return output;
}

std::vector<float> int4_matvec_neon(
    const QuantizedMatrix& matrix,
    const std::vector<float>& activation) {
  const int groups = internal::validate_quantized_matrix_storage(matrix);
  if (matrix.bits != QuantizationBits::kInt4) {
    throw std::invalid_argument(
        "fused_int4_dequant_matvec_with_backend requires INT4 storage");
  }
  if (activation.size() != static_cast<std::size_t>(matrix.cols)) {
    throw std::invalid_argument("activation size must equal matrix cols");
  }
  internal::validate_all_finite(activation, "activation");

  std::vector<float> output(static_cast<std::size_t>(matrix.rows), 0.0F);
  for (int row = 0; row < matrix.rows; ++row) {
    double sum = 0.0;
    int col = 0;
    while (col < matrix.cols) {
      const int group = col / matrix.group_size;
      const int group_begin = internal::checked_multiply_int(
          group, matrix.group_size, "INT4 group begin");
      const int group_width =
          std::min(matrix.group_size, matrix.cols - group_begin);
      const int group_end = internal::checked_add_int(
          group_begin, group_width, "INT4 group end");
      const auto global_index = internal::checked_flat_index(
          row, matrix.cols, col, "INT4 packed data");
      if (global_index % 2U == 0U && group_end - col >= 16) {
        const auto packed_index = global_index / 2U;
        const uint8x8_t packed =
            vld1_u8(matrix.int4_packed_data.data() + packed_index);
        const uint8x8_t low_nibbles = vand_u8(packed, vdup_n_u8(0x0FU));
        const uint8x8_t high_nibbles = vshr_n_u8(packed, 4);
        const int8x8_t low = vshr_n_s8(
            vshl_n_s8(vreinterpret_s8_u8(low_nibbles), 4), 4);
        const int8x8_t high = vshr_n_s8(
            vshl_n_s8(vreinterpret_s8_u8(high_nibbles), 4), 4);
        const int8x8x2_t values = vzip_s8(low, high);
        const float scale = matrix.scales[internal::checked_flat_index(
            row, groups, group, "INT4 scales")];
        const float32x4_t scale_vector = vdupq_n_f32(scale);

        const float32x4_t q0 =
            vmulq_f32(int8_low4_to_float(values.val[0]), scale_vector);
        const float32x4_t q1 =
            vmulq_f32(int8_high4_to_float(values.val[0]), scale_vector);
        const float32x4_t q2 =
            vmulq_f32(int8_low4_to_float(values.val[1]), scale_vector);
        const float32x4_t q3 =
            vmulq_f32(int8_high4_to_float(values.val[1]), scale_vector);
        // Match the scalar owner's multiplication range. A float32 vector
        // product can overflow even when the corresponding double product and
        // final reduced result are finite.
        sum += dot_product_four_as_double(q0, activation.data() + col);
        sum += dot_product_four_as_double(q1, activation.data() + col + 4);
        sum += dot_product_four_as_double(q2, activation.data() + col + 8);
        sum += dot_product_four_as_double(q3, activation.data() + col + 12);
        col += 16;
        continue;
      }

      sum += static_cast<double>(internal::dequantized_value_at(
                 matrix, groups, row, col, global_index)) *
             static_cast<double>(activation[static_cast<std::size_t>(col)]);
      ++col;
    }
    output[static_cast<std::size_t>(row)] = internal::checked_float_from_double(
        sum, "fused_int4_dequant_matvec_with_backend output");
  }
  return output;
}

#endif

}  // namespace

const char* edge_kernel_backend_name(const EdgeKernelBackend backend) noexcept {
  switch (backend) {
    case EdgeKernelBackend::kAuto:
      return "auto";
    case EdgeKernelBackend::kScalar:
      return "scalar";
    case EdgeKernelBackend::kArmNeon:
      return "arm_neon";
  }
  return "unknown";
}

EdgeIsaCapabilities detect_edge_isa_capabilities() {
  EdgeIsaCapabilities capabilities;
#if defined(__aarch64__)
  capabilities.aarch64_build = true;
#endif
  capabilities.neon_compiled = INFERLAB_EDGE_NEON_IMPLEMENTATION != 0;
  capabilities.sve_requested = INFERLAB_ENABLE_ARM_SVE_REQUESTED != 0;
  // The CMake switch is a reserved integration request. No SVE kernel exists
  // yet, so capability evidence must remain unavailable even when requested.
  capabilities.sve_compiled = false;

#if INFERLAB_EDGE_NEON_IMPLEMENTATION && defined(__linux__) && \
    defined(HWCAP_ASIMD)
  const unsigned long hardware_capabilities = getauxval(AT_HWCAP);
  capabilities.neon_runtime_available =
      (hardware_capabilities & HWCAP_ASIMD) != 0U;
#if defined(HWCAP_SVE)
  capabilities.sve_runtime_available =
      capabilities.sve_compiled &&
      (hardware_capabilities & HWCAP_SVE) != 0U;
#endif
  capabilities.runtime_probe_status = "linux_getauxval";
#elif INFERLAB_EDGE_NEON_IMPLEMENTATION && defined(__APPLE__) && \
    defined(__aarch64__)
  capabilities.neon_runtime_available = true;
  capabilities.runtime_probe_status = "apple_arm64_neon_baseline";
#elif INFERLAB_EDGE_NEON_IMPLEMENTATION
  capabilities.neon_runtime_available = false;
  capabilities.runtime_probe_status = "runtime_probe_unavailable_fail_closed";
#else
  capabilities.runtime_probe_status = "neon_not_compiled";
#endif
  return capabilities;
}

std::vector<float> rms_norm_with_backend(
    const std::vector<float>& matrix, const int rows, const int cols,
    const std::vector<float>& gamma, const float epsilon,
    const EdgeKernelBackend backend, EdgeKernelExecution* execution) {
  if (execution != nullptr) *execution = {};
  const auto selection = select_backend(backend);
  std::vector<float> output;
  if (selection.selected == EdgeKernelBackend::kScalar) {
    output = rms_norm_cpu(matrix, rows, cols, gamma, epsilon);
  } else {
#if INFERLAB_EDGE_NEON_IMPLEMENTATION
    output = rms_norm_neon(matrix, rows, cols, gamma, epsilon);
#else
    throw std::logic_error("selected ARM NEON implementation is not compiled");
#endif
  }
  assign_execution(backend, selection, execution);
  return output;
}

std::vector<float> apply_rope_with_backend(
    const std::vector<float>& input, const int seq_len, const int dim,
    const float base, const int position_offset,
    const EdgeKernelBackend backend, EdgeKernelExecution* execution) {
  if (execution != nullptr) *execution = {};
  const auto selection = select_backend(backend);
  std::vector<float> output;
  if (selection.selected == EdgeKernelBackend::kScalar) {
    output = apply_rope_cpu(input, seq_len, dim, base, position_offset);
  } else {
#if INFERLAB_EDGE_NEON_IMPLEMENTATION
    output = rope_neon(input, seq_len, dim, base, position_offset);
#else
    throw std::logic_error("selected ARM NEON implementation is not compiled");
#endif
  }
  assign_execution(backend, selection, execution);
  return output;
}

std::vector<float> fused_int4_dequant_matvec_with_backend(
    const QuantizedMatrix& matrix, const std::vector<float>& activation,
    const EdgeKernelBackend backend, EdgeKernelExecution* execution) {
  if (execution != nullptr) *execution = {};
  if (matrix.bits != QuantizationBits::kInt4) {
    throw std::invalid_argument(
        "fused_int4_dequant_matvec_with_backend requires INT4 storage");
  }
  const auto selection = select_backend(backend);
  std::vector<float> output;
  if (selection.selected == EdgeKernelBackend::kScalar) {
    output = fused_dequant_matvec(matrix, activation);
  } else {
#if INFERLAB_EDGE_NEON_IMPLEMENTATION
    output = int4_matvec_neon(matrix, activation);
#else
    throw std::logic_error("selected ARM NEON implementation is not compiled");
#endif
  }
  assign_execution(backend, selection, execution);
  return output;
}

}  // namespace inferlab
