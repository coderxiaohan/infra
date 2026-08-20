#include "matrix_ops_runtime.hpp"

#include <cuda_runtime.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

constexpr int kReductionBlockSize = 256;
constexpr int kMatmulTileSize = 16;

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

int ceil_div_int(const int value, const int divisor) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument("invalid CUDA matrix ceil-div dimensions");
  }
  return value / divisor + (value % divisor == 0 ? 0 : 1);
}

class DeviceBuffer {
 public:
  explicit DeviceBuffer(const std::size_t count) : count_(count) {
    if (count_ > 0) {
      check_cuda(cudaMalloc(&data_, checked_byte_count(count_, "CUDA matrix buffer")),
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
                        checked_byte_count(values.size(), "CUDA matrix H2D"),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");
}

std::vector<float> copy_to_host(const DeviceBuffer& buffer) {
  std::vector<float> values(buffer.count(), 0.0F);
  if (values.empty()) {
    return values;
  }
  check_cuda(cudaMemcpy(values.data(), buffer.data(),
                        checked_byte_count(values.size(), "CUDA matrix D2H"),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  return values;
}

__global__ void matvec_kernel(const float* matrix, const float* vector,
                              float* output, const int rows, const int cols) {
  const int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= rows) {
    return;
  }
  float sum = 0.0F;
  for (int col = 0; col < cols; ++col) {
    sum += matrix[row * cols + col] * vector[col];
  }
  output[row] = sum;
}

__global__ void matvec_shared_kernel(const float* matrix, const float* vector,
                                     float* output, const int rows,
                                     const int cols) {
  const int row = blockIdx.x;
  const int thread = threadIdx.x;
  if (row >= rows) {
    return;
  }

  __shared__ float partials[kReductionBlockSize];
  float sum = 0.0F;
  for (int col = thread; col < cols; col += blockDim.x) {
    sum += matrix[row * cols + col] * vector[col];
  }
  partials[thread] = sum;
  __syncthreads();

  for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
    if (thread < stride) {
      partials[thread] += partials[thread + stride];
    }
    __syncthreads();
  }
  if (thread == 0) {
    output[row] = partials[0];
  }
}

__global__ void matmul_kernel(const float* a, const float* b, float* output,
                              const int m, const int k, const int n) {
  const int index = blockIdx.x * blockDim.x + threadIdx.x;
  const int total = m * n;
  if (index >= total) {
    return;
  }
  const int row = index / n;
  const int col = index % n;
  float sum = 0.0F;
  for (int inner = 0; inner < k; ++inner) {
    sum += a[row * k + inner] * b[inner * n + col];
  }
  output[index] = sum;
}

__global__ void matmul_tiled_kernel(const float* a, const float* b,
                                    float* output, const int m, const int k,
                                    const int n) {
  __shared__ float tile_a[kMatmulTileSize][kMatmulTileSize];
  __shared__ float tile_b[kMatmulTileSize][kMatmulTileSize];

  const int row = blockIdx.y * kMatmulTileSize + threadIdx.y;
  const int col = blockIdx.x * kMatmulTileSize + threadIdx.x;
  float sum = 0.0F;

  for (int tile = 0; tile < k; tile += kMatmulTileSize) {
    const int a_col = tile + threadIdx.x;
    const int b_row = tile + threadIdx.y;
    tile_a[threadIdx.y][threadIdx.x] =
        (row < m && a_col < k) ? a[row * k + a_col] : 0.0F;
    tile_b[threadIdx.y][threadIdx.x] =
        (b_row < k && col < n) ? b[b_row * n + col] : 0.0F;
    __syncthreads();

    for (int inner = 0; inner < kMatmulTileSize; ++inner) {
      sum += tile_a[threadIdx.y][inner] * tile_b[inner][threadIdx.x];
    }
    __syncthreads();
  }

  if (row < m && col < n) {
    output[row * n + col] = sum;
  }
}

__global__ void row_softmax_kernel(const float* matrix, float* output,
                                   const int rows, const int cols) {
  const int row = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= rows) {
    return;
  }
  const int row_offset = row * cols;
  float row_max = matrix[row_offset];
  for (int col = 1; col < cols; ++col) {
    row_max = fmaxf(row_max, matrix[row_offset + col]);
  }
  float row_sum = 0.0F;
  for (int col = 0; col < cols; ++col) {
    const float value = expf(matrix[row_offset + col] - row_max);
    output[row_offset + col] = value;
    row_sum += value;
  }
  for (int col = 0; col < cols; ++col) {
    output[row_offset + col] /= row_sum;
  }
}

__global__ void row_softmax_block_kernel(const float* matrix, float* output,
                                         const int rows, const int cols) {
  const int row = blockIdx.x;
  const int thread = threadIdx.x;
  if (row >= rows) {
    return;
  }

  __shared__ float scratch[kReductionBlockSize];
  const int row_offset = row * cols;
  float local_max = -INFINITY;
  for (int col = thread; col < cols; col += blockDim.x) {
    local_max = fmaxf(local_max, matrix[row_offset + col]);
  }
  scratch[thread] = local_max;
  __syncthreads();
  for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
    if (thread < stride) {
      scratch[thread] = fmaxf(scratch[thread], scratch[thread + stride]);
    }
    __syncthreads();
  }
  const float row_max = scratch[0];

  float local_sum = 0.0F;
  for (int col = thread; col < cols; col += blockDim.x) {
    const float value = expf(matrix[row_offset + col] - row_max);
    output[row_offset + col] = value;
    local_sum += value;
  }
  scratch[thread] = local_sum;
  __syncthreads();
  for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
    if (thread < stride) {
      scratch[thread] += scratch[thread + stride];
    }
    __syncthreads();
  }
  const float row_sum = scratch[0];
  for (int col = thread; col < cols; col += blockDim.x) {
    output[row_offset + col] /= row_sum;
  }
}

}  // namespace

