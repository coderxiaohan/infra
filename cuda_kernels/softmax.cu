// Row-wise softmax kernel skeleton. One block handles one row.
// Launch contract: dynamic shared memory must provide at least blockDim.x
// floats. The production runtime wrapper uses
// cuda_kernels/matrix_ops_runtime.cu for checked launches; this skeleton is kept
// as a low-level kernel entry for external experiments.

#include <cuda_runtime.h>

extern "C" __global__ void inferlab_row_softmax_f32(
    const float* input, float* output, int rows, int cols) {
  int row = blockIdx.x;
  if (row >= rows) return;

  extern __shared__ float scratch[];
  float local_max = -3.402823466e+38F;
  for (int col = threadIdx.x; col < cols; col += blockDim.x) {
    float value = input[row * cols + col];
    local_max = value > local_max ? value : local_max;
  }
  scratch[threadIdx.x] = local_max;
  __syncthreads();

  for (int active = blockDim.x; active > 1; active = (active + 1) / 2) {
    const int next_active = (active + 1) / 2;
    const int paired = active - next_active;
    if (threadIdx.x < paired) {
      float other = scratch[threadIdx.x + next_active];
      scratch[threadIdx.x] = other > scratch[threadIdx.x] ? other : scratch[threadIdx.x];
    }
    __syncthreads();
  }
  float row_max = scratch[0];

  float local_sum = 0.0f;
  for (int col = threadIdx.x; col < cols; col += blockDim.x) {
    float e = expf(input[row * cols + col] - row_max);
    output[row * cols + col] = e;
    local_sum += e;
  }
  scratch[threadIdx.x] = local_sum;
  __syncthreads();
  for (int active = blockDim.x; active > 1; active = (active + 1) / 2) {
    const int next_active = (active + 1) / 2;
    const int paired = active - next_active;
    if (threadIdx.x < paired) scratch[threadIdx.x] += scratch[threadIdx.x + next_active];
    __syncthreads();
  }
  float row_sum = scratch[0];
  for (int col = threadIdx.x; col < cols; col += blockDim.x) {
    output[row * cols + col] /= row_sum;
  }
}
