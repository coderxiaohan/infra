#include "vector_add_runtime.hpp"

#include <cuda_runtime.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" void inferlab_vector_add_cuda(const float* a, const float* b,
                                         float* out, int n,
                                         cudaStream_t stream);

namespace inferlab {
namespace {

void check_cuda(const cudaError_t status, const char* operation) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string(operation) + ": " +
                             cudaGetErrorString(status));
  }
}

std::size_t checked_byte_count(const std::size_t count, const char* context) {
  if (count > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
    throw std::invalid_argument(std::string(context) +
                                " byte count overflows size_t");
  }
  return count * sizeof(float);
}

int checked_int_count(const std::size_t count, const char* context) {
  if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(context) +
                                " exceeds int-indexed CUDA kernel limit");
  }
  return static_cast<int>(count);
}

class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ == 0) {
      return;
    }
    check_cuda(cudaMalloc(&data_, checked_byte_count(count_, "CUDA vector add")),
               "cudaMalloc");
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
                        checked_byte_count(values.size(), "CUDA vector add H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count(values.size(), "CUDA vector add D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

}  // namespace

std::vector<float> vector_add_cuda_runtime(const std::vector<float>& a,
                                           const std::vector<float>& b) {
  return vector_add_cuda_runtime_profiled(a, b).output;
}

CudaVectorAddResult vector_add_cuda_runtime_profiled(
    const std::vector<float>& a, const std::vector<float>& b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument(
        "vector_add_cuda_runtime inputs must have equal sizes");
  }
  if (a.empty()) {
    return CudaVectorAddResult{};
  }
  const int element_count = checked_int_count(a.size(), "CUDA vector add input");

  DeviceBuffer device_a(a.size());
  DeviceBuffer device_b(b.size());
  DeviceBuffer device_out(a.size());
  copy_to_device(device_a, a);
  copy_to_device(device_b, b);

  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  inferlab_vector_add_cuda(device_a.data(), device_b.data(), device_out.data(),
                           element_count, nullptr);
  check_cuda(cudaGetLastError(), "inferlab_vector_add_cuda launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaVectorAddResult{copy_to_host(device_out), elapsed_ms};
}

}  // namespace inferlab
