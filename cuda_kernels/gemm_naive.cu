// Minimal CUDA naive GEMM kernel for InferLab profiling experiments.
// C[row, col] = sum_k A[row, k] * B[k, col], row-major matrices.

extern "C" __global__ void inferlab_gemm_naive_f32(
    const float* a, const float* b, float* c, int m, int n, int k) {
  int row = blockIdx.y * blockDim.y + threadIdx.y;
  int col = blockIdx.x * blockDim.x + threadIdx.x;
  if (row >= m || col >= n) return;
  float acc = 0.0f;
  for (int kk = 0; kk < k; ++kk) {
    acc += a[row * k + kk] * b[kk * n + col];
  }
  c[row * n + col] = acc;
}
