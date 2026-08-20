#include "paged_kv_runtime.hpp"

#include <cuda_runtime.h>

#include <algorithm>
#include <cstddef>
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
  if (a < 0 || b < 0) {
    throw std::invalid_argument(std::string(context) +
                                " dimensions must be non-negative");
  }
  const auto lhs = static_cast<std::size_t>(a);
  const auto rhs = static_cast<std::size_t>(b);
  if (rhs != 0 && lhs > std::numeric_limits<std::size_t>::max() / rhs) {
    throw std::invalid_argument(std::string(context) +
                                " element count overflows size_t");
  }
  return lhs * rhs;
}

std::size_t checked_product(const int a, const int b, const int c,
                            const char* context) {
  const std::size_t partial = checked_product(a, b, context);
  if (c < 0) {
    throw std::invalid_argument(std::string(context) +
                                " dimensions must be non-negative");
  }
  const auto rhs = static_cast<std::size_t>(c);
  if (rhs != 0 && partial > std::numeric_limits<std::size_t>::max() / rhs) {
    throw std::invalid_argument(std::string(context) +
                                " element count overflows size_t");
  }
  return partial * rhs;
}

std::size_t checked_product_size(const std::size_t lhs,
                                 const std::size_t rhs,
                                 const char* context) {
  if (rhs != 0 && lhs > std::numeric_limits<std::size_t>::max() / rhs) {
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

std::size_t ceil_div_size(const int value, const int divisor,
                          const char* context) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument(std::string(context) +
                                " invalid ceil-div dimensions");
  }
  return (static_cast<std::size_t>(value) +
          static_cast<std::size_t>(divisor) - 1U) /
         static_cast<std::size_t>(divisor);
}

int ceil_div_int(const int value, const int divisor) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument("invalid CUDA paged KV ceil-div dimensions");
  }
  return value / divisor + (value % divisor == 0 ? 0 : 1);
}

template <typename T>
class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ > 0) {
      check_cuda(cudaMalloc(&data_, checked_byte_count<T>(count_, "CUDA paged KV buffer")),
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
                        checked_byte_count<T>(values.size(), "CUDA paged KV H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer<float>& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count<float>(values.size(), "CUDA paged KV D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__device__ int physical_index(const int block_id, const int head,
                              const int block_offset, const int dim,
                              const int num_heads,
                              const int block_size_tokens,
                              const int head_dim) {
  return ((block_id * num_heads + head) * block_size_tokens + block_offset) *
             head_dim +
         dim;
}

__device__ int logical_index(const int head, const int token, const int dim,
                             const int token_count, const int head_dim) {
  return (head * token_count + token) * head_dim + dim;
}

__global__ void gather_paged_kv_kernel(const float* physical_cache,
                                       const int* block_table, float* output,
                                       const int token_count,
                                       const int num_heads,
                                       const int block_size_tokens,
                                       const int head_dim) {
  const int index = blockIdx.x * blockDim.x + threadIdx.x;
  const int total = num_heads * token_count * head_dim;
  if (index >= total) {
    return;
  }

  const int dim = index % head_dim;
  const int token = (index / head_dim) % token_count;
  const int head = index / (token_count * head_dim);
  const int logical_block = token / block_size_tokens;
  const int block_offset = token % block_size_tokens;
  const int block_id = block_table[logical_block];
  output[logical_index(head, token, dim, token_count, head_dim)] =
      physical_cache[physical_index(block_id, head, block_offset, dim,
                                    num_heads, block_size_tokens, head_dim)];
}

void validate_inputs(const std::vector<float>& physical_cache,
                     const RequestBlocks& blocks, const PagedKVShape& shape) {
  if (shape.num_heads <= 0 || shape.head_dim <= 0 ||
      shape.block_size_tokens <= 0) {
    throw std::invalid_argument("CUDA paged KV shape dimensions must be positive");
  }
  if (blocks.token_count < 0) {
    throw std::invalid_argument("CUDA paged KV token count must be non-negative");
  }
  const std::size_t required_blocks =
      ceil_div_size(blocks.token_count, shape.block_size_tokens,
                    "CUDA paged KV required blocks");
  if (required_blocks > blocks.block_table.size()) {
    throw std::invalid_argument("CUDA paged KV block table missing logical block");
  }
  if (required_blocks == 0) {
    return;
  }
  const auto max_block =
      *std::max_element(blocks.block_table.begin(),
                        blocks.block_table.begin() +
                            static_cast<std::ptrdiff_t>(required_blocks));
  if (max_block < 0) {
    throw std::invalid_argument("CUDA paged KV block id must be non-negative");
  }
  const auto block_elements =
      checked_product(shape.num_heads, shape.block_size_tokens, shape.head_dim,
                      "CUDA paged KV physical block");
  const auto required_elements =
      checked_product_size(static_cast<std::size_t>(max_block) + 1U,
                           block_elements,
                           "CUDA paged KV physical cache");
  checked_int_count(required_elements, "CUDA paged KV physical cache");
  if (required_elements > physical_cache.size()) {
    throw std::invalid_argument("CUDA paged KV physical cache too small");
  }
}

}  // namespace

std::vector<float> gather_paged_kv_cache_cuda_runtime(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape) {
  return gather_paged_kv_cache_cuda_runtime_profiled(physical_cache, blocks,
                                                     shape)
      .output;
}

CudaPagedKVResult gather_paged_kv_cache_cuda_runtime_profiled(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape) {
  validate_inputs(physical_cache, blocks, shape);
  const std::size_t required_blocks =
      ceil_div_size(blocks.token_count, shape.block_size_tokens,
                    "CUDA paged KV required blocks");
  const auto output_count =
      checked_product(shape.num_heads, blocks.token_count, shape.head_dim,
                      "CUDA paged KV output");
  if (output_count == 0) {
    return CudaPagedKVResult{};
  }
  const int total = checked_int_count(output_count, "CUDA paged KV output");

  std::vector<int> block_table;
  block_table.reserve(required_blocks);
  for (std::size_t i = 0; i < required_blocks; ++i) {
    block_table.push_back(blocks.block_table[i]);
  }

  DeviceBuffer<float> device_physical(physical_cache.size());
  DeviceBuffer<int> device_block_table(block_table.size());
  DeviceBuffer<float> device_output(output_count);
  copy_to_device(device_physical, physical_cache);
  copy_to_device(device_block_table, block_table);

  constexpr int kBlockSize = 128;
  const int grid = ceil_div_int(total, kBlockSize);
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  gather_paged_kv_kernel<<<grid, kBlockSize>>>(
      device_physical.data(), device_block_table.data(), device_output.data(),
      blocks.token_count, shape.num_heads, shape.block_size_tokens,
      shape.head_dim);
  check_cuda(cudaGetLastError(), "gather_paged_kv_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaPagedKVResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
