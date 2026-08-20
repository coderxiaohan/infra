#include "attention_runtime.hpp"

#include <cuda_runtime.h>

#include <cmath>
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

std::size_t checked_product(const int a, const int b, const int c,
                            const char* context) {
  const std::size_t partial = checked_product(a, b, context);
  if (c <= 0) {
    throw std::invalid_argument(std::string(context) +
                                " dimensions must be positive");
  }
  const auto rhs = static_cast<std::size_t>(c);
  if (partial > std::numeric_limits<std::size_t>::max() / rhs) {
    throw std::invalid_argument(std::string(context) +
                                " element count overflows size_t");
  }
  return partial * rhs;
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

int ceil_div_int(const int value, const int divisor) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument("invalid CUDA attention ceil-div dimensions");
  }
  return value / divisor + (value % divisor == 0 ? 0 : 1);
}

class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ > 0) {
      check_cuda(cudaMalloc(&data_, checked_byte_count(count_, "CUDA attention buffer")),
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
                        checked_byte_count(values.size(), "CUDA attention H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count(values.size(), "CUDA attention D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__device__ int q_index(const int head, const int token, const int dim,
                       const int query_length, const int head_dim) {
  return (head * query_length + token) * head_dim + dim;
}

__device__ int kv_index(const int head, const int token, const int dim,
                        const int key_length, const int head_dim) {
  return (head * key_length + token) * head_dim + dim;
}

__global__ void attention_kernel(const float* query, const float* key,
                                 const float* value, float* output,
                                 const int num_heads,
                                 const int query_length,
                                 const int key_length, const int head_dim,
                                 const bool causal) {
  const int index = blockIdx.x * blockDim.x + threadIdx.x;
  const int total = num_heads * query_length * head_dim;
  if (index >= total) {
    return;
  }

  const int dim = index % head_dim;
  const int query_token = (index / head_dim) % query_length;
  const int head = index / (query_length * head_dim);
  const float scale = rsqrtf(static_cast<float>(head_dim));

  float max_score = -INFINITY;
  for (int key_token = 0; key_token < key_length; ++key_token) {
    if (causal && key_token > query_token) {
      continue;
    }
    float dot = 0.0F;
    for (int inner = 0; inner < head_dim; ++inner) {
      dot += query[q_index(head, query_token, inner, query_length, head_dim)] *
             key[kv_index(head, key_token, inner, key_length, head_dim)];
    }
    max_score = fmaxf(max_score, dot * scale);
  }

  float denominator = 0.0F;
  float weighted = 0.0F;
  for (int key_token = 0; key_token < key_length; ++key_token) {
    if (causal && key_token > query_token) {
      continue;
    }
    float dot = 0.0F;
    for (int inner = 0; inner < head_dim; ++inner) {
      dot += query[q_index(head, query_token, inner, query_length, head_dim)] *
             key[kv_index(head, key_token, inner, key_length, head_dim)];
    }
    const float probability_numerator = expf(dot * scale - max_score);
    denominator += probability_numerator;
    weighted += probability_numerator *
                value[kv_index(head, key_token, dim, key_length, head_dim)];
  }
  output[index] = weighted / denominator;
}

__global__ void attention_shared_denominator_kernel(
    const float* query, const float* key, const float* value, float* output,
    const int num_heads, const int query_length, const int key_length,
    const int head_dim, const bool causal) {
  const int query_index = blockIdx.x;
  const int head = query_index / query_length;
  const int query_token = query_index % query_length;
  if (head >= num_heads) {
    return;
  }

  __shared__ float shared_max_score;
  __shared__ float shared_denominator;
  const float scale = rsqrtf(static_cast<float>(head_dim));
  if (threadIdx.x == 0) {
    float max_score = -INFINITY;
    for (int key_token = 0; key_token < key_length; ++key_token) {
      if (causal && key_token > query_token) {
        continue;
      }
      float dot = 0.0F;
      for (int inner = 0; inner < head_dim; ++inner) {
        dot += query[q_index(head, query_token, inner, query_length,
                             head_dim)] *
               key[kv_index(head, key_token, inner, key_length, head_dim)];
      }
      max_score = fmaxf(max_score, dot * scale);
    }

    float denominator = 0.0F;
    for (int key_token = 0; key_token < key_length; ++key_token) {
      if (causal && key_token > query_token) {
        continue;
      }
      float dot = 0.0F;
      for (int inner = 0; inner < head_dim; ++inner) {
        dot += query[q_index(head, query_token, inner, query_length,
                             head_dim)] *
               key[kv_index(head, key_token, inner, key_length, head_dim)];
      }
      denominator += expf(dot * scale - max_score);
    }
    shared_max_score = max_score;
    shared_denominator = denominator;
  }
  __syncthreads();

  for (int dim = threadIdx.x; dim < head_dim; dim += blockDim.x) {
    float weighted = 0.0F;
    for (int key_token = 0; key_token < key_length; ++key_token) {
      if (causal && key_token > query_token) {
        continue;
      }
      float dot = 0.0F;
      for (int inner = 0; inner < head_dim; ++inner) {
        dot += query[q_index(head, query_token, inner, query_length,
                             head_dim)] *
               key[kv_index(head, key_token, inner, key_length, head_dim)];
      }
      const float probability_numerator =
          expf(dot * scale - shared_max_score);
      weighted += probability_numerator *
                  value[kv_index(head, key_token, dim, key_length, head_dim)];
    }
    output[q_index(head, query_token, dim, query_length, head_dim)] =
        weighted / shared_denominator;
  }
}

void validate_inputs(const std::vector<float>& query,
                     const std::vector<float>& key,
                     const std::vector<float>& value,
                     const AttentionShape& shape, const bool causal) {
  if (shape.num_heads <= 0 || shape.query_length <= 0 ||
      shape.key_length <= 0 || shape.head_dim <= 0) {
    throw std::invalid_argument("CUDA attention dimensions must be positive");
  }
  if (causal && shape.query_length != shape.key_length) {
    throw std::invalid_argument(
        "CUDA causal attention requires query_length == key_length");
  }
  const std::size_t query_elements =
      checked_product(shape.num_heads, shape.query_length, shape.head_dim,
                      "CUDA attention query");
  const std::size_t kv_elements =
      checked_product(shape.num_heads, shape.key_length, shape.head_dim,
                      "CUDA attention key/value");
  checked_int_count(query_elements, "CUDA attention query");
  checked_int_count(kv_elements, "CUDA attention key/value");
  checked_int_count(checked_product(shape.num_heads, shape.query_length,
                                    "CUDA attention shared grid"),
                    "CUDA attention shared grid");
  if (query.size() != query_elements) {
    throw std::invalid_argument("CUDA attention query size mismatch");
  }
  if (key.size() != kv_elements) {
    throw std::invalid_argument("CUDA attention key size mismatch");
  }
  if (value.size() != kv_elements) {
    throw std::invalid_argument("CUDA attention value size mismatch");
  }
}

}  // namespace

std::vector<float> scaled_dot_product_attention_cuda_runtime(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape,
    const bool causal) {
  return scaled_dot_product_attention_cuda_runtime_profiled(
             query, key, value, shape, causal)
      .output;
}

CudaAttentionResult scaled_dot_product_attention_cuda_runtime_profiled(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape,
    const bool causal) {
  validate_inputs(query, key, value, shape, causal);

  DeviceBuffer device_query(query.size());
  DeviceBuffer device_key(key.size());
  DeviceBuffer device_value(value.size());
  DeviceBuffer device_output(query.size());
  copy_to_device(device_query, query);
  copy_to_device(device_key, key);
  copy_to_device(device_value, value);

  constexpr int kBlockSize = 128;
  const int total = checked_int_count(
      checked_product(shape.num_heads, shape.query_length, shape.head_dim,
                      "CUDA attention launch"),
      "CUDA attention launch");
  const int grid = ceil_div_int(total, kBlockSize);
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  attention_kernel<<<grid, kBlockSize>>>(
      device_query.data(), device_key.data(), device_value.data(),
      device_output.data(), shape.num_heads, shape.query_length,
      shape.key_length, shape.head_dim, causal);
  check_cuda(cudaGetLastError(), "attention_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaAttentionResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> scaled_dot_product_attention_shared_cuda_runtime(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape,
    const bool causal) {
  return scaled_dot_product_attention_shared_cuda_runtime_profiled(
             query, key, value, shape, causal)
      .output;
}

CudaAttentionResult scaled_dot_product_attention_shared_cuda_runtime_profiled(
    const std::vector<float>& query, const std::vector<float>& key,
    const std::vector<float>& value, const AttentionShape& shape,
    const bool causal) {
  validate_inputs(query, key, value, shape, causal);

  DeviceBuffer device_query(query.size());
  DeviceBuffer device_key(key.size());
  DeviceBuffer device_value(value.size());
  DeviceBuffer device_output(query.size());
  copy_to_device(device_query, query);
  copy_to_device(device_key, key);
  copy_to_device(device_value, value);

  constexpr int kBlockSize = 128;
  const int grid = checked_int_count(
      checked_product(shape.num_heads, shape.query_length,
                      "CUDA shared attention launch"),
      "CUDA shared attention launch");
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  attention_shared_denominator_kernel<<<grid, kBlockSize>>>(
      device_query.data(), device_key.data(), device_value.data(),
      device_output.data(), shape.num_heads, shape.query_length,
      shape.key_length, shape.head_dim, causal);
  check_cuda(cudaGetLastError(),
             "attention_shared_denominator_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaAttentionResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
