#include "quantization_runtime.hpp"

#include <cuda_runtime.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

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

template <typename T>
std::size_t checked_byte_count(const std::size_t count, const char* context) {
  if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
    throw std::invalid_argument(std::string(context) +
                                " byte count overflows size_t");
  }
  return count * sizeof(T);
}

int ceil_div_int(const int value, const int divisor, const char* context) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument(std::string(context) +
                                " invalid ceil-div dimensions");
  }
  return value / divisor + (value % divisor == 0 ? 0 : 1);
}

template <typename T>
class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ > 0) {
      check_cuda(cudaMalloc(&data_, checked_byte_count<T>(count_, "CUDA quantization buffer")),
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

  T* data() { return data_; }
  const T* data() const { return data_; }
  std::size_t count() const { return count_; }

 private:
  T* data_ = nullptr;
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

template <typename T>
void copy_to_device(DeviceBuffer<T>& buffer, const std::vector<T>& values) {
  if (values.empty()) {
    return;
  }
  check_cuda(cudaMemcpy(buffer.data(), values.data(),
                        checked_byte_count<T>(values.size(), "CUDA quantization H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer<float>& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count<float>(values.size(), "CUDA quantization D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__device__ int sign_extend_4bit(const std::uint8_t value) {
  const std::uint8_t nibble = value & 0x0FU;
  return (nibble & 0x08U) != 0U ? static_cast<int>(nibble) - 16
                                : static_cast<int>(nibble);
}

__device__ int read_int4(const std::uint8_t* data, const int index) {
  const std::uint8_t packed = data[index / 2];
  if (index % 2 == 0) {
    return sign_extend_4bit(packed);
  }
  return sign_extend_4bit(static_cast<std::uint8_t>(packed >> 4U));
}

__global__ void fused_int4_matvec_kernel(const std::uint8_t* packed,
                                         const float* scales,
                                         const float* activation,
                                         float* output, const int rows,
                                         const int cols,
                                         const int group_size) {
  const int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= rows) {
    return;
  }
  const int groups_per_row =
      cols / group_size + (cols % group_size == 0 ? 0 : 1);
  float sum = 0.0F;
  for (int col = 0; col < cols; ++col) {
    const int index = row * cols + col;
    const int group = col / group_size;
    const float scale = scales[row * groups_per_row + group];
    sum += static_cast<float>(read_int4(packed, index)) * scale *
           activation[col];
  }
  output[row] = sum;
}

}  // namespace

std::vector<float> fused_dequant_matvec_cuda_runtime(
    const QuantizedMatrix& matrix, const std::vector<float>& activation) {
  return fused_dequant_matvec_cuda_runtime_profiled(matrix, activation).output;
}

CudaQuantizationResult fused_dequant_matvec_cuda_runtime_profiled(
    const QuantizedMatrix& matrix, const std::vector<float>& activation) {
  if (matrix.bits != QuantizationBits::kInt4) {
    throw std::invalid_argument(
        "fused_dequant_matvec_cuda_runtime_profiled currently supports INT4 "
        "only");
  }
  if (matrix.rows <= 0 || matrix.cols <= 0 || matrix.group_size <= 0) {
    throw std::invalid_argument("invalid quantized matrix shape");
  }
  if (activation.size() != static_cast<std::size_t>(matrix.cols)) {
    throw std::invalid_argument("activation size must equal matrix cols");
  }
  const int groups_per_row =
      ceil_div_int(matrix.cols, matrix.group_size,
                   "fused_dequant_matvec_cuda_runtime_profiled groups");
  const std::size_t element_count =
      checked_product(matrix.rows, matrix.cols,
                      "fused_dequant_matvec_cuda_runtime_profiled matrix");
  checked_int_count(element_count,
                    "fused_dequant_matvec_cuda_runtime_profiled matrix");
  const std::size_t scale_count =
      checked_product(matrix.rows, groups_per_row,
                      "fused_dequant_matvec_cuda_runtime_profiled scales");
  checked_int_count(scale_count,
                    "fused_dequant_matvec_cuda_runtime_profiled scales");
  const std::size_t packed_bytes =
      element_count / 2 + (element_count % 2 == 0 ? 0 : 1);
  if (matrix.scales.size() != scale_count) {
    throw std::invalid_argument("scale count must equal rows * ceil(cols/group)");
  }
  if (matrix.int4_packed_data.size() != packed_bytes) {
    throw std::invalid_argument(
        "packed INT4 data size must equal ceil(rows * cols / 2)");
  }

  DeviceBuffer<std::uint8_t> device_packed(matrix.int4_packed_data.size());
  DeviceBuffer<float> device_scales(matrix.scales.size());
  DeviceBuffer<float> device_activation(activation.size());
  DeviceBuffer<float> device_output(static_cast<std::size_t>(matrix.rows));
  copy_to_device(device_packed, matrix.int4_packed_data);
  copy_to_device(device_scales, matrix.scales);
  copy_to_device(device_activation, activation);

  constexpr int kBlockSize = 128;
  const int grid =
      ceil_div_int(matrix.rows, kBlockSize,
                   "fused_dequant_matvec_cuda_runtime_profiled launch");
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  fused_int4_matvec_kernel<<<grid, kBlockSize>>>(
      device_packed.data(), device_scales.data(), device_activation.data(),
      device_output.data(), matrix.rows, matrix.cols, matrix.group_size);
  check_cuda(cudaGetLastError(), "fused_int4_matvec_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaQuantizationResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
