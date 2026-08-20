#pragma once

#include "inferlab/quantization.hpp"

#include <string>
#include <vector>

namespace inferlab {

enum class EdgeKernelBackend {
  kAuto,
  kScalar,
  kArmNeon,
};

const char* edge_kernel_backend_name(EdgeKernelBackend backend) noexcept;

struct EdgeIsaCapabilities {
  bool aarch64_build = false;
  bool neon_compiled = false;
  bool neon_runtime_available = false;
  bool sve_requested = false;
  bool sve_compiled = false;
  bool sve_runtime_available = false;
  std::string runtime_probe_status;
};

struct EdgeKernelExecution {
  EdgeKernelBackend requested = EdgeKernelBackend::kAuto;
  EdgeKernelBackend selected = EdgeKernelBackend::kScalar;
  bool fallback_used = false;
  std::string selection_reason;
};

EdgeIsaCapabilities detect_edge_isa_capabilities();

std::vector<float> rms_norm_with_backend(
    const std::vector<float>& matrix, int rows, int cols,
    const std::vector<float>& gamma, float epsilon,
    EdgeKernelBackend backend = EdgeKernelBackend::kAuto,
    EdgeKernelExecution* execution = nullptr);

std::vector<float> apply_rope_with_backend(
    const std::vector<float>& input, int seq_len, int dim, float base,
    int position_offset = 0,
    EdgeKernelBackend backend = EdgeKernelBackend::kAuto,
    EdgeKernelExecution* execution = nullptr);

std::vector<float> fused_int4_dequant_matvec_with_backend(
    const QuantizedMatrix& matrix, const std::vector<float>& activation,
    EdgeKernelBackend backend = EdgeKernelBackend::kAuto,
    EdgeKernelExecution* execution = nullptr);

}  // namespace inferlab
