#include "rope_runtime.hpp"

#include <cuda_runtime.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

bool finite_positive(const float value) {
  return std::isfinite(value) && value > 0.0F;
}

void check_cuda(const cudaError_t status, const char* operation) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string(operation) + ": " +
                             cudaGetErrorString(status));
  }
}

std::size_t checked_product(const int a, const int b, const char* context) {
  if (a <= 0 || b <= 0) {
    throw std::invalid_argument(std::string(context) +
                                " dimensions must be positive");
  }
  const auto lhs = static_cast<std::size_t>(a);
  const auto rhs = static_cast<std::size_t>(b);
  if (lhs > std::numeric_limits<std::size_t>::max() / rhs) {
    throw std::invalid_argument(std::string(context) +
                                " element count overflows size_t");
  }
  return lhs * rhs;
}

int checked_int_count(const std::size_t count, const char* context) {
  if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(context) +
                                " exceeds int-indexed CUDA kernel limit");
  }
  return static_cast<int>(count);
}

std::size_t checked_byte_count(const std::size_t count, const char* context) {
  if (count > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
    throw std::invalid_argument(std::string(context) +
                                " byte count overflows size_t");
  }
  return count * sizeof(float);
}

int ceil_div_int(const int value, const int divisor, const char* context) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument(std::string(context) +
                                " invalid ceil-div dimensions");
  }
  return value / divisor + (value % divisor == 0 ? 0 : 1);
}

class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ > 0) {
      check_cuda(cudaMalloc(&data_, checked_byte_count(count_, "CUDA RoPE buffer")),
                 "cudaMalloc");
    }
  }

  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

  ~DeviceBuffer() {
    if (data_ != nullptr) {
      (void)cudaFree(data_);
    }
  }

  float* data() { return data_; }
  const float* data() const { return data_; }
  std::size_t count() const { return count_; }

 private:
  float* data_ = nullptr;
  std::size_t count_ = 0;
};

class CudaEvent {
 public:
  CudaEvent() { check_cuda(cudaEventCreate(&event_), "cudaEventCreate"); }

  CudaEvent(const CudaEvent&) = delete;
  CudaEvent& operator=(const CudaEvent&) = delete;

  ~CudaEvent() {
    if (event_ != nullptr) {
      (void)cudaEventDestroy(event_);
    }
  }

  cudaEvent_t get() const { return event_; }

 private:
  cudaEvent_t event_ = nullptr;
};

void copy_to_device(DeviceBuffer& buffer, const std::vector<float>& values) {
  if (values.empty()) {
    return;
  }
  check_cuda(cudaMemcpy(buffer.data(), values.data(),
                        checked_byte_count(values.size(), "CUDA RoPE H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count(values.size(), "CUDA RoPE D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__global__ void rope_kernel(const float* input, float* output,
                            const int seq_len, const int dim,
                            const float base, const int position_offset) {
  const int pair_index = blockIdx.x * blockDim.x + threadIdx.x;
  const int pairs_per_token = dim / 2;
  const int total_pairs = seq_len * pairs_per_token;
  if (pair_index >= total_pairs) {
    return;
  }

  const int position = pair_index / pairs_per_token;
  const int pair = pair_index % pairs_per_token;
  const int i = pair * 2;
  const float frequency =
      1.0F / powf(base, static_cast<float>(i) / static_cast<float>(dim));
  const float angle = static_cast<float>(position + position_offset) * frequency;
  const float cos_value = cosf(angle);
  const float sin_value = sinf(angle);

  const int index0 = position * dim + i;
  const int index1 = index0 + 1;
  const float x0 = input[index0];
  const float x1 = input[index1];
  output[index0] = x0 * cos_value - x1 * sin_value;
  output[index1] = x0 * sin_value + x1 * cos_value;
}

}  // namespace

std::vector<float> apply_rope_cuda_runtime(const std::vector<float>& input,
                                           const int seq_len, const int dim,
                                           const float base,
                                           const int position_offset) {
  return apply_rope_cuda_runtime_profiled(input, seq_len, dim, base,
                                          position_offset)
      .output;
}

CudaRopeResult apply_rope_cuda_runtime_profiled(
    const std::vector<float>& input, const int seq_len, const int dim,
    const float base, const int position_offset) {
  if (seq_len <= 0 || dim <= 0 || dim % 2 != 0) {
    throw std::invalid_argument(
        "apply_rope_cuda_runtime_profiled requires even dim");
  }
  if (!finite_positive(base)) {
    throw std::invalid_argument(
        "apply_rope_cuda_runtime_profiled base must be finite and positive");
  }
  if (position_offset < 0) {
    throw std::invalid_argument(
        "apply_rope_cuda_runtime_profiled position_offset must be non-negative");
  }
  if (seq_len > std::numeric_limits<int>::max() - position_offset) {
    throw std::invalid_argument(
        "apply_rope_cuda_runtime_profiled position range exceeds int limit");
  }
  const std::size_t input_elements =
      checked_product(seq_len, dim, "apply_rope_cuda_runtime_profiled input");
  checked_int_count(input_elements,
                    "apply_rope_cuda_runtime_profiled input");
  if (input.size() != input_elements) {
    throw std::invalid_argument(
        "apply_rope_cuda_runtime_profiled input size mismatch");
  }
  const int pairs_per_token = dim / 2;
  const int total_pairs = checked_int_count(
      checked_product(seq_len, pairs_per_token,
                      "apply_rope_cuda_runtime_profiled total pairs"),
      "apply_rope_cuda_runtime_profiled total pairs");

  DeviceBuffer device_input(input.size());
  DeviceBuffer device_output(input.size());
  copy_to_device(device_input, input);

  constexpr int kBlockSize = 256;
  const int grid =
      ceil_div_int(total_pairs, kBlockSize,
                   "apply_rope_cuda_runtime_profiled launch grid");
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  rope_kernel<<<grid, kBlockSize>>>(device_input.data(), device_output.data(),
                                   seq_len, dim, base, position_offset);
  check_cuda(cudaGetLastError(), "rope_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaRopeResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