std::vector<float> matvec_cuda_runtime(const std::vector<float>& matrix,
                                       const int rows, const int cols,
                                       const std::vector<float>& vector) {
  return matvec_cuda_runtime_profiled(matrix, rows, cols, vector).output;
}

CudaMatrixOpResult matvec_cuda_runtime_profiled(
    const std::vector<float>& matrix, const int rows, const int cols,
    const std::vector<float>& vector) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument("matvec_cuda_runtime dimensions must be positive");
  }
  const std::size_t matrix_elements =
      checked_product(rows, cols, "matvec_cuda_runtime matrix");
  checked_int_count(matrix_elements, "matvec_cuda_runtime matrix");
  if (matrix.size() != matrix_elements) {
    throw std::invalid_argument("matvec_cuda_runtime matrix size mismatch");
  }
  if (vector.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument("matvec_cuda_runtime vector size mismatch");
  }

  DeviceBuffer device_matrix(matrix.size());
  DeviceBuffer device_vector(vector.size());
  DeviceBuffer device_output(static_cast<std::size_t>(rows));
  copy_to_device(device_matrix, matrix);
  copy_to_device(device_vector, vector);

  constexpr int kBlockSize = 256;
  const int grid = ceil_div_int(rows, kBlockSize);
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  matvec_kernel<<<grid, kBlockSize>>>(device_matrix.data(), device_vector.data(),
                                     device_output.data(), rows, cols);
  check_cuda(cudaGetLastError(), "matvec_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> matvec_shared_cuda_runtime(
    const std::vector<float>& matrix, const int rows, const int cols,
    const std::vector<float>& vector) {
  return matvec_shared_cuda_runtime_profiled(matrix, rows, cols, vector).output;
}

