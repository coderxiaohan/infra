#include <cuda_runtime.h>

__global__ void vector_add_kernel(const float* a, const float* b, float* out,
                                  int n) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < n) {
    out[idx] = a[idx] + b[idx];
  }
}

extern "C" void inferlab_vector_add_cuda(const float* a, const float* b,
                                         float* out, int n,
                                         cudaStream_t stream) {
  if (n <= 0) {
    return;
  }
  constexpr int kBlockSize = 256;
  const int grid = n / kBlockSize + (n % kBlockSize == 0 ? 0 : 1);
  vector_add_kernel<<<grid, kBlockSize, 0, stream>>>(a, b, out, n);
}
