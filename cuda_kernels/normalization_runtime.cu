#include "normalization_runtime.hpp"

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
      check_cuda(cudaMalloc(&data_, checked_byte_count(count_, "CUDA normalization buffer")),
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
                        checked_byte_count(values.size(), "CUDA normalization H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count(values.size(), "CUDA normalization D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__global__ void rms_norm_kernel(const float* matrix, const float* gamma,
                                float* output, const int rows, const int cols,
                                const float epsilon) {
  const int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= rows) {
    return;
  }
  const int row_offset = row * cols;
  float mean_square = 0.0F;
  for (int col = 0; col < cols; ++col) {
    const float value = matrix[row_offset + col];
    mean_square += value * value;
  }
  mean_square /= static_cast<float>(cols);
  const float inv_rms = rsqrtf(mean_square + epsilon);
  for (int col = 0; col < cols; ++col) {
    output[row_offset + col] =
        matrix[row_offset + col] * inv_rms * gamma[col];
  }
}

}  // namespace

std::vector<float> rms_norm_cuda_runtime(const std::vector<float>& matrix,
                                         const int rows, const int cols,
                                         const std::vector<float>& gamma,
                                         const float epsilon) {
  return rms_norm_cuda_runtime_profiled(matrix, rows, cols, gamma, epsilon)
      .output;
}

CudaNormalizationResult rms_norm_cuda_runtime_profiled(
    const std::vector<float>& matrix, const int rows, const int cols,
    const std::vector<float>& gamma, const float epsilon) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(
        "rms_norm_cuda_runtime_profiled dimensions must be positive");
  }
  const std::size_t matrix_elements =
      checked_product(rows, cols, "rms_norm_cuda_runtime_profiled matrix");
  checked_int_count(matrix_elements, "rms_norm_cuda_runtime_profiled matrix");
  if (matrix.size() != matrix_elements) {
    throw std::invalid_argument(
        "rms_norm_cuda_runtime_profiled matrix size mismatch");
  }
  if (gamma.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument(
        "rms_norm_cuda_runtime_profiled gamma size mismatch");
  }
  if (!finite_positive(epsilon)) {
    throw std::invalid_argument(
        "rms_norm_cuda_runtime_profiled epsilon must be finite and positive");
  }

  DeviceBuffer device_matrix(matrix.size());
  DeviceBuffer device_gamma(gamma.size());
  DeviceBuffer device_output(matrix.size());
  copy_to_device(device_matrix, matrix);
  copy_to_device(device_gamma, gamma);

  constexpr int kBlockSize = 128;
  const int grid =
      ceil_div_int(rows, kBlockSize, "rms_norm_cuda_runtime_profiled launch");
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  rms_norm_kernel<<<grid, kBlockSize>>>(device_matrix.data(),
                                       device_gamma.data(),
                                       device_output.data(), rows, cols,
                                       epsilon);
  check_cuda(cudaGetLastError(), "rms_norm_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaNormalizationResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