CudaMatrixOpResult matvec_shared_cuda_runtime_profiled(
    const std::vector<float>& matrix, const int rows, const int cols,
    const std::vector<float>& vector) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(
        "matvec_shared_cuda_runtime dimensions must be positive");
  }
  const std::size_t matrix_elements =
      checked_product(rows, cols, "matvec_shared_cuda_runtime matrix");
  checked_int_count(matrix_elements, "matvec_shared_cuda_runtime matrix");
  if (matrix.size() != matrix_elements) {
    throw std::invalid_argument(
        "matvec_shared_cuda_runtime matrix size mismatch");
  }
  if (vector.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument(
        "matvec_shared_cuda_runtime vector size mismatch");
  }

  DeviceBuffer device_matrix(matrix.size());
  DeviceBuffer device_vector(vector.size());
  DeviceBuffer device_output(static_cast<std::size_t>(rows));
  copy_to_device(device_matrix, matrix);
  copy_to_device(device_vector, vector);

  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  matvec_shared_kernel<<<rows, kReductionBlockSize>>>(
      device_matrix.data(), device_vector.data(), device_output.data(), rows,
      cols);
  check_cuda(cudaGetLastError(), "matvec_shared_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> matmul_cuda_runtime(const std::vector<float>& a, const int m,
                                       const int k,
                                       const std::vector<float>& b,
                                       const int n) {
  return matmul_cuda_runtime_profiled(a, m, k, b, n).output;
}

CudaMatrixOpResult matmul_cuda_runtime_profiled(
    const std::vector<float>& a, const int m, const int k,
    const std::vector<float>& b, const int n) {
  if (m <= 0 || k <= 0 || n <= 0) {
    throw std::invalid_argument("matmul_cuda_runtime dimensions must be positive");
  }
  const std::size_t lhs_elements =
      checked_product(m, k, "matmul_cuda_runtime lhs");
  const std::size_t rhs_elements =
      checked_product(k, n, "matmul_cuda_runtime rhs");
  const std::size_t output_elements =
      checked_product(m, n, "matmul_cuda_runtime output");
  checked_int_count(lhs_elements, "matmul_cuda_runtime lhs");
  checked_int_count(rhs_elements, "matmul_cuda_runtime rhs");
  const int total = checked_int_count(output_elements,
                                      "matmul_cuda_runtime output");
  if (a.size() != lhs_elements) {
    throw std::invalid_argument("matmul_cuda_runtime lhs size mismatch");
  }
  if (b.size() != rhs_elements) {
    throw std::invalid_argument("matmul_cuda_runtime rhs size mismatch");
  }

  DeviceBuffer device_a(a.size());
  DeviceBuffer device_b(b.size());
  DeviceBuffer device_output(output_elements);
  copy_to_device(device_a, a);
  copy_to_device(device_b, b);

  constexpr int kBlockSize = 256;
  const int grid = ceil_div_int(total, kBlockSize);
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  matmul_kernel<<<grid, kBlockSize>>>(device_a.data(), device_b.data(),
                                     device_output.data(), m, k, n);
  check_cuda(cudaGetLastError(), "matmul_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> matmul_tiled_cuda_runtime(const std::vector<float>& a,
                                             const int m, const int k,
                                             const std::vector<float>& b,
                                             const int n) {
  return matmul_tiled_cuda_runtime_profiled(a, m, k, b, n).output;
}

CudaMatrixOpResult matmul_tiled_cuda_runtime_profiled(
    const std::vector<float>& a, const int m, const int k,
    const std::vector<float>& b, const int n) {
  if (m <= 0 || k <= 0 || n <= 0) {
    throw std::invalid_argument(
        "matmul_tiled_cuda_runtime dimensions must be positive");
  }
  const std::size_t lhs_elements =
      checked_product(m, k, "matmul_tiled_cuda_runtime lhs");
  const std::size_t rhs_elements =
      checked_product(k, n, "matmul_tiled_cuda_runtime rhs");
  const std::size_t output_elements =
      checked_product(m, n, "matmul_tiled_cuda_runtime output");
  checked_int_count(lhs_elements, "matmul_tiled_cuda_runtime lhs");
  checked_int_count(rhs_elements, "matmul_tiled_cuda_runtime rhs");
  checked_int_count(output_elements, "matmul_tiled_cuda_runtime output");
  if (a.size() != lhs_elements) {
    throw std::invalid_argument("matmul_tiled_cuda_runtime lhs size mismatch");
  }
  if (b.size() != rhs_elements) {
    throw std::invalid_argument("matmul_tiled_cuda_runtime rhs size mismatch");
  }

  DeviceBuffer device_a(a.size());
  DeviceBuffer device_b(b.size());
  DeviceBuffer device_output(output_elements);
  copy_to_device(device_a, a);
  copy_to_device(device_b, b);

  const dim3 block(kMatmulTileSize, kMatmulTileSize);
  const dim3 grid(ceil_div_int(n, kMatmulTileSize),
                  ceil_div_int(m, kMatmulTileSize));
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  matmul_tiled_kernel<<<grid, block>>>(device_a.data(), device_b.data(),
                                      device_output.data(), m, k, n);
  check_cuda(cudaGetLastError(), "matmul_tiled_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> row_softmax_cuda_runtime(const std::vector<float>& matrix,
                                            const int rows, const int cols) {
  return row_softmax_cuda_runtime_profiled(matrix, rows, cols).output;
}

CudaMatrixOpResult row_softmax_cuda_runtime_profiled(
    const std::vector<float>& matrix, const int rows, const int cols) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(
        "row_softmax_cuda_runtime dimensions must be positive");
  }
  const std::size_t matrix_elements =
      checked_product(rows, cols, "row_softmax_cuda_runtime matrix");
  checked_int_count(matrix_elements, "row_softmax_cuda_runtime matrix");
  if (matrix.size() != matrix_elements) {
    throw std::invalid_argument("row_softmax_cuda_runtime matrix size mismatch");
  }

  DeviceBuffer device_matrix(matrix.size());
  DeviceBuffer device_output(matrix.size());
  copy_to_device(device_matrix, matrix);

  constexpr int kBlockSize = 128;
  const int grid = ceil_div_int(rows, kBlockSize);
  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  row_softmax_kernel<<<grid, kBlockSize>>>(device_matrix.data(),
                                           device_output.data(), rows, cols);
  check_cuda(cudaGetLastError(), "row_softmax_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

std::vector<float> row_softmax_block_cuda_runtime(
    const std::vector<float>& matrix, const int rows, const int cols) {
  return row_softmax_block_cuda_runtime_profiled(matrix, rows, cols).output;
}

CudaMatrixOpResult row_softmax_block_cuda_runtime_profiled(
    const std::vector<float>& matrix, const int rows, const int cols) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(
        "row_softmax_block_cuda_runtime dimensions must be positive");
  }
  const std::size_t matrix_elements =
      checked_product(rows, cols, "row_softmax_block_cuda_runtime matrix");
  checked_int_count(matrix_elements, "row_softmax_block_cuda_runtime matrix");
  if (matrix.size() != matrix_elements) {
    throw std::invalid_argument(
        "row_softmax_block_cuda_runtime matrix size mismatch");
  }

  DeviceBuffer device_matrix(matrix.size());
  DeviceBuffer device_output(matrix.size());
  copy_to_device(device_matrix, matrix);

  CudaEvent start;
  CudaEvent stop;
  check_cuda(cudaEventRecord(start.get(), nullptr), "cudaEventRecord start");
  row_softmax_block_kernel<<<rows, kReductionBlockSize>>>(
      device_matrix.data(), device_output.data(), rows, cols);
  check_cuda(cudaGetLastError(), "row_softmax_block_kernel launch");
  check_cuda(cudaEventRecord(stop.get(), nullptr), "cudaEventRecord stop");
  check_cuda(cudaEventSynchronize(stop.get()), "cudaEventSynchronize stop");
  float elapsed_ms = 0.0F;
  check_cuda(cudaEventElapsedTime(&elapsed_ms, start.get(), stop.get()),
             "cudaEventElapsedTime");
  check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
  return CudaMatrixOpResult{copy_to_host(device_output), elapsed_ms};
}

}  // namespace inferlab
